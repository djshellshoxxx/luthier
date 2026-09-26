/*  SPEC-SWEEP UI wiring checks (ui-wiring, gui-engine-dataflow, keyboard
    shortcuts): widgets that must stay live without being touched, and editor
    keys that must reach the processor.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../UI/Widgets.h"
#include "../UI/FretboardComponent.h"
#include "../UI/GuitarBodyComponent.h"
#include "../UI/RoutingPanel.h"
#include "../UI/CircuitPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/SetupGroup.h"
#include "../UI/PracticePanel.h"
#include "../UI/NextStrumArrow.h"
#include "../UI/Overlays.h"
#include "../UI/PedalRack.h"
#include "../Live/Setlist.h"
#include "../Rhythm/Patterns.h"
#include "../Accessibility/Accessibility.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;

        return {};
    }

    struct EditorFixture
    {
        EditorFixture()
        {
            processor.prepareToPlay (kSr, kBlock);
            editor.reset (processor.createEditor());

            if (editor != nullptr)
            {
                editor->setVisible (true);
                editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                                 LuthierAudioProcessorEditor::defaultHeight);
            }
        }

        bool press (const juce::KeyPress& key) { return editor != nullptr && editor->keyPressed (key); }

        LuthierAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
    };
}

//==============================================================================
/*  UW-35 / GD-17: a knob under an LFO or macro route repaints its arc from the
    shared 30 Hz hub even though nobody touches it, and an unmodulated knob does
    not repaint at all. */
LUTHIER_TEST (ModMatrixUi, aModulatedKnobRepaintsWithoutBeingTouched)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    LuthierKnob modulated ("Gain"), still ("Tone");
    modulated.attachTo (processor, ParamIDs::ampGain);
    still.attachTo (processor, ParamIDs::ampTreble);

    for (auto* knob : { &modulated, &still })
    {
        knob->setSize (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Normal),
                       LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
        knob->setPollArcWhileHidden (true);   // no desktop peer in a headless run
    }

    auto& matrix = processor.getModMatrix();

    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = ParamIDs::ampGain;
    route.depth = 1.0f;
    route.enabled = true;
    CHECK (matrix.addRoute (route));

    ModBlockContext context;
    int modulatedRepaints = 0, stillRepaints = 0;

    // The macro sweeps like a slow LFO; the hub polls once per "frame".
    for (int frame = 0; frame < 30; ++frame)
    {
        matrix.setMacroValue (0, 0.5 + 0.5 * std::sin ((double) frame * 0.4));

        for (int b = 0; b < 4; ++b)
            matrix.processBlock (kBlock, context);

        modulatedRepaints += modulated.pollModulationArc() ? 1 : 0;
        stillRepaints += still.pollModulationArc() ? 1 : 0;
    }

    CHECK_MSG (modulatedRepaints >= 20, "the modulated knob repainted " + juce::String (modulatedRepaints)
                                          + " times in 30 frames of a moving macro");
    CHECK_MSG (stillRepaints == 0, "an unmodulated knob repainted " + juce::String (stillRepaints) + " times");
    CHECK (modulated.getArcRepaintCount() == modulatedRepaints);

    // Holding the macro still stops the repaints after the arc settles.
    modulated.pollModulationArc();
    CHECK (! modulated.pollModulationArc());
}

/*  UW-5: the MOD card's source settings reach the source on the audio thread,
    at the top of the matrix's next block; with no audio running they apply at
    once, so a stopped host still sees the edit. */
LUTHIER_TEST (ModMatrixUi, sourceEditsApplyOnTheAudioThread)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& matrix = processor.getModMatrix();
    ModBlockContext context;
    matrix.processBlock (kBlock, context);   // audio is "running"

    const double before = matrix.getLfo (0).getRateHz();

    ModSourceEdit edit;
    edit.kind = ModSourceEdit::Kind::lfo;
    edit.index = 0;
    edit.lfoRateHz = before + 2.5;
    edit.lfoDepth = 0.6;
    CHECK (matrix.postSourceEdit (edit));

    CHECK (matrix.getNumPendingSourceEdits() == 1);
    CHECK_NEAR (matrix.getLfo (0).getRateHz(), before, 1.0e-9);

    matrix.processBlock (kBlock, context);

    CHECK (matrix.getNumPendingSourceEdits() == 0);
    CHECK_NEAR (matrix.getLfo (0).getRateHz(), before + 2.5, 1.0e-9);
    CHECK_NEAR (matrix.getLfo (0).getDepth(), 0.6, 1.0e-9);

    // No block for longer than the idle window: applied immediately.
    juce::Thread::sleep ((int) ModMatrix::kIdleApplyMs + 50);
    edit.lfoRateHz = before + 4.0;
    CHECK (matrix.postSourceEdit (edit));
    CHECK (matrix.getNumPendingSourceEdits() == 0);
    CHECK_NEAR (matrix.getLfo (0).getRateHz(), before + 4.0, 1.0e-9);
}

//==============================================================================
/*  KS-9 / KS-10 (docs/KEYBOARD_SHORTCUTS.md, live-performance 2): digits 1-9
    recall snapshots 1-9 and Shift+digit recalls 10-18. Shift+1 types '!', and
    the editor used to read the typed character, so the second bank never
    answered. */
LUTHIER_TEST (Editor, digitsRecallSnapshots)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    auto& bank = f.processor.getSnapshots();
    bank.setCrossfadeMs (0.0);

    for (int i = 0; i < 12; ++i)
        CHECK (f.processor.captureSnapshot (i, "S" + juce::String (i + 1)));

    CHECK (f.press (juce::KeyPress ('2')));
    CHECK_MSG (bank.getCurrentSnapshot() == 1, "2 recalled " + juce::String (bank.getCurrentSnapshot()));

    CHECK (f.press (juce::KeyPress ('1', juce::ModifierKeys::shiftModifier, '!')));
    CHECK_MSG (bank.getCurrentSnapshot() == 9, "Shift+1 recalled " + juce::String (bank.getCurrentSnapshot()));

    CHECK (f.press (juce::KeyPress ('3', juce::ModifierKeys::shiftModifier, '#')));
    CHECK (bank.getCurrentSnapshot() == 11);

    // A digit past the bank is not swallowed.
    CHECK (! f.press (juce::KeyPress ('9', juce::ModifierKeys::shiftModifier, '(')));
}

/*  KS-7: Panic, Tap tempo and the kill switch keys reach the processor. */
LUTHIER_TEST (Editor, panicTapAndKillKeysAct)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    const int pending = f.processor.getNumPendingEngineCommands();
    CHECK (f.press (shortcutFor ("panic")));
    CHECK (f.processor.getNumPendingEngineCommands() == pending + 1);

    const int taps = f.processor.getTapTempo().getTapCount();
    CHECK (f.press (shortcutFor ("tapTempo")));
    CHECK (f.processor.getTapTempo().getTapCount() == taps + 1);

    const bool killed = f.processor.getKillSwitch().isActive();
    CHECK (f.press (shortcutFor ("killSwitch")));
    CHECK (f.processor.getKillSwitch().isActive() != killed);
    CHECK (f.press (shortcutFor ("killSwitch")));
    CHECK (f.processor.getKillSwitch().isActive() == killed);
}

/*  KS-5: Space starts and stops the audition. */
LUTHIER_TEST (Editor, spaceTogglesTheAudition)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    CHECK (! f.processor.isAuditioning());
    CHECK (f.press (shortcutFor ("audition")));
    CHECK (f.processor.isAuditioning());
    CHECK (f.press (shortcutFor ("audition")));
    CHECK (! f.processor.isAuditioning());
}

/*  KS-15: Ctrl+L arms MIDI Learn and again disarms it. */
LUTHIER_TEST (Editor, ctrlLArmsMidiLearn)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    CHECK (! f.processor.getMidiLearn().isArmed());
    CHECK (f.press (shortcutFor ("midiLearnArm")));
    CHECK (f.processor.getMidiLearn().isArmed());
    CHECK (f.press (shortcutFor ("midiLearnArm")));
    CHECK (! f.processor.getMidiLearn().isArmed());
}

/*  KS-16 / KS-17: undo and redo, and the A/B key. */
LUTHIER_TEST (Editor, undoRedoAndABKeysReachTheProcessor)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    auto* gain = f.processor.getState().getParameter (ParamIDs::ampGain);
    CHECK (gain != nullptr);

    if (gain == nullptr)
        return;

    const float before = gain->getValue();
    f.processor.pushUndoState ("Gain");
    gain->setValueNotifyingHost (before > 0.5f ? 0.1f : 0.9f);
    const float after = gain->getValue();

    CHECK (f.press (shortcutFor ("undo")));
    CHECK_NEAR (gain->getValue(), before, 1.0e-4);

    CHECK (f.press (shortcutFor ("redo")));
    CHECK_NEAR (gain->getValue(), after, 1.0e-4);

    const bool b = f.processor.isSlotBActive();
    CHECK (f.press (shortcutFor ("abCompare")));
    CHECK (f.processor.isSlotBActive() != b);
}

//==============================================================================
/*  GD-8 (gui-engine-dataflow 3): the output LED refreshes at 60 Hz, holds red
    for 400 ms after an over, and goes unlit 100 ms after the last block. */
LUTHIER_TEST (Dataflow, theOutputLedHoldsRedFor400ms)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    OutputLed led;
    led.setSource (&processor);
    CHECK (OutputLed::kRefreshHz == 60);
    CHECK (OutputLed::darkColour() == juce::Colour (0xff5a5f66));

    auto& bus = processor.getEngine().getMasterBus();
    bus.setLimiterEnabled (false);

    auto feed = [&bus] (float level)
    {
        juce::AudioBuffer<float> b (2, kBlock);

        // A fresh buffer each block (the bus works in place), alternating signs
        // (its DC blocker would eat a constant).
        for (int block = 0; block < 8; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    b.setSample (ch, i, (i % 2 == 0) ? level : -level);

            bus.processBlock (b);
        }
    };

    feed (2.0f);   // +6 dBFS
    led.tick (1000.0);
    CHECK (led.isRed());

    feed (0.1f);   // well under, still playing
    led.tick (1200.0);
    CHECK_MSG (bus.getPeakDb() < 0.0, "peak after the over " + juce::String (bus.getPeakDb(), 2) + " dB");
    CHECK_MSG (led.isRed(), "red did not hold for 400 ms");

    feed (0.1f);
    led.tick (1450.0);
    CHECK_MSG (! led.isRed(), "red held past 400 ms; peak " + juce::String (bus.getPeakDb(), 2) + " dB");
    CHECK (led.isLit());

    // No new block for more than 100 ms: dark.
    led.tick (1600.0);
    CHECK (! led.isLit());
}

/*  GD-2 (gui-engine-dataflow 0.2): each live element drains at its spec rate -
    the fretboard and the illustration at 60 Hz, the routing meters and the
    circuit response at 30 Hz. */
LUTHIER_TEST (Dataflow, everyLiveElementDrainsAtItsSpecRate)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto intervalFor = [] (int hz) { return 1000 / hz; };

    FretboardComponent fretboard (processor);
    GuitarBodyComponent body (processor);
    RoutingPanel routing (processor);
    CircuitResponseView circuit (processor);

    CHECK (FretboardComponent::kRefreshHz == 60);
    CHECK (GuitarBodyComponent::kRefreshHz == 60);
    CHECK (RoutingPanel::kRefreshHz == 30);
    CHECK (CircuitResponseView::kRefreshHz == 30);

    CHECK_MSG (fretboard.getRefreshIntervalMs() == intervalFor (60), juce::String (fretboard.getRefreshIntervalMs()));
    CHECK_MSG (body.getRefreshIntervalMs() == intervalFor (60), juce::String (body.getRefreshIntervalMs()));
    CHECK_MSG (routing.getRefreshIntervalMs() == intervalFor (30), juce::String (routing.getRefreshIntervalMs()));
    CHECK_MSG (circuit.getRefreshIntervalMs() == intervalFor (30), juce::String (circuit.getRefreshIntervalMs()));

    OutputLed led;
    CHECK (OutputLed::kRefreshHz == 60);
}

/*  GD-9 (gui-engine-dataflow 4): the Easy chord readout keeps the last chord
    after the hand lifts and dims it after three seconds without a new one. */
LUTHIER_TEST (EasyLayout, theChordReadoutDimsAfterThreeSeconds)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    EasyPanel panel (processor);

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);
    auto render = [&] (juce::MidiBuffer& midi) { buffer.clear(); processor.processBlock (buffer, midi); };

    juce::MidiBuffer midi;

    for (int n : { 48, 52, 55 })
        midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);

    render (midi);

    for (int i = 0; i < 4; ++i) { juce::MidiBuffer none; render (none); }

    panel.tickChordReadout (0.0);
    const auto chord = panel.getChordReadoutText();
    CHECK_MSG (chord.isNotEmpty(), "no chord shown while a C major was held");
    CHECK (! panel.isChordReadoutDimmed());

    midi.clear();

    for (int n : { 48, 52, 55 })
        midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);

    render (midi);

    panel.tickChordReadout (1000.0);
    CHECK (panel.getChordReadoutText() == chord);
    CHECK (! panel.isChordReadoutDimmed());

    panel.tickChordReadout (3500.0);
    CHECK (panel.getChordReadoutText() == chord);
    CHECK (panel.isChordReadoutDimmed());
}

/*  UW-14 (ui-wiring 2): a momentary toggle is on while held and releases on
    mouse up. */
LUTHIER_TEST (Widgets, aMomentaryToggleReleasesOnMouseUp)
{
    LuthierAudioProcessor processor;
    auto* param = processor.getState().getParameter (ParamIDs::mpeEnabled);
    CHECK (param != nullptr);

    if (param == nullptr)
        return;

    LuthierToggle toggle ("MPE");
    toggle.attachTo (processor, ParamIDs::mpeEnabled);
    toggle.setMomentary (true);
    CHECK (toggle.isMomentary());

    CHECK (param->getValue() < 0.5f);

    toggle.getButton().setState (juce::Button::buttonDown);
    CHECK_MSG (param->getValue() > 0.5f, "pressing did not switch it on");

    toggle.getButton().setState (juce::Button::buttonNormal);
    CHECK_MSG (param->getValue() < 0.5f, "letting go did not switch it off");
}

/*  KS-8: [ and ] step presets, or snapshots while Live Mode is on. */
LUTHIER_TEST (Editor, bracketsStepPresetsOrSnapshotsInLiveMode)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    auto& presets = f.processor.getPresetManager();
    CHECK (presets.getNumPresets() >= 3);
    presets.loadPreset (1);

    CHECK (f.press (shortcutFor ("nextItem")));
    CHECK (presets.getCurrentPresetIndex() == 2);
    CHECK (f.press (shortcutFor ("previousItem")));
    CHECK (presets.getCurrentPresetIndex() == 1);

    auto& bank = f.processor.getSnapshots();
    bank.setCrossfadeMs (0.0);
    CHECK (f.processor.captureSnapshot (0, "A"));
    CHECK (f.processor.captureSnapshot (1, "B"));
    CHECK (f.processor.recallSnapshot (0));

    f.processor.setLiveMode (true);
    const int presetBefore = presets.getCurrentPresetIndex();

    CHECK (f.press (shortcutFor ("nextItem")));
    CHECK_MSG (bank.getCurrentSnapshot() == 1, "] in Live Mode left snapshot " + juce::String (bank.getCurrentSnapshot()));
    CHECK (presets.getCurrentPresetIndex() == presetBefore);
}

/*  KS-17: Ctrl+R moves an unlocked parameter; Ctrl+Shift+R returns it. */
LUTHIER_TEST (Editor, randomiseAndResetKeys)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    auto* gain = f.processor.getState().getParameter (ParamIDs::ampGain);
    CHECK (gain != nullptr);

    if (gain == nullptr)
        return;

    bool moved = false;

    for (int i = 0; i < 5 && ! moved; ++i)
    {
        const float before = gain->getValue();
        CHECK (f.press (shortcutFor ("randomise")));
        moved = std::abs (gain->getValue() - before) > 1.0e-4f;
    }

    CHECK_MSG (moved, "five randomises never moved amp_gain");

    CHECK (f.press (shortcutFor ("resetAll")));
    CHECK_NEAR (gain->getValue(), gain->getDefaultValue(), 1.0e-4);
}

/*  KS-20: double-click on a knob returns it to the parameter's default. */
LUTHIER_TEST (Widgets, doubleClickReturnsAKnobToItsDefault)
{
    LuthierAudioProcessor processor;
    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);

    auto& slider = knob.getSlider();
    CHECK (slider.isDoubleClickReturnEnabled());

    const auto range = processor.getState().getParameterRange (ParamIDs::ampGain);
    auto* param = processor.getState().getParameter (ParamIDs::ampGain);
    CHECK_NEAR (slider.getDoubleClickReturnValue(), range.convertFrom0to1 (param->getDefaultValue()), 1.0e-3);
}

/*  RE-38 (rhythm-engine 8, gui-integration 3.5): picking a kit in the Easy
    strip turns the rhythm engine on with that kit; outside Poly mode the strip
    says it needs Poly. */
LUTHIER_TEST (EasyLayout, theRhythmStripEnablesAKitAndHintsInMono)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    EasyPanel panel (processor);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    rhythm.setEnabled (false);

    auto& box = panel.getRhythmGenreBox();
    CHECK (box.getNumItems() > 0);
    box.setSelectedId (1, juce::sendNotificationSync);

    CHECK (rhythm.isEnabled());
    CHECK (! rhythm.getPattern().isEmpty());

    if (auto* mode = processor.getState().getParameter (ParamIDs::playingMode))
    {
        mode->setValueNotifyingHost (mode->convertTo0to1 (0.0f));   // Mono
        panel.refreshRhythmStripForTest();
        CHECK_MSG (panel.getRhythmHintText().isNotEmpty(), "no hint in Mono");

        mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));   // Poly
        panel.refreshRhythmStripForTest();
        CHECK (panel.getRhythmHintText().isEmpty());
    }
}

namespace
{
    juce::TextButton* headerButton (juce::Component& root, const juce::String& text)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == text)
                return b;

            if (auto* found = headerButton (*child, text))
                return found;
        }

        return nullptr;
    }
}

/*  UW-47 (ui-wiring 18): the header's undo button follows this instance's
    stack: enabled with "Undo <what>", and "Nothing to undo" once undone. */
LUTHIER_TEST (Undo, theHeaderFollowsTheStack)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.pushUndoState ("Test");

    HeaderBar header (processor);
    auto* undo = headerButton (header, "Undo");
    CHECK (undo != nullptr);

    if (undo == nullptr)
        return;

    CHECK (undo->isEnabled());
    CHECK_MSG (undo->getTooltip() == "Undo Test", "tooltip was '" + undo->getTooltip() + "'");

    undo->onClick();
    CHECK (! undo->isEnabled());
    CHECK (undo->getTooltip() == "Nothing to undo");

    // Per instance: another processor's header has nothing to undo.
    LuthierAudioProcessor other;
    HeaderBar otherHeader (other);

    if (auto* otherUndo = headerButton (otherHeader, "Undo"))
        CHECK (! otherUndo->isEnabled());
}

/*  GD-29 (gui-engine-dataflow 19): the A/B buttons show the active slot, and
    clicking one switches the processor's slot. */
LUTHIER_TEST (Editor, abButtonsHighlightTheActiveSlot)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    HeaderBar header (processor);

    auto* a = headerButton (header, "A");
    auto* b = headerButton (header, "B");
    CHECK (a != nullptr && b != nullptr);

    if (a == nullptr || b == nullptr)
        return;

    processor.setSlotBActive (true);
    header.refreshPresetDisplay();
    CHECK (b->getToggleState());
    CHECK (! a->getToggleState());

    a->onClick();
    CHECK (! processor.isSlotBActive());
    CHECK (a->getToggleState());
    CHECK (! b->getToggleState());
}

/*  GD-30 (gui-engine-dataflow 20): while armed, the control being learned
    pulses at 1 Hz - bright for half a second, dim for half a second. */
LUTHIER_TEST (MidiLearn, armedButtonAndTargetPulse)
{
    LuthierAudioProcessor processor;
    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);

    CHECK (LearnPulse::isOn (0.0));
    CHECK (! LearnPulse::isOn (600.0));
    CHECK (LearnPulse::isOn (1100.0));

    processor.getMidiLearn().startLearning (ParamIDs::ampGain);

    CHECK (knob.pollLearnPulse (100.0));
    CHECK (knob.isLearnOutlineBright());
    CHECK (knob.pollLearnPulse (600.0));
    CHECK (! knob.isLearnOutlineBright());
    CHECK (! knob.pollLearnPulse (700.0));   // no repaint inside a half-period
    CHECK (knob.pollLearnPulse (1050.0));
    CHECK (knob.isLearnOutlineBright());

    processor.getMidiLearn().cancelLearning();
    knob.pollLearnPulse (1100.0);
    CHECK (! knob.isLearnOutlineBright());
}

/*  GD-13 (gui-engine-dataflow 6.3): the buzz heatmap is stale after 500 ms
    without a change and fades out over 200 ms rather than switching off. */
LUTHIER_TEST (BuzzUi, theHeatmapFadesWhenStale)
{
    CHECK_NEAR (BuzzHeatmap::freshnessFor (0.0), 1.0f, 1.0e-6);
    CHECK_NEAR (BuzzHeatmap::freshnessFor (0.5), 1.0f, 1.0e-6);
    CHECK_NEAR (BuzzHeatmap::freshnessFor (0.6), 0.5f, 1.0e-3);
    CHECK_NEAR (BuzzHeatmap::freshnessFor (0.7), 0.0f, 1.0e-6);
    CHECK_NEAR (BuzzHeatmap::freshnessFor (5.0), 0.0f, 1.0e-6);

    LuthierAudioProcessor processor;
    BuzzHeatmap heatmap (processor);
    CHECK (heatmap.isStale());   // never changed
}

/*  GD-14 (gui-engine-dataflow 6.4): the slide bar draws at 80% while moving and
    freezes at 60% of that after 200 ms without movement. */
LUTHIER_TEST (SlideUi, theBarFreezesDimmedWhenStale)
{
    CHECK_NEAR (FretboardComponent::slideBarAlpha (1.0f, 0.0), 0.8f, 1.0e-6);
    CHECK_NEAR (FretboardComponent::slideBarAlpha (1.0f, 150.0), 0.8f, 1.0e-6);
    CHECK_NEAR (FretboardComponent::slideBarAlpha (1.0f, 250.0), 0.48f, 1.0e-6);
    CHECK_NEAR (FretboardComponent::slideBarAlpha (0.5f, 250.0), 0.24f, 1.0e-6);
}

/*  GD-31 (gui-engine-dataflow 21): the looper LED is red and pulsing at 4 Hz
    while recording or overdubbing, green while playing, off when stopped. */
LUTHIER_TEST (PracticeDrawer, theLooperLedFollowsTheState)
{
    using S = Looper::State;

    CHECK (LooperTab::ledColourFor (S::stopped, 0.0).isTransparent());
    CHECK (LooperTab::ledColourFor (S::playing, 0.0).getGreen() > LooperTab::ledColourFor (S::playing, 0.0).getRed());

    for (auto s : { S::recordingFirst, S::overdubbing })
    {
        const auto a = LooperTab::ledColourFor (s, 0.0);
        const auto b = LooperTab::ledColourFor (s, 130.0);   // half a 4 Hz period later
        const auto c = LooperTab::ledColourFor (s, 260.0);

        CHECK (a.getRed() > a.getGreen());
        CHECK (a.getAlpha() > b.getAlpha());
        CHECK (a == c);
    }

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    LooperTab tab (processor);
    tab.refresh();
    CHECK (tab.getLedColour().isTransparent());
}

/*  GD-33 (gui-engine-dataflow 23): the session tab's fill bar is recorded over
    capacity. */
LUTHIER_TEST (PracticeDrawer, theSessionFillBarIsRecordedOverCapacity)
{
    LuthierAudioProcessor processor;
    processor.setRateAndBufferSizeDetails (kSr, kBlock);   // as a host does; the tab reads getSampleRate()
    processor.prepareToPlay (kSr, kBlock);
    processor.setPracticePanelOpen (true);

    SessionTab tab (processor);
    tab.setSize (400, 300);
    tab.refresh();
    CHECK_NEAR (tab.getFillFraction(), 0.0f, 1.0e-6);

    auto& recorder = processor.getSessionRecorder();

    // The way the drawer turns it on (it sizes the ring first).
    tab.getEnableToggle().getButton().setToggleState (true, juce::dontSendNotification);
    tab.getEnableToggle().getButton().onClick();
    CHECK (recorder.isEnabled());

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);

    for (int i = 0; i < 400; ++i)   // about 4 s
    {
        juce::MidiBuffer midi;
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    tab.refresh();

    const double expected = ((double) recorder.getRecordedSamples() / kSr / 60.0)
                              / juce::jmax (1.0e-9, recorder.getCapacityMinutes());
    CHECK (tab.getFillFraction() > 0.0f);
    CHECK_NEAR (tab.getFillFraction(), (float) expected, 1.0e-3);
}

/*  GD-10 (gui-engine-dataflow 5): the next-strum arrow flashes on each stroke
    and hides when the rhythm engine has not reported for 500 ms. */
LUTHIER_TEST (EasyLayout, theNextStrumArrowFlashesAndHides)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    PatternLibrary patterns;
    const auto strums = patterns.findByKind (RhythmPattern::Kind::strum);
    CHECK (! strums.isEmpty());

    if (strums.isEmpty())
        return;

    rhythm.setPattern (patterns.getPattern (strums[0]));
    rhythm.setFreeRun (true);
    rhythm.setEnabled (true);

    NextStrumArrow arrow (processor);
    CHECK (NextStrumArrow::kRefreshHz == 60);
    arrow.tick (0.0);
    CHECK (! arrow.isShown());

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);
    bool flashed = false;
    double now = 0.0;

    for (int block = 0; block < 200 && ! flashed; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            for (int n : { 48, 52, 55 })
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);

        now += 1000.0 * kBlock / kSr;
        arrow.tick (now);
        flashed = arrow.isFlashing();
    }

    CHECK (arrow.isShown());
    CHECK_MSG (flashed, "the arrow never flashed on a stroke");

    arrow.tick (now + 100.0);
    CHECK (! arrow.isFlashing());   // a flash is brief
    CHECK (arrow.isShown());

    arrow.tick (now + 600.0);   // no block for 600 ms
    CHECK (! arrow.isShown());
}

/*  KS-11: PageDown / PageUp step the setlist; with nowhere to go the key is
    not swallowed. */
LUTHIER_TEST (Editor, pageKeysStepTheSetlist)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    Setlist list;
    SetlistEntry a, b;
    a.presetPath = "missing-a.luthierpreset";
    b.presetPath = "missing-b.luthierpreset";
    list.addEntry (a);
    list.addEntry (b);

    auto& player = f.processor.getSetlist();
    player.setSetlist (list);
    player.goTo (0);
    CHECK (player.getPosition() == 0);

    f.press (shortcutFor ("setlistNext"));
    CHECK_MSG (player.getPosition() == 1, "PageDown left the setlist at " + juce::String (player.getPosition()));

    CHECK (! f.press (shortcutFor ("setlistNext")));   // the end: not swallowed
    CHECK (player.getPosition() == 1);

    f.press (shortcutFor ("setlistPrevious"));
    CHECK (player.getPosition() == 0);
}


/*  KS-27 / IR-17 (docs/KEYBOARD_SHORTCUTS.md "In an overlay"): every overlay
    closes by Escape (covered elsewhere), by a click outside it on the scrim,
    and by its Close button. A click inside the panel does not close it. */
LUTHIER_TEST (Editor, overlaysCloseFromTheScrimAndTheirCloseButton)
{
    EditorFixture f;
    CHECK (f.editor != nullptr);

    OverlayHost* host = nullptr;

    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* h = dynamic_cast<OverlayHost*> (child))
                host = h;

            if (host == nullptr)
                find (*child);
        }
    };

    find (*f.editor);
    CHECK (host != nullptr);

    if (host == nullptr)
        return;

    auto click = [host] (juce::Point<float> p)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        host->mouseDown (juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, host, host,
                                           juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false));
    };

    // ---- the scrim --------------------------------------------------------------
    CHECK (f.press (shortcutFor ("help")));
    CHECK (host->isShowingOverlay());

    auto* panel = host->getCurrentOverlay();
    CHECK (panel != nullptr);

    if (panel != nullptr)
    {
        click (panel->getBounds().getCentre().toFloat());
        CHECK_MSG (host->isShowingOverlay(), "a click inside the panel closed it");
    }

    click ({ 2.0f, 2.0f });
    CHECK_MSG (! host->isShowingOverlay(), "a click on the scrim did not close the overlay");

    // ---- the Close button -------------------------------------------------------
    CHECK (f.press (shortcutFor ("options")));
    CHECK (host->isShowingOverlay());

    juce::TextButton* close = nullptr;

    if (auto* shown = host->getCurrentOverlay())
        for (auto* child : shown->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "Close")
                close = b;

    CHECK (close != nullptr && close->onClick != nullptr);

    if (close != nullptr && close->onClick != nullptr)
    {
        close->onClick();
        CHECK_MSG (! host->isShowingOverlay(), "the Close button did not close the overlay");
    }
}

/*  KS-19 (docs/KEYBOARD_SHORTCUTS.md "On any control"): Shift + drag is coarse,
    Ctrl/Cmd + drag ultra-fine, a plain drag in between. */
LUTHIER_TEST (Widgets, modifierDragSensitivity)
{
    LuthierAudioProcessor processor;
    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);
    knob.setSize (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Normal),
                  LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    auto& slider = knob.getSlider();

    auto dragBy = [&slider] (juce::ModifierKeys extra, float pixelsUp)
    {
        slider.setValue (slider.getMinimum() + 0.25 * (slider.getMaximum() - slider.getMinimum()),
                         juce::sendNotificationSync);
        const double before = slider.getValue();

        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const juce::Point<float> start ((float) slider.getWidth() / 2.0f, (float) slider.getHeight() / 2.0f);
        const auto mods = extra.withFlags (juce::ModifierKeys::leftButtonModifier);
        const auto now = juce::Time::getCurrentTime();

        slider.mouseDown (juce::MouseEvent (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                            &slider, &slider, now, start, now, 1, false));

        for (int step = 1; step <= 4; ++step)
        {
            const juce::Point<float> p (start.x, start.y - pixelsUp * (float) step / 4.0f);
            slider.mouseDrag (juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                &slider, &slider, now, start, now, 1, true));
        }

        slider.mouseUp (juce::MouseEvent (source, { start.x, start.y - pixelsUp }, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          &slider, &slider, now, start, now, 1, true));

        return std::abs (slider.getValue() - before);
    };

    const double normal = dragBy ({}, 20.0f);
    const double coarse = dragBy (juce::ModifierKeys::shiftModifier, 20.0f);
    const double fine = dragBy (juce::ModifierKeys::commandModifier, 20.0f);

    CHECK_MSG (normal > 0.0, "a plain drag did not move the knob");
    CHECK_MSG (coarse > normal, "Shift was not coarser: " + juce::String (coarse) + " vs " + juce::String (normal));
    CHECK_MSG (fine < normal && fine > 0.0, "Ctrl/Cmd was not finer: " + juce::String (fine) + " vs " + juce::String (normal));
}

/*  KS-26 (docs/KEYBOARD_SHORTCUTS.md "In the pedal rack"): dragging a slot onto
    another moves that pedal there, with its settings, in the parameters. */
LUTHIER_TEST (PedalRack, dragOntoAnotherSlotReorders)
{
    LuthierAudioProcessor processor;
    PedalRack rack (processor, false);
    CHECK (rack.getNumSlots() >= 2);

    auto& state = processor.getState();
    auto* type0 = state.getParameter (ParamIDs::slotType (false, 0));
    auto* type1 = state.getParameter (ParamIDs::slotType (false, 1));
    auto* mix0 = state.getParameter (ParamIDs::slotMix (false, 0));

    type0->setValueNotifyingHost (type0->convertTo0to1 (1.0f));
    type1->setValueNotifyingHost (type1->convertTo0to1 (2.0f));
    mix0->setValueNotifyingHost (0.3f);

    const float a = type0->getValue(), b = type1->getValue();

    auto* slot = rack.getSlot (0);
    CHECK (slot != nullptr && slot->onReorderRequested != nullptr);

    if (slot == nullptr || slot->onReorderRequested == nullptr)
        return;

    slot->onReorderRequested (0, 1);   // what a drag released over slot 1 calls

    CHECK_NEAR (type1->getValue(), a, 1.0e-6);
    CHECK_NEAR (type0->getValue(), b, 1.0e-6);
    CHECK_NEAR (state.getParameter (ParamIDs::slotMix (false, 1))->getValue(), 0.3f, 1.0e-6);
}
