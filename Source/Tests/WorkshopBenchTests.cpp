/*  The Workshop bench's model: workshop-ui.md 4, 7, 8 and 11. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/Guitar/GuitarRenderer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Bench
    {
        LuthierAudioProcessor processor;

        Bench()
        {
            processor.prepareToPlay (48000.0, 512);
            loadType (GuitarType::LesPaul);
        }

        void loadType (GuitarType type)
        {
            auto* p = processor.getState().getParameter (ParamIDs::guitarType);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
            processor.getParameterBridge().applyAllNow();
        }

        WorkshopBench& bench() { return processor.getBench(); }
        const WorkshopGuitar& guitar() { return processor.getCurrentGuitar(); }

        PartPtr otherPart (PartType type, const PartPtr& notThis)
        {
            for (const auto& p : processor.getPartLibrary().getParts (type))
                if (notThis == nullptr || p->name != notThis->name)
                    return p;
            return nullptr;
        }

        /** A plucked chord's RMS through the whole plugin, for "the audio changed". */
        std::vector<float> render()
        {
            processor.getEngine().reset();
            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<float> out;

            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    for (int note : { 40, 47, 52 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
            }

            return out;
        }
    };

    double difference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0.0;
        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            d += std::abs ((double) a[i] - (double) b[i]);
        return d;
    }
}

//==============================================================================
LUTHIER_TEST (WorkshopBench, aDragIsOneUndoEntryWithItsBeforeAndAfter)
{
    Bench b;
    const double start = b.guitar().placements[0].positionMm;
    const int stepsBefore = b.processor.getNumUndoSteps();

    b.bench().beginGesture();
    for (double x = start; x >= start - 8.0; x -= 1.0)
        b.bench().movePickup (0, x);
    b.bench().endGesture();

    CHECK_MSG (b.processor.getNumUndoSteps() == stepsBefore + 1,
               juce::String (b.processor.getNumUndoSteps() - stepsBefore) + " undo entries for one drag");

    const auto expected = "Moved neck pickup " + juce::String (juce::roundToInt (start)) + " " + juce::String::fromUTF8 ("\xe2\x86\x92")
                        + " " + juce::String (juce::roundToInt (start - 8.0)) + " mm";
    CHECK_MSG (b.processor.getUndoDescription() == expected,
               "undo reads \"" + b.processor.getUndoDescription() + "\", want \"" + expected + "\"");
    CHECK (std::abs (b.guitar().placements[0].positionMm - (start - 8.0)) < 1.0e-6);

    // And undo puts it back.
    b.processor.undo();
    CHECK (std::abs (b.guitar().placements[0].positionMm - start) < 1.0e-6);
}

LUTHIER_TEST (WorkshopBench, aClickWithoutAMoveChangesNothing)
{
    Bench b;
    const int stepsBefore = b.processor.getNumUndoSteps();
    b.bench().beginGesture();
    b.bench().endGesture();
    CHECK (b.processor.getNumUndoSteps() == stepsBefore);
}

LUTHIER_TEST (WorkshopBench, aPickupStopsBeforeItOverlapsAndSaysWhy)
{
    // The Les Paul has neck and bridge humbuckers and no middle one.
    Bench b;
    juce::String why;

    const double landed = b.bench().movePickup (0, 10.0, &why);
    const double bridgeAt = b.guitar().placements[2].positionMm;

    CHECK_MSG (landed >= bridgeAt + 39.0, "the neck pickup ended at " + juce::String (landed, 1)
                                             + " mm, over the bridge pickup at " + juce::String (bridgeAt, 1));
    CHECK_MSG (why.contains ("bridge pickup"), "stopped because \"" + why + "\"");

    // And the bridge pickup stops at the bridge.
    b.bench().movePickup (2, 0.0, &why);
    CHECK_MSG (why.contains ("bridge"), "stopped because \"" + why + "\"");
    CHECK (b.guitar().placements[2].positionMm > 20.0);

    // Toward the neck, the neck pickup stops where the fretboard ends.
    b.bench().movePickup (0, 400.0, &why);
    CHECK_MSG (why.contains ("fretboard"), "stopped because \"" + why + "\"");
}

LUTHIER_TEST (WorkshopBench, snapIsOneMillimetreFineWithShiftFreeWithAlt)
{
    CHECK_NEAR (WorkshopBench::snap (7.4, false, false), 7.0, 1.0e-9);
    CHECK_NEAR (WorkshopBench::snap (7.4, true, false), 7.4, 1.0e-9);
    CHECK_NEAR (WorkshopBench::snap (7.43, false, true), 7.43, 1.0e-9);
    CHECK_NEAR (WorkshopBench::snap (2.46, false, false, 0.1), 2.5, 1.0e-9);
}

LUTHIER_TEST (WorkshopBench, fittingAPartSaysWhatItReplaced)
{
    Bench b;
    const auto old = b.guitar().get (GuitarSlot::bridge);
    const auto next = b.otherPart (PartType::bridge, old);

    CHECK (b.bench().fit (GuitarSlot::bridge, next));
    CHECK_MSG (b.processor.getUndoDescription() == "Fitted " + next->name + " (was " + old->name + ")",
               "undo reads \"" + b.processor.getUndoDescription() + "\"");
    CHECK (b.guitar().get (GuitarSlot::bridge)->name == next->name);

    // Fitting the same part again changes nothing and pushes nothing.
    const int steps = b.processor.getNumUndoSteps();
    CHECK (! b.bench().fit (GuitarSlot::bridge, next));
    CHECK (b.processor.getNumUndoSteps() == steps);

    // A required slot cannot be emptied; an optional one can.
    CHECK (! b.bench().fit (GuitarSlot::bridge, nullptr));
    CHECK (b.bench().fit (GuitarSlot::pickupNeck, nullptr));
    CHECK (b.processor.getUndoDescription().startsWith ("Removed neck pickup"));
}

LUTHIER_TEST (WorkshopBench, auditionNeverCommits)
{
    Bench b;
    const auto committed = b.guitar();
    const int steps = b.processor.getNumUndoSteps();
    const auto candidate = b.otherPart (PartType::bridge, committed.get (GuitarSlot::bridge));

    b.bench().beginAudition (GuitarSlot::bridge, candidate);
    CHECK (b.bench().isAuditioning());
    CHECK (b.bench().getAuditionGuitar()->get (GuitarSlot::bridge)->name == candidate->name);
    CHECK (b.guitar() == committed);
    CHECK (b.processor.getNumUndoSteps() == steps);
    CHECK (! b.processor.isGuitarEdited());

    b.bench().endAudition();
    CHECK (! b.bench().isAuditioning());
    CHECK (b.guitar() == committed);
    CHECK (b.processor.getNumUndoSteps() == steps);
}

LUTHIER_TEST (WorkshopBench, abRecallRoundTrips)
{
    Bench b;
    b.bench().storeSlot (1);
    const auto stored = b.guitar();
    CHECK (b.bench().hasSlot (1));

    // Change six parts.
    for (auto slot : { GuitarSlot::bridge, GuitarSlot::pickupBridge, GuitarSlot::strings,
                       GuitarSlot::nut, GuitarSlot::tuners, GuitarSlot::wiring })
    {
        const auto type = getSlotPartType (slot);
        b.bench().fit (slot, b.otherPart (type, b.guitar().get (slot)));
    }

    CHECK (! (b.guitar() == stored));

    CHECK (b.bench().recallSlot (1));
    CHECK_MSG (b.guitar() == stored, "the recalled guitar is not the stored one");
    CHECK (b.processor.getUndoDescription() == "Recalled bench slot B");

    b.bench().clearSlot (1);
    CHECK (! b.bench().hasSlot (1));
    CHECK (! b.bench().recallSlot (1));
}

LUTHIER_TEST (WorkshopBench, aMovedPickupIsSeenReadAndHeard)
{
    // Ground rule 3: visual (the drawing changes), numeric (the value), audible.
    Bench b;
    const auto keyBefore = GuitarRenderer::keyFor (b.guitar(), {});
    const auto soundBefore = b.render();
    const double from = b.guitar().placements[0].positionMm;

    b.bench().movePickup (0, from - 30.0);

    // The neck pickup alone, so the move is what we hear.
    CHECK (GuitarRenderer::keyFor (b.guitar(), {}) != keyBefore);
    CHECK (std::abs (b.bench().current().placements[0].positionMm - (from - 30.0)) < 1.0e-6);

    auto* selector = b.processor.getState().getParameter (ParamIDs::pickupSelector);
    selector->setValueNotifyingHost (selector->convertTo0to1 ((float) (int) PickupSelector::Neck));
    b.processor.getParameterBridge().applyAllNow();
    const auto soundAfter = b.render();

    b.bench().movePickup (0, from);
    b.processor.getParameterBridge().applyAllNow();
    const auto soundBack = b.render();

    CHECK_MSG (difference (soundAfter, soundBack) > 1.0e-3, "moving the neck pickup 30 mm changed nothing audible");
    juce::ignoreUnused (soundBefore);
}

LUTHIER_TEST (WorkshopBench, heightsAndSetupEditsAreOneEntryEach)
{
    Bench b;
    const auto& p = b.guitar().placements[2];
    const double treble = p.heightTrebleMm;

    b.bench().setPickupHeights (2, treble - 0.3, p.heightBassMm);
    CHECK (b.processor.getUndoDescription().startsWith ("Raised bridge pickup treble side"));

    b.bench().setPickupHeights (2, 0.1, 99.0);
    CHECK_NEAR (b.guitar().placements[2].heightTrebleMm, WorkshopBench::kMinPickupHeight, 1.0e-9);
    CHECK_NEAR (b.guitar().placements[2].heightBassMm, WorkshopBench::kMaxPickupHeight, 1.0e-9);

    b.bench().setIntonation (2, 0.5);
    CHECK (b.processor.getUndoDescription().startsWith ("Moved string 3 saddle"));

    b.bench().setNutSlotDepth (1, 0.35);
    CHECK (b.processor.getUndoDescription().startsWith ("Set string 2 nut slot"));
    CHECK_NEAR (b.guitar().setup.nutSlotDepthsMm[1], 0.35, 1.0e-9);
}

//==============================================================================
#include "../Workshop/SpectrumDelta.h"

LUTHIER_TEST (WorkshopSpectrum, aNullChangeIsFlat)
{
    // workshop-ui.md 11: auditioning a part against itself is within +-0.05 dB everywhere.
    Bench b;
    const auto r = SpectrumDelta::compute (b.guitar(), b.guitar(), GuitarType::LesPaul);

    CHECK ((int) r.deltaDb.size() == SpectrumDelta::kNumPoints);
    CHECK_MSG (r.largestDb <= 0.05f, "a null change drew " + juce::String (r.largestDb, 3) + " dB");
    CHECK (r.noChange);
    CHECK (r.summary.startsWith ("No audible change"));
}

LUTHIER_TEST (WorkshopSpectrum, aRealChangeShowsAndIsDescribed)
{
    Bench b;
    const auto committed = b.guitar();
    auto candidate = committed;

    // A single-coil in place of the bridge humbucker (the default selector plays the bridge).
    for (const auto& p : b.processor.getPartLibrary().getParts (PartType::pickup))
        if (p->name.startsWith ("T-Style Bridge"))
            candidate.parts[(size_t) GuitarSlot::pickupBridge] = p;

    const auto r = SpectrumDelta::compute (committed, candidate, GuitarType::LesPaul);
    CHECK_MSG (! r.noChange, "a single-coil for a humbucker drew no change (" + juce::String (r.largestDb, 2) + " dB)");
    CHECK (r.summary.startsWith ("Candidate"));
}

LUTHIER_TEST (WorkshopSpectrum, combNotchesSitWhereThePickupIsANode)
{
    // A pickup a quarter of the way along cancels the 4th, 8th, 12th ... harmonics.
    const auto notches = SpectrumDelta::combNotches (162.0, 648.0, 100.0, 1300.0);
    CHECK ((int) notches.size() == 3);
    CHECK_NEAR (notches[0], 400.0, 0.01);
    CHECK_NEAR (notches[2], 1200.0, 0.01);
}

LUTHIER_TEST (WorkshopSpectrum, theWorkerCoalescesAndStaysInBudget)
{
    // Section 6: latest request wins; the render budget is 40 ms per delta.
    Bench b;
    SpectrumDelta delta;
    const auto committed = b.guitar();
    juce::uint32 last = 0;

    for (const auto& p : b.processor.getPartLibrary().getParts (PartType::bridge))
        last = delta.request (committed, b.bench().withPart (GuitarSlot::bridge, p), GuitarType::LesPaul);

    // Wait for the last one; anything older that never started was dropped.
    SpectrumDelta::Result r;
    double best = 1.0e9;
    const auto deadline = juce::Time::getMillisecondCounter() + 20000;

    while (juce::Time::getMillisecondCounter() < deadline)
    {
        if (delta.takeResult (r))
        {
            best = juce::jmin (best, r.renderMs);
            if (r.requestId == last)
                break;
        }

        juce::Thread::sleep (5);
    }

    CHECK_MSG (r.requestId == last, "never got the latest request's result");

    // Timed again warm, as a drag would see it.
    const auto warm = [&]
    {
        delta.request (committed, b.bench().withPart (GuitarSlot::bridge, committed.get (GuitarSlot::bridge)), GuitarType::LesPaul);
        SpectrumDelta::Result w;
        const auto until = juce::Time::getMillisecondCounter() + 10000;
        while (! delta.takeResult (w) && juce::Time::getMillisecondCounter() < until)
            juce::Thread::sleep (2);
        return w.renderMs;
    };

    for (int i = 0; i < 3; ++i)
        best = juce::jmin (best, warm());

    CHECK_MSG (best < 40.0, "a spectrum delta took " + juce::String (best, 1) + " ms at best (budget 40 ms)");
}

//==============================================================================
LUTHIER_TEST (WorkshopBench, aStringOverrideIsOneEntryInRealUnitsAndReachesTheEngine)
{
    // workshop-ui.md 8: "Set string 3 to 0.018 plain (was 0.017 plain)"; the
    // engine's gauge for that string follows (guitar-workshop.md 3.3).
    Bench b;
    const int steps = b.processor.getNumUndoSteps();
    const auto was = WorkshopBench::describeString (b.guitar(), 2);
    const auto before = mapSpec (b.guitar());

    StringOverride heavier;
    heavier.stringIndex = 2;
    heavier.gaugeIn = 0.018;
    heavier.wound = 0;
    CHECK (b.bench().setStringOverride (heavier));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription() == "Set string 3 to 0.018 plain (was " + was + ")",
               "undo reads \"" + b.processor.getUndoDescription() + "\"");
    CHECK (b.processor.isGuitarEdited());

    const auto after = mapSpec (b.guitar());
    CHECK_NEAR (after.gaugesIn[2], 0.018, 1.0e-12);
    CHECK (after.tensionNewtons[2] > before.tensionNewtons[2]);   // a heavier string at the same pitch pulls harder
    CHECK (after.stringMaterials[2] == before.stringMaterials[2]);

    // The same override again changes nothing and pushes nothing.
    CHECK (! b.bench().setStringOverride (heavier));
    CHECK (b.processor.getNumUndoSteps() == steps + 1);

    // A material of its own reaches the derived acoustics per string.
    StringOverride bronze = heavier;
    bronze.material = "phosphor_bronze";
    CHECK (b.bench().setStringOverride (bronze));
    CHECK (mapSpec (b.guitar()).stringMaterials[2] == StringMaterial::PhosphorBronze);
    CHECK (mapSpec (b.guitar()).stringMaterials[1] == before.stringMaterials[1]);
    CHECK (b.processor.getUndoDescription().contains ("phosphor bronze"));

    // Clearing says so, and undo walks it all back.
    CHECK (b.bench().clearStringOverride (2));
    CHECK (b.processor.getUndoDescription().startsWith ("Cleared string 3 override"));
    CHECK (b.guitar().getStringOverride (2) == nullptr);

    b.processor.undo();
    CHECK (b.guitar().getStringOverride (2) != nullptr && b.guitar().getStringOverride (2)->material == "phosphor_bronze");
    b.processor.undo();
    b.processor.undo();
    CHECK (b.guitar().getStringOverride (2) == nullptr);
    CHECK_NEAR (mapSpec (b.guitar()).gaugesIn[2], before.gaugesIn[2], 1.0e-12);
}

LUTHIER_TEST (WorkshopBench, thePickSlideAndCapoMoveAsOneEntryEach)
{
    // workshop-ui.md 4's last three rows: the accessories drag like parts, one
    // undo entry with the before and after in real units, through the
    // parameters they already have (capo_fret, pick_angle, pluck_position, slide_slant).
    Bench b;
    auto plain = [&] (const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (b.processor.getState().getParameter (id));
        return (double) p->convertFrom0to1 (p->getValue());
    };

    int steps = b.processor.getNumUndoSteps();
    CHECK (b.bench().getCapoFret() == 0);

    b.bench().setCapoFret (3);
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription() == "Put the capo on fret 3");
    CHECK_NEAR (plain (ParamIDs::capoFret), 3.0, 1.0e-6);

    b.bench().setCapoFret (5);
    CHECK (b.processor.getUndoDescription() == "Moved capo fret 3 " + juce::String::fromUTF8 ("\xe2\x86\x92") + " 5");

    b.bench().setCapoFret (99);
    CHECK (b.bench().getCapoFret() == WorkshopBench::kMaxCapoFret);

    b.bench().setCapoFret (0);
    CHECK (b.processor.getUndoDescription().startsWith ("Took the capo off (was fret 12)"));

    // Undo puts the capo back where it was.
    b.processor.undo();
    CHECK (b.bench().getCapoFret() == 12);
    CHECK_NEAR (plain (ParamIDs::capoFret), 12.0, 1.0e-6);

    // A drag: several moves, one entry.
    steps = b.processor.getNumUndoSteps();
    b.bench().beginGesture();
    for (int f = 11; f >= 2; --f)
        b.bench().setCapoFret (f);
    b.bench().endGesture();
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription() == "Moved capo fret 12 " + juce::String::fromUTF8 ("\xe2\x86\x92") + " 2");

    // The pick: position in mm from the saddle, angle in degrees.
    const double scale = b.bench().getScaleLengthMm();
    b.bench().setPickPlacement (60.0, 20.0);
    CHECK_NEAR (plain (ParamIDs::pluckPosition) * scale, 60.0, 0.5);
    CHECK_NEAR (Parameters::pickAngleDegrees (plain (ParamIDs::pickAngle)), 20.0, 0.1);
    CHECK_NEAR (b.bench().getPickPositionMm(), 60.0, 0.5);
    CHECK_MSG (b.processor.getUndoDescription().startsWith ("Moved pick") && b.processor.getUndoDescription().contains ("Angled pick"),
               "undo reads \"" + b.processor.getUndoDescription() + "\"");

    // Out of the parameter's reach it stops at the edge.
    b.bench().setPickPlacement (1000.0, 500.0);
    CHECK_NEAR (plain (ParamIDs::pluckPosition), 0.5, 1.0e-6);
    CHECK_NEAR (b.bench().getPickAngleDegrees(), WorkshopBench::kMaxPickAngle, 1.0e-6);

    // The slide: slant is the parameter, position is the bench's.
    b.bench().setSlidePlacement (7.0, 10.0);
    CHECK_NEAR (plain (ParamIDs::slideSlant), 10.0, 1.0e-6);
    CHECK_NEAR (b.bench().getSlideFret(), 7.0, 1.0e-9);
    CHECK (b.processor.getUndoDescription().contains ("Moved slide to fret 7.0"));
    CHECK (b.processor.getUndoDescription().contains ("Slanted slide"));

    b.bench().setSlidePlacement (7.0, 90.0);
    CHECK_NEAR (b.bench().getSlideSlantDegrees(), WorkshopBench::kMaxSlideSlant, 1.0e-6);

    // A click that moves nothing pushes nothing.
    steps = b.processor.getNumUndoSteps();
    b.bench().beginGesture();
    b.bench().setCapoFret (b.bench().getCapoFret());
    b.bench().endGesture();
    CHECK (b.processor.getNumUndoSteps() == steps);

    // The slide part maps onto the engine's bar description.
    for (const auto& p : b.processor.getPartLibrary().getParts (PartType::slide))
        if (p->name.startsWith ("Brass"))
        {
            auto* on = b.processor.getState().getParameter (ParamIDs::slideGuitar);
            on->setValueNotifyingHost (1.0f);
            CHECK (b.bench().fitAccessory (p));
            CHECK (b.bench().getSlideBar().material == SlideMaterial::brass);
            CHECK_NEAR (b.bench().getSlideBar().massGrams, 150.0, 1.0e-9);
        }
}
