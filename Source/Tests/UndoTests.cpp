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
