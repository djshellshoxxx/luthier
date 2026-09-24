/*  action-and-undo.md: the processor's undo stack.

    What counts as one entry (0.1), the 200-entry cap (2), what never makes an
    entry (3.1, 7, 11), boundaries (5), grouping (4) and the action classes of
    section 3 that the processor owns. Each test goes through the real
    processor, because a snapshot-based stack is only as good as what the
    snapshot captures and what the restore leaves alone.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/OptionsPages.h"

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
