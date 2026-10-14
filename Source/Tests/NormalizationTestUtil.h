#pragma once

/*  Shared plumbing for the output-normalization tests (output-normalization.md
    15): a fresh, prepared processor per rendered combination, the factory
    preset x guitar type grid, the two phrases ON-02 hashes, and SHA-256 of the
    float main output.

    The golden hashes (Tests/Golden/NormalizationOffHashes.json) are produced
    by this file's renderer, so anything that changes how a combination is set
    up here changes every hash: regenerate them with
    scripts/regen_normalization_hashes.sh after such a change.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Presets/PresetManager.h"
#include "../Presets/FactoryPresets.h"
#include "../Support/NormalizationPhrase.h"

#include <juce_cryptography/juce_cryptography.h>

#include <functional>
#include <memory>

namespace luthier::normtest
{

inline constexpr double kSr    = 48000.0;
inline constexpr int    kBlock = 256;

/** ON-02's two phrases. */
enum class GoldenPhrase { normalization, comboChord };

inline const char* goldenPhraseName (GoldenPhrase p)
{
    return p == GoldenPhrase::normalization ? "phrase" : "chord";
}

/** -1 means "the preset's own guitar". */
struct Combo
{
    int preset = 0;
    int guitarType = -1;
    GoldenPhrase phrase = GoldenPhrase::normalization;

    juce::String key() const
    {
        return "p" + juce::String (preset).paddedLeft ('0', 2)
             + "_g" + (guitarType < 0 ? juce::String ("own") : juce::String (guitarType).paddedLeft ('0', 2))
             + "_" + goldenPhraseName (phrase);
    }
};

inline int numGuitarTypes() { return (int) GuitarType::NumTypes; }
inline int numFactoryPresets() { return FactoryPresets::getNumPresets(); }

/*  A guitar type the running edition actually ships. The factory normalization
    table is generated for the Pro catalogue; in a Free binary the Pro-only
    guitars are gated and a combination built on one renders a different sound
    whose hash is not in the table. So the combo grids below skip guitars the
    current (compile-time) edition does not have, and every combination they do
    produce is one the table knows. */
inline bool editionHasGuitar (int guitarType) noexcept
{
    return luthier::edition::isPro || luthier::edition::isFreeGuitarIndex (guitarType);
}

/** A fresh processor, prepared at 48 kHz / 256. */
inline std::unique_ptr<LuthierAudioProcessor> makeProcessor (double sr = kSr, int block = kBlock)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (sr, block);
    return p;
}

inline bool setPlain (LuthierAudioProcessor& p, const juce::String& id, float plain)
{
    if (auto* prm = p.getState().getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        return true;
    }

    return false;
}

/** Loads factory preset `index` (by name, so user presets on the machine
    cannot shift the index) and, when asked, a guitar type on top. */
inline bool loadCombo (LuthierAudioProcessor& p, int presetIndex, int guitarType)
{
    auto& presets = p.getPresetManager();
    const auto& def = FactoryPresets::getPreset (presetIndex);
    const int index = presets.indexOfPreset (def.name);

    if (index < 0 || ! presets.loadPreset (index))
        return false;

    p.getParameterBridge().applyAllNow();

    if (guitarType >= 0)
    {
        setPlain (p, ParamIDs::guitarType, (float) guitarType);
        p.getParameterBridge().applyAllNow();
    }

    return true;
}

/** The phrase as sample-time events. */
inline juce::MidiBuffer phraseEvents (LuthierAudioProcessor& p, GoldenPhrase phrase, double sr, int& lengthSamples)
{
    juce::MidiBuffer events;

    if (phrase == GoldenPhrase::normalization)
    {
        auto& engine = p.getEngine();
        const bool bass = engine.getGuitarSpec().category == GuitarCategory::Bass;
        events = NormalizationPhrase::buildBuffer (bass, NormalizationPhrase::rootNoteFor (engine),
                                                   engine.getNumStrings(), sr);
        lengthSamples = (int) (NormalizationPhrase::kWindowSeconds * sr);
    }
    else
    {
        // ComboHarness Phrase::chord: an open E chord held 0.8 s, then a tail.
        const int notes[] = { 40, 47, 52, 56, 59, 64 };
        for (int n : notes) events.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 96), 0);
        for (int n : notes) events.addEvent (juce::MidiMessage::noteOff (1, n), (int) (0.8 * sr));
        lengthSamples = (int) (1.6 * sr);
    }

    return events;
}

/** Renders `lengthSamples` of main output (interleaved L/R floats) with the
    events given in sample time. `perBlock` runs before each block with the
    block's start sample. */
inline std::vector<float> renderEvents (LuthierAudioProcessor& p, const juce::MidiBuffer& events,
                                        int lengthSamples, int block = kBlock,
                                        std::function<void (int)> perBlock = {})
{
    std::vector<float> out;
    out.reserve ((size_t) lengthSamples * 2);

    const int channels = juce::jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, block);

    for (int pos = 0; pos < lengthSamples; pos += block)
    {
        const int n = juce::jmin (block, lengthSamples - pos);

        if (perBlock)
            perBlock (pos);

        buffer.setSize (channels, n, false, false, true);
        buffer.clear();

        juce::MidiBuffer midi;
        midi.addEvents (events, pos, n, -pos);

        p.processBlock (buffer, midi);

        for (int i = 0; i < n; ++i)
        {
            out.push_back (buffer.getSample (0, i));
            out.push_back (buffer.getSample (1, i));
        }
    }

    return out;
}

inline void renderSilence (LuthierAudioProcessor& p, double seconds, double sr = kSr, int block = kBlock)
{
    renderEvents (p, {}, (int) (seconds * sr), block);
}

inline juce::String sha256 (const std::vector<float>& samples)
{
    return juce::SHA256 (samples.data(), samples.size() * sizeof (float)).toHexString();
}

/** ON-02's unit of work: a fresh processor, the combination loaded, 0.5 s of
    settling silence, then the phrase. `setup` runs after the load. */
inline std::vector<float> renderCombo (const Combo& combo,
                                       std::function<void (LuthierAudioProcessor&)> setup = {})
{
    auto p = makeProcessor();

    if (! loadCombo (*p, combo.preset, combo.guitarType))
        return {};

    if (setup)
        setup (*p);

    renderSilence (*p, NormalizationPhrase::kSettleSeconds);

    int length = 0;
    const auto events = phraseEvents (*p, combo.phrase, kSr, length);
    return renderEvents (*p, events, length);
}

/** The grid ON-02 covers. The full grid (36 x 26 x 2) takes a long time, so
    the default run uses a fixed subset: every preset on its own guitar, and
    the first preset on every guitar type, both phrases. LUTHIER_SLOW_TESTS=1
    runs the full grid. */
inline std::vector<Combo> goldenGrid (bool full)
{
    std::vector<Combo> grid;

    for (int ph = 0; ph < 2; ++ph)
    {
        const auto phrase = (GoldenPhrase) ph;

        if (full)
        {
            for (int pr = 0; pr < numFactoryPresets(); ++pr)
                for (int g = 0; g < numGuitarTypes(); ++g)
                    if (editionHasGuitar (g))
                        grid.push_back ({ pr, g, phrase });
        }
        else
        {
            for (int pr = 0; pr < numFactoryPresets(); ++pr)
                grid.push_back ({ pr, -1, phrase });

            for (int g = 0; g < numGuitarTypes(); ++g)
                if (editionHasGuitar (g))
                    grid.push_back ({ 0, g, phrase });
        }
    }

    return grid;
}

inline bool slowTestsEnabled()
{
    return juce::SystemStats::getEnvironmentVariable ("LUTHIER_SLOW_TESTS", {}).getIntValue() > 0;
}

inline juce::File goldenFile()
{
    return juce::File (__FILE__).getParentDirectory().getChildFile ("Golden").getChildFile ("NormalizationOffHashes.json");
}

/** What the hashes are only valid for: bit-exact float output depends on the
    compiler and the target, so a hash file is checked only on the toolchain
    that wrote it. */
inline juce::String toolchainFingerprint()
{
   #if defined (__clang__)
    juce::String compiler = "clang-" + juce::String (__clang_major__);
   #elif defined (__GNUC__)
    juce::String compiler = "gcc-" + juce::String (__GNUC__);
   #elif defined (_MSC_VER)
    juce::String compiler = "msvc-" + juce::String (_MSC_VER);
   #else
    juce::String compiler = "unknown";
   #endif

   #if JUCE_LINUX
    compiler << "-linux";
   #elif JUCE_MAC
    compiler << "-mac";
   #elif JUCE_WINDOWS
    compiler << "-windows";
   #endif

   #if defined (__x86_64__) || defined (_M_X64)
    compiler << "-x64";
   #elif defined (__aarch64__) || defined (_M_ARM64)
    compiler << "-arm64";
   #endif

    return compiler;
}

} // namespace luthier::normtest

//==============================================================================
// Helpers that need the normalization code (NormalizationTests.cpp).
#include "../DSP/Master/Bs1770Meter.h"
#include "../DSP/Master/TruePeakDetector.h"

namespace luthier::normtest
{

/** Integrated loudness of interleaved stereo. */
inline double loudnessOf (const std::vector<float>& interleaved, double sr = kSr)
{
    const size_t n = interleaved.size() / 2;
    std::vector<float> l (n), r (n);

    for (size_t i = 0; i < n; ++i)
    {
        l[i] = interleaved[2 * i];
        r[i] = interleaved[2 * i + 1];
    }

    Bs1770Meter meter;
    meter.prepare (sr, 2);
    meter.process (l.data(), r.data(), (int) n);
    return meter.getIntegratedLufs();
}

inline double truePeakDbOf (const std::vector<float>& interleaved)
{
    const size_t n = interleaved.size() / 2;
    std::vector<float> ch (n);
    double peak = 0.0;

    for (int c = 0; c < 2; ++c)
    {
        for (size_t i = 0; i < n; ++i)
            ch[i] = interleaved[2 * i + (size_t) c];

        peak = std::max (peak, TruePeakDetector::measure (ch.data(), (int) n));
    }

    return peak > 1.0e-9 ? 20.0 * std::log10 (peak) : -200.0;
}

inline double samplePeakDbOf (const std::vector<float>& interleaved)
{
    double peak = 0.0;

    for (float v : interleaved)
        peak = std::max (peak, (double) std::abs (v));

    return peak > 1.0e-9 ? 20.0 * std::log10 (peak) : -200.0;
}

/** Normalization on (non-realtime, so every request is waited for) and
    settled: the request fires in the first block, the result arrives, the
    glide completes. */
inline void enableAndSettle (LuthierAudioProcessor& p, double target = -18.0, int block = kBlock)
{
    p.setNonRealtime (true);
    auto& n = p.getOutputNormalization();
    n.setTargetLufs (target);
    n.setEnabled (true);
    renderEvents (p, {}, (int) (0.6 * kSr), block);
}

/** Renders with the events fed as the engine's direct MIDI, the way the
    calibration render plays NormalizationPhrase (4.3): played as written. */
inline std::vector<float> renderDirect (LuthierAudioProcessor& p, const juce::MidiBuffer& events,
                                        int lengthSamples, int block = kBlock)
{
    juce::MidiBuffer slice;
    auto& n = p.getOutputNormalization();

    auto out = renderEvents (p, {}, lengthSamples, block, [&] (int pos)
    {
        slice.clear();
        slice.addEvents (events, pos, block, -pos);
        n.setCalibrationDirectMidi (slice.isEmpty() ? nullptr : &slice);
    });

    n.setCalibrationDirectMidi (nullptr);
    return out;
}

/** The phrase, played on the live instance as the calibration plays it
    (direct MIDI), measured: 15's definition of "loudness". */
inline double phraseLoudness (LuthierAudioProcessor& p, std::vector<float>* keep = nullptr)
{
    int length = 0;
    const auto events = phraseEvents (p, GoldenPhrase::normalization, kSr, length);
    auto out = renderDirect (p, events, length);
    const double l = loudnessOf (out);

    if (keep != nullptr)
        *keep = std::move (out);

    return l;
}

/** Test isolation: private disk cache, empty memory cache, a factory table
    that exists only in memory. */
struct IsolatedCaches
{
    juce::TemporaryFile folder;
    juce::TemporaryFile factory { ".json" };

    IsolatedCaches()
    {
        folder.getFile().createDirectory();
        NormalizationCalibrator::setDiskCacheFolderForTesting (folder.getFile());
        NormalizationCalibrator::setFactoryTableFileForTesting (factory.getFile());
        NormalizationCalibrator::clearMemoryCache();
    }

    ~IsolatedCaches()
    {
        NormalizationCalibrator::clearMemoryCache();
        NormalizationCalibrator::setDiskCacheFolderForTesting ({});
        NormalizationCalibrator::setFactoryTableFileForTesting ({});
        NormalizationCalibrator::failRendersForTesting().store (false);
        NormalizationCalibrator::maxInjectedDelayMsForTesting().store (0);
        folder.getFile().deleteRecursively();
    }
};

} // namespace luthier::normtest
