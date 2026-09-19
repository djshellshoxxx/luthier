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

    /*  Two entries, because one stored state is not something to go back *to*:
        pushUndoState records where things stood before a change, so canUndo only
        becomes true once there is a previous state as well as a current one. */
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
    CHECK_MSG (processor.isSlotBActive(), "a preset load changed the A/B slot");
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
        so the parameter only moves as blocks are rendered. */
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int i = 0; i < 16; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
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
