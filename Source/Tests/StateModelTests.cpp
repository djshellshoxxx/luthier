/*  state-model.md, the layering rules.

    These tests exist because of a build change rather than a code change:
    PluginProcessor used to be excluded from this target, so everything the
    processor alone owns - the undo stack, uiState, the A/B slots, snapshot recall
    - could be read in the source but never exercised. state-model.md specifies all
    of it precisely, and a specification nothing checks is a wish.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
}

//==============================================================================
/*  state-model.md 2, the "never touches" list for a preset load.

    A preset carries the sound. It does not carry which tab you had open, whether
    you were in Live Mode, what is on your undo stack, or whether MIDI Learn was
    armed - those live in layers above it (section 1), and a load that reached into
    them would lose the user's place every time they auditioned a sound. */
LUTHIER_TEST (StateModel, loadingAPresetLeavesTheLayersAboveItAlone)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 2)
    {
        CHECK_MSG (false, "no factory presets to load, so this proves nothing");
        return;
    }

    // ---- set up every layer the load must not touch -----------------------------
    auto& ui = processor.getUiState();

    ui.advancedMode = true;
    ui.advancedTab = 3;
    ui.selectedString = 4;
    ui.tooltipsEnabled = false;

    processor.setLiveMode (true);
    processor.getMidiLearn().setArmed (true);

    // One entry is enough: each entry holds the state from before its action.
    processor.pushUndoState ("Something the user did");
    processor.pushUndoState ("Something else the user did");

    CHECK_MSG (processor.canUndo(), "the undo stack was empty, so this proves nothing");

    processor.setSlotBActive (true);

    // ---- load ---------------------------------------------------------------------
    CHECK (presets.loadPreset (0));

    // ---- uiState is per-instance and does not travel with presets (section 0.6) ---
    CHECK_MSG (ui.advancedMode, "a preset load reset the Easy/Advanced mode");
    CHECK_MSG (ui.advancedTab == 3, "a preset load moved the open tab");
    CHECK_MSG (ui.selectedString == 4, "a preset load changed the selected string");
    CHECK_MSG (! ui.tooltipsEnabled, "a preset load re-enabled tooltips");
    CHECK_MSG (processor.isLiveMode(), "a preset load left Live Mode");

    // ---- session state survives (section 1) --------------------------------------
    CHECK_MSG (processor.canUndo(), "a preset load cleared the undo stack");
    CHECK_MSG (processor.getMidiLearn().isArmed(),
               "a preset load disarmed MIDI Learn");

    /*  SPEC-SWEEP: SM-46. state-model.md 8.1 is the specific rule for A/B and
        wins over section 2's general "never touches" list: the compare clears,
        because either slot recalled after a load would silently undo it. */
    CHECK_MSG (! processor.isSlotBActive(), "a preset load left A/B compare active (state-model 8.1)");
}

//==============================================================================
/*  state-model.md 3: a snapshot lives inside the preset, so recalling one is an
    in-preset operation. It must not change the guitar, and it must not reach the
    layers above the preset either. */
LUTHIER_TEST (StateModel, recallingASnapshotStaysInsideThePreset)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& ui = processor.getUiState();

    ui.advancedTab = 2;
    processor.setLiveMode (true);

    const auto guitarBefore = processor.getEngine().getGuitarSpec().name;

    // Two snapshots that differ, so a recall has something to do.
    if (auto* gain = processor.getState().getParameter (ParamIDs::ampGain))
        gain->setValueNotifyingHost (0.2f);

    CHECK (processor.captureSnapshot (0, "Quiet"));

    if (auto* gain = processor.getState().getParameter (ParamIDs::ampGain))
        gain->setValueNotifyingHost (0.9f);

    CHECK (processor.captureSnapshot (1, "Loud"));

    processor.pushUndoState ("Before recall");
    const bool couldUndo = processor.canUndo();

    CHECK (processor.recallSnapshot (0));

    /*  A recall returns before it has finished. The crossfade is carried by the
        audio thread's own clock (live-performance.md, and state-model 3 step 3),
        so the parameter only moves as blocks are rendered - and the processor's
        timer applies that time on the message thread, as advancePending does
        here. */
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int i = 0; i < 16; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            processor.getSnapshots().advancePending();
        }
    }

    // The recall reached the parameter.
    if (auto* gain = processor.getState().getParameter (ParamIDs::ampGain))
        CHECK_MSG (gain->getValue() < 0.6f,
                   "recalling the quiet snapshot did not lower the gain");

    // And nothing above the preset moved.
    CHECK_MSG (processor.getEngine().getGuitarSpec().name == guitarBefore,
               "a snapshot recall changed the guitar, which section 3 forbids");
    CHECK_MSG (ui.advancedTab == 2, "a snapshot recall moved the open tab");
    CHECK_MSG (processor.isLiveMode(), "a snapshot recall left Live Mode");
    CHECK_MSG (processor.canUndo() == couldUndo,
               "a snapshot recall disturbed the undo stack's availability");
}

//==============================================================================
/*  state-model.md 0.2: a load is atomic per layer and does not interrupt a block.

    Rendering across a load is the cheap way to check that: if the swap tore, or a
    module were left half-configured, it would show up as a non-finite sample long
    before it showed up as a crash. */
LUTHIER_TEST (StateModel, loadingAPresetWhileRenderingProducesNoGarbage)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 3)
    {
        CHECK_MSG (false, "not enough factory presets to switch between");
        return;
    }

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

    for (int i = 0; i < 12; ++i)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();

        // Swap the sound out from under the note that is still ringing.
        presets.loadPreset (i % 3);

        buffer.clear();
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            CHECK_FINITE (buffer.getReadPointer (channel), buffer.getNumSamples());

            CHECK_MSG (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) < 8.0f,
                       "a preset swap mid-note produced a burst");
        }
    }
}

//==============================================================================
/*  The Ableton program-change quirk.

    Live calls setCurrentProgram(0) immediately after setStateInformation, to
    "restore" the plugin to its first program. Luthier's setCurrentProgram really
    does load a preset, so obeying that call overwrites the state the host just
    restored: the user reopens a project and finds factory preset 0 instead of the
    sound they saved.

    Documented in troubleshooting/parameter-issues/
    ableton-preset-interference-state-restoration-JUCE-20251107.md. That note's own
    fix - return 0 from getNumPrograms - is not available here, because
    host-integration.md 12 requires the program interface so Program Change can
    address presets. So the first program change after a restore is swallowed
    instead. */
LUTHIER_TEST (StateModel, aProgramChangeRightAfterAStateRestoreDoesNotWipeIt)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (kSr, kBlock);

    if (source.getPresetManager().getNumPresets() < 2)
    {
        CHECK_MSG (false, "not enough factory presets, so this proves nothing");
        return;
    }

    source.getPresetManager().loadPreset (1);

    juce::MemoryBlock block;
    source.getStateInformation (block);

    // ---- what the host does when the project reopens -----------------------------
    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);

    restored.setStateInformation (block.getData(), (int) block.getSize());

    /*  The probe is the selected preset index rather than a parameter value.

        A parameter is the more obvious choice and it is the wrong one: whether
        loading a preset moves a given parameter depends on what that preset
        happens to contain, so the test would quietly stop proving anything if the
        bank changed. loadPreset always sets the index, so the index always moves
        when a program change is obeyed. */
    const int afterRestore = restored.getPresetManager().getCurrentPresetIndex();
    const int different = (afterRestore == 0) ? 1 : 0;

    // Live's tidy-up call, which used to load a preset over the restored state.
    restored.setCurrentProgram (different);

    CHECK_MSG (restored.getPresetManager().getCurrentPresetIndex() == afterRestore,
               "a program change straight after a state restore was obeyed: the user "
               "reopens the project and gets a different preset than they saved");

    /*  Only the first one is swallowed. A Program Change the user actually sends
        still has to work, or the fix would break the feature host-integration.md
        12 asks for. */
    restored.setCurrentProgram (different);

    CHECK_MSG (restored.getPresetManager().getCurrentPresetIndex() == different,
               "a later program change was ignored too, so Program Change no longer "
               "selects presets");
}

//=======================================================================