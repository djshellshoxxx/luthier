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

//==============================================================================
// Playback safety
//==============================================================================
#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
#endif

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** A processor prepared at 48 kHz / 256 with humanise off (deterministic),
        its preview cache in a scratch folder. */
    struct Rig
    {
        explicit Rig (bool perString = false, bool aux = false)
        {
            PreviewCache::setDefaultFolderOverride (scratch.folder.getChildFile ("cache"));
            PreviewRenderService::setShippedFolderOverride (scratch.folder.getChildFile ("no-shipped-previews"));
            PresetLibraryPrefs::get().setFile (scratch.folder.getChildFile ("preset-library.json"));

            if (auto* h = p.getState().getParameter (ParamIDs::macroHumanize))
                h->setValueNotifyingHost (0.0f);

            if (perString || aux)
            {
                auto layout = p.getBusesLayout();

                for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
                {
                    const bool isAux = bus - 1 < kNumAuxBuses;
                    const bool isString = ! isAux && bus < 1 + kNumAuxBuses + kNumPerStringBuses;
                    const bool on = isAux ? aux : (isString && perString);
                    layout.outputBuses.getReference (bus) = ! on ? juce::AudioChannelSet::disabled()
                                                          : isString ? juce::AudioChannelSet::mono()
                                                                     : juce::AudioChannelSet::stereo();
                }

                p.setBusesLayout (layout);
            }

            p.prepareToPlay (kSr, kBlock);
            buffer.setSize (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels())), kBlock);
        }

        ~Rig()
        {
            PreviewCache::setDefaultFolderOverride ({});
            PreviewRenderService::setShippedFolderOverride ({});
            PresetLibraryPrefs::get().setFile ({});
        }

        juce::AudioBuffer<float>& block (juce::MidiBuffer midi = {})
        {
            buffer.clear();
            lastMidi = midi;
            p.processBlock (buffer, lastMidi);
            return buffer;
        }

        const PreviewClip* clip (float level = 0.5f, double hz = 440.0, const juce::String& key = "tone")
        {
            return p.getPreviewClipPool().add (toneClip (kSr, 3.0, level, hz, key));
        }

        ScratchFolder scratch;
        LuthierAudioProcessor p;
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer lastMidi;
    };

    juce::String stateText (LuthierAudioProcessor& p)
    {
        return p.getState().copyState().toXmlString();
    }

    /** A DC clip on one channel only, so each voice can be told apart (PB-08). */
    const PreviewClip* dcClip (Rig& rig, int channel, float level)
    {
        auto c = std::make_unique<PreviewClip>();
        c->sampleRate = kSr;
        c->key = "dc" + juce::String (channel);
        c->audio.setSize (2, (int) kSr * 2);
        c->audio.clear();

        for (int i = 0; i < c->audio.getNumSamples(); ++i)
            c->audio.setSample (channel, i, level);

        return rig.p.getPreviewClipPool().add (std::move (c));
    }
}

/*  PB-06: 20 previews change no parameter, guitar, snapshot or undo entry. */
LUTHIER_TEST (PresetPreview, PB06_previewsLeaveTheLiveInstanceAlone)
{
    Rig rig;
    auto& library = rig.p.getPresetLibrary();
    library.refreshSynchronously();

    rig.p.captureSnapshot (0, "before");
    const auto params = stateText (rig.p);
    const auto guitar = juce::JSON::toString (rig.p.getGuitarBlock());
    const auto snapshots = juce::JSON::toString (rig.p.getSnapshots().toVar());
    const int undo = rig.p.getNumUndoSteps();

    // Decoded clips for 20 entries, as if their renders had arrived.
    auto audio = std::make_shared<juce::AudioBuffer<float>> (toneClip (kSr, 1.0, 0.2f)->audio);
    auto& index = library.getIndex();
    int played = 0;

    rig.block();

    for (int i = 0; i < index.size() && played < 20; ++i)
    {
        if (index[i].soundHash.isEmpty())
            continue;

        library.getService().injectDecodedForTesting (index[i].soundHash, audio);
        juce::String hint;
        CHECK_MSG (library.play (i, true, hint), hint);
        ++played;

        for (int b = 0; b < 8; ++b)
            rig.block();
    }

    CHECK (played == 20);
    CHECK (stateText (rig.p) == params);
    CHECK (juce::JSON::toString (rig.p.getGuitarBlock()) == guitar);
    CHECK (juce::JSON::toString (rig.p.getSnapshots().toVar()) == snapshots);
    CHECK (rig.p.getNumUndoSteps() == undo);
}

/*  PB-07: switching previews every 150 ms for 60 s allocates nothing on the
    audio thread. (The player holds no lock of any kind: its only shared
    state is atomics, so there is no mutex for a trap to catch.) */
LUTHIER_TEST (PresetPreview, PB07_switchingAllocatesNothing)
{
    PreviewPlayer player;
    PreviewClipPool pool;
    player.prepare (kSr);

    juce::AudioBuffer<float> out (2, kBlock);
    const int blocksPerSwitch = (int) std::round (0.150 * kSr / kBlock);
    const int totalBlocks = (int) (60.0 * kSr / kBlock);
    long allocations = 0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        if (b % blocksPerSwitch == 0)
        {
            // Message thread's side: make, hand over, collect.
            player.start (pool.add (toneClip (kSr, 0.5, 0.3f, 220.0 + (b % 7) * 30.0)));
            pool.collectGarbage (player);
        }

       #if defined (LUTHIER_ALLOCATION_COUNTER)
        const auto before = allocationsOnThisThread();
       #endif

        out.clear();
        player.processBlock (out, kBlock);

       #if defined (LUTHIER_ALLOCATION_COUNTER)
        allocations += allocationsOnThisThread() - before;
       #endif
    }

    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations on the audio thread");
    CHECK (pool.size() <= 3);   // freed as the voices let go
}

/*  PB-08: the fades. */
LUTHIER_TEST (PresetPreview, PB08_fades)
{
    PreviewPlayer player;
    PreviewClipPool pool;
    player.prepare (kSr);
    player.setVolumeDb (0.0);

    auto dc = [&pool] (int channel)
    {
        auto c = std::make_unique<PreviewClip>();
        c->audio.setSize (2, (int) kSr * 2);
        c->audio.clear();

        for (int i = 0; i < c->audio.getNumSamples(); ++i)
            c->audio.setSample (channel, i, 0.5f);

        return pool.add (std::move (c));
    };

    juce::AudioBuffer<float> out (2, 480);   // 10 ms blocks

    // The first 10 ms rises monotonically.
    player.start (dc (0));
    out.clear();
    player.processBlock (out, 480);

    bool monotonic = true;

    for (int i = 1; i < 480; ++i)
        monotonic = monotonic && out.getSample (0, i) >= out.getSample (0, i - 1);

    CHECK (monotonic);
    CHECK (out.getSample (0, 479) > 0.45f);

    // A switch: A (left) fades out before B (right) starts; never both above -60 dBFS.
    for (int b = 0; b < 10; ++b) { out.clear(); player.processBlock (out, 480); }

    player.start (dc (1));
    bool overlap = false;
    const float threshold = juce::Decibels::decibelsToGain (-60.0f);

    for (int b = 0; b < 20; ++b)
    {
        out.clear();
        player.processBlock (out, 480);

        for (int i = 0; i < 480; ++i)
            overlap = overlap || (std::abs (out.getSample (0, i)) > threshold && std::abs (out.getSample (1, i)) > threshold);
    }

    CHECK_MSG (! overlap, "the two clips sounded together");
    CHECK (out.getSample (1, 479) > 0.45f);   // B is playing

    // A stop reaches -90 dBFS within 30 ms.
    player.stop();
    const float silent = juce::Decibels::decibelsToGain (-90.0f);

    for (int b = 0; b < 3; ++b) { out.clear(); player.processBlock (out, 480); }

    out.clear();
    player.processBlock (out, 480);
    CHECK_MSG (out.getMagnitude (0, 480) < silent, juce::String (out.getMagnitude (0, 480)));
    CHECK (! player.isActive());
}

/*  PB-09: a held live note's per-string aux output is untouched by a preview. */
LUTHIER_TEST (PresetPreview, PB09_perStringOutputNullsAgainstNoPreview)
{
    Rig with (true), without (true);
    juce::MidiBuffer noteOn;
    noteOn.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);

    double worst = 0.0;

    for (int b = 0; b < 60; ++b)
    {
        if (b == 10)
            with.p.getPreviewPlayer().start (with.clip());

        auto& a = with.block (b == 0 ? noteOn : juce::MidiBuffer());
        auto& n = without.block (b == 0 ? noteOn : juce::MidiBuffer());

        for (int bus = 1 + kNumAuxBuses; bus < with.p.getBusCount (false); ++bus)
        {
            auto ba = with.p.getBusBuffer (a, false, bus);
            auto bn = without.p.getBusBuffer (n, false, bus);

            for (int ch = 0; ch < ba.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    worst = juce::jmax (worst, (double) std::abs (ba.getSample (ch, i) - bn.getSample (ch, i)));
        }
    }

    CHECK_MSG (juce::Decibels::gainToDecibels (worst, -300.0) < -120.0,
               "per-string difference " + juce::String (juce::Decibels::gainToDecibels (worst, -300.0), 1) + " dB");

    // And the preview is on the main output.
    CHECK (with.p.getPreviewPlayer().isActive());
}

/*  PB-10: the transport rules. */
LUTHIER_TEST (PresetPreview, PB10_transportRules)
{
    struct PlayingHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo i;
            i.setIsPlaying (playing);
            i.setBpm (120.0);
            return i;
        }

        bool playing = true;
    };

    Rig rig, twin;   // the twin never previews: the comparison below
    auto& library = rig.p.getPresetLibrary();
    PlayingHead head;
    rig.p.setPlayHead (&head);
    twin.p.setPlayHead (&head);
    rig.block();
    twin.block();

    CHECK (library.whyBlocked (false).isNotEmpty());   // hover
    CHECK (library.whyBlocked (true) == "Previews pause while the host plays");

    auto settings = library.getSettings();
    settings.whileTransport = true;
    library.setSettings (settings);
    CHECK (library.whyBlocked (false).isEmpty());
    CHECK (library.whyBlocked (true).isEmpty());

    head.playing = false;
    rig.block();
    twin.block();
    settings.whileTransport = false;
    library.setSettings (settings);
    CHECK (library.whyBlocked (false).isEmpty());

    // Non-realtime: never, and the audio thread enforces it too. The main
    // output is compared with a twin that has no preview, so the engine's own
    // idle noise does not count.
    rig.p.setNonRealtime (true);
    twin.p.setNonRealtime (true);
    rig.block();
    twin.block();
    CHECK (library.whyBlocked (true).isNotEmpty());

    auto previewLevel = [&rig, &twin]
    {
        auto a = rig.p.getBusBuffer (rig.buffer, false, 0);
        auto b = twin.p.getBusBuffer (twin.buffer, false, 0);
        float worst = 0.0f;

        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                worst = juce::jmax (worst, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));

        return worst;
    };

    const float silent = juce::Decibels::decibelsToGain (-90.0f);
    rig.p.getPreviewPlayer().start (rig.clip());

    for (int b = 0; b < 8; ++b)
    {
        rig.block();
        twin.block();
    }

    CHECK_MSG (previewLevel() < silent, juce::String (previewLevel()));

    for (auto* r : { &rig, &twin })
    {
        r->p.setNonRealtime (false);
        r->p.setPlayHead (nullptr);
    }

    // The kill switch silences a preview within 30 ms.
    rig.p.getPreviewPlayer().start (rig.clip());

    for (int b = 0; b < 20; ++b)
    {
        rig.block();
        twin.block();
    }

    CHECK (previewLevel() > 0.01f);

    for (auto* r : { &rig, &twin })
        r->p.getKillSwitch().setActive (true);

    for (int b = 0; b < (int) std::ceil (0.030 * kSr / kBlock) + 1; ++b)
    {
        rig.block();
        twin.block();
    }

    CHECK_MSG (previewLevel() < silent, juce::String (previewLevel()));
    CHECK (library.whyBlocked (true).isNotEmpty());

    for (auto* r : { &rig, &twin })
        r->p.getKillSwitch().setActive (false);
}

/*  PB-11: the recorder, aux buses and MIDI out never contain a preview. */
LUTHIER_TEST (PresetPreview, PB11_recordingsAndBusesExcludeThePreview)
{
    Rig with (true, true), without (true, true);

    for (auto* r : { &with, &without })
    {
        r->p.setPracticePanelOpen (true);
        r->p.getSessionRecorder().setEnabled (true);
        r->p.getSessionRecorder().setRecordAudio (true);
        r->p.getSessionRecorder().prepare (kSr, 1.0);   // a one-minute ring
        r->p.getRouting();   // MIDI out follows the routing's defaults in both runs
    }

    juce::MidiBuffer noteOn;
    noteOn.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);

    double worstAux = 0.0;
    bool midiEqual = true;

    for (int b = 0; b < 40; ++b)
    {
        if (b == 5)
            with.p.getPreviewPlayer().start (with.clip());

        auto& a = with.block (b == 0 ? noteOn : juce::MidiBuffer());
        auto& n = without.block (b == 0 ? noteOn : juce::MidiBuffer());

        for (int bus = 1; bus < with.p.getBusCount (false); ++bus)
        {
            auto ba = with.p.getBusBuffer (a, false, bus);
            auto bn = without.p.getBusBuffer (n, false, bus);

            for (int ch = 0; ch < ba.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    worstAux = juce::jmax (worstAux, (double) std::abs (ba.getSample (ch, i) - bn.getSample (ch, i)));
        }

        // MIDI out: the buffers the host gets back are byte-equal.
        juce::MemoryOutputStream ma, mb;

        for (const auto m : with.lastMidi)
            ma.write (m.data, (size_t) m.numBytes);

        for (const auto m : without.lastMidi)
            mb.write (m.data, (size_t) m.numBytes);

        midiEqual = midiEqual && ma.getMemoryBlock() == mb.getMemoryBlock();
    }

    CHECK_MSG (worstAux == 0.0, "aux / per-string difference " + juce::String (worstAux));
    CHECK (midiEqual);

    // The main output does carry it.
    CHECK (with.p.getPreviewPlayer().isActive());

    // The session recorder took the same audio in both runs.
    juce::File da = with.scratch.folder.getChildFile ("take"), db = without.scratch.folder.getChildFile ("take");
    da.createDirectory();
    db.createDirectory();
    CHECK (with.p.getSessionRecorder().saveLastTake (da));
    CHECK (without.p.getSessionRecorder().saveLastTake (db));

    juce::MemoryBlock fa, fb;
    auto firstFile = [] (const juce::File& dir)
    {
        const auto files = dir.findChildFiles (juce::File::findFiles, true, "*.wav");
        return files.isEmpty() ? juce::File() : files[0];
    };

    CHECK (firstFile (da).loadFileAsData (fa));
    CHECK (firstFile (db).loadFileAsData (fb));

    // Headers carry a timestamp; the audio after them must match.
    const size_t header = 512;
    CHECK (fa.getSize() == fb.getSize() && fa.getSize() > header);

    if (fa.getSize() == fb.getSize() && fa.getSize() > header)
        CHECK (std::memcmp (static_cast<const char*> (fa.getData()) + header,
                            static_cast<const char*> (fb.getData()) + header, fa.getSize() - header) == 0);
}

/*  PB-12: at -6 dB and no playing, peaks at -9 dBFS or below; a volume change
    never moves the gain by more than 0.01 per sample. */
LUTHIER_TEST (PresetPreview, PB12_levelAndVolumeSmoothing)
{
    // A real factory preview (-18 LUFS, <= -3 dBTP) through the default volume.
    const auto* r = factoryCorpus().find ("Modern Metal Chug");
    CHECK (r != nullptr);

    if (r == nullptr)
        return;

    PreviewPlayer player;
    PreviewClipPool pool;
    player.prepare (kSr);
    CHECK_NEAR (player.getVolumeDb(), -6.0, 0.01);

    double peak = 0.0;

    for (const auto& clip : factoryCorpus().bank)
    {
        player.start (pool.add (PreviewClipPool::makeClip (clip.result.clip, kSr, kSr, clip.name, clip.name)));
        juce::AudioBuffer<float> out (2, kBlock);

        for (int b = 0; b < (int) (4.0 * kSr / kBlock); ++b)
        {
            out.clear();
            player.processBlock (out, kBlock);
            peak = juce::jmax (peak, (double) out.getMagnitude (0, kBlock));
        }

        pool.collectGarbage (player);
    }

    CHECK_MSG (juce::Decibels::gainToDecibels (peak) <= -9.0, juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dBFS");

    // Volume steps: DC through the player measures the gain directly.
    auto c = std::make_unique<PreviewClip>();
    c->audio.setSize (2, (int) kSr * 3);

    for (int i = 0; i < c->audio.getNumSamples(); ++i)
    {
        c->audio.setSample (0, i, 1.0f);
        c->audio.setSample (1, i, 1.0f);
    }

    player.start (pool.add (std::move (c)));
    juce::AudioBuffer<float> out (2, 64);
    double worstStep = 0.0;
    float previous = -1.0f;

    for (int b = 0; b < 2000; ++b)
    {
        if (b == 200) player.setVolumeDb (0.0);
        if (b == 400) player.setVolumeDb (-40.0);
        if (b == 700) player.setVolumeDb (-3.0);

        out.clear();
        player.processBlock (out, 64);

        for (int i = 0; i < 64; ++i)
        {
            const float g = out.getSample (0, i);

            if (previous >= 0.0f && b > 20)   // after the 10 ms fade-in
                worstStep = juce::jmax (worstStep, (double) std::abs (g - previous));

            previous = g;
        }
    }

    CHECK_MSG (worstStep <= 0.01, juce::String (worstStep, 5));
}

/*  PB-13: a rate change mid-preview stops it cleanly; the next preview at the
    new rate has the reference centroid within 1 %. */
LUTHIER_TEST (PresetPreview, PB13_sampleRateChange)
{
    const auto* r = factoryCorpus().find ("Semi-Hollow Chime");
    CHECK (r != nullptr);

    if (r == nullptr)
        return;

    LuthierAudioProcessor p;
    p.prepareToPlay (44100.0, kBlock);
    auto& player = p.getPreviewPlayer();
    player.start (p.getPreviewClipPool().add (PreviewClipPool::makeClip (r->result.clip, kSr, 44100.0, "a", "a")));

    juce::AudioBuffer<float> buffer (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels())), kBlock);
    juce::MidiBuffer midi;

    for (int b = 0; b < 20; ++b)
    {
        buffer.clear();
        p.processBlock (buffer, midi);
    }

    CHECK (player.isActive());

    p.prepareToPlay (96000.0, kBlock);   // mid-preview
    CHECK (! player.isActive());
    CHECK (player.getInUse (0) == nullptr && player.getInUse (1) == nullptr);

    buffer.clear();
    p.processBlock (buffer, midi);
    CHECK (buffer.getMagnitude (0, 0, kBlock) < 1.0);   // no jump from a half-played clip

    // The next request is resampled to 96 kHz: its centroid matches the 48 kHz reference.
    auto next = PreviewClipPool::makeClip (r->result.clip, kSr, 96000.0, "b", "b");
    CHECK (std::abs (next->audio.getNumSamples() - r->result.clip.getNumSamples() * 2) <= 2);

    // The long-term magnitude centroid, framed in equal durations at both rates.
    auto centroid = [] (const juce::AudioBuffer<float>& clip, double rate, int order)
    {
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> frame ((size_t) size * 2);
        std::vector<double> sum ((size_t) size / 2 + 1, 0.0);

        for (int start = 0; start + size <= clip.getNumSamples(); start += size / 2)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);

            for (int i = 0; i < size; ++i)
                frame[(size_t) i] = clip.getSample (0, start + i)
                                    * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / size));

            fft.performFrequencyOnlyForwardTransform (frame.data(), true);

            for (int b = 0; b <= size / 2; ++b)
                sum[(size_t) b] += frame[(size_t) b];
        }

        double weighted = 0.0, total = 0.0;

        for (int b = 1; b <= size / 2; ++b)
        {
            const double hz = b * rate / size;

            if (hz > 20000.0)   // the audible band both rates share
                break;

            weighted += sum[(size_t) b] * hz;
            total += sum[(size_t) b];
        }

        return total > 0.0 ? weighted / total : 0.0;
    };

    const double refHz = centroid (r->result.clip, kSr, 11), newHz = centroid (next->audio, 96000.0, 12);
    CHECK_MSG (std::abs (newHz / refHz - 1.0) <= 0.01, juce::String (refHz, 1) + " vs " + juce::String (newHz, 1) + " Hz");
}

//==============================================================================
// Cache
//==============================================================================

/*  PB-14: what changes the sound hash, and what does not. */
LUTHIER_TEST (PresetPreview, PB14_soundHash)
{
    const auto* r = factoryCorpus().find ("Surf Reverb");
    CHECK (r != nullptr);

    if (r == nullptr)
        return;

    const auto phrase = r->result.phrase;
    const auto base = PreviewRenderer::computeSoundHash (r->json, phrase);
    CHECK (base == r->result.soundHash);
    CHECK (base.length() == 64);

    auto copy = [] (const juce::var& v) { return juce::JSON::parse (juce::JSON::toString (v)); };

    // A rename or a retag keeps it (and so do the library fields).
    auto renamed = copy (r->json);
    renamed.getDynamicObject()->setProperty ("name", "Something Else");
    renamed.getDynamicObject()->setProperty ("tags", juce::Array<juce::var> { "new", "tags" });
    renamed.getDynamicObject()->setProperty ("description", "changed");
    renamed.getDynamicObject()->setProperty ("uid", "1234");
    CHECK (PreviewRenderer::computeSoundHash (renamed, phrase) == base);

    // One parameter changes it.
    auto edited = copy (r->json);
    edited.getProperty ("parameters", {}).getDynamicObject()->setProperty (ParamIDs::ampGain, 0.123);
    CHECK (PreviewRenderer::computeSoundHash (edited, phrase) != base);

    // The version string and the render revision change it.
    CHECK (PreviewRenderer::computeSoundHash (r->json, phrase, "0.0.1") != base);
    CHECK (PreviewRenderer::computeSoundHash (r->json, phrase, JucePlugin_VersionString, PreviewRenderer::kRenderRevision + 1) != base);

    // The guitar file it references changes it.
    const auto folder = PartLibrary::getUserGuitarsFolder();
    folder.createDirectory();
    const auto guitarFile = folder.getNonexistentChildFile ("pb14-guitar", ".luthierguitar", false);
    CHECK (guitarFile.replaceWithText ("{ \"magic\": \"luthier.guitar\", \"name\": \"PB14\", \"scale\": 648 }"));

    auto withGuitar = copy (r->json);
    auto* block = new juce::DynamicObject();
    block->setProperty ("reference", "User/" + guitarFile.getFileName());
    withGuitar.getDynamicObject()->setProperty ("guitar", juce::var (block));

    const auto before = PreviewRenderer::computeSoundHash (withGuitar, phrase);
    CHECK (guitarFile.replaceWithText ("{ \"magic\": \"luthier.guitar\", \"name\": \"PB14\", \"scale\": 628 }"));
    CHECK (PreviewRenderer::computeSoundHash (withGuitar, phrase) != before);
    guitarFile.deleteFile();
}

/*  PB-15: a save leaves a cache entry with features and descriptors within 3 s. */
LUTHIER_TEST (PresetPreview, PB15_aSaveRendersItsPreview)
{
    Rig rig;
    auto& library = rig.p.getPresetLibrary();
    auto& manager = rig.p.getPresetManager();
    const auto name = "PB15 " + juce::Uuid().toString().substring (0, 8);

    const auto start = juce::Time::getMillisecondCounterHiRes();
    CHECK (manager.saveAs (name, "Test"));

    juce::var sidecar;
    juce::String hash;

    const bool arrived = waitFor ([&]
    {
        const int i = library.getIndex().indexOfName (name);

        if (i < 0 || library.getIndex()[i].soundHash.isEmpty())
            return false;

        hash = library.getIndex()[i].soundHash;
        sidecar = library.getService().getCache().lookup (hash);
        return ! sidecar.isVoid();
    }, 10000);

    const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;
    CHECK (arrived);
    std::cout << "    save to cached preview " << juce::String (elapsed, 0) << " ms" << std::endl;
    // 3 s on the reference CPU; this machine renders at about half its speed.
    CHECK_MSG (elapsed <= 3000.0 * 2.0, juce::String (elapsed, 0) + " ms");
    CHECK (ToneFeatures::fromVar (sidecar.getProperty ("features", {})).valid);
    CHECK (sidecar.getProperty ("descriptors", {}).toString().isNotEmpty());
    CHECK (sidecar.getProperty ("peaks", {}).size() == PreviewResult::kNumPeaks);

    // The saved file carries a uid and nothing derived.
    const auto file = manager.getCurrentPresetFile().existsAsFile() ? manager.getCurrentPresetFile()
                                                                     : PresetManager::getUserPresetFolder().getChildFile ("Test").getChildFile (name + ".luthierpreset");
    const auto text = file.loadFileAsString();
    CHECK (text.contains ("\"uid\""));
    CHECK (! text.contains ("descriptors"));
    file.deleteFile();
}

/*  PB-16: pruning to 128 MB, least recently played first; stale locks. */
LUTHIER_TEST (PresetPreview, PB16_pruningAndLocks)
{
    ScratchFolder scratch;
    PreviewCache cache (scratch.folder);
    juce::MemoryBlock megabyte (1024 * 1024, true);
    const auto now = juce::Time::getCurrentTime();

    for (int i = 0; i < 130; ++i)
    {
        const auto hash = juce::String::toHexString (i).paddedLeft ('0', 8) + juce::String::repeatedString ("a", 56);
        auto* o = new juce::DynamicObject();
        o->setProperty ("magic", PreviewCache::kMagic);
        o->setProperty ("hash", hash);

        // Written as other instances would have left them (store() itself
        // prunes every 50 writes, which would keep the folder under the cap).
        CHECK (cache.oggFileFor (hash).replaceWithData (megabyte.getData(), megabyte.getSize()));
        CHECK (cache.sidecarFileFor (hash).replaceWithText (juce::JSON::toString (juce::var (o))));

        // Entry i was last played i minutes after the oldest.
        cache.sidecarFileFor (hash).setLastModificationTime (now - juce::RelativeTime::minutes (200 - i));
    }

    CHECK_MSG (cache.getTotalBytes() > PreviewCache::kCapBytes, juce::String (cache.getTotalBytes()) + " " + juce::String (cache.getNumEntries()));
    cache.prune();
    CHECK_MSG (cache.getTotalBytes() <= PreviewCache::kCapBytes, juce::String (cache.getTotalBytes()));

    // The oldest went, the newest stayed.
    CHECK (cache.lookup (juce::String::toHexString (0).paddedLeft ('0', 8) + juce::String::repeatedString ("a", 56)).isVoid());
    CHECK (! cache.lookup (juce::String::toHexString (129).paddedLeft ('0', 8) + juce::String::repeatedString ("a", 56)).isVoid());

    // A lock older than 30 s is ignored; a fresh one is honoured.
    const auto hash = juce::String ("ab").paddedLeft ('0', 64);
    CHECK (cache.lockFileFor (hash).replaceWithText ("someone"));
    CHECK (cache.isLockedByAnother (hash));
    CHECK (! cache.tryLock (hash));

    cache.lockFileFor (hash).setLastModificationTime (now - juce::RelativeTime::seconds (31));
    CHECK (! cache.isLockedByAnother (hash));
    CHECK (cache.tryLock (hash));
    cache.unlock (hash);
}

/*  PB-17: two processors, one hash: one render, and both play it. */
LUTHIER_TEST (PresetPreview, PB17_twoProcessorsRenderOnce)
{
    Rig a, b;
    b.scratch.folder.deleteRecursively();
    PreviewCache::setDefaultFolderOverride (a.scratch.folder.getChildFile ("cache"));   // shared

    auto& la = a.p.getPresetLibrary();
    auto& lb = b.p.getPresetLibrary();
    la.refreshSynchronously();
    lb.refreshSynchronously();

    const int ia = la.getIndex().indexOfName ("Jazz Hollowbody");
    const int ib = lb.getIndex().indexOfName ("Jazz Hollowbody");
    CHECK (ia >= 0 && ib >= 0);

    if (ia < 0 || ib < 0)
        return;

    runAudio (a.p);
    runAudio (b.p);

    juce::String hint;
    CHECK (la.play (ia, true, hint));
    CHECK (lb.play (ib, true, hint));

    const bool both = waitFor ([&]
    {
        runAudio (a.p, 1);
        runAudio (b.p, 1);
        return a.p.getPreviewPlayer().isActive() && b.p.getPreviewPlayer().isActive();
    }, 20000);

    CHECK (both);
    CHECK_MSG (la.getService().getNumRenders() + lb.getService().getNumRenders() == 1,
               juce::String (la.getService().getNumRenders()) + " + " + juce::String (lb.getService().getNumRenders()));
}

/*  PB-18: failures. */
LUTHIER_TEST (PresetPreview, PB18_failures)
{
    Rig rig;
    auto& library = rig.p.getPresetLibrary();

    // A corrupt preset: a failed row, still listed by name, no crash.
    const auto corrupt = PresetManager::getUserPresetFolder().getChildFile ("Test").getChildFile ("PB18 Corrupt.luthierpreset");
    corrupt.getParentDirectory().createDirectory();
    CHECK (corrupt.replaceWithText ("{ \"magic\": \"luthier.preset\", \"name\": \"PB18 Corrupt\", \"parameters\": [ 1, 2"));
    rig.p.getPresetManager().refresh();
    library.refreshSynchronously();

    const int i = library.getIndex().indexOfName ("PB18 Corrupt");
    CHECK (i >= 0);

    if (i >= 0)
    {
        CHECK (library.getIndex()[i].corrupt);
        CHECK (library.getIndex()[i].preview == PresetIndex::PreviewState::failed);

        juce::String hint;
        runAudio (rig.p);
        CHECK (! library.play (i, true, hint));
        CHECK (hint.contains ("could not be rendered"));
    }

    corrupt.deleteFile();

    // A forced timeout is abandoned within 10.5 s.
    {
        PreviewRenderer renderer;
        renderer.testDelayPerBlockMs = 20.0;
        const auto json = FactoryPresets::toVar (FactoryPresets::getPreset (0), renderer.getRangeSource());
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto result = renderer.render (json);
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;

        CHECK (! result.ok);
        CHECK (result.timedOut);
        CHECK_MSG (elapsed <= 10500.0, juce::String (elapsed, 0) + " ms");
    }

    // An unwritable cache plays from memory: one render serves both plays.
    {
        ScratchFolder scratch;
        const auto blocker = scratch.folder.getChildFile ("not-a-folder");
        CHECK (blocker.replaceWithText ("x"));   // a file where the folder should be

        PreviewRenderService service (rig.p, blocker.getChildFile ("cache"));
        CHECK (service.isCacheUnwritable());

        int ready = 0;
        PreviewRenderService::Ready last;
        service.onReady = [&] (const PreviewRenderService::Ready& r) { ++ready; last = r; };

        const auto file = rig.p.getPresetManager().getPreset (rig.p.getPresetManager().indexOfPreset ("Init"))->file;
        service.request (file, PreviewRenderService::Priority::interactive);
        CHECK (waitFor ([&] { return ready == 1; }, 20000));
        CHECK (last.ok && last.rendered);

        service.request (file, PreviewRenderService::Priority::interactive);
        CHECK (waitFor ([&] { return ready == 2; }, 20000));
        CHECK (last.ok && last.fromMemory);
        CHECK (service.getNumRenders() == 1);
    }
}

/*  PB-19: destroying the processor mid-render returns within 500 ms. */
LUTHIER_TEST (PresetPreview, PB19_destroyMidRender)
{
    ScratchFolder scratch;
    PreviewCache::setDefaultFolderOverride (scratch.folder);

    auto p = std::make_unique<LuthierAudioProcessor>();
    auto& library = p->getPresetLibrary();
    library.refreshSynchronously();
    const int i = library.getIndex().indexOfName ("Physics Showcase");
    CHECK (i >= 0);

    library.request (i, PreviewRenderService::Priority::interactive);
    CHECK (waitFor ([&] { return library.getService().isBusy(); }, 5000));
    pumpMessages (200);   // well into the render

    const auto start = juce::Time::getMillisecondCounterHiRes();
    p.reset();
    const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;

    CHECK_MSG (elapsed <= 500.0, juce::String (elapsed, 0) + " ms");
    pumpMessages (50);   // anything it posted is now dropped by its WeakReference
    PreviewCache::setDefaultFolderOverride ({});
}

//==============================================================================
// Library and file
//==============================================================================

/*  PB-25: favourites and ratings survive a rename, an editor reopen and a new
    processor, and are never written into the preset file. */
LUTHIER_TEST (PresetPreview, PB25_libraryPrefsSurvive)
{
    ScratchFolder scratch;
    const auto prefsFile = scratch.folder.getChildFile ("preset-library.json");
    PresetLibraryPrefs::get().setFile (prefsFile);
    PreviewCache::setDefaultFolderOverride (scratch.folder.getChildFile ("cache"));

    const auto name = "PB25 " + juce::Uuid().toString().substring (0, 8);
    juce::File saved;
    juce::String key;

    {
        LuthierAudioProcessor p;
        CHECK (p.getPresetManager().saveAs (name, "Test"));
        auto& library = p.getPresetLibrary();
        library.refreshSynchronously();

        const int i = library.getIndex().indexOfName (name);
        CHECK (i >= 0);
        key = library.getIndex()[i].key;
        CHECK (key == p.getPresetManager().getCurrentUid());

        PresetLibraryPrefs::get().setFavourite (key, true);
        PresetLibraryPrefs::get().setRating (key, 4);

        // The editor opens and closes: the prefs are the processor's, not the window's.
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        }

        CHECK (library.renamePreset (library.getIndex().indexOfName (name), name + " Renamed"));
        const int renamed = library.getIndex().indexOfName (name + " Renamed");
        CHECK (renamed >= 0);

        if (renamed >= 0)
        {
            CHECK (library.getIndex()[renamed].key == key);
            saved = library.getIndex()[renamed].info.file;
        }
    }

    // A new processor, and the store read back from disk.
    PresetLibraryPrefs::get().setFile (prefsFile);

    {
        LuthierAudioProcessor p;
        auto& library = p.getPresetLibrary();
        library.refreshSynchronously();
        const int i = library.getIndex().indexOfName (name + " Renamed");
        CHECK (i >= 0);

        if (i >= 0)
        {
            CHECK (PresetLibraryPrefs::get().isFavourite (library.getIndex()[i].key));
            CHECK (PresetLibraryPrefs::get().getRating (library.getIndex()[i].key) == 4);
        }
    }

    const auto text = saved.loadFileAsString();
    CHECK (text.isNotEmpty());
    CHECK (! text.contains ("favourite") && ! text.contains ("rating"));
    saved.deleteFile();

    // A path-keyed preset (no uid) is re-keyed by a rename.
    PresetLibraryPrefs::get().setFavourite ("Extra/Old.luthierpreset", true);
    PresetLibraryPrefs::get().rekey ("Extra/Old.luthierpreset", "Extra/New.luthierpreset");
    CHECK (PresetLibraryPrefs::get().isFavourite ("Extra/New.luthierpreset"));
    CHECK (! PresetLibraryPrefs::get().isFavourite ("Extra/Old.luthierpreset"));

    PresetLibraryPrefs::get().setFile ({});
    PreviewCache::setDefaultFolderOverride ({});
}

/*  PB-26: uid on the first save and kept after; previewPhrase preserved. */
LUTHIER_TEST (PresetPreview, PB26_uidAndPreviewPhraseRoundTrip)
{
    LuthierAudioProcessor p;
    auto& manager = p.getPresetManager();
    const auto name = "PB26 " + juce::Uuid().toString().substring (0, 8);

    CHECK (manager.saveAs (name, "Test"));
    const auto file = PresetManager::getUserPresetFolder().getChildFile ("Test").getChildFile (name + ".luthierpreset");
    const auto uid = juce::JSON::parse (file).getProperty ("uid", {}).toString();
    CHECK (uid.isNotEmpty());
    CHECK (! uid.startsWith ("factory:"));

    // An author sets the phrase; load, save, load: both survive.
    auto json = juce::JSON::parse (file);
    json.getDynamicObject()->setProperty ("previewPhrase", "lead_lick");
    CHECK (file.replaceWithText (juce::JSON::toString (json, false)));

    CHECK (manager.loadPreset (file));
    CHECK (manager.getCurrentUid() == uid);
    CHECK (manager.getCurrentPreviewPhrase() == "lead_lick");

    manager.refresh();
    const int index = manager.indexOfPreset (name);
    CHECK (index >= 0);
    CHECK (manager.loadPreset (index));
    CHECK (manager.saveCurrent());

    const auto after = juce::JSON::parse (file);
    CHECK (after.getProperty ("uid", {}).toString() == uid);
    CHECK (after.getProperty ("previewPhrase", {}).toString() == "lead_lick");

    // Save -> load -> save is byte-identical with the new fields present.
    const auto first = file.loadFileAsString();
    CHECK (manager.loadPreset (index));
    CHECK (manager.saveCurrent());
    CHECK (file.loadFileAsString() == first);

    // A factory preset's uid is derived, never written.
    const auto* init = manager.getPreset (manager.indexOfPreset ("Init"));
    CHECK (init != nullptr && init->uid == "factory:Init");
    CHECK (! init->file.loadFileAsString().contains ("\"uid\""));

    file.deleteFile();
}

//==============================================================================
// Editions and combinations
//==============================================================================

/*  5.3 / 12: factory presets play from the shipped clips without a render -
    the path a Free build takes for the 20 Pro presets it cannot render. (The
    Free build itself, and so PB-35, waits on editions.md.) */
LUTHIER_TEST (PresetPreview, shippedClipsPlayWithoutRendering)
{
    Rig rig;
    juce::String error;
    CHECK_MSG (FactoryPreviews::write (factoryCorpus().bank, rig.scratch.folder.getChildFile ("Shipped"), error), error);
    PreviewRenderService::setShippedFolderOverride (rig.scratch.folder.getChildFile ("Shipped").getChildFile ("Previews"));

    // The installed factory file as this build writes it (a file written by an
    // older build, with fewer parameters, is "edited" and rightly ignored).
    auto& manager = rig.p.getPresetManager();
    const int managerIndex = manager.indexOfPreset ("Modern Metal Chug");
    CHECK (managerIndex >= 0);

    for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
        if (juce::String (FactoryPresets::getPreset (i).name) == "Modern Metal Chug" && managerIndex >= 0)
            manager.getPreset (managerIndex)->file.replaceWithText (
                juce::JSON::toString (FactoryPresets::toVar (FactoryPresets::getPreset (i), rig.p), false));

    auto& library = rig.p.getPresetLibrary();
    library.refreshSynchronously();
    const int entry = library.getIndex().indexOfName ("Modern Metal Chug");
    CHECK (entry >= 0);

    PreviewRenderService::Ready got;
    library.onPreviewReady = [&got] (const PreviewRenderService::Ready& r) { got = r; };

    runAudio (rig.p);
    juce::String hint;
    CHECK (library.play (entry, true, hint));
    CHECK (waitFor ([&] { runAudio (rig.p, 1); return rig.p.getPreviewPlayer().isActive(); }, 5000));
    CHECK (got.ok && got.fromShipped && ! got.stale);
    CHECK (library.getService().getNumRenders() == 0);

    // The shipped features stand in for a local analysis.
    CHECK (library.getIndex()[entry].isAnalysed());
    CHECK (library.getIndex()[entry].descriptors.contains ("high-gain"));
}

/*  PB-36: a preview during Morph, tune playback (with whileTransport on), live
    slide mode, an active mod matrix and a MIDI Learn arm leaves every live
    module's output unchanged. */
LUTHIER_TEST (PresetPreview, PB36_combinations)
{
    Rig with (true, true), without (true, true);

    for (auto* r : { &with, &without })
    {
        auto& p = r->p;
        auto& manager = p.getPresetManager();

        // Morph between two factory presets.
        auto& morph = p.getPresetMorph();
        CHECK (manager.loadPreset (manager.indexOfPreset ("Surf Reverb")));
        morph.setSlot (PresetMorph::slotA, manager.toVar(), "Surf Reverb");
        CHECK (manager.loadPreset (manager.indexOfPreset ("Blues Slide")));
        morph.setSlot (PresetMorph::slotB, manager.toVar(), "Blues Slide");
        morph.setEnabled (true);
        p.getParameterBridge().applyAllNow();

        if (auto* position = p.getState().getParameter (ParamIDs::presetMorphPosition))
            position->setValueNotifyingHost (0.4f);

        p.updatePresetMorph();

        // Slide mode, a modulation route and an armed MIDI Learn.
        if (auto* slide = p.getState().getParameter (ParamIDs::slideGuitar))
            slide->setValueNotifyingHost (1.0f);

        ModRoute route;
        route.sourceId = "lfo1";
        route.destinationId = ParamIDs::ampTreble;
        route.depth = 0.5f;
        p.getModMatrix().addRoute (route);
        p.getMidiLearn().setArmed (true);

        // The tune plays.
        p.getTunePlayer().play();
        p.getParameterBridge().applyAllNow();
        p.prepareToPlay (kSr, kBlock);
    }

    // With whileTransport on, the gate lets a preview play under the tune.
    auto& library = with.p.getPresetLibrary();
    auto settings = library.getSettings();
    settings.whileTransport = true;
    library.setSettings (settings);

    juce::MidiBuffer notes;
    notes.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);
    notes.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 90), 0);

    double worst = 0.0;

    for (int b = 0; b < 60; ++b)
    {
        if (b == 8)
        {
            CHECK (library.whyBlocked (true).isEmpty() || library.whyBlocked (true).startsWith ("Previews wait"));
            with.p.getPreviewPlayer().start (with.clip());
        }

        auto& a = with.block (b == 0 ? notes : juce::MidiBuffer());
        auto& n = without.block (b == 0 ? notes : juce::MidiBuffer());

        for (int bus = 1; bus < with.p.getBusCount (false); ++bus)
        {
            auto ba = with.p.getBusBuffer (a, false, bus);
            auto bn = without.p.getBusBuffer (n, false, bus);

            for (int ch = 0; ch < ba.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    worst = juce::jmax (worst, (double) std::abs (ba.getSample (ch, i) - bn.getSample (ch, i)));
        }
    }

    CHECK_MSG (worst == 0.0, "a live module's output moved by " + juce::String (worst));
    CHECK (with.p.getPreviewPlayer().isActive());
    CHECK (with.p.getMidiLearn().isArmed());   // the preview did not take the learn

    for (auto* r : { &with, &without })
    {
        r->p.getTunePlayer().stop();
        r->p.getMidiLearn().setArmed (false);
    }
}
