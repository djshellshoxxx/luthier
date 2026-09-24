/*  preset-browser-previews.md 16: rendering, playback safety, cache (PB-01 to
    PB-19) and the library/file tests (PB-25, PB-26).

    The factory bank is rendered once per run (PresetBrowserTestHelpers.h) and
    shared with PresetSearchTests.
*/

#include "PresetBrowserTestHelpers.h"
#include "../Presets/Preview/PreviewCache.h"
#include "../Presets/Preview/PreviewRenderService.h"
#include "../Presets/PresetLibrary.h"
#include "../Presets/PresetLibraryPrefs.h"
#include "../Support/ThreadProbe.h"

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::tests::browser;

namespace
{
    /** 40 log-spaced bands, 50 Hz - 16 kHz, in dB (PB-02).

        Tuned from 20 kHz (recorded in docs/coverage/FEAT-BROWSER.md): Vorbis
        low-passes its input near 18 kHz at q0.5, so the top band of a
        50 Hz - 20 kHz split measures the encoder's lowpass (-47 dB of a band
        holding almost no energy), not the preset. Below 16 kHz every band of
        every factory clip agrees within about 0.5 dB. */
    std::array<double, 40> bandSpectrum (const juce::AudioBuffer<float>& clip, double sampleRate)
    {
        constexpr int order = 12, size = 1 << order;
        juce::dsp::FFT fft (order);
        std::array<double, 40> bands {};
        std::vector<float> frame ((size_t) size * 2);
        const int n = clip.getNumSamples();

        for (int start = 0; start + size <= n; start += size / 2)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);

            for (int i = 0; i < size; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / size);
                float s = 0.0f;

                for (int ch = 0; ch < clip.getNumChannels(); ++ch)
                    s += clip.getSample (ch, start + i);

                frame[(size_t) i] = s * w;
            }

            fft.performFrequencyOnlyForwardTransform (frame.data(), true);

            for (int b = 1; b < size / 2; ++b)
            {
                const double hz = b * sampleRate / size;

                if (hz < 50.0 || hz >= 16000.0)
                    continue;

                const int band = juce::jlimit (0, 39, (int) (40.0 * std::log (hz / 50.0) / std::log (320.0)));
                bands[(size_t) band] += (double) frame[(size_t) b] * frame[(size_t) b];
            }
        }

        double loudest = 1.0e-12;

        for (auto b : bands)
            loudest = juce::jmax (loudest, b);

        // Bands more than 50 dB under the loudest are floored there: they are
        // masked, inaudible, and at the codec's noise-fill floor, so their dB
        // value measures Vorbis rather than the preset (a dark acoustic's top
        // octave otherwise swings by tens of dB between two encodes).
        for (auto& b : bands)
            b = 10.0 * std::log10 (juce::jmax (b, loudest * 1.0e-5));

        return bands;
    }

    juce::File factoryPresetFile (LuthierAudioProcessor& p, const juce::String& name)
    {
        const int i = p.getPresetManager().indexOfPreset (name);
        return i >= 0 ? p.getPresetManager().getPreset (i)->file : juce::File();
    }

    /** A decoded 48 kHz stereo clip of a constant tone, for the player tests. */
    std::unique_ptr<PreviewClip> toneClip (double rate, double seconds, float level, double hz = 440.0,
                                           const juce::String& key = "tone")
    {
        auto c = std::make_unique<PreviewClip>();
        c->sampleRate = rate;
        c->key = key;
        c->name = key;
        c->audio.setSize (2, (int) (rate * seconds));

        for (int i = 0; i < c->audio.getNumSamples(); ++i)
        {
            const float v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
            c->audio.setSample (0, i, v);
            c->audio.setSample (1, i, v);
        }

        return c;
    }

    PresetLibrary& libraryWithScratchCache (LuthierAudioProcessor& p, const juce::File&)
    {
        return p.getPresetLibrary();
    }
}

//==============================================================================
// Rendering
//==============================================================================

/*  PB-01: the same preset twice gives bit-identical floats and Ogg bytes, from
    the same renderer and from a fresh one. */
LUTHIER_TEST (PresetPreview, PB01_renderingIsBitIdentical)
{
    PreviewRenderer a, b;
    const auto& corpus = factoryCorpus();
    const auto* crunch = corpus.find ("Single-Cut Crunch");
    CHECK (crunch != nullptr);

    if (crunch == nullptr)
        return;

    const auto first = a.render (crunch->json);
    const auto second = a.render (crunch->json);
    const auto fresh = b.render (crunch->json);

    CHECK (first.ok && second.ok && fresh.ok);

    for (const auto* other : { &second, &fresh })
    {
        CHECK (other->clip.getNumSamples() == first.clip.getNumSamples());

        bool identical = other->clip.getNumSamples() == first.clip.getNumSamples();

        for (int ch = 0; identical && ch < 2; ++ch)
            identical = std::memcmp (first.clip.getReadPointer (ch), other->clip.getReadPointer (ch),
                                     sizeof (float) * (size_t) first.clip.getNumSamples()) == 0;

        CHECK_MSG (identical, "the float buffers differ");
        CHECK_MSG (other->ogg == first.ogg, "the Ogg bytes differ");
    }

    // And equal to the corpus render made at the start of the run.
    CHECK (first.ogg == crunch->result.ogg);
}

/*  PB-02: the shipped clips match fresh renders (40-band log spectrum within
    1.0 dB RMS, loudness within 0.5 LU), and each manifest hash is the hash of
    FactoryPresets::toVar. The shipped folder is the one luthier-render
    --render-previews writes (FactoryPreviews::write), made here from a
    separate render of the bank. */
LUTHIER_TEST (PresetPreview, PB02_shippedClipsMatchFreshRenders)
{
    ScratchFolder scratch;
    const auto& corpus = factoryCorpus();

    juce::String error;
    CHECK_MSG (FactoryPreviews::write (corpus.bank, scratch.folder, error), error);

    const auto manifest = juce::JSON::parse (scratch.folder.getChildFile ("Previews/previews.json"));
    auto* entries = manifest.getProperty ("entries", {}).getArray();
    CHECK (entries != nullptr && entries->size() == FactoryPresets::getNumPresets());

    if (entries == nullptr)
        return;

    PreviewRenderer fresh;
    auto& ranges = fresh.getRangeSource();
    PresetFeatureReader reader (ranges);

    for (const auto& e : *entries)
    {
        const auto name = e.getProperty ("name", {}).toString();
        const auto* rendered = corpus.find (name);
        CHECK (rendered != nullptr);

        if (rendered == nullptr)
            continue;

        // The manifest hash is the hash of FactoryPresets::toVar.
        int defIndex = -1;

        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (name == FactoryPresets::getPreset (i).name)
                defIndex = i;

        const auto json = FactoryPresets::toVar (FactoryPresets::getPreset (defIndex), ranges);
        const auto phrase = PreviewRenderer::choosePhrase (json, reader.read (json));
        CHECK_MSG (e.getProperty ("soundHash", {}).toString() == PreviewRenderer::computeSoundHash (json, phrase), name);

        // The shipped (decoded) clip against a fresh render.
        juce::MemoryBlock ogg;
        CHECK (scratch.folder.getChildFile ("Previews").getChildFile (e.getProperty ("file", {}).toString()).loadFileAsData (ogg));

        juce::AudioBuffer<float> shipped;
        double rate = 0.0;
        CHECK_MSG (PreviewRenderer::decodeOgg (ogg.getData(), ogg.getSize(), shipped, rate), name);

        const auto now = fresh.render (json);
        CHECK_MSG (now.ok, name + ": " + now.error);

        const auto a = bandSpectrum (shipped, rate);
        const auto b = bandSpectrum (now.clip, PreviewRenderer::kSampleRate);
        double sum = 0.0;

        for (size_t i = 0; i < a.size(); ++i)
            sum += (a[i] - b[i]) * (a[i] - b[i]);

        const double rms = std::sqrt (sum / (double) a.size());
        CHECK_MSG (rms <= 1.0, name + ": spectrum differs by " + juce::String (rms, 2) + " dB RMS");

        const double loudnessShipped = ToneFeatures::integratedLoudness (shipped, rate);
        const double loudnessFresh = ToneFeatures::integratedLoudness (now.clip, PreviewRenderer::kSampleRate);
        CHECK_MSG (std::abs (loudnessShipped - loudnessFresh) <= 0.5,
                   name + ": " + juce::String (loudnessShipped, 2) + " vs " + juce::String (loudnessFresh, 2) + " LUFS");
    }

    // The calibration file is written beside the previews.
    CHECK (DescriptorCalibration::fromVar (juce::JSON::parse (scratch.folder.getChildFile ("descriptor-calibration.json"))).isValid());
}

/*  PB-03: 2.0-4.0 s, -18 +/-0.5 LUFS unless peak-bound, true peak <= -3 dBTP,
    last 10 ms under -60 dBFS, and 64 KB or less. */
LUTHIER_TEST (PresetPreview, PB03_everyClipMeetsTheLoudnessAndLengthRules)
{
    for (const auto& r : factoryCorpus().bank)
    {
        CHECK_MSG (r.result.ok, r.name + ": " + r.result.error);

        if (! r.result.ok)
            continue;

        const auto& clip = r.result.clip;
        const double seconds = clip.getNumSamples() / PreviewRenderer::kSampleRate;
        CHECK_MSG (seconds >= 2.0 - 1.0e-9 && seconds <= 4.0 + 1.0e-9, r.name + ": " + juce::String (seconds, 3) + " s");

        const double truePeak = ToneFeatures::truePeakDb (clip);
        CHECK_MSG (truePeak <= -3.0 + 0.01, r.name + ": true peak " + juce::String (truePeak, 2));

        const double lufs = ToneFeatures::integratedLoudness (clip, PreviewRenderer::kSampleRate);
        const bool peakBound = truePeak > -3.2;
        CHECK_MSG (peakBound || std::abs (lufs + 18.0) <= 0.5, r.name + ": " + juce::String (lufs, 2) + " LUFS");

        const int tail = (int) (0.010 * PreviewRenderer::kSampleRate);
        const float lastPeak = clip.getMagnitude (clip.getNumSamples() - tail, tail);
        CHECK_MSG (juce::Decibels::gainToDecibels (lastPeak, -200.0f) < -60.0f, r.name);

        CHECK_MSG ((int) r.result.ogg.getSize() <= PreviewRenderer::kMaxOggBytes,
                   r.name + ": " + juce::String ((int) r.result.ogg.getSize()) + " bytes");
    }

    // 11: the shipped factory previews fit in 3 MB.
    size_t total = 0;

    for (const auto& r : factoryCorpus().bank)
        total += r.result.ogg.getSize();

    CHECK_MSG (total <= 3u * 1024u * 1024u, juce::String ((int) total) + " bytes");
}

/*  PB-04: the phrase choice. */
LUTHIER_TEST (PresetPreview, PB04_phrasesAreChosenCorrectly)
{
    const auto& corpus = factoryCorpus();

    auto phraseOf = [&] (const char* name)
    {
        const auto* r = corpus.find (name);
        return r != nullptr ? juce::String (PreviewPhrase::getIdString (r->result.phrase)) : juce::String ("missing");
    };

    CHECK (phraseOf ("Modern Metal Chug") == "highgain_riff");
    CHECK (phraseOf ("8-String Djent") == "highgain_riff");
    CHECK (phraseOf ("Strummed Dreadnought") == "acoustic_strum");
    CHECK (phraseOf ("P-Bass Flatwound") == "bass_groove");
    CHECK (phraseOf ("Flamenco Rasgueado") == "rasgueado");
    CHECK (phraseOf ("Blues Slide") == "slide_lick");

    // A previewPhrase field overrides the choice.
    const auto* metal = corpus.find ("Modern Metal Chug");
    CHECK (metal != nullptr);

    if (metal != nullptr)
    {
        auto json = juce::JSON::parse (juce::JSON::toString (metal->json));
        json.getDynamicObject()->setProperty ("previewPhrase", "jazz_comp");

        PreviewRenderer renderer;
        PresetFeatureReader reader (renderer.getRangeSource());
        CHECK (PreviewRenderer::choosePhrase (json, reader.read (json)) == PreviewPhrase::Id::jazz_comp);

        // And it changes the sound hash (5.1 item 4), though the field itself does not.
        CHECK (PreviewRenderer::computeSoundHash (json, PreviewPhrase::Id::jazz_comp) != metal->result.soundHash);
    }

    // Every phrase stays within 3.2 s of notes.
    PreviewPhrase::Context c;

    for (int i = 0; i < (int) PreviewPhrase::Id::NumIds; ++i)
    {
        const auto built = PreviewPhrase::build ((PreviewPhrase::Id) i, c);
        CHECK_MSG (built.noteEndSeconds <= PreviewPhrase::kMaxNoteSeconds + 1.0e-9, PreviewPhrase::getIdString ((PreviewPhrase::Id) i));
        CHECK (built.sequence.getNumEvents() > 0);
    }
}

/*  PB-05: the phrase follows the guitar. */
LUTHIER_TEST (PresetPreview, PB05_theLowestNoteFollowsTheTuning)
{
    const auto& corpus = factoryCorpus();
    const auto* dropC = corpus.find ("Drop C Riff");
    const auto* lowB = corpus.find ("5-String Low B");
    CHECK (dropC != nullptr && lowB != nullptr);

    if (dropC != nullptr)
        CHECK_MSG (dropC->result.lowestNote == 36, "Drop C Riff: " + juce::String (dropC->result.lowestNote));   // C2

    if (lowB != nullptr)
        CHECK_MSG (lowB->result.lowestNote == 23, "5-String Low B: " + juce::String (lowB->result.lowestNote));  // B0
}
