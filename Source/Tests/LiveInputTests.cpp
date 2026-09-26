/*  SPEC-SWEEP: the live surface's input side and its setup controls
    (live-performance.md 2, 3, 5, 6, 7, 8, 9, 10; gui-integration.md 19).

    Groups: LiveInput (live actions on CCs), LiveExpression (the calibration on
    the MIDI path), LiveSnapshots (the automatable morph), LiveTapTempo,
    LivePanelUi / LiveStripUi (the setup and runtime controls) and Editor (the
    Live Mode keys).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Live/LiveInput.h"
#include "../UI/LivePanel.h"
#include "../UI/LiveStrip.h"
#include "../UI/LiveSetup.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** A processor whose live-action map writes to a temporary file, never the
        user's real config. */
    struct Rig
    {
        Rig()
        {
            processor.prepareToPlay (kSr, kBlock);
            processor.getLiveActions().setConfigFile (config.getFile());
            processor.getLiveActions().clearAll();
        }

        ~Rig() { processor.releaseResources(); }

        /** One block carrying `midi`, then the timer's live work. */
        void block (juce::MidiBuffer midi = {})
        {
            juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
            buffer.clear();
            processor.processBlock (buffer, midi);
            lastPeak = buffer.getMagnitude (0, kBlock);
            processor.getLiveActions().service();
            processor.getSnapshots().advancePending();
        }

        void cc (int number, int value)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, number, value), 0);
            block (midi);
        }

        /** Four snapshots, each with a distinct amp gain. */
        void fillSnapshots (int count = 4)
        {
            auto* gain = processor.getState().getParameter (ParamIDs::ampGain);

            for (int i = 0; i < count; ++i)
            {
                gain->setValueNotifyingHost ((float) i / (float) count);
                processor.captureSnapshot (i, "S" + juce::String (i));
            }

            processor.getSnapshots().setCrossfadeMs (0.0);
        }

        juce::TemporaryFile config { ".json" };
        LuthierAudioProcessor processor;
        float lastPeak = 0.0f;
    };

    float normalised (LuthierAudioProcessor& p, const char* id)
    {
        return p.getState().getParameter (id)->getValue();
    }
}

//==============================================================================
/*  live-performance 2 (LP-11): a CC can be assigned to next / previous /
    snapshot-by-value, through learning, and the CC is consumed. */
LUTHIER_TEST (LiveInput, learnedCcsDriveSnapshotStepAndByValue)
{
    Rig rig;
    auto& p = rig.processor;
    auto& map = p.getLiveActions();

    rig.fillSnapshots();

    // A parameter mapping on the same CC is dropped when a live action takes it.
    p.getMidiLearn().addMapping (ParamIDs::ampGain, 20);

    map.beginLearning (LiveAction::snapshotNext);
    rig.cc (20, 127);
    CHECK (map.getLearningAction() < 0);
    CHECK (map.getCcFor (LiveAction::snapshotNext) == 20);
    CHECK (p.getMidiLearn().getCcForParameter (ParamIDs::ampGain) < 0);

    map.assign (LiveAction::snapshotPrevious, 21);
    map.assign (LiveAction::snapshotByValue, 22);

    p.recallSnapshot (0);
    rig.block();
    CHECK (p.getSnapshots().getCurrentSnapshot() == 0);

    // Press and release: one step.
    rig.cc (20, 127);
    rig.cc (20, 0);
    CHECK_MSG (p.getSnapshots().getCurrentSnapshot() == 1,
               "next landed on " + juce::String (p.getSnapshots().getCurrentSnapshot()));

    rig.cc (21, 127);
    rig.cc (21, 0);
    CHECK (p.getSnapshots().getCurrentSnapshot() == 0);

    rig.cc (22, 3);
    CHECK (p.getSnapshots().getCurrentSnapshot() == 3);

    // Consumed: the gain did not jump with the footswitch.
    CHECK_NEAR (normalised (p, ParamIDs::ampGain), 0.75f, 0.01f);
}

/*  LP-29 / LP-37: kill is momentary on the CC and engages inside the block;
    panic silences a ringing chord. */
LUTHIER_TEST (LiveInput, killAndPanicRideOnCcs)
{
    Rig rig;
    auto& p = rig.processor;
    auto& map = p.getLiveActions();

    map.assign (LiveAction::killSwitch, 30);
    map.assign (LiveAction::panic, 31);

    rig.cc (30, 127);
    CHECK (p.getKillSwitch().isActive());
    rig.cc (30, 0);
    CHECK (! p.getKillSwitch().isActive());

    juce::MidiBuffer chord;
    for (int note : { 40, 47, 52, 56 })
        chord.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);

    rig.block (chord);

    float ringing = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        rig.block();
        ringing = juce::jmax (ringing, rig.lastPeak);
    }

    CHECK_MSG (ringing > 1.0e-4f, "the chord never sounded");

    rig.cc (31, 127);

    float after = 1.0f;
    for (int b = 0; b < 40; ++b)
    {
        rig.block();
        after = rig.lastPeak;
    }

    CHECK_MSG (after < ringing * 0.05f,
               "panic on a CC left " + juce::String (after) + " of " + juce::String (ringing));
}

/*  LP-26: tap tempo on a footswitch. The tap is stamped on arrival. */
LUTHIER_TEST (LiveInput, aFootswitchTapsTheTempo)
{
    Rig rig;
    auto& p = rig.processor;

    p.getLiveActions().assign (LiveAction::tapTempo, 40);

    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            juce::Thread::sleep (400);   // 150 bpm

        rig.cc (40, 127);
        rig.cc (40, 0);
    }

    CHECK (p.getTapTempo().hasTempo());
    CHECK_MSG (std::abs (p.getTapTempo().getTappedBpm() - 150.0) < 8.0,
               "tapped " + juce::String (p.getTapTempo().getTappedBpm()));
}

/*  LP-1 / LP-21: every live action has a key and a CC route. */
LUTHIER_TEST (LiveInput, everyLiveActionHasAKeyAndACcRoute)
{
    LiveActionMap map;
    map.setConfigFile ({});

    int next = 0, previous = 0, panics = 0, setNext = 0, setPrevious = 0, recalled = -1;
    double tapped = -1.0;

    map.handlers.nextSnapshot     = [&] { ++next; };
    map.handlers.previousSnapshot = [&] { ++previous; };
    map.handlers.panic            = [&] { ++panics; };
    map.handlers.setlistNext      = [&] { ++setNext; };
    map.handlers.setlistPrevious  = [&] { ++setPrevious; };
    map.handlers.recallSnapshot   = [&] (int i) { recalled = i; };
    map.handlers.tapAt            = [&] (double t) { tapped = t; };

    KillSwitch kill;
    map.setKillSwitch (&kill);

    for (int i = 0; i < LiveActionMap::kNumActions; ++i)
    {
        map.assign ((LiveAction) i, 50 + i);
        CHECK (map.getActionForCc (50 + i) == i);
        CHECK (map.wants (50 + i));
        CHECK (map.handleController (50 + i, 127));
    }

    map.service();

    CHECK (next == 1 && previous == 1 && panics == 1 && setNext == 1 && setPrevious == 1);
    CHECK (recalled == 127);
    CHECK (tapped > 0.0);
    CHECK (kill.isActive());

    // An unassigned CC is not the live surface's.
    CHECK (! map.wants (10));
    CHECK (! map.handleController (10, 127));

    // The keyboard side of every action (accessibility shortcut table).
    for (const char* id : { "nextItem", "previousItem", "tapTempo", "killSwitch", "panic",
                            "setlistNext", "setlistPrevious" })
        CHECK_MSG (AccessibilitySettings::get().findShortcut (id) != nullptr,
                   juce::String ("no key for ") + id);
}

LUTHIER_TEST (LiveInput, assignmentsRoundTripThroughTheirConfigFile)
{
    juce::TemporaryFile file (".json");

    {
        LiveActionMap map;
        map.setConfigFile (file.getFile());
        map.assign (LiveAction::panic, 9);
        map.assign (LiveAction::setlistNext, 81);
        CHECK (map.save());
    }

    LiveActionMap restored;
    restored.setConfigFile (file.getFile());
    CHECK (restored.load());
    CHECK (restored.getCcFor (LiveAction::panic) == 9);
    CHECK (restored.getCcFor (LiveAction::setlistNext) == 81);
    CHECK (restored.getCcFor (LiveAction::tapTempo) < 0);

    // One CC per action: a re-assignment moves it.
    restored.assign (LiveAction::panic, 10);
    CHECK (restored.getActionForCc (9) < 0);
    CHECK (restored.getActionForCc (10) == (int) LiveAction::panic);
}

//==============================================================================
/*  live-performance 8 (LP-34): a calibrated pedal reaches the full range
    downstream - MIDI Learn sees 0..127, not the pedal's 12..118. */
LUTHIER_TEST (LiveExpression, aCalibratedPedalReachesFullRangeDownstream)
{
    Rig rig;
    auto& p = rig.processor;

    const auto savedCalibrations = p.getExpression().toVar();

    ExpressionCalibration c;
    c.ccNumber = 11;
    c.rawMinimum = 12;
    c.rawMaximum = 118;
    c.heelDeadZone = 0.0;
    c.toeDeadZone = 0.0;
    p.getExpression().set (c);
    p.getExpressionInput().syncWith (p.getExpression());

    CHECK (p.getExpressionInput().isCalibrated (11));
    CHECK (p.getExpressionInput().mapForTest (11, 118) == 127);
    CHECK (p.getExpressionInput().mapForTest (11, 12) == 0);

    p.getMidiLearn().addMapping (ParamIDs::ampGain, 11);

    rig.cc (11, 118);
    p.getParameterBridge().applyAllNow();
    CHECK_MSG (normalised (p, ParamIDs::ampGain) > 0.99f,
               "toe reached " + juce::String (normalised (p, ParamIDs::ampGain)));

    rig.cc (11, 12);
    CHECK_MSG (normalised (p, ParamIDs::ampGain) < 0.01f,
               "heel reached " + juce::String (normalised (p, ParamIDs::ampGain)));

    // An uncalibrated CC passes through untouched.
    CHECK (p.getExpressionInput().mapForTest (12, 64) == 64);

    p.getExpression().fromVar (savedCalibrations);
}

/*  LP-33: the wizard hears the pedal through the processor. */
LUTHIER_TEST (LiveExpression, theWizardHearsThePedalThroughTheProcessor)
{
    Rig rig;
    auto& p = rig.processor;
    auto& set = p.getExpression();

    const auto savedCalibrations = set.toVar();
    using Stage = ExpressionCalibrationSet::WizardStage;

    set.beginCalibration (7);
    p.getExpressionInput().feedWizard (set);   // arms the audio side

    rig.cc (7, 9);
    rig.cc (7, 5);
    rig.cc (7, 6);
    CHECK (p.getExpressionInput().feedWizard (set));
    CHECK (set.confirmStage() == Stage::toe);

    p.getExpressionInput().feedWizard (set);
    rig.cc (7, 110);
    rig.cc (7, 121);
    CHECK (p.getExpressionInput().feedWizard (set));
    CHECK (set.confirmStage() == Stage::done);

    const auto cal = set.get (7);
    CHECK_MSG (cal.rawMinimum == 5 && cal.rawMaximum == 121,
               "calibrated " + juce::String (cal.rawMinimum) + ".." + juce::String (cal.rawMaximum));

    set.fromVar (savedCalibrations);
}

//==============================================================================
/*  live-performance 3 (LP-16): the morph is a parameter, and a modulation
    source on it sweeps the morph. */
LUTHIER_TEST (LiveSnapshots, aModSourceOnTheMorphParameterSweepsTheMorph)
{
    Rig rig;
    auto& p = rig.processor;
    auto& bank = p.getSnapshots();

    rig.fillSnapshots (2);    // amp gain 0 in slot 0, 0.5 in slot 1

    bank.setMorphSlots (0, 1);
    bank.setMorphEnabled (true);

    // The parameter itself.
    p.getState().getParameter (ParamIDs::snapshotMorph)->setValueNotifyingHost (0.5f);
    p.updateSnapshotMorph();
    CHECK_NEAR (bank.getMorphPosition(), 0.5, 1.0e-3);
    CHECK_NEAR (normalised (p, ParamIDs::ampGain), 0.25f, 0.01f);

    // A macro routed onto it (the same path an LFO or a pedal takes).
    p.getState().getParameter (ParamIDs::snapshotMorph)->setValueNotifyingHost (0.0f);

    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = ParamIDs::snapshotMorph;
    route.depth = 1.0f;
    route.enabled = true;
    CHECK (p.getModMatrix().addRoute (route));

    p.getModMatrix().setMacroValue (0, 1.0);
    ModBlockContext context;
    for (int b = 0; b < 64; ++b)
        p.getModMatrix().processBlock (kBlock, context);

    p.updateSnapshotMorph();
    CHECK_MSG (bank.getMorphPosition() > 0.95,
               "the modulated morph sat at " + juce::String (bank.getMorphPosition()));
    CHECK_NEAR (normalised (p, ParamIDs::ampGain), 0.5f, 0.02f);

    // The morph never captures its own position.
    CHECK (p.captureSnapshot (5));
    CHECK (! bank.getSnapshot (5).parameters.getDynamicObject()->hasProperty (ParamIDs::snapshotMorph));
}

//==============================================================================
/*  live-performance 5 (LP-25): a tap with the host stopped reaches the engine
    (rhythm, synced delays and LFOs all read the engine's tempo). */
LUTHIER_TEST (LiveTapTempo, aTapDrivesTheEngineWhileTheHostIsStopped)
{
    Rig rig;
    auto& p = rig.processor;

    for (int i = 0; i < 5; ++i)
        p.tapTempoAt (100.0 + i * (60.0 / 90.0));

    rig.block();

    CHECK_NEAR (p.getEffectiveTempo(), 90.0, 0.5);
    CHECK_NEAR (p.getEngine().getTempoBpm(), 90.0, 0.5);
}

//==============================================================================
/*  LP-18: the exclusions menu toggles a parameter out of the morph. */
LUTHIER_TEST (LivePanelUi, excludingAParameterHoldsItAtA)
{
    Rig rig;
    auto& p = rig.processor;

    rig.fillSnapshots (2);
    p.getSnapshots().setMorphSlots (0, 1);
    p.getSnapshots().setMorphEnabled (true);

    LivePanel panel (p);
    panel.setSize (420, 900);

    auto& morph = panel.getMorphSetup();
    const int gainIndex = p.getState().getParameter (ParamIDs::ampGain)->getParameterIndex();

    bool found = false;
    for (juce::PopupMenu::MenuItemIterator it (morph.buildExclusionMenu(), true); it.next();)
        if (it.getItem().itemID == gainIndex + 1)
            found = true;

    CHECK_MSG (found, "amp gain is not in the exclusions menu");

    morph.applyExclusionMenuResult (gainIndex + 1);
    CHECK (p.getSnapshots().isParameterExcludedFromMorph (ParamIDs::ampGain));

    p.getState().getParameter (ParamIDs::snapshotMorph)->setValueNotifyingHost (1.0f);
    p.updateSnapshotMorph();
    CHECK_NEAR (normalised (p, ParamIDs::ampGain), 0.0f, 0.01f);

    morph.applyExclusionMenuResult (gainIndex + 1);
    CHECK (! p.getSnapshots().isParameterExcludedFromMorph (ParamIDs::ampGain));

    // The morph position control is the parameter's.
    CHECK (morph.getPositionControl().getLearnParameterId() == ParamIDs::snapshotMorph);
}

/*  LP-19: the Bezier handles appear for the Bezier curve and set its points. */
LUTHIER_TEST (LivePanelUi, bezierHandlesSetTheCurve)
{
    Rig rig;
    auto& p = rig.processor;

    LivePanel panel (p);
    panel.setSize (420, 900);
    auto& morph = panel.getMorphSetup();

    p.getSnapshots().setMorphEnabled (true);
    p.getSnapshots().setMorphCurve (MorphCurve::bezier);
    panel.refresh();

    // The timer-driven refresh is what shows them; drive it directly.
    morph.getBezierSlider (0).setValue (0.4);
    morph.getBezierSlider (3).setValue (0.6);

    CHECK_NEAR (p.getSnapshots().getBezierControlPoint (0), 0.4, 1.0e-6);
    CHECK_NEAR (p.getSnapshots().getBezierControlPoint (3), 0.6, 1.0e-6);
    CHECK_NEAR (applyMorphCurve (0.5, MorphCurve::bezier, 0.4, p.getSnapshots().getBezierControlPoint (1),
                                 p.getSnapshots().getBezierControlPoint (2), 0.6),
                applyMorphCurve (0.5, MorphCurve::bezier, 0.4, 0.1, 0.75, 0.6), 1.0e-9);
}

/*  LP-31 / GI-119: the monitor's level, pan and EQ are on the LIVE tab. */
LUTHIER_TEST (LivePanelUi, monitorPanAndEqShapeTheMonitorOnly)
{
    Rig rig;
    auto& p = rig.processor;

    LivePanel panel (p);
    panel.setSize (420, 900);
    auto& monitor = panel.getMonitorSetup();

    monitor.getControl (MonitorSetupPanel::level).setValue (-6.0);
    monitor.getControl (MonitorSetupPanel::pan).setValue (-0.5);
    monitor.getControl (MonitorSetupPanel::low).setValue (3.0);
    monitor.getControl (MonitorSetupPanel::mid).setValue (-2.0);
    monitor.getControl (MonitorSetupPanel::high).setValue (4.5);

    auto& mix = p.getMonitorMix();
    CHECK_NEAR (mix.getLevelDb(), -6.0, 0.05);
    CHECK_NEAR (mix.getPan(), -0.5, 0.01);
    CHECK_NEAR (mix.getEqLowDb(), 3.0, 0.05);
    CHECK_NEAR (mix.getEqMidDb(), -2.0, 0.05);
    CHECK_NEAR (mix.getEqHighDb(), 4.5, 0.05);

    for (int i = 0; i < MonitorSetupPanel::numControls; ++i)
        CHECK (monitor.getControl ((MonitorSetupPanel::Control) i).isShowing()
               || monitor.getControl ((MonitorSetupPanel::Control) i).isVisible());
}

/*  LP-11 on the LIVE tab and the Live strip: the CC menu lists every action and
    arms learning. */
LUTHIER_TEST (LiveStripUi, theCcButtonArmsLearningForEachAction)
{
    Rig rig;
    auto& p = rig.processor;

    LiveActionButton button (p, "CC");

    int items = 0;
    for (juce::PopupMenu::MenuItemIterator it (button.buildMenu()); it.next();)
        if (it.getItem().itemID >= 1 && it.getItem().itemID <= LiveActionMap::kNumActions)
            ++items;

    CHECK (items == LiveActionMap::kNumActions);

    button.applyMenuResult (1 + (int) LiveAction::panic);
    CHECK (p.getLiveActions().getLearningAction() == (int) LiveAction::panic);
    CHECK (button.getButtonText() == "LEARN...");

    rig.cc (64 + 1, 127);
    CHECK (p.getLiveActions().getCcFor (LiveAction::panic) == 65);

    button.applyMenuResult (100 + (int) LiveAction::panic);
    CHECK (p.getLiveActions().getCcFor (LiveAction::panic) < 0);
}

/*  LP-39 / GI-75: every Live strip target is 44 px, and the morph knob is
    the automatable parameter's. */
LUTHIER_TEST (LiveStripUi, everyTargetIsTouchSized)
{
    Rig rig;
    LiveStrip strip (rig.processor);
    strip.setSize (1400, LiveStrip::preferredHeight);

    for (auto* child : strip.getChildren())
        if (child->isVisible() && dynamic_cast<juce::Label*> (child) == nullptr)
            CHECK_MSG (child->getHeight() >= LiveStrip::kTouchTargetHeight,
                       child->getName() + " / " + juce::String (typeid (*child).name())
                         + " is " + juce::String (child->getHeight()) + " px");

    // The strip's morph knob writes the parameter.
    juce::Slider* morph = nullptr;
    for (auto* child : strip.getChildren())
        if (auto* s = dynamic_cast<juce::Slider*> (child); s != nullptr && s->getMaximum() == 1.0)
            morph = s;

    CHECK (morph != nullptr);

    if (morph != nullptr)
    {
        morph->setValue (0.7, juce::sendNotificationSync);
        CHECK_NEAR (normalised (rig.processor, ParamIDs::snapshotMorph), 0.7f, 0.01f);
    }
}

//==============================================================================
/*  LP-12: [ ] step and digits recall snapshots in Live Mode. */
LUTHIER_TEST (Editor, bracketsAndDigitsRecallSnapshotsInLiveMode)
{
    Rig rig;
    auto& p = rig.processor;

    rig.fillSnapshots (12);
    p.setLiveMode (true);
    p.recallSnapshot (0);
    p.getSnapshots().advancePending();

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    auto current = [&p] { p.getSnapshots().advancePending(); return p.getSnapshots().getCurrentSnapshot(); };

    CHECK (editor->keyPressed (juce::KeyPress (']', 0, ']')));
    CHECK (current() == 1);

    CHECK (editor->keyPressed (juce::KeyPress ('[', 0, '[')));
    CHECK (current() == 0);

    CHECK (editor->keyPressed (juce::KeyPress ('3', 0, '3')));
    CHECK (current() == 2);

    CHECK (editor->keyPressed (juce::KeyPress ('3', juce::ModifierKeys::shiftModifier, '3')));
    CHECK_MSG (current() == 11, "shift+3 landed on " + juce::String (current()));

    p.setLiveMode (false);
}

//==============================================================================
/*  GI-4: the snapshot colour tag is on the LIVE tab, not only behind the
    strip's right-click. */
LUTHIER_TEST (Live, theSnapshotColourIsReachableWithoutRightClick)
{
    Rig rig;
    auto& p = rig.processor;
    CHECK (p.captureSnapshot (0, "Verse"));

    LivePanel panel (p);
    panel.setSize (420, 900);
    panel.refresh();

    CHECK (panel.getColourButton().isVisible());
    CHECK (panel.getColourButton().isEnabled());

    int items = 0;
    for (juce::PopupMenu::MenuItemIterator it (panel.buildColourMenu()); it.next();)
        if (it.getItem().itemID > 0)
            ++items;

    CHECK (items == Snapshot::kNumColourTags);

    panel.applyColourMenuResult (1 + 7);
    CHECK (p.getSnapshots().getSnapshot (0).colourTag == 7);
}

/*  LP-8: a bypass (a discrete parameter) flips at the crossfade midpoint, not
    at either end. */
LUTHIER_TEST (LiveSnapshots, bypassFlipsAtTheMidpoint)
{
    Rig rig;
    auto& p = rig.processor;
    auto& bank = p.getSnapshots();

    const auto bypassId = ParamIDs::slotBypass (true, 0);
    auto* bypass = p.getState().getParameter (bypassId);
    CHECK (bypass != nullptr);

    if (bypass == nullptr)
        return;

    bypass->setValueNotifyingHost (0.0f);
    CHECK (p.captureSnapshot (0, "On"));
    bypass->setValueNotifyingHost (1.0f);
    CHECK (p.captureSnapshot (1, "Bypassed"));

    bank.setCrossfadeMs (0.0);
    CHECK (p.recallSnapshot (0));
    CHECK (bypass->getValue() < 0.5f);

    bank.setCrossfadeMs (100.0);
    CHECK (p.recallSnapshot (1));

    bank.advance (0.045);
    CHECK_MSG (bypass->getValue() < 0.5f, "the bypass flipped before the midpoint");

    bank.advance (0.010);
    CHECK_MSG (bypass->getValue() > 0.5f, "the bypass had not flipped past the midpoint");

    bank.advance (0.1);
    CHECK (! bank.isRecalling());
}
