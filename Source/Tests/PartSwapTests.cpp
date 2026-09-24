/*  TODO 6e / DECISIONS C-09 / ui-wiring.md 6.2 (MODEL-GAPS): a part swap that
    keeps the string count is built on the message thread and swapped in at
    the audio thread's next block boundary - no park, no silence, no click. */

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Model/Workshop/PartAcoustics.h"

#include <thread>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    WorkshopGuitar factoryGuitar (PartLibrary& library, GuitarType type)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        library.loadGuitar (PartLibrary::getFactoryGuitarsFolder()
                              .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (type)), g, report);
        return g;
    }

    /** Renders on its own thread as a host does; calls `midway` on this one once `atSample` are down. */
    template <typename Fn>
    int renderAround (LuthierEngine& engine, std::vector<float>& out, int atSample, Fn&& midway)
    {
        std::atomic<int> written { 0 };

        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, kBlock);

            while (written.load() + kBlock <= (int) out.size())
            {
                juce::MidiBuffer midi;

                if (written.load() == 0)
                    for (int note : { 45, 52, 57 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

                engine.processBlock (buffer, midi);
                std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock, out.begin() + written.load());
                written += kBlock;
                std::this_thread::yield();
            }
        });

        while (written.load() < atSample)
            std::this_thread::yield();

        const int startedAt = written.load();
        midway();
        audio.join();
        return startedAt;
    }

    float largestStep (const std::vector<float>& x, int from, int to)
    {
        float largest = 0.0f;

        for (int i = juce::jmax (1, from); i < juce::jmin ((int) x.size(), to); ++i)
            largest = juce::jmax (largest, std::abs (x[(size_t) i] - x[(size_t) i - 1]));

        return largest;
    }
}

LUTHIER_TEST (PartSwap, aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence)
{
    PartLibrary library;
    library.refresh();

    const auto before = factoryGuitar (library, GuitarType::LesPaul);
    auto after = before;

    for (const auto& part : library.getParts (PartType::pickup))
        if (part->name != before.get (GuitarSlot::pickupBridge)->name)
            { after.parts[(size_t) GuitarSlot::pickupBridge] = part; break; }

    const auto derivedAfter = mapSpec (after);

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.applyWorkshopGuitar (mapSpec (before), GuitarType::LesPaul);
    CHECK (engine.partSwapKeepsStructure (derivedAfter));

    std::vector<float> out ((size_t) (kSr * 1.5), 0.0f);
    bool taken = false;

    const int swapAt = renderAround (engine, out, (int) (kSr * 0.6), [&]
    {
        taken = engine.swapPartsAtBlockBoundary (derivedAfter, GuitarType::LesPaul);
    });

    CHECK_MSG (taken, "the swap was not taken at a block boundary");
    CHECK (engine.getLivePartSwapCount() == 1);

    // The new pickup is the engine's now.
    CHECK_NEAR (engine.getPartsPickup (0).position, derivedAfter.pickups[0].spec.position, 1.0e-12);

    // No park: the note never goes silent around the swap ...
    int longestSilence = 0, run = 0;

    for (int i = swapAt - 512; i < swapAt + (int) (kSr * 0.1); ++i)
    {
        run = out[(size_t) i] == 0.0f ? run + 1 : 0;
        longestSilence = juce::jmax (longestSilence, run);
    }

    CHECK_MSG (longestSilence < 8, "the note went silent for " + juce::String (longestSilence) + " samples");

    // ... and does not click: no step larger than the note's own, plus -60 dBFS.
    const float natural = largestStep (out, (int) (kSr * 0.3), swapAt - 512);
    const float atSwap = largestStep (out, swapAt - 512, swapAt + (int) (kSr * 0.1));

    CHECK_MSG (natural > 1.0e-4f, "the note is not sounding before the swap");
    CHECK_MSG (atSwap <= natural + 0.001f,
               "the swap stepped by " + juce::String (atSwap, 5) + " against the note's own " + juce::String (natural, 5));

    for (float v : out)
        CHECK (std::isfinite (v));
}

LUTHIER_TEST (PartSwap, aStructuralChangeIsNotALiveSwap)
{
    PartLibrary library;
    library.refresh();

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.applyWorkshopGuitar (mapSpec (factoryGuitar (library, GuitarType::LesPaul)), GuitarType::LesPaul);

    // Another string count, or another family: a load, not a swap.
    CHECK (! engine.partSwapKeepsStructure (mapSpec (factoryGuitar (library, GuitarType::SevenString))));
    CHECK (! engine.partSwapKeepsStructure (mapSpec (factoryGuitar (library, GuitarType::JazzBass))));

    // No audio thread running: nothing to wait for, so no live swap; the caller applies it.
    CHECK (! engine.swapPartsAtBlockBoundary (mapSpec (factoryGuitar (library, GuitarType::LesPaul)), GuitarType::LesPaul));
    CHECK (engine.getLivePartSwapCount() == 0);

    // A compiled guitar has no parts to swap.
    LuthierEngine compiled;
    compiled.prepare (kSr, kBlock);
    compiled.setGuitarType (GuitarType::LesPaul);
    CHECK (! compiled.partSwapKeepsStructure (mapSpec (factoryGuitar (library, GuitarType::LesPaul))));
}

/*  The same swap, live or parked, leaves the engine in the same place: a note
    played after either renders the same. */
LUTHIER_TEST (PartSwap, aLiveSwapEndsWhereAParkedOneDoes)
{
    PartLibrary library;
    library.refresh();

    const auto before = factoryGuitar (library, GuitarType::Stratocaster);
    auto after = before;

    for (const auto& part : library.getParts (PartType::pickup))
        if (part->name != before.get (GuitarSlot::pickupBridge)->name)
            { after.parts[(size_t) GuitarSlot::pickupBridge] = part; break; }

    const auto derivedAfter = mapSpec (after);

    auto noteAfter = [&] (bool live)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.applyWorkshopGuitar (mapSpec (before), GuitarType::Stratocaster);

        std::vector<float> warm ((size_t) (kSr * 0.3), 0.0f);
        renderAround (engine, warm, (int) (kSr * 0.1), [&]
        {
            if (! (live && engine.swapPartsAtBlockBoundary (derivedAfter, GuitarType::Stratocaster)))
                engine.applyWorkshopGuitar (derivedAfter, GuitarType::Stratocaster);
        });

        CHECK (engine.getLivePartSwapCount() == (live ? 1 : 0));
        engine.reset();

        NoteOnEvent e;
        e.stringIndex = 1;
        e.velocity = 0.9;
        e.pitchHz = 246.94;
        engine.triggerNoteNow (e);

        std::vector<double> x;
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < 60; ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);
            x.insert (x.end(), block.getReadPointer (0), block.getReadPointer (0) + kBlock);
        }

        return x;
    };

    const auto live = noteAfter (true);
    const auto parked = noteAfter (false);

    double diff = 0.0, peak = 0.0;

    for (size_t i = 0; i < live.size(); ++i)
    {
        diff = juce::jmax (diff, std::abs (live[i] - parked[i]));
        peak = juce::jmax (peak, std::abs (parked[i]));
    }

    CHECK_MSG (peak > 1.0e-3, "no note");
    CHECK_MSG (diff <= peak * 1.0e-3, "live and parked swaps differ by " + juce::String (juce::Decibels::gainToDecibels (diff / peak), 1) + " dB");
}
