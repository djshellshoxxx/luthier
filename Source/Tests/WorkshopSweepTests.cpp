/*  SPEC-SWEEP ui-wiring UW-T7 (ui-wiring 23): auditions never commit. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Model/Workshop/PartAcoustics.h"

#include <functional>

using namespace luthier;
using namespace luthier::tests;

/*  UW-T7: a hundred seeded random auditions across every slot leave the
    committed guitar byte-for-byte as it was, and the undo stack untouched. */
LUTHIER_TEST (WorkshopBench, aHundredRandomAuditionsNeverCommit)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& bench = processor.getBench();
    auto& library = processor.getPartLibrary();

    const auto before = juce::JSON::toString (processor.getCurrentGuitar().toEmbeddedVar());
    const int undoBefore = processor.getNumUndoSteps();

    juce::Random random (0x5eed7);
    int auditioned = 0;

    for (int i = 0; i < 100; ++i)
    {
        const auto slot = (GuitarSlot) random.nextInt (kNumGuitarSlots);
        const auto parts = library.getParts (getSlotPartType (slot));

        if (parts.isEmpty())
            continue;

        bench.beginAudition (slot, parts[random.nextInt (parts.size())]);
        auditioned += bench.isAuditioning() ? 1 : 0;

        // Some auditions render a block, as a player listening would.
        if (i % 10 == 0)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        bench.endAudition();
    }

    CHECK (auditioned >= 50);
    CHECK_MSG (juce::JSON::toString (processor.getCurrentGuitar().toEmbeddedVar()) == before,
               "an audition changed the committed guitar");
    CHECK (processor.getNumUndoSteps() == undoBefore);
    CHECK (! processor.isGuitarEdited());
}

//==============================================================================
namespace
{
    /** Renders @p seconds of a ringing chord (re-struck every second) through
        @p engine, calling @p betweenBlocks before every block after the first. */
    std::vector<float> ringAndSwap (LuthierEngine& engine, double seconds, const std::function<void (int)>& betweenBlocks)
    {
        constexpr int block = 256;
        juce::AudioBuffer<float> buffer (2, block);
        std::vector<float> out;
        const int blocks = (int) (seconds * 48000.0 / block);

        for (int b = 0; b < blocks; ++b)
        {
            if (b > 0 && betweenBlocks)
                betweenBlocks (b);

            juce::MidiBuffer midi;

            if (b % 188 == 0)   // about once a second
                for (int note : { 45, 52, 57 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            buffer.clear();
            engine.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }

        return out;
    }

    float largestStepOf (const std::vector<float>& x)
    {
        float largest = 0.0f;

        for (size_t i = 1; i < x.size(); ++i)
            largest = juce::jmax (largest, std::abs (x[i] - x[i - 1]));

        return largest;
    }
}

/*  UW-T9 (ui-wiring 23): every Workshop slot swaps between two parts a hundred
    times while a chord rings, and no swap steps harder than the playing does. */
LUTHIER_TEST (WorkshopSwap, everySlotSwapsHundredTimesClickFree)
{
    PartLibrary library;
    library.refresh();

    WorkshopGuitar base;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder()
                                 .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (GuitarType::LesPaul)),
                               base, report));

    // The playing's own steepest step, with no swaps at all.
    float natural = 0.0f;
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.applyWorkshopGuitar (mapSpec (base), GuitarType::LesPaul);
        natural = largestStepOf (ringAndSwap (engine, 2.2, {}));
    }

    CHECK (natural > 1.0e-4f);

    for (auto slot : { GuitarSlot::pickupBridge, GuitarSlot::pickupNeck, GuitarSlot::bridge,
                       GuitarSlot::nut, GuitarSlot::strings, GuitarSlot::wiring })
    {
        const auto current = base.get (slot);

        if (current == nullptr)
            continue;

        PartPtr other;

        for (const auto& p : library.getParts (getSlotPartType (slot)))
            if (p->name != current->name)
            {
                other = p;
                break;
            }

        if (other == nullptr)
            continue;

        auto swapped = base;
        swapped.parts[(size_t) slot] = other;

        const auto specA = mapSpec (base);
        const auto specB = mapSpec (swapped);

        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.applyWorkshopGuitar (specA, GuitarType::LesPaul);

        int swaps = 0;

        const auto out = ringAndSwap (engine, 2.2, [&] (int b)
        {
            // A swap every fourth block: a hundred in 400 blocks.
            if (b % 4 == 0 && swaps < 100)
            {
                engine.applyWorkshopGuitar ((swaps % 2 == 0) ? specB : specA, GuitarType::LesPaul);
                ++swaps;
            }
        });

        const float step = largestStepOf (out);

        CHECK (swaps == 100);
        CHECK_MSG (step <= natural * 1.1f + 0.001f,
                   juce::String (getSlotId (slot)) + ": a swap stepped " + juce::String (step, 5)
                     + " against the playing's own " + juce::String (natural, 5));
    }
}

//==============================================================================
/*  UW-T3 (ui-wiring 23): preset loads in a random order, interleaved with
    rendering, leave a deterministic instrument - a second instance given the
    same sequence renders the same final clip (< -120 dB difference). */
LUTHIER_TEST (Presets, randomPresetLoadsAreDeterministic)
{
    auto run = [] (const juce::Array<int>& order)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels()), 512);
        auto blocks = [&] (int count, bool strike)
        {
            std::vector<float> out;

            for (int b = 0; b < count; ++b)
            {
                juce::MidiBuffer midi;

                if (strike && b == 0)
                    for (int note : { 40, 47, 52, 56 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
            }

            return out;
        };

        for (int index : order)
        {
            processor.getPresetManager().loadPreset (index);
            processor.getParameterBridge().applyAllNow();
            blocks (4, true);
        }

        processor.getEngine().reset();
        return blocks (48, true);   // the final clip, half a second
    };

    LuthierAudioProcessor probe;
    const int available = probe.getPresetManager().getNumPresets();
    CHECK (available >= 10);

    if (available < 10)
        return;

    juce::Random random (0x7e57);

    for (int trial = 0; trial < 3; ++trial)
    {
        juce::Array<int> order;

        for (int i = 0; i < 10; ++i)
            order.add (random.nextInt (juce::jmin (20, available)));

        const auto a = run (order);
        const auto b = run (order);

        double worst = 0.0, level = 0.0;

        for (size_t i = 0; i < a.size(); ++i)
        {
            worst = juce::jmax (worst, (double) std::abs (a[i] - b[i]));
            level = juce::jmax (level, (double) std::abs (a[i]));
        }

        CHECK (level > 1.0e-3);
        CHECK_MSG (worst < 1.0e-6,
                   "trial " + juce::String (trial) + ": the same load sequence rendered differently by "
                     + juce::String (juce::Decibels::gainToDecibels (worst, -200.0), 1) + " dB");
    }
}
