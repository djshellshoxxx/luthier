/*  action-and-undo.md: the processor's undo stack as a whole.

    The stack existed before these tests and was exercised piecemeal (a lock
    is undoable, a bench drag is one entry). What nothing checked was the
    shape the spec asks for: that an undo entry is the sound and not the tune
    (0.5 / 3.9 - undoing a knob used to reload the tune and wipe its own
    history), that a preset or family load is a boundary a plain undo stops
    at (5), that a recall or a route edit can be undone at all (3.6 / 3.7),
    that two quick gestures on one knob are one entry (3.1 / 4), and the
    section 13 limits: 250 pushes keep 200, a thousand mod-driven writes push
    nothing, a gesture from the wrong thread pushes nothing.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../Tune/TuneModel.h"

#include <thread>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::RangedAudioParameter* rangedOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        return processor.getState().getParameter (id);
    }

    float plainOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (rangedOf (processor, id));
        return parameter != nullptr ? parameter->get() : -1.0f;
    }

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* parameter = rangedOf (processor, id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    /** A user's drag, as JUCE reports it: begin, write, end. */
    void gesture (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* parameter = rangedOf (processor, id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            parameter->endChangeGesture();
        }
    }

    double tuneTempo (LuthierAudioProcessor& processor)
    {
        return processor.getTuneSession().getTune().meta.tempoBpm;
    }

    bool setTuneTempo (LuthierAudioProcessor& processor, double bpm)
    {
        return processor.getTuneSession().edit (TuneEditClass::other, "Change tempo",
                                                [bpm] (Tune& t) { return t.setTempo (bpm); });
    }
}

//==============================================================================
/*  0.5 / 3.9, the ship blocker: a knob undo is not a tune load. Before the
    fix every undo entry carried the whole state block, tune included, and
    restoring one went through TuneSession::restoreState, which is a boundary
    that clears the tune's own history. Undoing one knob move reverted every
    tune edit and left the Tune Builder with nothing to undo. */
LUTHIER_TEST (Undo, undoingAKnobLeavesTheTuneAndItsHistoryAlone)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    const float original = plainOf (processor, gain);

    gesture (processor, gain, 0.25f);
    CHECK (processor.getNumUndoSteps() == 1);

    CHECK (setTuneTempo (processor, 97.0));
    CHECK (processor.getTuneSession().canUndo());
    CHECK (processor.getTuneSession().getNumUndoSteps() == 1);

    processor.undo();

    CHECK_MSG (std::abs (plainOf (processor, gain) - original) < 1.0e-3f, "the knob did not go back");
    CHECK_MSG (std::abs (tuneTempo (processor) - 97.0) < 1.0e-6,
               "undoing a knob reverted the tune edit: tempo is " + juce::String (tuneTempo (processor)));
    CHECK_MSG (processor.getTuneSession().canUndo(),
               "undoing a knob wiped the tune's own undo history");
    CHECK (processor.getTuneSession().getNumUndoSteps() == 1);

    processor.redo();
    CHECK (std::abs (plainOf (processor, gain) - 0.25f) < 1.0e-3f);
    CHECK (std::abs (tuneTempo (processor) - 97.0) < 1.0e-6);
    CHECK (processor.getTuneSession().canUndo());

    // The tune's own undo still works, and stays its own.
    CHECK (processor.getTuneSession().undo());
    CHECK (std::abs (tuneTempo (processor) - 120.0) < 1.0e-6);
    CHECK (std::abs (plainOf (processor, gain) - 0.25f) < 1.0e-3f);
}

/*  The same for A/B: flipping the slots compares two sounds (section 7 calls
    it a viewport) and must not reload the tune or clear its history. */
LUTHIER_TEST (Undo, abCompareLeavesTheTuneAndItsHistoryAlone)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);

    // B holds 0.8; A takes what is on screen (0.2) as the switch to B happens.
    setPlain (processor, gain, 0.8f);
    processor.storeToSlot (true);
    setPlain (processor, gain, 0.2f);

    CHECK (setTuneTempo (processor, 77.0));
    CHECK (processor.getTuneSession().canUndo());

    processor.setSlotBActive (true);
    CHECK (std::abs (plainOf (processor, gain) - 0.8f) < 1.0e-3f);
    CHECK_MSG (std::abs (tuneTempo (processor) - 77.0) < 1.0e-6, "switching to B reloaded the tune");
    CHECK_MSG (processor.getTuneSession().canUndo(), "switching to B wiped the tune's history");

    processor.setSlotBActive (false);
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f,
               "back on A the gain is " + juce::String (plainOf (processor, gain)));
    CHECK (std::abs (tuneTempo (processor) - 77.0) < 1.0e-6);
    CHECK (processor.getTuneSession().canUndo());
}

//==============================================================================
/*  Section 5: a boundary can itself be undone (it reverses the load), but a
    plain undo will not step from there into the entries older than it.
    undo (true) - Ctrl+Alt+Z - does. */
LUTHIER_TEST (Undo, aPlainUndoStopsAtABoundaryAndACrossingUndoDoesNot)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    const float original = plainOf (processor, gain);

    processor.pushUndoState ("before the load");
    setPlain (processor, gain, 0.2f);

    processor.pushUndoBoundary ("Load preset Test");
    setPlain (processor, gain, 0.5f);

    processor.pushUndoState ("after the load");
    setPlain (processor, gain, 0.9f);

    CHECK (! processor.isUndoBlockedByBoundary());

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 0.5f) < 1.0e-3f);
    CHECK (! processor.isUndoBlockedByBoundary());
    CHECK (processor.getUndoDescription() == "Load preset Test");

    // The boundary itself reverses the load.
    processor.undo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f, "the boundary entry could not be undone");
    CHECK_MSG (processor.isUndoBlockedByBoundary(), "the undo is not blocked below the boundary");
    CHECK (processor.getUndoBoundaryDescription() == "Load preset Test");
    CHECK (processor.canUndo());

    // A plain undo refuses to cross.
    processor.undo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f, "a plain undo crossed the boundary");
    CHECK (processor.getNumUndoSteps() == 1);

    // Ctrl+Alt+Z crosses.
    processor.undo (true);
    CHECK_MSG (std::abs (plainOf (processor, gain) - original) < 1.0e-3f, "the crossing undo did not step");
    CHECK (! processor.canUndo());

    // Redo walks back over the boundary without ceremony.
    processor.redo();
    processor.redo();
    processor.redo();
    CHECK (std::abs (plainOf (processor, gain) - 0.9f) < 1.0e-3f);
    CHECK (! processor.canRedo());
}

/*  3.4 / 5 / 8: a family switch is a boundary with a warning about the parts
    it loses. */
LUTHIER_TEST (Undo, aFamilySwitchIsABoundaryThatWarnsWhenUndone)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    processor.pushUndoState ("before");
    const auto familyBefore = processor.getCurrentGuitar().family;

    CHECK (processor.switchGuitarFamily ("bass"));
    processor.getParameterBridge().applyAllNow();
    CHECK (processor.getCurrentGuitar().family == "bass");

    CHECK_MSG (processor.getUndoDescription().startsWith ("Change guitar family to bass"),
               "got " + processor.getUndoDescription());
    CHECK_MSG (processor.getUndoWarning().isNotEmpty(), "the family switch carries no warning");
    CHECK (! processor.isUndoBlockedByBoundary());

    processor.undo();
    processor.getParameterBridge().applyAllNow();
    CHECK (processor.getCurrentGuitar().family == familyBefore);
    CHECK_MSG (processor.isUndoBlockedByBoundary(), "the family switch is not a boundary");
    CHECK (processor.getUndoWarning().isEmpty());
}

/*  Section 9 through the window: Ctrl+Z stops at a boundary and says so;
    Ctrl+Alt+Z crosses it and posts the section-15 banner; Ctrl+Y redoes. */
LUTHIER_TEST (Undo, theWindowBindsCrossingUndoAndTheAlternateRedo)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

    CHECK_MSG (window != nullptr, "createEditor returned nullptr - is this target headless?");

    if (window == nullptr)
        return;

    auto& shortcuts = AccessibilitySettings::get();
    shortcuts.resetAllShortcuts();

    const auto* crossing = shortcuts.findShortcut ("undoAcrossBoundary");
    const auto* redoAlt = shortcuts.findShortcut ("redoAlt");
    const auto* undo = shortcuts.findShortcut ("undo");

    CHECK_MSG (crossing != nullptr, "no undoAcrossBoundary action in the registry");
    CHECK_MSG (redoAlt != nullptr, "no redoAlt action in the registry");

    if (crossing == nullptr || redoAlt == nullptr || undo == nullptr)
        return;

    CHECK (crossing->key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0));
    CHECK (redoAlt->key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0));

    const juce::String gain (ParamIDs::ampGain);

    processor.pushUndoState ("before");
    setPlain (processor, gain, 0.2f);
    processor.pushUndoBoundary ("Load preset Boundary");
    setPlain (processor, gain, 0.7f);

    auto& centre = window->getNotifications();
    centre.clear();

    // Undo the boundary itself: fine, no banner.
    CHECK (window->keyPressed (undo->key));
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);
    CHECK (processor.isUndoBlockedByBoundary());

    // A plain undo now stops, and says why.
    CHECK (window->keyPressed (undo->key));
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);
    CHECK_MSG (centre.contains ("undo-boundary"), "the stop at the boundary posted no banner");
    CHECK (centre.getCurrentMessage().contains ("Load preset Boundary"));
    centre.clear();

    // Ctrl+Alt+Z crosses with the section-15 banner.
    CHECK (window->keyPressed (crossing->key));
    CHECK_MSG (! processor.canUndo(), "Ctrl+Alt+Z did not cross the boundary");
    CHECK_MSG (centre.contains ("undo-boundary"), "the crossing undo posted no banner");
    CHECK (centre.getCurrentMessage().contains ("Load preset Boundary"));

    // Ctrl+Y is the alternate redo.
    CHECK (window->keyPressed (redoAlt->key));
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);
    CHECK (window->keyPressed (redoAlt->key));
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.7f) < 1.0e-3f, "Ctrl+Y did not redo");
}

//==============================================================================
/*  3.7: a recall rewrites every parameter and used to be the one thing that
    could not be undone. */
LUTHIER_TEST (Undo, recallingASnapshotIsOneEntryThatUndoesToThePreRecallState)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);

    // Applied outright: the crossfade is the audio thread's business.
    processor.getSnapshots().setCrossfadeMs (0.0);

    setPlain (processor, gain, 0.2f);
    CHECK (processor.captureSnapshot (0, "Verse"));
    CHECK (processor.getUndoDescription() == "Save snapshot 1 Verse");

    setPlain (processor, gain, 0.9f);

    const int before = processor.getNumUndoSteps();

    CHECK (processor.recallSnapshot (0));
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);
    CHECK_MSG (processor.getNumUndoSteps() == before + 1,
               "a recall pushed " + juce::String (processor.getNumUndoSteps() - before) + " entries");
    CHECK_MSG (processor.getUndoDescription() == "Recall snapshot 1 Verse", "got " + processor.getUndoDescription());

    processor.undo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.9f) < 1.0e-3f, "undo did not reverse the recall");

    processor.redo();
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);

    // An empty slot recalls nothing and pushes nothing.
    const int steps = processor.getNumUndoSteps();
    CHECK (! processor.recallSnapshot (5));
    CHECK (processor.getNumUndoSteps() == steps);
}

/*  3.6: creating and deleting a route are entries with the section's
    wording, and undoing them puts the matrix back. */
LUTHIER_TEST (Undo, modRouteCreateAndDeleteAreEntriesThatReverse)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& matrix = processor.getModMatrix();
    const int routesBefore = matrix.getNumRoutes();

    ModRoute route;
    route.sourceId = modSourceIdForSlot (ModSourceSlots::lfoBase);
    route.destinationId = ParamIDs::ampGain;
    route.depth = 0.33f;
    route.enabled = true;

    CHECK (processor.addModRoute (route));
    CHECK (matrix.getNumRoutes() == routesBefore + 1);

    const auto expectedAdd = "Add " + modSourceDisplayName (ModSourceSlots::lfoBase) + " to "
                               + rangedOf (processor, ParamIDs::ampGain)->getName (40) + " depth 0.33";
    CHECK_MSG (processor.getUndoDescription() == expectedAdd,
               "got \"" + processor.getUndoDescription() + "\", expected \"" + expectedAdd + "\"");

    processor.undo();
    CHECK_MSG (matrix.getNumRoutes() == routesBefore, "undoing the create left the route in place");

    processor.redo();
    CHECK (matrix.getNumRoutes() == routesBefore + 1);

    processor.removeModRoute (matrix.getNumRoutes() - 1);
    CHECK (matrix.getNumRoutes() == routesBefore);

    const auto expectedRemove = "Remove " + modSourceDisplayName (ModSourceSlots::lfoBase) + " from "
                                  + rangedOf (processor, ParamIDs::ampGain)->getName (40);
    CHECK_MSG (processor.getUndoDescription() == expectedRemove, "got \"" + processor.getUndoDescription() + "\"");

    processor.undo();
    CHECK_MSG (matrix.getNumRoutes() == routesBefore + 1, "undoing the delete did not restore the route");
    CHECK (matrix.getRoute (matrix.getNumRoutes() - 1).destinationId == juce::String (ParamIDs::ampGain));

    // An index that names nothing pushes nothing.
    const int steps = processor.getNumUndoSteps();
    processor.removeModRoute (999);
    CHECK (processor.getNumUndoSteps() == steps);
}

//==============================================================================
/*  3.1 / 3.2 / 4: "Change X from A to B", and a second gesture on the same
    parameter inside 200 ms joins the first. 199 merges, 201 does not. */
LUTHIER_TEST (Undo, gesturesOnOneParameterMergeAt199msAndSplitAt201ms)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    double now = 1000.0;
    processor.setUndoClock ([&now] { return now; });

    const juce::String gain (ParamIDs::ampGain);
    auto* parameter = rangedOf (processor, gain);
    CHECK (parameter != nullptr);

    if (parameter == nullptr)
        return;

    const auto name = parameter->getName (64);
    const float original = plainOf (processor, gain);
    const auto originalText = parameter->getCurrentValueAsText();

    gesture (processor, gain, 0.2f);
    CHECK (processor.getNumUndoSteps() == 1);
    CHECK_MSG (processor.getUndoDescription() == "Change " + name + " from " + originalText + " to "
                                                   + parameter->getCurrentValueAsText(),
               "got \"" + processor.getUndoDescription() + "\"");

    now += 199.0;
    gesture (processor, gain, 0.4f);
    CHECK_MSG (processor.getNumUndoSteps() == 1, "a gesture 199 ms later did not merge");
    CHECK_MSG (processor.getUndoDescription() == "Change " + name + " from " + originalText + " to "
                                                   + parameter->getCurrentValueAsText(),
               "the merged entry does not carry the first before and the last after: \""
                 + processor.getUndoDescription() + "\"");

    now += 201.0;
    gesture (processor, gain, 0.6f);
    CHECK_MSG (processor.getNumUndoSteps() == 2, "a gesture 201 ms later merged");

    // Undo reverses the second entry to 0.4, then the merged one to the start.
    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 0.4f) < 1.0e-3f);
    processor.undo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - original) < 1.0e-3f,
               "the merged entry did not undo to the value before the first gesture");

    // Section 4: something else in between stops the merge.
    processor.redo();
    processor.redo();
    now += 300.0;
    gesture (processor, gain, 0.7f);
    now += 1.0;
    gesture (processor, ParamIDs::ampMaster, 0.5f);
    now += 1.0;
    gesture (processor, gain, 0.8f);
    CHECK (processor.getNumUndoSteps() == 5);

    // A boundary never merges.
    now += 1.0;
    processor.pushUndoBoundary ("Load preset");
    now += 1.0;
    gesture (processor, gain, 0.9f);
    CHECK (processor.getNumUndoSteps() == 7);
}

//==============================================================================
/*  Section 2 / 13: 250 actions keep the newest 200, and undoing them all
    lands on the state before the 51st. */
LUTHIER_TEST (Undo, overflowDropsTheOldestAndStillReversesCleanly)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    auto* parameter = rangedOf (processor, gain);
    const auto range = parameter->getNormalisableRange();

    auto valueFor = [&range] (int i) { return range.convertFrom0to1 ((float) i / 250.0f); };

    for (int i = 1; i <= 250; ++i)
    {
        processor.pushUndoState ("push " + juce::String (i));
        setPlain (processor, gain, valueFor (i));
    }

    CHECK_MSG (processor.getNumUndoSteps() == LuthierAudioProcessor::getMaxUndoSteps(),
               "the stack holds " + juce::String (processor.getNumUndoSteps()));
    CHECK (processor.getUndoDescription() == "push 250");
    CHECK (! processor.canRedo());

    for (int i = 0; i < 200; ++i)
        processor.undo();

    CHECK (! processor.canUndo());
    CHECK (processor.getNumUndoSteps() == 0);
    CHECK_MSG (std::abs (plainOf (processor, gain) - valueFor (50)) < 1.0e-3f,
               "200 undos landed on " + juce::String (plainOf (processor, gain)) + ", not the value set by push 50");
    CHECK (processor.getRedoDescription() == "push 51");
    CHECK (processor.getNumRedoSteps() == 200);
}

/*  3.1 / 11 / 13: modulation is not user intent. A thousand blocks of an LFO
    writing to a parameter push nothing. */
LUTHIER_TEST (Undo, aThousandModDrivenChangesPushNothing)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 0.5f);

    ModRoute route;
    route.sourceId = modSourceIdForSlot (ModSourceSlots::lfoBase);
    route.destinationId = gain;
    route.depth = 1.0f;
    route.enabled = true;

    CHECK (processor.getModMatrix().addRoute (route));

    const int before = processor.getNumUndoSteps();
    const float start = plainOf (processor, gain);

    /*  The matrix modulates as an offset the bridge reads through
        ModMatrix::apply; the stored parameter value is untouched. That is the
        mechanism by which modulation can never be a gesture, and the value the
        engine sees is what is watched here. */
    const int index = [&]
    {
        const auto& parameters = processor.getParameters();

        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
                if (withId->paramID == gain)
                    return i;

        return -1;
    }();

    CHECK (index >= 0);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    bool moved = false;

    for (int i = 0; i < 1000; ++i)
    {
        buffer.clear();
        midi.clear();
        processor.processBlock (buffer, midi);

        if (index >= 0 && std::abs (processor.getModMatrix().apply (index, start) - start) > 1.0e-4f)
            moved = true;
    }

    CHECK_MSG (moved, "the LFO never moved the destination, so the test proves nothing");
    CHECK (std::abs (plainOf (processor, gain) - start) < 1.0e-6f);
    CHECK_MSG (processor.getNumUndoSteps() == before,
               "mod-driven changes pushed " + juce::String (processor.getNumUndoSteps() - before) + " entries");
}

/*  Section 11 / 0.4: a gesture from anywhere but the message thread is the
    host or a controller, not the user's hand on this UI. It pushes nothing. */
LUTHIER_TEST (Undo, aGestureFromAnotherThreadPushesNothing)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    const int before = processor.getNumUndoSteps();

    std::thread worker ([&processor, &gain] { gesture (processor, gain, 0.35f); });
    worker.join();

    CHECK (std::abs (plainOf (processor, gain) - 0.35f) < 1.0e-3f);
    CHECK_MSG (processor.getNumUndoSteps() == before, "a gesture off the message thread pushed an entry");

    // And the same gesture from here does.
    gesture (processor, gain, 0.45f);
    CHECK (processor.getNumUndoSteps() == before + 1);
}

/*  Section 2 / 13: any new action clears the redo tail, and a fresh instance
    starts empty (10). */
LUTHIER_TEST (Undo, aNewActionClearsRedoAndAFreshInstanceIsEmpty)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    CHECK (! processor.canUndo());
    CHECK (! processor.canRedo());
    CHECK (processor.getNumRedoSteps() == 0);

    processor.pushUndoState ("one");
    processor.pushUndoState ("two");
    processor.undo();
    CHECK (processor.canRedo());
    CHECK (processor.getNumRedoSteps() == 1);

    processor.pushUndoState ("three");
    CHECK (! processor.canRedo());
    CHECK (processor.getNumRedoSteps() == 0);

    // The stack is session state: it is not in the state block.
    juce::MemoryBlock block;
    processor.getStateInformation (block);

    LuthierAudioProcessor fresh;
    fresh.prepareToPlay (kSr, kBlock);
    fresh.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (! fresh.canUndo());
}
