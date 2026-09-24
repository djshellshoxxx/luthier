/*  action-and-undo.md: the processor's undo stack.

    What counts as one entry (0.1), the 200-entry cap (2), what never makes an
    entry (3.1, 7, 11), boundaries (5), grouping (4) and the action classes of
    section 3 that the processor owns. Each test goes through the real
    processor, because a snapshot-based stack is only as good as what the
    snapshot captures and what the restore leaves alone.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/HeaderBar.h"
#include "../UI/OptionsPages.h"
#include "../UI/PedalRack.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::RangedAudioParameter* paramOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        return processor.getState().getParameter (id);
    }

    float plainOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (paramOf (processor, id));
        return parameter != nullptr ? parameter->get() : -1.0f;
    }

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (paramOf (processor, id)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    /*  Two state blobs are the same state: byte-identical, or differing only
        in numbers by less than 1e-6. A guitar restore rewrites the parameters
        its parts overlap (applyGuitar), and a float going plain -> 0..1 ->
        plain there can move by one ulp (seen: realism_detune, 3e-8). That is
        rounding, not an undo that failed to reverse something. */
    bool sameValue (const juce::var& a, const juce::var& b, juce::String& where, const juce::String& path)
    {
        if (a.isDouble() || b.isDouble() || a.isInt() || b.isInt())
        {
            if ((a.isDouble() || a.isInt() || a.isInt64()) && (b.isDouble() || b.isInt() || b.isInt64())
                  && std::abs ((double) a - (double) b) < 1.0e-6)
                return true;
        }

        if (auto* oa = a.getDynamicObject())
        {
            auto* ob = b.getDynamicObject();

            if (ob == nullptr || oa->getProperties().size() != ob->getProperties().size())
            {
                where = path;
                return false;
            }

            for (const auto& p : oa->getProperties())
                if (! ob->hasProperty (p.name) || ! sameValue (p.value, ob->getProperty (p.name), where, path + "/" + p.name.toString()))
                {
                    if (where.isEmpty()) where = path + "/" + p.name.toString();
                    return false;
                }

            return true;
        }

        if (auto* aa = a.getArray())
        {
            auto* ab = b.getArray();

            if (ab == nullptr || aa->size() != ab->size())
            {
                where = path;
                return false;
            }

            for (int i = 0; i < aa->size(); ++i)
                if (! sameValue (aa->getReference (i), ab->getReference (i), where, path + "/" + juce::String (i)))
                    return false;

            return true;
        }

        if (a == b)
            return true;

        where = path;
        return false;
    }

    bool sameState (const juce::MemoryBlock& a, const juce::MemoryBlock& b, juce::String& where)
    {
        if (a == b)
            return true;

        return sameValue (juce::JSON::parse (a.toString()), juce::JSON::parse (b.toString()), where, {});
    }

    /** A user's drag: a gesture around `steps` writes ending on `endPlain`. */
    void dragTo (LuthierAudioProcessor& processor, const juce::String& id, float endPlain, int steps = 40)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (paramOf (processor, id));

        if (parameter == nullptr)
            return;

        const float start = parameter->get();

        parameter->beginChangeGesture();

        for (int i = 1; i <= steps; ++i)
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (
                start + (endPlain - start) * (float) i / (float) steps));

        parameter->endChangeGesture();
    }
}

//==============================================================================
/*  0.1: the Reset All shortcut used to push "Reset everything" and then call
    resetEverything, which pushed "Reset" too - two entries for one action. */
LUTHIER_TEST (Undo, resetEverythingIsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 0.8f);

    const int before = processor.getNumUndoSteps();
    processor.resetEverything();

    CHECK (processor.getNumUndoSteps() == before + 1);
    CHECK (processor.getUndoDescription() == "Reset everything");

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 0.8f) < 1.0e-3f);
}

//==============================================================================
/*  0.1: the Ranges page's per-row Clamp pushed its own entry and then wrote the
    value inside a gesture, which pushed "Change X" as well. */
LUTHIER_TEST (Undo, clampToStockIsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RangesPage page (processor);
    page.setSize (700, 500);
    page.setAllFamilies (true);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 1.5f);

    const int before = processor.getNumUndoSteps();
    page.clampOne (gain);

    CHECK (std::abs (plainOf (processor, gain) - 1.0f) < 1.0e-3f);
    CHECK_MSG (processor.getNumUndoSteps() == before + 1,
               "a clamp made " + juce::String (processor.getNumUndoSteps() - before) + " entries");

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 1.5f) < 1.0e-3f);
}

//==============================================================================
/*  2 and 13: 250 actions in a row keep the newest 200, and undoing all of them
    lands on the state before the 51st action, which is the 50th's result. */
LUTHIER_TEST (Undo, overflowDropsTheOldest)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    auto valueOf = [] (int i) { return 0.05f + 0.0035f * (float) i; };

    for (int i = 0; i < 250; ++i)
    {
        processor.pushUndoState ("action " + juce::String (i + 1));
        setPlain (processor, gain, valueOf (i));
    }

    CHECK_MSG (processor.getNumUndoSteps() == 200,
               "the stack holds " + juce::String (processor.getNumUndoSteps()));

    int undone = 0;

    while (processor.canUndo() && undone < 1000)
    {
        processor.undo();
        ++undone;
    }

    CHECK (undone == 200);
    CHECK_NEAR (plainOf (processor, gain), valueOf (49), 1.0e-3);
}

//==============================================================================
/*  0.1 / 3.1: a drag is one entry however many values it passes through, and a
    click that does not move the value makes none. */
LUTHIER_TEST (Undo, aGestureIsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 0.2f);

    const int before = processor.getNumUndoSteps();
    dragTo (processor, gain, 0.7f);

    CHECK_MSG (processor.getNumUndoSteps() == before + 1,
               "a 40-step drag made " + juce::String (processor.getNumUndoSteps() - before) + " entries");

    // 3.1: "Change [param] from X to Y".
    CHECK_MSG (processor.getUndoDescription() == "Change Gain from 0.20 to 0.70"
                 || processor.getUndoDescription().startsWith ("Change Gain from 0.2"),
               "description: " + processor.getUndoDescription());
    CHECK (processor.getUndoDescription().contains (" to 0.7"));

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);

    // A click that selects the knob without moving it.
    const int afterUndo = processor.getNumUndoSteps();
    auto* parameter = paramOf (processor, gain);
    parameter->beginChangeGesture();
    parameter->endChangeGesture();

    CHECK (processor.getNumUndoSteps() == afterUndo);
}

//==============================================================================
/*  3.1, 11 and 13: modulation, host automation and learned CCs write without a
    gesture, so 1000 of them make no entries. */
LUTHIER_TEST (Undo, writesWithoutAGestureMakeNoEntries)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const int before = processor.getNumUndoSteps();
    auto* parameter = paramOf (processor, ParamIDs::ampGain);

    for (int i = 0; i < 1000; ++i)
        parameter->setValueNotifyingHost ((float) (i % 100) / 100.0f);

    CHECK (processor.getNumUndoSteps() == before);
}

//==============================================================================
/*  3.3: a toggle reads "Turn on X" / "Turn off X". */
LUTHIER_TEST (Undo, aToggleSaysTurnOnOrOff)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::RangedAudioParameter* toggle = nullptr;

    for (auto* p : processor.getParameters())
        if (auto* b = dynamic_cast<juce::AudioParameterBool*> (p); b != nullptr && ! b->get())
        {
            toggle = b;
            break;
        }

    CHECK (toggle != nullptr);

    if (toggle == nullptr)
        return;

    toggle->beginChangeGesture();
    toggle->setValueNotifyingHost (1.0f);
    toggle->endChangeGesture();

    CHECK_MSG (processor.getUndoDescription() == "Turn on " + toggle->getName (64),
               "description: " + processor.getUndoDescription());
}

//==============================================================================
/*  3.17 and 7: undo reverses values, not the view. The tab, Live Mode, the A/B
    slot and the tune (which keeps its own history) stay where they are. */
LUTHIER_TEST (Undo, doesNotMoveTheViewOrTheTune)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    auto& ui = processor.getUiState();

    ui.advancedTab = 3;
    processor.pushUndoState ("an edit");
    setPlain (processor, gain, 0.9f);

    ui.advancedTab = 1;
    processor.setLiveMode (true);

    auto& tune = processor.getTuneSession();
    const double tempo = tune.getTune().meta.tempoBpm;
    CHECK (tune.edit (TuneEditClass::other, "Change tempo",
                      [tempo] (Tune& t) { return t.setTempo (tempo + 7.0); }));
    CHECK (tune.canUndo());

    processor.undo();

    CHECK (std::abs (plainOf (processor, gain) - 0.9f) > 1.0e-3f);
    CHECK_MSG (ui.advancedTab == 1, "undo moved the open tab");
    CHECK_MSG (processor.isLiveMode(), "undo left Live Mode");
    CHECK_MSG (std::abs (tune.getTune().meta.tempoBpm - (tempo + 7.0)) < 1.0e-6, "undo reverted the tune");
    CHECK_MSG (tune.canUndo(), "undo wiped the tune's own history");
}

//==============================================================================
/*  3.8 / 6 / 13: a preset load is one entry named after the preset, and one
    undo puts back every parameter it touched. */
LUTHIER_TEST (Undo, aPresetLoadIsOneNamedEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();
    CHECK (presets.getNumPresets() > 1);

    if (presets.getNumPresets() < 2)
        return;

    const auto before = processor.captureStateBlock();
    const auto parameterBefore = [&processor]
    {
        juce::Array<float> values;
        for (auto* p : processor.getParameters())
            values.add (p->getValue());
        return values;
    }();

    const int before_steps = processor.getNumUndoSteps();
    CHECK (processor.loadPresetAsUserAction (1));
    CHECK (processor.getNumUndoSteps() == before_steps + 1);
    CHECK (processor.getUndoDescription() == "Load preset " + presets.getPreset (1)->name);

    processor.undo();

    int differing = 0;
    const auto& all = processor.getParameters();

    for (int i = 0; i < all.size(); ++i)
        if (std::abs (all[i]->getValue() - parameterBefore[i]) > 1.0e-5f)
            ++differing;

    CHECK_MSG (differing == 0, juce::String (differing) + " parameters did not come back");
}

//==============================================================================
/*  5 and 13: a preset load is a boundary. Plain undo reverses the load itself
    and stops; Ctrl-Alt-Z (undoAcrossBoundary) crosses to what came before. */
LUTHIER_TEST (Undo, aPresetLoadIsABoundary)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    if (processor.getPresetManager().getNumPresets() < 2)
    {
        CHECK_MSG (false, "no presets to load");
        return;
    }

    const juce::String gain (ParamIDs::ampGain);

    processor.pushUndoState ("first edit");
    setPlain (processor, gain, 0.11f);

    processor.loadPresetAsUserAction (1);
    const float loaded = plainOf (processor, gain);

    processor.pushUndoState ("second edit");
    setPlain (processor, gain, 0.93f);

    processor.undo();
    CHECK_NEAR (plainOf (processor, gain), loaded, 1.0e-3);

    processor.undo();   // reverses the load
    CHECK_NEAR (plainOf (processor, gain), 0.11f, 1.0e-3);

    CHECK_MSG (! processor.canUndo(), "plain undo would cross the boundary");
    CHECK (processor.isUndoStoppedAtBoundary());

    processor.undo();   // does nothing
    CHECK_NEAR (plainOf (processor, gain), 0.11f, 1.0e-3);

    const auto history = processor.getUndoHistory();
    CHECK (history.size() == 1 && history[0].description == "first edit");

    processor.undoAcrossBoundary();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.11f) > 1.0e-3f, "crossing did not undo the first edit");
    CHECK (! processor.canUndo());

    // Redo walks forward through the boundary as normal.
    processor.redo();
    processor.redo();
    processor.redo();
    CHECK_NEAR (plainOf (processor, gain), 0.93f, 1.0e-3);
}

//==============================================================================
/*  4 and 13: gestures on one parameter 199 ms apart are one entry, 201 ms apart
    are two, and different parameters never merge. The clock is pinned. */
LUTHIER_TEST (Undo, gesturesGroupWithin200ms)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    double now = 10000.0;
    processor.setUndoClock ([&now] { return now; });

    const juce::String gain (ParamIDs::ampGain);
    const juce::String master (ParamIDs::ampMaster);

    setPlain (processor, gain, 0.1f);
    const int start = processor.getNumUndoSteps();

    dragTo (processor, gain, 0.2f, 4);
    now += 199.0;
    dragTo (processor, gain, 0.3f, 4);

    CHECK_MSG (processor.getNumUndoSteps() == start + 1, "199 ms apart did not merge");
    CHECK_MSG (processor.getUndoDescription().contains ("from 0.1") && processor.getUndoDescription().contains ("to 0.3"),
               "merged description: " + processor.getUndoDescription());

    now += 201.0;
    dragTo (processor, gain, 0.4f, 4);
    CHECK_MSG (processor.getNumUndoSteps() == start + 2, "201 ms apart merged");

    now += 10.0;
    dragTo (processor, master, 0.5f, 4);
    CHECK_MSG (processor.getNumUndoSteps() == start + 3, "different parameters merged");

    // Undo of the merged pair goes back to before the first of them.
    processor.undo();
    processor.undo();
    processor.undo();
    CHECK_NEAR (plainOf (processor, gain), 0.1f, 1.0e-3);
}

//==============================================================================
/*  3.7: saving and recalling a snapshot from the UI are one entry each, and
    undoing the recall returns to the pre-recall values. */
LUTHIER_TEST (Undo, snapshotSaveAndRecallAreEntries)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.getSnapshots().setCrossfadeMs (0.0);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 0.25f);

    const int start = processor.getNumUndoSteps();
    CHECK (processor.captureSnapshotAsUserAction (0, "Verse"));
    CHECK (processor.getNumUndoSteps() == start + 1);
    CHECK (processor.getUndoDescription() == "Save snapshot 1 Verse");

    setPlain (processor, gain, 0.75f);
    CHECK (processor.recallSnapshotAsUserAction (0));
    CHECK_NEAR (plainOf (processor, gain), 0.25f, 1.0e-3);
    CHECK (processor.getUndoDescription() == "Recall snapshot 1 Verse");

    processor.undo();
    CHECK_NEAR (plainOf (processor, gain), 0.75f, 1.0e-3);

    processor.undo();   // the save
    CHECK (processor.getSnapshots().getSnapshot (0).isEmpty());
}

//==============================================================================
/*  3.6 / ui-wiring 18: the right-click Modulate and Remove modulation items
    push entries, so undo takes a route back out (and puts it back). */
LUTHIER_TEST (Undo, rightClickModulationIsUndoable)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::Component owner;
    auto& matrix = processor.getModMatrix();
    matrix.clearRoutes();

    const juce::String destination (ParamIDs::masterGain);

    applyParameterMenuResult (kModulateMenuBase + ModSourceSlots::lfoBase, owner, processor, destination);
    CHECK (matrix.getRouteCountForDestination (destination) == 1);
    CHECK_MSG (processor.getUndoDescription().startsWith ("Add "), processor.getUndoDescription());

    applyParameterMenuResult (9, owner, processor, destination);   // Remove modulation
    CHECK (matrix.getRouteCountForDestination (destination) == 0);

    processor.undo();
    CHECK (matrix.getRouteCountForDestination (destination) == 1);

    processor.undo();
    CHECK (matrix.getRouteCountForDestination (destination) == 0);
}

//==============================================================================
/*  3.13: moving a pedal is one entry, and undo puts every slot back. */
LUTHIER_TEST (Undo, movingAPedalIsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto typeOf = [&processor] (int slot)
    {
        return processor.getState().getParameter (ParamIDs::slotType (false, slot))->getValue();
    };

    // Two different pedals in the first two slots.
    for (int slot = 0; slot < 2; ++slot)
    {
        auto* p = processor.getState().getParameter (ParamIDs::slotType (false, slot));
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (slot + 1)));
    }

    const float first = typeOf (0), second = typeOf (1);
    CHECK (first != second);

    PedalRack rack (processor, false);
    const int start = processor.getNumUndoSteps();

    rack.reorder (0, 1);
    CHECK_NEAR (typeOf (0), second, 1.0e-6);
    CHECK_MSG (processor.getNumUndoSteps() == start + 1,
               "a move made " + juce::String (processor.getNumUndoSteps() - start) + " entries");

    processor.undo();
    CHECK_NEAR (typeOf (0), first, 1.0e-6);
    CHECK_NEAR (typeOf (1), second, 1.0e-6);
}

//==============================================================================
/*  3.12: a completed learn is one entry. */
LUTHIER_TEST (Undo, aMidiLearnIsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& learn = processor.getMidiLearn();
    const juce::String gain (ParamIDs::ampGain);
    const int start = processor.getNumUndoSteps();

    /*  processMidi defers the mapping to the message thread with callAsync,
        and nothing here runs a dispatch loop, so this does what that deferred
        call does: the hook, then the mapping. */
    CHECK (learn.onBeforeLearn != nullptr);
    learn.onBeforeLearn (gain, 21);
    learn.addMapping (gain, 21);

    CHECK (learn.getCcForParameter (gain) == 21);
    CHECK (processor.getNumUndoSteps() == start + 1);
    CHECK (processor.getUndoDescription().startsWith ("Learn CC 21"));

    processor.undo();
    CHECK (learn.getCcForParameter (gain) != 21);
}

//==============================================================================
/*  3.14: clearing the looper can be taken back (layer deletion is restorable). */
LUTHIER_TEST (Undo, aClearedLoopLayerComesBack)
{
    Looper looper;
    looper.prepare (kSr, 4.0);

    auto& layer = looper.getLayer (0);
    layer.getAudio().setSample (0, 10, 0.5f);
    layer.setRecordedSamples (1000);

    looper.clear();
    CHECK (looper.getNumRecordedLayers() == 0);

    CHECK (looper.restoreCleared());
    CHECK (looper.getLayer (0).getRecordedSamples() == 1000);
    CHECK_NEAR (looper.getLayer (0).getAudio().getSample (0, 10), 0.5f, 1.0e-6);
}

//==============================================================================
/*  ui-wiring 17 / workshop-ui 7: the bench's A/B slots and the setlist travel
    in the plugin state; undo does not restore the bench slots. */
LUTHIER_TEST (Undo, benchSlotsAndSetlistAreInTheStateButBenchSlotsIgnoreUndo)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    processor.getUiState().benchSlots[2] = juce::var ("slot three");

    Setlist set;
    SetlistEntry entry;
    entry.presetPath = "/nonexistent/preset.lthp";
    entry.snapshotIndex = 1;
    set.addEntry (entry);
    processor.getSetlist().setSetlist (set);

    const auto state = processor.captureStateBlock();

    LuthierAudioProcessor other;
    other.prepareToPlay (kSr, kBlock);
    other.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (other.getUiState().benchSlots[2].toString() == "slot three");
    CHECK (other.getSetlist().getSetlist().getNumEntries() == 1);

    // Undo leaves the workspace slots alone.
    processor.pushUndoState ("edit");
    processor.getUiState().benchSlots[2] = juce::var ("changed");
    processor.undo();
    CHECK (processor.getUiState().benchSlots[2].toString() == "changed");
}

//==============================================================================
/*  8 (the snapshot bleed guard): with every section 3 class tracked, undo
    reverses one action and nothing the user did after it that is not a value
    - locks, the metronome, the open tab, Live Mode. */
LUTHIER_TEST (Undo, laterUntrackedEditsSurviveUndo)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::Component owner;
    const juce::String gain (ParamIDs::ampGain);
    const juce::String master (ParamIDs::masterGain);

    setPlain (processor, gain, 0.3f);
    dragTo (processor, gain, 0.6f);                    // entry 1
    applyParameterMenuResult (kModulateMenuBase + ModSourceSlots::lfoBase,
                              owner, processor, master);   // entry 2

    processor.setParameterLocked (master, true);
    processor.getMetronome().setTempo (97.0);
    processor.getUiState().advancedTab = 2;
    processor.setLiveMode (true);

    processor.undo();
    CHECK (processor.getModMatrix().getRouteCountForDestination (master) == 0);
    CHECK_NEAR (plainOf (processor, gain), 0.6f, 1.0e-3);

    processor.undo();
    CHECK_NEAR (plainOf (processor, gain), 0.3f, 1.0e-3);

    CHECK_MSG (processor.isParameterLocked (master), "undo dropped a lock");
    CHECK_NEAR (processor.getMetronome().getTempo(), 97.0, 1.0e-6);
    CHECK (processor.getUiState().advancedTab == 2);
    CHECK (processor.isLiveMode());
}

//==============================================================================
/*  13: undo and redo between blocks while a note rings produce no garbage. */
LUTHIER_TEST (Undo, undoMidPlayProducesNoGarbage)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);

    for (int i = 0; i < 6; ++i)
    {
        processor.pushUndoState ("edit " + juce::String (i));
        setPlain (processor, gain, 0.1f + 0.1f * (float) i);
    }

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

    for (int i = 0; i < 12; ++i)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();

        if (i < 6) processor.undo();
        else       processor.redo();

        buffer.clear();
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            CHECK_FINITE (buffer.getReadPointer (channel), buffer.getNumSamples());
            CHECK_MSG (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) < 8.0f,
                       "an undo mid-note produced a burst");
        }
    }
}

//==============================================================================
/*  gui-integration 22 / ui-wiring 23: a 1000-operation random walk. The stack
    holds 200 entries (action-and-undo.md 2), so the walk runs in segments of
    125 operations: after each, a full undo must give back the segment's start
    state (byte-identical, or within 1e-6 per number: see sameState), and a full redo its end state. Operations: parameter
    gestures, Workshop part swaps and mod routes. */
LUTHIER_TEST (Undo, randomWalkUndoesBackToTheStart)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    double now = 0.0;
    processor.setUndoClock ([&now] { return now; });   // 1 s apart: nothing groups

    juce::Component owner;
    juce::Random random (20260924);

    const juce::StringArray params { ParamIDs::ampGain, ParamIDs::ampMaster, ParamIDs::masterGain };
    const PartType types[] = { PartType::bridge, PartType::pickup };
    const GuitarSlot slots[] = { GuitarSlot::bridge, GuitarSlot::pickupNeck };

    /*  A fresh instance plays its guitar type without having applied the
        type's parts file, so its per-string custom gauges (the side channel
        of GAPS B1) read 0 until a parts guitar has been applied once; after
        that a restore of the same state reads the strings part's gauges. One
        fit and its undo settle that before the walk, so what is compared is
        undo itself, not that first-load difference. */
    {
        const auto& parts = processor.getPartLibrary().getParts (PartType::bridge);
        processor.getBench().fit (GuitarSlot::bridge, parts[parts.size() - 1]);
        processor.undo();
    }

    int ops = 0;

    while (ops < 1000)
    {
        const auto start = processor.captureStateBlock();

        for (int i = 0; i < 125; ++i, ++ops)
        {
            now += 1000.0;
            const int kind = random.nextInt (10);

            if (kind < 7)
            {
                const auto id = params[random.nextInt (params.size())];
                auto* p = dynamic_cast<juce::AudioParameterFloat*> (paramOf (processor, id));
                dragTo (processor, id, p->convertFrom0to1 (random.nextFloat()), 3);
            }
            else if (kind < 8)
            {
                const int which = random.nextInt (2);
                const auto& parts = processor.getPartLibrary().getParts (types[which]);

                if (! parts.isEmpty())
                    processor.getBench().fit (slots[which], parts[random.nextInt (parts.size())]);
            }
            else
            {
                const juce::String destination = params[random.nextInt (params.size())];

                if (processor.getModMatrix().getRouteCountForDestination (destination) > 0)
                    applyParameterMenuResult (9, owner, processor, destination);
                else
                    applyParameterMenuResult (kModulateMenuBase + ModSourceSlots::lfoBase + random.nextInt (3),
                                              owner, processor, destination);
            }
        }

        CHECK (processor.getNumUndoSteps() <= 200);

        const auto end = processor.captureStateBlock();
        int undone = 0;

        while (processor.getNumUndoSteps() > 0)
        {
            processor.undoAcrossBoundary();
            ++undone;
        }

        juce::String where;
        CHECK_MSG (sameState (processor.captureStateBlock(), start, where),
                   "a full undo did not return to the segment's start (after " + juce::String (ops)
                     + " ops), first difference at " + where);

        for (int i = 0; i < undone; ++i)
            processor.redo();

        CHECK_MSG (sameState (processor.captureStateBlock(), end, where),
                   "a full redo did not return to the segment's end, first difference at " + where);

        // Back to the start again: the next segment's first push drops the
        // redo tail, so every segment starts on an empty stack.
        while (processor.getNumUndoSteps() > 0)
            processor.undoAcrossBoundary();
    }
}

//==============================================================================
/*  1 / 9 / 12: the history (newest first, boundaries flagged), the redo count,
    and File -> Undo history, whose click undoes back to before that entry. */
LUTHIER_TEST (Undo, theHistoryListsNewestFirstAndUndoesToAPoint)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    setPlain (processor, gain, 0.1f);

    processor.pushUndoState ("one");
    setPlain (processor, gain, 0.2f);
    processor.pushUndoBoundary ("Load preset X");
    setPlain (processor, gain, 0.3f);
    processor.pushUndoState ("three");
    setPlain (processor, gain, 0.4f);

    const auto history = processor.getUndoHistory();
    CHECK (history.size() == 3);
    CHECK (history[0].description == "three" && history[0].stepsBack == 1);
    CHECK (history[1].boundary && history[1].stepsBack == 2);
    CHECK (history[2].description == "one" && history[2].stepsBack == 3);

    const auto menu = HeaderBar::buildUndoHistoryMenu (processor);
    juce::Array<int> ids;
    int separators = 0;

    for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
    {
        if (it.getItem().isSeparator)
            ++separators;
        else if (it.getItem().itemID != 0)
            ids.add (it.getItem().itemID);
    }

    CHECK (ids == juce::Array<int> ({ 2, 3, 4 }));
    CHECK (separators == 1);

    // Click "one": undo back to before it, across the boundary.
    HeaderBar::applyUndoHistoryChoice (processor, 4);
    CHECK_NEAR (plainOf (processor, gain), 0.1f, 1.0e-3);
    CHECK (processor.getNumUndoSteps() == 0);
    CHECK (processor.getNumRedoSteps() == 3);

    processor.redo();
    CHECK_NEAR (plainOf (processor, gain), 0.2f, 1.0e-3);
}
