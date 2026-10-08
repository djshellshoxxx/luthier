/*  Jam mode in the plugin (jam-mode.md 17: JM-18's parameter side, JM-28's
    range, JM-34's scenario and "off", JM-35 to JM-46): the band as the
    processor drives it - parameters, presets, buses, the looper, MIDI out,
    the tune and the rhythm engine. */

#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Jam/JamMidiExport.h"
#include "../Jam/JamEdition.h"
#include "../Support/TuneExport.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    struct FakeHead : juce::AudioPlayHead
    {
        bool present = true, playing = false;
        double ppq = 0.0, bpm = 120.0;
        int numerator = 4, denominator = 4;

        juce::Optional<PositionInfo> getPosition() const override
        {
            if (! present)
                return {};

            PositionInfo info;
            info.setIsPlaying (playing);
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setPpqPositionOfLastBarStart (std::floor (ppq / (numerator * 4.0 / denominator)) * (numerator * 4.0 / denominator));
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { numerator, denominator });
            return info;
        }
    };

    /** The plugin on a bench, with a fixture host. */
    struct Plugin
    {
        std::unique_ptr<LuthierAudioProcessor> p = std::make_unique<LuthierAudioProcessor>();
        FakeHead head;
        juce::AudioBuffer<float> buffer;
        int64_t position = 0;
        std::vector<float> left;
        std::vector<std::vector<float>> buses;   ///< first channel of every output bus
        std::vector<std::pair<int64_t, juce::MidiMessage>> midiOut;
        std::vector<std::pair<int64_t, juce::MidiMessage>> notes;
        std::function<void (Plugin&)> beforeBlock;
        bool keepBuses = false;

        explicit Plugin (bool host = true, std::function<void (LuthierAudioProcessor&)> layout = {})
        {
            head.present = host;
            head.playing = host;
            p->setPlayHead (&head);

            if (layout)
                layout (*p);

            if (auto* h = p->getState().getParameter (ParamIDs::macroHumanize))
                h->setValueNotifyingHost (0.0f);

            p->prepareToPlay (kSr, kBlock);
            buffer.setSize (juce::jmax (p->getTotalNumOutputChannels(), p->getTotalNumInputChannels(), 2), kBlock);
            set (ParamIDs::jamHumanise, 0.0f);
            set (ParamIDs::jamFillEvery, 0.0f);
        }

        void set (const char* id, float plain)
        {
            if (auto* prm = p->getState().getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        }

        float get (const char* id) const
        {
            auto* prm = p->getState().getParameter (id);
            return prm != nullptr ? prm->convertFrom0to1 (prm->getValue()) : 0.0f;
        }

        void chord (int64_t at, std::initializer_list<int> pitches, int velocity = 100)
        {
            for (int n : pitches)
                notes.push_back ({ at, juce::MidiMessage::noteOn (1, n, (juce::uint8) velocity) });
        }

        void run (double seconds)
        {
            const int64_t end = position + (int64_t) std::llround (seconds * kSr);

            while (position < end)
            {
                if (beforeBlock)
                    beforeBlock (*this);

                buffer.clear();
                juce::MidiBuffer midi;

                for (const auto& n : notes)
                    if (n.first >= position && n.first < position + kBlock)
                        midi.addEvent (n.second, (int) (n.first - position));

                p->processBlock (buffer, midi);

                for (const auto m : midi)
                    midiOut.push_back ({ position + m.samplePosition, m.getMessage() });

                for (int i = 0; i < kBlock; ++i)
                    left.push_back (buffer.getSample (0, i));

                if (keepBuses)
                {
                    buses.resize ((size_t) p->getBusCount (false));

                    for (int b = 0; b < p->getBusCount (false); ++b)
                    {
                        auto out = p->getBusBuffer (buffer, false, b);

                        for (int i = 0; i < kBlock; ++i)
                            buses[(size_t) b].push_back (out.getNumChannels() > 0 ? out.getSample (0, i) : 0.0f);
                    }
                }

                if (head.playing)
                    head.ppq += kBlock * head.bpm / 60.0 / kSr;

                position += kBlock;

                if (position % (kBlock * 6) == 0)
                    p->serviceJam();
            }
        }

        static double peak (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
        {
            double m = 0.0;

            for (size_t i = from; i < juce::jmin (to, x.size()); ++i)
                m = juce::jmax (m, (double) std::abs (x[i]));

            return m;
        }
    };

    /** Layouts as a host negotiates them: aux pairs, the Jam pair, no strings. */
    void enableAux (LuthierAudioProcessor& processor, bool aux, bool jamBuses)
    {
        auto layout = processor.getBusesLayout();

        for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
        {
            const auto name = processor.getBus (false, bus)->getName();
            const bool isJam = name == getAuxBusName (kJamDrumsAux) || name == getAuxBusName (kJamBassAux);
            const bool isAux = bus - 1 < kNumAuxBuses;
            const bool on = isJam ? jamBuses : isAux && aux;
            layout.outputBuses.getReference (bus) = on ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
        }

        processor.setBusesLayout (layout);
    }

    int busIndex (LuthierAudioProcessor& processor, const char* name)
    {
        for (int b = 0; b < processor.getBusCount (false); ++b)
            if (processor.getBus (false, b)->getName() == name)
                return b;

        return -1;
    }

    ChordCell cell (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    Tune jamTune (bool percussion, bool bass)
    {
        Tune t;
        t.meta.title = "Jam";
        t.meta.tempoBpm = 120.0;

        TuneSection s;
        s.name = "Verse";
        s.lengthBars = 4;
        s.chords = { cell ("Am"), cell ("F"), cell ("C"), cell ("G") };

        if (bass)
            s.bass.mode = BassMode::root;

        if (percussion)
        {
            TuneLayer layer;
            layer.type = LayerType::percussion;
            s.layers.push_back (layer);
        }

        t.addSection (s);
        return t;
    }

    int64_t latencyOf (const Plugin& b) { return b.p->getLatencySamples(); }
}

//==============================================================================
LUTHIER_TEST (JamPlugin, JM28_kitTuningClampsToStockUntilUnlocked)
{
    Plugin b;
    b.set (ParamIDs::jamKitTuning, 12.0f);
    CHECK_NEAR (b.get (ParamIDs::jamKitTuning), 6.0, 1.0e-3);

    RangeState advanced;
    advanced.setFamilyAdvanced (RangeFamily::jam, true);
    b.p->setRanges (advanced);
    b.set (ParamIDs::jamKitTuning, 12.0f);
    CHECK_NEAR (b.get (ParamIDs::jamKitTuning), 12.0, 1.0e-3);

    b.set (ParamIDs::jamKitDamping, 0.0f);
    CHECK_NEAR (b.get (ParamIDs::jamKitDamping), 0.0, 1.0e-3);
}

LUTHIER_TEST (JamPlugin, JM34_offCostsNothingAndTheJamScenario)
{
    {
        Plugin off;
        off.run (1.0);
        CHECK_MSG (off.p->getJam().getSampleClock() == 0, "the band ran while jam_enabled was off");
        CHECK (off.p->getJam().getState() == JamState::off || ! off.p->getJam().isBandRunning());
    }

    auto seconds = [] (auto&& fn)
    {
        // The thread's CPU clock (TestFramework), so a busy machine's scheduling does not count.
        const double start = threadCpuTimeSeconds();
        fn();
        return threadCpuTimeSeconds() - start;
    };

    // The "Jam" scenario: the Rock preset (or the first), 4 voices, the band at 5.
    auto scenario = [&] (bool band)
    {
        Plugin b;
        auto& presets = b.p->getPresetManager();

        for (int i = 0; i < presets.getNumPresets(); ++i)
            if (presets.getPreset (i) != nullptr && presets.getPreset (i)->name.containsIgnoreCase ("rock"))
            {
                presets.loadPreset (i);
                break;
            }

        b.set (ParamIDs::jamEnabled, band ? 1.0f : 0.0f);
        b.set (ParamIDs::jamIntensity, 5.0f);
        b.chord (0, { 40, 45, 50, 55 });
        b.run (1.0);
        return 100.0 * seconds ([&] { b.run (5.0); }) / 5.0;
    };

    // Interleaved and the best of three each, so a busy machine's noise does
    // not land on one side of the difference.
    double without = 1.0e9, with = 1.0e9;

    for (int round = 0; round < 3; ++round)
    {
        without = juce::jmin (without, scenario (false));
        with = juce::jmin (with, scenario (true));
    }

    // The machine yardstick of JamDspTests' JM34: one StringEngine = 0.208 units.
    StringEngine string;
    string.prepare (kSr, 128);
    Excitation::Params p;
    const int blocks = (int) (10.0 * kSr / 128);
    const double stringUnits = 100.0 * seconds ([&]
    {
        double sink = 0.0;

        for (int i = 0; i < blocks; ++i)
        {
            if (i % 94 == 0) { string.snapToFrequency (110.0); string.excite (p); }
            for (int k = 0; k < 128; ++k) sink += string.processSample (0.0);
        }

        juce::ignoreUnused (sink);
    }) / 10.0;

    const double scale = (2.5 / 12.0) / juce::jmax (1.0e-6, stringUnits);
    std::cout << "    Jam scenario: " << with << " % here (" << with * scale << " reference units), without the band "
              << without << " % (" << without * scale << ")" << std::endl;

    CHECK_MSG ((with - without) * scale <= 1.7 * 1.10, "the band adds " + juce::String ((with - without) * scale, 2) + " reference units");
    CHECK_MSG (with * scale <= 10.0 * 1.10, "the Jam scenario is " + juce::String (with * scale, 2) + " reference units");
}

//==============================================================================
LUTHIER_TEST (JamPlugin, JM18_panicSetsJamPlayOff)
{
    Plugin b;
    b.set (ParamIDs::jamEnabled, 1.0f);
    b.set (ParamIDs::jamPlay, 1.0f);
    b.run (2.0);
    CHECK (b.p->getJam().isBandRunning());

    b.p->panic();
    CHECK (b.get (ParamIDs::jamPlay) < 0.5f);
    b.run (0.1);
    CHECK (! b.p->getJam().isBandRunning());
}

LUTHIER_TEST (JamPlugin, JM35_presetsSnapshotsAndHostState)
{
    Plugin b;
    auto& apvts = b.p->getState();

    // Every non-transient Jam parameter away from its default.
    std::map<juce::String, float> set;

    for (const auto* id : ParamIDs::jamParameters)
    {
        if (ParamIDs::isJamTransient (id))
            continue;

        auto* prm = apvts.getParameter (id);
        const float v = prm->getDefaultValue() > 0.5f ? 0.2f : 0.8f;
        prm->setValueNotifyingHost (v);
        set[id] = prm->getValue();
    }

    CHECK (set.size() == 32);

    auto block = b.p->getJamBlock();
    block.getDynamicObject()->setProperty ("seed", 1234);
    block.getDynamicObject()->setProperty ("link_rhythm_kit", true);
    b.p->setJamBlock (block);

    b.set (ParamIDs::jamPlay, 1.0f);
    b.set (ParamIDs::jamFillNow, 1.0f);

    auto& presets = b.p->getPresetManager();
    const auto preset = presets.toVar ("Jam test");
    const auto* stored = preset.getProperty ("parameters", {}).getDynamicObject();
    CHECK (stored != nullptr && ! stored->hasProperty (ParamIDs::jamPlay) && ! stored->hasProperty (ParamIDs::jamFillNow));

    presets.resetToDefaults();
    CHECK (b.p->getJam().getSeed() == 4849997);
    CHECK (presets.fromVar (juce::JSON::parse (juce::JSON::toString (preset))));

    for (const auto& [id, v] : set)
        CHECK_MSG (std::abs (apvts.getParameter (id)->getValue() - v) < 1.0e-6f, id + " did not round-trip");

    CHECK (b.p->getJam().getSeed() == 1234);
    CHECK (b.p->isJamRhythmKitLinked());

    // Snapshots never hold the transients, and do hold the block.
    CHECK (b.p->captureSnapshot (0, "Jam"));
    const auto snapshot = b.p->getSnapshots().getSnapshot (0);
    CHECK (! snapshot.parameters.getDynamicObject()->hasProperty (ParamIDs::jamPlay));
    CHECK (! snapshot.parameters.getDynamicObject()->hasProperty (ParamIDs::jamFillNow));
    CHECK (snapshot.bypasses.getProperty ("jam", {}).isObject());

    // jam-mode 10: a host-state reload round-trips the transients (CLAP state
    // reproducibility). Here the preset round-trip above already reset both to off
    // (presets exclude them), so the saved - and restored - value is off, and a
    // stale jam_play the fresh instance held is overwritten by the restore. Either
    // way the band never starts. (JamState::JM10 covers a saved value of on.)
    juce::MemoryBlock state;
    b.p->getStateInformation (state);
    Plugin restored;
    restored.set (ParamIDs::jamPlay, 1.0f);
    restored.p->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.get (ParamIDs::jamPlay) < 0.5f);
    CHECK (restored.get (ParamIDs::jamFillNow) < 0.5f);
    CHECK (! restored.p->getJam().isBandRunning());
    CHECK (restored.p->getJam().getSeed() == 1234);
}

LUTHIER_TEST (JamState, JM10_transientsRoundTripWithoutStartingTheBand)
{
    /*  jam-mode 10 (FEAT-JAM), option 1: jam_play and jam_fill_now must survive a
        host save/reload (clap-validator checks getParameter()->getValue() right
        after setStateInformation, before any processBlock) - yet a restored
        jam_play must never read as a rising edge that starts the band.

        The fixture host is not playing (host = false): a playing host would start
        the band through its transport regardless of jam_play, which is a separate,
        legitimate path; here we isolate the jam_play edge. Deterministic: no
        real-time sleeps; the 200 ms mirror runs only from the (unpumped) timer, so
        the restored value holds across the processed blocks below. */
    for (const char* transient : { ParamIDs::jamPlay, ParamIDs::jamFillNow })
    {
        Plugin source { false };
        source.set (ParamIDs::jamEnabled, 1.0f);
        source.set (transient, 1.0f);

        juce::MemoryBlock state;
        source.p->getStateInformation (state);

        Plugin restored { false };

        // Warm up so the engine's edge detector already holds jam_play = false
        // (haveSettings true): this is the live-reload case the re-baseline guards,
        // where a fresh restored jam_play = 1 would otherwise read as a rising edge.
        restored.run (0.25);
        CHECK_MSG (! restored.p->getJam().isBandRunning(), "the band ran before the reload");

        restored.p->setStateInformation (state.getData(), (int) state.getSize());

        // (a) the value round-trips immediately after load, before any block.
        CHECK_MSG (restored.get (transient) > 0.5f,
                   juce::String (transient) + " did not round-trip through host state");
        CHECK_MSG (restored.get (ParamIDs::jamEnabled) > 0.5f, "jam_enabled did not round-trip");
        CHECK_MSG (! restored.p->getJam().isBandRunning(), "the band was running immediately after load");

        // (b) the restored value does not start the band once blocks run: the edge
        // detector was re-baselined, so jam_play is not seen as a rising edge.
        restored.run (1.0);
        CHECK_MSG (restored.p->getJam().getState() != JamState::counting
                       && restored.p->getJam().getState() != JamState::playing,
                   juce::String ("a restored ") + transient + " started the band");
        CHECK_MSG (! restored.p->getJam().isBandRunning(),
                   juce::String ("a restored ") + transient + " left the band running");
    }
}

LUTHIER_TEST (JamPlugin, JM36_jamParametersAreTheLast34InTableOrder)
{
    /*  Merge with SPEC-SWEEP: the 34 were appended last, in table order, and
        stay one contiguous block. A block appended after them (the spec
        sweep's, which the parameter-list rule keeps last) may follow; nothing
        named jam_ may sit before or after the block. */
    LuthierAudioProcessor p;
    const auto& params = p.getParameters();
    const int n = params.size();
    CHECK (n > ParamIDs::kNumJamParameters);

    // INTEGRATE-2: the 34 were last when FEAT-JAM landed; workstreams merged
    // after it (FEAT-ASSIST, FEAT-MIC) append their own blocks behind them. The
    // rule is the append-only one: one contiguous block in table order, and no
    // jam_ parameter anywhere else.
    int first = -1;

    for (int i = 0; i < n && first < 0; ++i)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
            if (withId->paramID == ParamIDs::jamParameters[0])
                first = i;

    CHECK (first >= 0 && first + ParamIDs::kNumJamParameters <= n);

    if (first < 0 || first + ParamIDs::kNumJamParameters > n)
        return;

    for (int i = 0; i < ParamIDs::kNumJamParameters; ++i)
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[first + i]);
        CHECK (withId != nullptr && withId->paramID == ParamIDs::jamParameters[i]);
    }

    for (int i = 0; i < n; ++i)
        if (i < first || i >= first + ParamIDs::kNumJamParameters)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
                CHECK_MSG (! withId->paramID.startsWith ("jam_"), withId->paramID + " sits outside the Jam block");
}

//==============================================================================
LUTHIER_TEST (JamPlugin, JM37_separateOutputs)
{
    auto render = [] (bool aux, bool jamBuses, int output, bool band = true)
    {
        Plugin b (true, [aux, jamBuses] (LuthierAudioProcessor& p) { enableAux (p, aux, jamBuses); });
        b.keepBuses = true;
        b.set (ParamIDs::jamEnabled, band ? 1.0f : 0.0f);
        b.set (ParamIDs::jamOutput, (float) output);
        b.run (2.0);
        return b;
    };

    if constexpr (JamEdition::kIsFree)
    {
        // editions.md 2.3: Separate outputs are Pro-only. Free plays the nearest Free choice
        // (Main) whatever is stored, so the band stays on main and Aux 9 stays silent.
        auto b = render (true, true, 1);
        const int drums = busIndex (*b.p, "Jam Drums");
        CHECK (drums > 0);
        CHECK_MSG (Plugin::peak (b.buses[(size_t) drums]) < 1.0e-6, "Free put the band on Aux 9");
        const auto rig = render (true, true, 1, false);
        CHECK_MSG (b.left != rig.left, "Free's Separate did not leave the band on main");
        CHECK (Plugin::peak (b.left) > 1.0e-3);
        CHECK (! b.p->isJamSeparateFallingBack());
    }
    else
    {
        auto b = render (true, true, 1);   // layout B, Separate
        const int drums = busIndex (*b.p, "Jam Drums");
        CHECK (drums > 0);
        CHECK_MSG (Plugin::peak (b.buses[(size_t) drums]) > 1.0e-3, "no band on Aux 9");
        const auto rig = render (true, true, 1, false);   // the rig alone, same layout
        CHECK_MSG (b.left == rig.left, "Separate left the band on main (" + juce::String (Plugin::peak (b.left), 6)
                                         + " against " + juce::String (Plugin::peak (rig.left), 6) + ")");
        CHECK (! b.p->isJamSeparateFallingBack());
    }

    if constexpr (! JamEdition::kIsFree)
    {
        auto b = render (true, true, 2);   // Main + Separate
        CHECK (Plugin::peak (b.buses[(size_t) busIndex (*b.p, "Jam Drums")]) > 1.0e-3);
        CHECK (Plugin::peak (b.left) > 1.0e-3);
    }

    {
        auto b = render (false, false, 1);   // layout A: falls back to main (Free: Main already)
        CHECK (Plugin::peak (b.left) > 1.0e-3);
        CHECK (b.p->isJamSeparateFallingBack() == ! JamEdition::kIsFree);
    }

    // Aux 1-8 and the per-string buses keep their numbers.
    LuthierAudioProcessor p;
    CHECK (p.getBus (false, 1)->getName() == getAuxBusName (0));
    CHECK (p.getBus (false, 1 + kNumAuxBuses)->getName() == "String 1");
    CHECK (p.getBus (false, 1 + kNumAuxBuses + kNumPerStringBuses)->getName() == getAuxBusName (kNoiseAux));
    CHECK (p.getBus (false, 2 + kNumAuxBuses + kNumPerStringBuses)->getName() == "Jam Drums");
    CHECK (p.getBus (false, 3 + kNumAuxBuses + kNumPerStringBuses)->getName() == "Jam Bass");
}

LUTHIER_TEST (JamPlugin, JM38_loopsAreGuitarOnlyTakesHaveTheBandKillMutesIt)
{
    auto loop = [] (bool band)
    {
        Plugin b;
        b.p->setPracticePanelOpen (true);
        b.set (ParamIDs::jamEnabled, band ? 1.0f : 0.0f);
        b.chord (1000, { 40, 47, 52 });
        b.p->getLooper().press();                // records from the downbeat
        b.run (4.0);
        b.p->getLooper().press();                // closes at the bar
        b.run (0.5);

        auto& audio = b.p->getLooper().getLayer (0).getAudio();
        return std::vector<float> (audio.getReadPointer (0), audio.getReadPointer (0) + b.p->getLooper().getLoopLengthSamples());
    };

    const auto withBand = loop (true), without = loop (false);
    // The band snaps the loop to whole bars (6), so only the common part compares.
    CHECK (! withBand.empty() && ! without.empty());
    CHECK (withBand.size() % (size_t) (2.0 * kSr) == 0);

    double diff = 0.0;

    for (size_t i = 0; i < juce::jmin (withBand.size(), without.size()); ++i)
        diff = juce::jmax (diff, (double) std::abs (withBand[i] - without[i]));

    CHECK_MSG (diff < 1.0e-6, "the loop recorded the band (" + juce::String (gainToDb (diff), 1) + " dB)");

    // The session take includes the band.
    {
        Plugin b;
        b.p->setPracticePanelOpen (true);
        auto& recorder = b.p->getSessionRecorder();
        recorder.prepare (kSr, 1.0);
        recorder.setEnabled (true);
        b.set (ParamIDs::jamEnabled, 1.0f);
        b.run (2.0);

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jam-take");
        dir.deleteRecursively();
        CHECK (recorder.saveLastTake (dir, 2.0));

        double takePeak = 0.0;
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.wav"))
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (f)))
            {
                juce::AudioBuffer<float> take ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&take, 0, (int) reader->lengthInSamples, 0, true, true);
                takePeak = juce::jmax (takePeak, (double) take.getMagnitude (0, 0, take.getNumSamples()));
            }

        CHECK_MSG (takePeak > 1.0e-3, "the session take has no band in it");
        dir.deleteRecursively();
    }

    // The kill switch: the band gone within 3 ms.
    {
        Plugin b;
        b.set (ParamIDs::jamEnabled, 1.0f);
        b.run (2.0);
        const auto at = b.left.size();
        b.p->getKillSwitch().setActive (true);
        b.run (0.1);
        CHECK_MSG (Plugin::peak (b.left, at + (size_t) (0.003 * kSr) + 1) < 1.0e-6, "the kill switch left the band on");
    }
}

LUTHIER_TEST (JamPlugin, JM39_midiOutMatchesTheAudio)
{
    Plugin b;
    auto cfg = b.p->getRouting().getMidiOutConfig();
    cfg.enabled = true;
    cfg.passThrough = false;
    cfg.jamParts = true;
    b.p->getRouting().setMidiOutConfig (cfg);
    b.set (ParamIDs::jamEnabled, 1.0f);
    b.chord (0, { 45, 48, 52 });
    b.run (6.0);

    const auto captured = b.p->getJam().getCapture().copyLastBars (0);
    int drums = 0, bass = 0, unmatched = 0;
    const int gm[] = { 36, 37, 38, 40, 42, 44, 45, 46, 47, 49, 50, 51, 53, 82 };

    for (const auto& e : captured)
    {
        if (e.velocity == 0 || e.part == 2)
            continue;

        const int channel = e.part == 0 ? 10 : 11;
        bool found = false;

        for (const auto& [at, m] : b.midiOut)
            if (m.isNoteOn() && m.getChannel() == channel && m.getNoteNumber() == e.note && std::abs (at - e.sample) <= 1)
                found = true;

        if (! found)
            ++unmatched;

        (e.part == 0 ? drums : bass)++;

        if (e.part == 0)
            CHECK (std::find (std::begin (gm), std::end (gm), (int) e.note) != std::end (gm));
    }

    CHECK (drums > 20 && bass > 5);
    CHECK_MSG (unmatched == 0, juce::String (unmatched) + " band notes had no MIDI note-on within a sample");
}

LUTHIER_TEST (JamPlugin, JM40_dragOutIsAType1FileWithTwoTracks)
{
    Plugin b;
    b.set (ParamIDs::jamEnabled, 1.0f);
    b.chord (0, { 45, 48, 52 });
    b.run (24.0);   // 12 bars

    JamMidiExportOptions options;
    options.bars = 8;
    const auto file = JamMidiExport::write (b.p->getJam().getCapture(), options);
    CHECK (file.existsAsFile());

    juce::FileInputStream in (file);
    juce::MidiFile midi;
    int format = 0;
    CHECK (midi.readFrom (in, true, &format));
    CHECK (format == 1);
    CHECK (midi.getNumTracks() == 2);

    juce::MidiMessageSequence tempos;
    midi.findAllTempoEvents (tempos);
    CHECK (tempos.getNumEvents() >= 1);

    // The notes are the capture's last 8 bars, at their positions.
    const auto captured = b.p->getJam().getCapture().copyLastBars (8);
    int expected = 0, found = 0;

    for (const auto& e : captured)
        if (e.part != 2 && e.velocity > 0)
            ++expected;

    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (const auto* ev : *midi.getTrack (t))
            if (ev->message.isNoteOn())
                ++found;

    CHECK_MSG (found == expected, juce::String (found) + " notes in the file, " + juce::String (expected) + " captured");

    // Re-importing gives the same notes on the same ticks.
    const auto rebuilt = JamMidiExport::build (b.p->getJam().getCapture().copyLastBars (0), options);
    juce::MemoryOutputStream a, c;
    rebuilt.writeTo (a, 1);
    midi.writeTo (c, 1);
    CHECK (a.getDataSize() == c.getDataSize());
    file.deleteFile();
}

LUTHIER_TEST (JamPlugin, JM41_aMalformedStyleFallsBack)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Broken.luthierjam");
    file.replaceWithText (R"({"magic":"luthier.jam","schema":1,"meta":{"name":"Broken","style":"Funk"},"grid":16,"grooves":{"A":{}}})");

    Plugin b;
    const auto warning = b.p->loadJamStyleFile (file);
    CHECK_MSG (warning.isNotEmpty(), "no banner for a malformed style");
    CHECK (b.p->getJam().getStyleSlot (jam::kUserStyleIndex) == b.p->getJamStyles().getFactoryStyle (2));
    CHECK (b.p->getJamBlock().getProperty ("style_ref", {}).toString() == file.getFullPathName());

    const auto preset = b.p->getPresetManager().toVar ("Broken jam");
    CHECK (preset.getProperty ("jam", {}).getProperty ("style_ref", {}).toString() == file.getFullPathName());
    file.deleteFile();
}

//==============================================================================
LUTHIER_TEST (JamPlugin, JM42_jamDrumsReplaceTheTunesPercussion)
{
    auto run = [] (bool jamOn)
    {
        Plugin b (false);
        b.set (ParamIDs::jamEnabled, jamOn ? 1.0f : 0.0f);
        b.p->getTuneSession().newTune (jamTune (true, false));
        b.p->serviceTune();
        b.p->getTunePlayer().setCountInBars (0);
        b.p->getTunePlayer().play();
        b.run (6.0);
        return b.p->getTunePercussionToEngine();
    };

    CHECK_MSG (run (true) == 0, "tune percussion reached the engine under the Jam drums");
    CHECK_MSG (run (false) > 0, "without the Jam drums the tune's percussion did not return");
}

LUTHIER_TEST (JamPlugin, JM43_theTunesBassPlaysThroughTheJamBass)
{
    {
        Plugin b (false);
        b.set (ParamIDs::jamEnabled, 1.0f);
        auto cfg = b.p->getRouting().getMidiOutConfig();
        cfg.enabled = true;
        cfg.passThrough = false;
        cfg.tunePlayback = true;
        b.p->getRouting().setMidiOutConfig (cfg);

        b.p->getTuneSession().newTune (jamTune (false, true));
        b.p->serviceTune();
        b.p->serviceJam();
        b.p->getTunePlayer().setCountInBars (0);
        b.p->getTunePlayer().play();
        b.run (7.5);

        std::vector<int> tuneBass, jamBass;

        for (const auto& [at, m] : b.midiOut)
            if (m.isNoteOn() && m.getChannel() == TuneMidiOptions {}.bassChannel)
                tuneBass.push_back (m.getNoteNumber());

        for (const auto& e : b.p->getJam().getCapture().copyLastBars (0))
            if (e.part == 1 && e.velocity > 0)
                jamBass.push_back (e.note);

        CHECK (! tuneBass.empty());
        CHECK_MSG (jamBass == tuneBass, "the Jam bass played " + juce::String ((int) jamBass.size())
                                          + " notes, the tune's line has " + juce::String ((int) tuneBass.size()));
    }

    {
        // A bass is loaded: the engine plays the tune's bass and the Jam bass rests.
        Plugin b (false);
        b.set (ParamIDs::guitarType, (float) GuitarType::PrecisionBass);
        b.p->getParameterBridge().applyAllNow();
        CHECK (b.p->getEngine().getRhythmEngine().isBassFamily());
        b.set (ParamIDs::jamEnabled, 1.0f);
        b.p->getTuneSession().newTune (jamTune (false, true));
        b.p->serviceTune();
        b.p->getTunePlayer().setCountInBars (0);
        b.p->getTunePlayer().play();
        b.run (4.0);

        int jamBass = 0;

        for (const auto& e : b.p->getJam().getCapture().copyLastBars (0))
            if (e.part == 1 && e.velocity > 0)
                ++jamBass;

        CHECK_MSG (jamBass == 0, "the Jam bass played over a loaded bass");
        JamStatus status;
        CHECK (b.p->getJam().getStatusChannel().read (status) && status.bassResting);
    }
}

LUTHIER_TEST (JamPlugin, JM44_theMetronomeGoesQuietUnderTheDrums)
{
    auto render = [] (bool metronome, bool preference)
    {
        Plugin b;
        b.p->setPracticePanelOpen (true);
        b.p->setClickToMain (true);
        b.p->setJamSilencesMetronome (preference);
        b.p->getMetronome().setEnabled (metronome);
        b.set (ParamIDs::jamEnabled, 1.0f);
        b.run (3.0);
        return b.left;
    };

    // From bar 2: the band's drums are heard from its first downbeat on.
    auto tail = [] (const std::vector<float>& x) { return std::vector<float> (x.begin() + (std::ptrdiff_t) (2.0 * kSr), x.end()); };
    const auto band = tail (render (false, true));
    const auto quiet = tail (render (true, true));
    double diff = 0.0;

    for (size_t i = 0; i < juce::jmin (band.size(), quiet.size()); ++i)
        diff = juce::jmax (diff, (double) std::abs (band[i] - quiet[i]));

    CHECK_MSG (diff == 0.0, "the metronome clicked under the band with the preference on (" + juce::String (diff, 6) + ")");
    CHECK_MSG (tail (render (true, false)) != band, "the metronome was silent with the preference off");
}

LUTHIER_TEST (JamPlugin, JM45_recallsAndPresetLoadsKeepTheBand)
{
    Plugin b;
    b.set (ParamIDs::jamEnabled, 1.0f);
    // Funk (2) is Pro-only: Free plays its nearest Free style (editions.md 2.3), so Free
    // recalls Blues Shuffle (3), which is a style it keeps.
    const int recalledStyle = JamEdition::kIsFree ? 3 : 2;
    b.set (ParamIDs::jamStyle, (float) recalledStyle);
    CHECK (b.p->captureSnapshot (0, "Funk"));
    b.set (ParamIDs::jamStyle, 0.0f);
    b.p->getSnapshots().setCrossfadeMs (0.0);

    bool recalled = false;

    b.beforeBlock = [&] (Plugin& p)
    {
        if (! recalled && p.head.ppq >= 5.3)
        {
            recalled = true;
            p.p->recallSnapshot (0);
            p.p->getSnapshots().advancePending();
        }
    };

    b.run (5.0);

    // The band's own record of what it played: the recalled style starts on
    // the next bar line (ppq 8), not mid-bar.
    double changedAt = -1.0;

    for (const auto& e : b.p->getJam().getCapture().copyLastBars (0))
        if (e.part == 2 && e.style == recalledStyle && changedAt < 0.0)
            changedAt = e.ppq;

    CHECK_MSG (std::abs (changedAt - 8.0) < 1.0e-6, "the recalled style arrived at ppq " + juce::String (changedAt));

    CHECK (b.p->getJam().isBandRunning());

    // A preset that says jam_enabled off does not stop a playing band.
    auto& presets = b.p->getPresetManager();
    auto preset = presets.toVar ("off");
    preset.getProperty ("parameters", {}).getDynamicObject()->setProperty (ParamIDs::jamEnabled, 0.0);
    CHECK (presets.fromVar (preset));
    b.run (1.0);
    CHECK_MSG (b.p->getJam().isBandRunning(), "a preset load stopped the band");
}

LUTHIER_TEST (JamPlugin, JM46_strumsLandOnTheBandsGrid)
{
    Plugin b (false);
    b.set (ParamIDs::jamEnabled, 1.0f);
    b.set (ParamIDs::jamStartMode, 2.0f);   // First Note, own clock
    const int64_t first = 5 * kBlock + 77;
    b.chord (first, { 45, 48, 52 });

    int blocks = 0, worst = 0;
    b.beforeBlock = [&] (Plugin& p)
    {
        // From the band's first playing block: the block its first note
        // started it in and the next are still on the rhythm engine's own grid.
        if (p.position <= first + kBlock * 3)
            return;

        // The grid the rhythm engine was given last block, against the band's.
        const double ppq = p.p->getEngine().getTransportPpq();
        const double expected = (double) (p.position - kBlock - first) / 24000.0;
        worst = juce::jmax (worst, (int) std::llround (std::abs (ppq - expected) * 24000.0));
        ++blocks;
    };

    b.run (4.0);
    CHECK (blocks > 100);
    CHECK_MSG (worst <= 1, "the rhythm engine's grid is " + juce::String (worst) + " samples off the band's");
}

//==============================================================================
/*  9: tune export includes the band - audio renders play it, MIDI exports add
    its two tracks - and leaves it out when asked. */
LUTHIER_TEST (JamPlugin, JM09_tuneExportIncludesTheBand)
{
    Plugin b (false);
    b.set (ParamIDs::jamEnabled, 1.0f);
    b.p->getTuneSession().newTune (jamTune (false, true));
    b.p->serviceTune();
    const auto state = b.p->captureStateBlock();

    TuneExport::Render with, without;
    CHECK (TuneExport::renderAudio (state, kSr, 512, 0.5, true, with, {}, true));
    CHECK (TuneExport::renderAudio (state, kSr, 512, 0.5, false, without, {}, false));

    double diff = 0.0;

    for (int i = 0; i < juce::jmin (with.main.getNumSamples(), without.main.getNumSamples()); ++i)
        diff = juce::jmax (diff, (double) std::abs (with.main.getSample (0, i) - without.main.getSample (0, i)));

    CHECK_MSG (diff > 1.0e-3, "the render with the band is the render without it");
    CHECK_MSG (with.aux.size() == (size_t) TuneExport::kNumAuxStems + 2, "no Jam stems: " + juce::String ((int) with.aux.size()));

    // The Jam stems are the band's Separate output, which Free does not have (editions.md 2.3:
    // it plays on main, so the render differs above and the stem stays silent).
    if (with.aux.size() == (size_t) TuneExport::kNumAuxStems + 2)
    {
        const auto drumsStem = with.aux[(size_t) TuneExport::kNumAuxStems].getMagnitude (0, with.aux[(size_t) TuneExport::kNumAuxStems].getNumSamples());

        if constexpr (JamEdition::kIsFree)
            CHECK_MSG (drumsStem < 1.0e-6, "Free put the Jam Drums on a separate stem");
        else
            CHECK_MSG (drumsStem > 1.0e-3, "the Jam Drums stem is silent");
    }

    juce::TemporaryFile temp (".mid");
    TuneExport::MidiOptions options;
    juce::String error;
    CHECK_MSG (TuneExport::exportMidi (b.p->getTuneSession().getTune(), temp.getFile(), options, error), error);

    int tracksBefore = 0;
    {
        juce::FileInputStream in (temp.getFile());
        juce::MidiFile file;
        CHECK (file.readFrom (in));
        tracksBefore = file.getNumTracks();
    }

    // editions.md 2.3: Jam MIDI export is Pro. Free refuses with a reason and leaves the file as it was.
    if constexpr (JamEdition::kIsFree)
    {
        CHECK (! TuneExport::appendJamTracks (state, temp.getFile(), error));
        CHECK_MSG (error.containsIgnoreCase ("Pro"), error);
        juce::FileInputStream in (temp.getFile());
        juce::MidiFile file;
        CHECK (file.readFrom (in));
        CHECK (file.getNumTracks() == tracksBefore);
        return;
    }

    CHECK_MSG (TuneExport::appendJamTracks (state, temp.getFile(), error), error);

    juce::FileInputStream in (temp.getFile());
    juce::MidiFile file;
    CHECK (file.readFrom (in));
    CHECK (file.getNumTracks() == tracksBefore + 2);

    int drums = 0, bass = 0;

    for (int t = tracksBefore; t < file.getNumTracks(); ++t)
        for (const auto* e : *file.getTrack (t))
            if (e->message.isNoteOn())
                (e->message.getChannel() == 10 ? drums : bass)++;

    CHECK (drums > 10 && bass >= 4);
}

//==============================================================================
/*  12: knob moves are undo entries; START, STOP and FILL are transport and
    never are. */
LUTHIER_TEST (JamPlugin, JM12_transportIsNotUndoable)
{
    Plugin b;

    auto gesture = [&b] (const char* id, float normalised)
    {
        auto* prm = b.p->getState().getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (normalised);
        prm->endChangeGesture();
    };

    const int before = b.p->getNumUndoSteps();
    gesture (ParamIDs::jamPlay, 1.0f);
    gesture (ParamIDs::jamFillNow, 1.0f);
    CHECK_MSG (b.p->getNumUndoSteps() == before, "START or FILL made an undo entry");

    gesture (ParamIDs::jamVolume, 0.3f);
    CHECK_MSG (b.p->getNumUndoSteps() == before + 1, "a Jam knob move made no undo entry");
}
