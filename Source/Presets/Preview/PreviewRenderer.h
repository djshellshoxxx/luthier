#pragma once

/*  preset-browser-previews.md 3.2: renders one preset's preview.

    No UI, worker thread only, shared by the plugin and luthier-render. Each job
    resets a private offline instance of the plugin, loads the preset through
    that instance's own PresetManager (so the fallbacks are a live load's),
    plays the chosen phrase through the real engine, measures it, normalises it
    to -18 LUFS under a -3 dBTP ceiling and encodes it as Ogg Vorbis.

    The live instance is never touched: the offline instance is a separate
    LuthierAudioProcessor with its timer stopped and its library hooks off.
*/

#include "PreviewPhrase.h"
#include "ToneFeatures.h"
#include "../Search/PresetFeatures.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** A finished preview: what the cache stores and the player plays. */
struct PreviewResult
{
    static constexpr int kNumPeaks = 64;

    bool ok = false;
    bool timedOut = false;
    bool cancelled = false;
    bool approximate = false;     ///< rendered with a fallback guitar (7.3 "~")
    juce::String error;

    juce::String soundHash;
    PreviewPhrase::Id phrase = PreviewPhrase::Id::clean_arp;
    int lowestNote = 0;

    juce::AudioBuffer<float> clip;          ///< 48 kHz stereo, loudness gain applied
    juce::MemoryBlock ogg;
    std::array<float, kNumPeaks> peaks {};

    ToneFeatures features;
    PresetFeatures params;
    double gainDb = 0.0;
    double renderMs = 0.0;
};

//==============================================================================
class PreviewRenderer
{
public:
    static constexpr int kRenderRevision = 1;          ///< 5.1 item 6
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 256;
    static constexpr double kSettleSeconds = 0.5;
    static constexpr double kTargetLufs = -18.0;
    static constexpr double kCeilingDbtp = -3.0;
    static constexpr int kMaxOggBytes = 64 * 1024;
    static constexpr double kTimeoutSeconds = 10.0;

    using Factory = std::function<std::unique_ptr<LuthierAudioProcessor>()>;

    /** Without a factory the renderer makes plain LuthierAudioProcessors. */
    explicit PreviewRenderer (Factory factory = {});
    ~PreviewRenderer();

    /** Renders a preset file. `cancel` is polled every block. */
    PreviewResult render (const juce::File& presetFile, const std::atomic<bool>* cancel = nullptr,
                          double timeoutSeconds = kTimeoutSeconds);

    /** Renders preset data by writing it to a private temporary file first, so
        the load is still the instance's own loadPreset (File). */
    PreviewResult render (const juce::var& presetJson, const std::atomic<bool>* cancel = nullptr,
                          double timeoutSeconds = kTimeoutSeconds);

    /** The offline instance is created lazily and kept for the next job. */
    bool hasInstance() const noexcept { return instance != nullptr; }
    void releaseInstance();

    /** The instance, created if needed (tests use it for the parameter ranges). */
    LuthierAudioProcessor& getInstance();

    /** Hands over an instance made elsewhere: the service makes it on the
        message thread, because the constructor touches user-global singletons
        the UI reads. */
    void adoptInstance (std::unique_ptr<LuthierAudioProcessor>);
    std::unique_ptr<LuthierAudioProcessor> takeInstance();

    /** A render instance kept only for its parameter ranges (building preset
        JSON, reading features); never replaced by a render, unlike the
        per-job instance behind getInstance(). */
    LuthierAudioProcessor& getRangeSource();

    /** Makes an instance the way this renderer would (any thread). */
    std::unique_ptr<LuthierAudioProcessor> makeInstance() const;

    /** Tests: slows every block down, to force the 10 s abandonment. */
    std::atomic<double> testDelayPerBlockMs { 0.0 };

    //==========================================================================
    /** 5.1: the sound hash, 64 hex digits. */
    static juce::String computeSoundHash (const juce::var& presetJson, PreviewPhrase::Id phrase,
                                          const juce::String& pluginVersion = JucePlugin_VersionString,
                                          int renderRevision = kRenderRevision);

    /** The canonical JSON of 5.1 item 1: sorted keys, %.9g numbers, the
        naming and library fields removed. */
    static juce::String canonicalSoundJson (const juce::var& presetJson);

    /** 3.1: the phrase for a preset. */
    static PreviewPhrase::Context phraseContext (const juce::var& presetJson, const PresetFeatures&);
    static PreviewPhrase::Id choosePhrase (const juce::var& presetJson, const PresetFeatures&);

    /** 64-point peak envelope of a clip. */
    static std::array<float, PreviewResult::kNumPeaks> computePeaks (const juce::AudioBuffer<float>&);

    /** Ogg Vorbis at q0.5 and below until it fits 64 KB, with a fixed stream
        serial so identical audio encodes to identical bytes. */
    static juce::MemoryBlock encodeOgg (const juce::AudioBuffer<float>&, double sampleRate, juce::uint32 serial);
    static bool decodeOgg (const void* data, size_t size, juce::AudioBuffer<float>& out, double& sampleRate);

    /** The 250 ms end fade, ending in true silence (3.1). */
    static void applyEndFade (juce::AudioBuffer<float>&, double sampleRate);

private:
    PreviewResult renderLoaded (const juce::File& file, const std::atomic<bool>* cancel, double timeoutSeconds);

    Factory factory;
    std::unique_ptr<LuthierAudioProcessor> instance, rangeInstance;
    std::unique_ptr<PresetFeatureReader> reader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewRenderer)
};

} // namespace luthier
