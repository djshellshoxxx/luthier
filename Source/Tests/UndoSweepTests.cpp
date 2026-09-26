/*  SPEC-SWEEP: action-and-undo.md 0.5 / gui-integration.md 18 (AU-5, GI-106)
    and gui-integration.md 2's transient A/B compare (GI-21). */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    void set (LuthierAudioProcessor& p, const char* id, float normalised)
    {
        auto* parameter = p.getState().getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    }

    float get (LuthierAudioProcessor& p, const char* id)
    {
        return p.getState().getParameter (id)->getValue();
    }
}

//==============================================================================
/*  AU-5 / GI-106: each instance has its own history, and a saved state carries
    none of it. */
LUTHIER_TEST (Undo, isPerInstanceAndNotPersisted)
{
    LuthierAudioProcessor a, b;

    const int bBefore = b.getNumUndoSteps();
    const float bGain = get (b, ParamIDs::ampGain);

    a.pushUndoState ("sweep");
    set (a, ParamIDs::ampGain, 0.9f);

    CHECK (a.canUndo());
    CHECK (b.getNumUndoSteps() == bBefore);

    a.undo();
    CHECK_NEAR (get (b, ParamIDs::ampGain), bGain, 1.0e-6f);

    juce::MemoryBlock state;
    a.getStateInformation (state);

    LuthierAudioProcessor c;
    const int cBefore = c.getNumUndoSteps();
    c.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_MSG (c.getNumUndoSteps() <= cBefore + 1,
               "a restored session came back with " + juce::String (c.getNumUndoSteps()) + " undo steps");
    CHECK (! c.canRedo());
}

/*  GI-21: A/B compare flips the sound and is never saved. */
LUTHIER_TEST (Editor, abCompareIsTransientAndNotSaved)
{
    LuthierAudioProcessor p;

    // What is on screen belongs to the active slot: A, then B.
    set (p, ParamIDs::ampGain, 0.2f);
    p.setSlotBActive (true);
    set (p, ParamIDs::ampGain, 0.8f);

    p.setSlotBActive (false);
    CHECK_NEAR (get (p, ParamIDs::ampGain), 0.2f, 1.0e-4f);

    p.setSlotBActive (true);
    CHECK_NEAR (get (p, ParamIDs::ampGain), 0.8f, 1.0e-4f);

    juce::MemoryBlock state;
    p.getStateInformation (state);

    const auto text = state.toString();
    CHECK (! text.contains ("slotA"));
    CHECK (! text.contains ("slotB"));

    LuthierAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (! restored.isSlotBActive());

    const auto preset = juce::JSON::toString (p.getPresetManager().toVar ("sweep"));
    CHECK (! preset.contains ("slotA") && ! preset.contains ("slotB"));
}
