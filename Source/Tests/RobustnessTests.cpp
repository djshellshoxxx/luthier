/*  qa-polish.md 2.2 / 2.3 / 3 / 5.4: fuzzing, state fuzzing and stress.

    Each stress test is kept to a few seconds in the default run; the long
    forms (100 000 parameter states, 10 000 state operations, 32 instances)
    run under LUTHIER_PERF=1.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

#include <thread>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    struct BlockStats
    {
        bool finite = true;
        float peak = 0.0f;
    };

    BlockStats renderBlocks (LuthierAudioProcessor& p, int blocks, juce::MidiBuffer* first = nullptr)
    {
        BlockStats stats;
        juce::AudioBuffer<float> buffer (juce::jmax (p.getTotalNumOutputChannels(), p.getTotalNumInputChannels()), kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;

            if (b == 0 && first != nullptr)
                midi = *first;

            buffer.clear();
            p.processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const auto* d = buffer.getReadPointer (ch);

                for (int i = 0; i < kBlock; ++i)
                {
                    stats.finite = stats.finite && std::isfinite (d[i]);
                    stats.peak = juce::jmax (stats.peak, std::abs (d[i]));
                }
            }
        }

        return stats;
    }

    void setParam (LuthierAudioProcessor& p, const juce::String& id, float normalised)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (normalised);
    }

    juce::MidiBuffer chordOn (int root)
    {
        juce::MidiBuffer midi;

        for (int k = 0; k < 4; ++k)
            midi.addEvent (juce::MidiMessage::noteOn (1, root + k * 5, (juce::uint8) 100), k);

        return midi;
    }

    /** After panic, the strings fall silent. */
    double loudestString (LuthierAudioProcessor& p)
    {
        double loudest = 0.0;

        for (int s = 0; s < p.getEngine().getNumStrings(); ++s)
            loudest = juce::jmax (loudest, p.getEngine().getStringLevel (s));

        return loudest;
    }
}

//==============================================================================
/*  QA-2.2, nightly form: a hundred thousand random parameter states, one in a
    hundred rendered with audio on the sidechain (re-amp on for half of those),
    checked finite, bounded and free of denormals. The default run's ten
    thousand live in IntegrationTests (Parameters.fuzzAcrossTenThousandStates). */
LUTHIER_TEST (Parameters, fuzzAcrossHundredThousandStates)
{
    if (! perfRunRequested())
        return;

    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);

    auto& engine = p->getEngine();
    auto& bridge = p->getParameterBridge();
    juce::Random rng (0x100000);

    std::vector<float> sidechain ((size_t) kBlock);
    int rendered = 0, bad = 0, tiny = 0;

    for (int state = 0; state < 100000; ++state)
    {
        for (auto* param : p->getParameters())
            param->setValue (rng.nextFloat());

        bridge.applyToEngine();

        if (state % 100 != 0)
            continue;

        bridge.applyAllNow();
        engine.panic();
        engine.setSidechainToAmp ((state / 100) % 2 == 0);

        juce::AudioBuffer<float> buffer (2, kBlock);

        for (int b = 0; b < 30; ++b)
        {
            for (auto& s : sidechain)
                s = rng.nextFloat() * 0.4f - 0.2f;

            const float* channels[2] = { sidechain.data(), sidechain.data() };
            engine.setSidechainInput (channels, 2, kBlock);

            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40 + state % 30, 0.9f), 0);

            buffer.clear();
            engine.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                {
                    const auto v = buffer.getSample (ch, i);
                    bad += (! std::isfinite (v) || std::abs (v) > 1.05f) ? 1 : 0;
                    tiny += (v != 0.0f && std::abs (v) < 1.0e-30f) ? 1 : 0;
                }
        }

        engine.setSidechainInput (nullptr, 0, 0);
        ++rendered;
    }

    CHECK (rendered == 1000);
    CHECK_MSG (bad == 0, juce::String (bad) + " non-finite or over-full-scale samples");
    CHECK_MSG (tiny == 0, juce::String (tiny) + " denormal-range samples");
}

//==============================================================================
/*  QA-2.3: random operations - preset loads, snapshot captures and recalls,
    setlist steps, modulation routes, Workshop fits, undo and redo - with a
    block rendered between each. Nothing goes non-finite, the undo stack stays
    within its cap, and after panic() no string is left sounding. 1000
    operations by default, 10 000 under LUTHIER_PERF=1. */
LUTHIER_TEST (StateModel, tenThousandRandomOperationsLeaveNoStuckState)
{
    const int operations = perfRunRequested() ? 10000 : 1000;

    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);
    FactoryPresets::setProcessorForRanges (p.get());

    auto& presets = p->getPresetManager();
    auto& snapshots = p->getSnapshots();
    auto& matrix = p->getModMatrix();
    auto& bench = p->getBench();
    auto& library = p->getPartLibrary();

    // A setlist of the first few installed presets.
    {
        Setlist list;

        for (int i = 0; i < juce::jmin (6, presets.getNumPresets()); ++i)
            if (const auto* info = presets.getPreset (i))
            {
                SetlistEntry entry;
                entry.presetPath = info->file.getFullPathName();
                list.addEntry (entry);
            }

        p->getSetlist().setSetlist (list);
    }

    static const char* const destinations[] = { ParamIDs::macroTone, ParamIDs::macroDrive, ParamIDs::macroSpace,
                                                ParamIDs::pluckPosition, ParamIDs::stereoWidth };

    juce::Random rng (0x2300);
    bool finite = true;
    int maxUndo = 0;

    for (int op = 0; op < operations; ++op)
    {
        // Undo and redo are the costly operations (a whole state restore);
        // they get one slot in twenty each.
        const int pick = rng.nextInt (20);

        const int operation = pick < 6 ? pick : pick == 6 ? 6 : pick == 7 ? 7 : pick < 14 ? pick - 8 : 8;

        switch (operation)
        {
            case 0:
            {
                const int i = rng.nextInt (FactoryPresets::getNumPresets());
                presets.fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), *p));
                break;
            }
            case 1:  snapshots.capture (rng.nextInt (8)); break;
            case 2:  snapshots.recall (rng.nextInt (8)); break;
            case 3:  p->getSetlist().next(); break;
            case 4:
            {
                ModRoute route;
                route.sourceId = "lfo" + juce::String (1 + rng.nextInt (4));
                route.destinationId = destinations[rng.nextInt ((int) std::size (destinations))];
                route.depth = rng.nextFloat() * 2.0f - 1.0f;

                if (matrix.getNumRoutes() > 64)
                    matrix.clearRoutes();

                matrix.addRoute (route);
                break;
            }
            case 5:
            {
                const auto parts = library.getParts (PartType::bridge);

                if (! parts.isEmpty())
                    bench.fit (GuitarSlot::bridge, parts[rng.nextInt (parts.size())]);
                break;
            }
            case 6:  p->undo(); break;
            case 7:  p->redo(); break;
            default:
            {
                auto midi = chordOn (40 + rng.nextInt (12));
                finite = finite && renderBlocks (*p, 1, &midi).finite;
                break;
            }
        }

        finite = finite && renderBlocks (*p, 1).finite;
        maxUndo = juce::jmax (maxUndo, p->getNumUndoSteps());
    }

    CHECK_MSG (finite, "a non-finite sample during the state fuzz");
    CHECK_MSG (maxUndo <= 201, "the undo stack grew to " + juce::String (maxUndo));

    // A snapshot recall in flight finishes within its crossfade (<= 500 ms):
    // none is stuck.
    renderBlocks (*p, (int) (0.6 * kSr / kBlock));
    CHECK_MSG (! snapshots.isRecalling(), "a snapshot recall never finished");

    // Back to a neutral sound (the last random preset may have feedback, an
    // EBow or the rhythm engine playing on its own), then panic: nothing is
    // left held or ringing.
    for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
        if (juce::String (FactoryPresets::getPreset (i).name) == "Init")
            presets.fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), *p));

    matrix.clearRoutes();
    p->getParameterBridge().applyAllNow();
    p->panic();
    renderBlocks (*p, (int) (0.5 * kSr / kBlock));
    CHECK_MSG (loudestString (*p) < 1.0e-3, "a string still sounds after panic: " + juce::String (loudestString (*p), 6));

    // And once the room and delay tails have rung out, the output is back to
    // the idle noise floor (a stuck noise voice held it near -13 dBFS).
    renderBlocks (*p, (int) (2.5 * kSr / kBlock));
    const auto tail = renderBlocks (*p, (int) (0.5 * kSr / kBlock));
    CHECK_MSG (tail.peak < 0.01f, "the output never returned to idle: peak " + juce::String (tail.peak, 5));
}

//==============================================================================
/*  QA-3.x: a MIDI storm - a thousand notes a second - on an audio thread while
    the message thread loads presets, recalls snapshots and loads guitars. */
LUTHIER_TEST (Stress, midiStormDuringLoadsRecallsAndGuitarLoads)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);
    FactoryPresets::setProcessorForRanges (p.get());

    for (int s = 0; s < 4; ++s)
        p->getSnapshots().capture (s);

    std::atomic<bool> stop { false }, finite { true };
    std::atomic<int> notes { 0 }, blocks { 0 };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), kBlock);
        juce::MidiBuffer midi;
        midi.ensureSize (4096);
        juce::Random rng (99);

        // 1000 notes/s at 48 kHz / 256: about five and a third a block.
        double owed = 0.0;

        while (! stop.load())
        {
            midi.clear();
            owed += 1000.0 * kBlock / kSr;

            for (; owed >= 1.0; owed -= 1.0)
            {
                const int note = 40 + rng.nextInt (36);
                const int at = rng.nextInt (kBlock);
                midi.addEvent (juce::MidiMessage::noteOn (1 + rng.nextInt (6), note, (juce::uint8) (40 + rng.nextInt (87))), at);
                midi.addEvent (juce::MidiMessage::noteOff (1 + rng.nextInt (6), 40 + rng.nextInt (36)), rng.nextInt (kBlock));
                ++notes;
            }

            buffer.clear();
            p->processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        finite = false;

            ++blocks;
            std::this_thread::sleep_for (std::chrono::microseconds (500));
        }
    });

    juce::Random rng (7);
    static constexpr GuitarType types[] = { GuitarType::Stratocaster, GuitarType::LesPaul, GuitarType::JazzBass, GuitarType::Dreadnought };

    for (int round = 0; round < 12; ++round)
    {
        p->getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (rng.nextInt (FactoryPresets::getNumPresets())), *p));
        p->getSnapshots().recall (rng.nextInt (4));

        if (auto* param = p->getState().getParameter (ParamIDs::guitarType))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) types[round % 4]));

        p->getParameterBridge().applyAllNow();
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }

    stop = true;
    audio.join();

    CHECK (finite.load());
    CHECK_MSG (notes.load() > 500, juce::String (notes.load()) + " notes sent");
    CHECK (blocks.load() > 50);
}

/*  QA-3.x: MIDI Learn armed and disarmed a hundred times while CCs arrive. */
LUTHIER_TEST (Stress, midiLearnArmDisarmHundredTimes)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);
    auto& learn = p->getMidiLearn();

    juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), kBlock);

    for (int i = 0; i < 100; ++i)
    {
        learn.setArmed (true);
        learn.claimArmedLearn (i % 2 == 0 ? ParamIDs::macroDrive : ParamIDs::macroTone);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 20 + i % 10, 64), 0);
        buffer.clear();
        p->processBlock (buffer, midi);

        if (i % 3 == 0)
            learn.servicePendingLearn();

        learn.setArmed (false);
        CHECK (! learn.isLearning());
    }

    CHECK (! learn.isArmed());
    CHECK (learn.getNumMappings() <= 2);   // one CC per parameter at most
}

/*  QA-3.x: undo and redo a thousand times (see the step count below). */
LUTHIER_TEST (Stress, undoRedoThousandTimes)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);
    auto& bench = p->getBench();
    const auto parts = p->getPartLibrary().getParts (PartType::bridge);

    for (int i = 0; i < 20 && ! parts.isEmpty(); ++i)
        bench.fit (GuitarSlot::bridge, parts[i % parts.size()]);

    const int depth = p->getNumUndoSteps();
    bool finite = true;

    // Each step is a whole state restore (~65 ms here): 1000 under
    // LUTHIER_PERF=1, 60 in the default run to keep it to a few seconds.
    const int steps = perfRunRequested() ? 1000 : 60;

    for (int i = 0; i < steps; ++i)
    {
        if ((i / 7) % 2 == 0)
            p->undo();
        else
            p->redo();

        if (i % 10 == 0)
            finite = finite && renderBlocks (*p, 1).finite;
    }

    CHECK (finite);
    CHECK (p->getNumUndoSteps() >= 1 && p->getNumUndoSteps() <= depth);
}

/*  QA-3.x: the slide switched on and off a hundred times while a chord rings,
    and the advanced ranges toggled a hundred times mid-play. */
LUTHIER_TEST (Stress, slideAndAdvancedRangeTogglesMidPlay)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);

    auto chord = chordOn (40);
    auto stats = renderBlocks (*p, 2, &chord);

    for (int i = 0; i < 100; ++i)
    {
        setParam (*p, ParamIDs::slideGuitar, (i % 2 == 0) ? 1.0f : 0.0f);
        const auto s = renderBlocks (*p, 1);
        stats.finite = stats.finite && s.finite;
        stats.peak = juce::jmax (stats.peak, s.peak);
    }

    for (int i = 0; i < 100; ++i)
    {
        auto ranges = p->getRanges();
        ranges.setFamilyAdvanced ((RangeFamily) (i % 3), i % 2 == 0);
        p->changeRanges (ranges, "stress");

        const auto s = renderBlocks (*p, 1);
        stats.finite = stats.finite && s.finite;
        stats.peak = juce::jmax (stats.peak, s.peak);
    }

    CHECK (stats.finite);
    CHECK_MSG (stats.peak <= 1.05f, "peak " + juce::String (stats.peak, 3));
}

/*  QA-3.x: the host changes the bus layout mid-play (aux and per-string buses
    on and off), re-preparing as a host does. */
LUTHIER_TEST (Stress, busLayoutChangesMidPlay)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, kBlock);
    auto chord = chordOn (45);
    renderBlocks (*p, 2, &chord);

    bool finite = true;

    for (int i = 0; i < 12; ++i)
    {
        auto layout = p->getBusesLayout();

        for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
        {
            const bool on = ((bus + i) % 3) == 0;
            const bool isString = bus > kNumAuxBuses && bus <= kNumAuxBuses + kNumPerStringBuses;
            layout.outputBuses.getReference (bus) = ! on ? juce::AudioChannelSet::disabled()
                                                  : isString ? juce::AudioChannelSet::mono()
                                                             : juce::AudioChannelSet::stereo();
        }

        p->setBusesLayout (layout);          // may be refused; either is fine

        if (i % 2 == 0)
            p->prepareToPlay (kSr, kBlock);

        auto again = chordOn (40 + i);
        finite = finite && renderBlocks (*p, 3, &again).finite;
    }

    CHECK (finite);
}

/*  QA-3.x: many instances in one process, rendering in turn. Eight by default
    (each instance is ~125 MB resident, most of it the looper), thirty-two
    under LUTHIER_PERF=1. */
LUTHIER_TEST (Stress, thirtyTwoInstancesRenderInTurn)
{
    const int count = perfRunRequested() ? 32 : 8;
    std::vector<std::unique_ptr<LuthierAudioProcessor>> instances;

    for (int i = 0; i < count; ++i)
    {
        instances.push_back (std::make_unique<LuthierAudioProcessor>());
        instances.back()->prepareToPlay (kSr, kBlock);
    }

    bool finite = true;
    float quietest = 1.0e9f;

    for (int round = 0; round < 4; ++round)
        for (int i = 0; i < count; ++i)
        {
            auto midi = chordOn (40 + i % 12);
            const auto s = renderBlocks (*instances[(size_t) i], 4, round == 0 ? &midi : nullptr);
            finite = finite && s.finite;

            if (round == 0)
                quietest = juce::jmin (quietest, s.peak);
        }

    CHECK (finite);
    CHECK_MSG (quietest > 1.0e-4f, "an instance was silent: every instance must play independently");
}

//==============================================================================
/*  QA-5.4: "DC null: silent input produces silent output within -100 dBFS
    RMS." The default rig's amp buzz (mains hum through the single coils) is a
    deliberate noise floor, so the null is asserted with it zeroed; the default
    floor is Engine.silenceInSilenceOut's. */
LUTHIER_TEST (Engine, silenceInSilenceOutWithNoiseFloorOff)
{
    for (auto type : { GuitarType::Stratocaster, GuitarType::LesPaul, GuitarType::Dreadnought, GuitarType::JazzBass })
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (type);
        engine.setAmpBuzzAmount (0.0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;
        double energy = 0.0;
        int n = 0;

        for (int b = 0; b < (int) (1.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, none);

            if (b >= 20)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < kBlock; ++i, ++n)
                        energy += (double) buffer.getSample (ch, i) * buffer.getSample (ch, i);
        }

        const double db = 10.0 * std::log10 (energy / juce::jmax (1, n) + 1.0e-30);
        CHECK_MSG (db <= -100.0, "guitar type " + juce::String ((int) type) + ": silence renders at " + juce::String (db, 1) + " dBFS RMS");
    }
}
