/*  The string roll (StringRoll.h): the lanes mirror the capture, clicking a
    lane plucks the string, and reduced motion slows the refresh. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/StringRoll.h"
#include "../UI/NotationPanel.h"
#include "../UI/UiPreferences.h"
#include "../Accessibility/Accessibility.h"
#include "../Routing/MidiOutRouter.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    CaptureClock stoppedAt (juce::int64 blockStart)
    {
        CaptureClock clock;
        clock.blockStartSample = blockStart;
        clock.sampleRate = kSr;
        clock.transportPlaying = false;
        return clock;
    }

    /** One block of string activity into the capture, as the processor feeds it. */
    void report (PerformanceCapture& capture, juce::int64 blockStart,
                 std::initializer_list<StringActivityEvent> events)
    {
        StringActivityQueue activity;

        for (const auto& e : events)
            activity.push (e);

        capture.beginBlock (stoppedAt (blockStart));
        capture.captureStringActivity (activity);
    }

    /** A real MouseEvent aimed at a component that has no peer (EditorTests
        does the same for the guitar illustration). */
    juce::MouseEvent eventAt (juce::Component& target, juce::Point<float> p, juce::ModifierKeys mods)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();

        return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(),
                                 p, juce::Time::getCurrentTime(), 1, false);
    }

    /** Runs one silent block, so preview MIDI reaches the engine, and returns
        the activity it produced. */
    std::vector<StringActivityEvent> runBlock (LuthierAudioProcessor& processor)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        std::vector<StringActivityEvent> out;
        const auto& activity = processor.getEngine().getStringActivity();

        for (int i = 0; i < activity.size(); ++i)
            out.push_back (activity[i]);

        return out;
    }

    bool hasNoteOn (const std::vector<StringActivityEvent>& events, int stringIndex, bool on)
    {
        for (const auto& e : events)
            if (e.stringIndex == stringIndex && e.isNoteOn == on)
                return true;

        return false;
    }
}

//==============================================================================
/*  Notes pushed through the capture, drained, show up as lane bars; a note
    still sounding counts too, and the window is free play's eight seconds. */
LUTHIER_TEST (StringRoll, lanesMirrorTheCapture)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    StringRollComponent roll (processor);
    roll.setSize (400, 140);

    CHECK (roll.getNumLanes() == processor.getEngine().getNumStrings());
    CHECK (roll.isShowingEmptyHint());

    auto& capture = processor.getPerformanceCapture();
    capture.setState (CaptureState::rolling);
    processor.drainPerformanceCapture();   // sets the capture's tuning before any note

    report (capture, 0,     { { 10, 0, 64, 0.9f, true }, { 12, 2, 55, 0.6f, true }, { 14, 4, 45, 0.8f, true } });
    report (capture, 24000, { { 0, 0, 64, 0.0f, false }, { 0, 2, 55, 0.0f, false } });   // string 4 still rings

    processor.drainPerformanceCapture();
    roll.refresh();

    CHECK_MSG (roll.getVisibleNoteCount() == 3, "visible notes: " + juce::String (roll.getVisibleNoteCount()));
    CHECK (! roll.isShowingEmptyHint());
    CHECK_NEAR (roll.getWindowSeconds(), 8.0, 1.0e-9);

    // A note that ended long before the window is not drawn; one that ended
    // inside it still is.
    report (capture, (juce::int64) (20.0 * kSr), { { 0, 1, 59, 0.7f, true } });
    report (capture, (juce::int64) (21.0 * kSr), { { 0, 1, 59, 0.0f, false } });
    processor.drainPerformanceCapture();
    roll.refresh();

    CHECK_MSG (roll.getVisibleNoteCount() == 2, "after 21 s, visible notes: " + juce::String (roll.getVisibleNoteCount()));

    // Capture off: the empty hint, not stale bars.
    capture.setState (CaptureState::off);
    capture.clearTake();
    roll.refresh();
    CHECK (roll.isShowingEmptyHint());
}

//==============================================================================
/*  The other direction: a click on lane 2 plucks string 2 (guitar-controller
    mode honours the preview's channel), the tooltip names the string and
    fret, and Enter does what the click does. */
LUTHIER_TEST (StringRoll, clickingALanePlucksThatString)
{
    LuthierAudioProcessor processor;

    if (auto* mode = processor.getState().getParameter (ParamIDs::playingMode))
        mode->setValueNotifyingHost (1.0f);   // Guitar Controller: channel = string

    if (auto* humanise = processor.getState().getParameter (ParamIDs::macroHumanize))
        humanise->setValueNotifyingHost (0.0f);   // a deterministic velocity

    processor.prepareToPlay (kSr, kBlock);
    runBlock (processor);   // lets the mode change reach the interpreter

    StringRollComponent roll (processor);
    roll.setSize (400, 140);

    auto& lane = roll.getLane (2);
    const auto bottomOfLane = juce::Point<float> ((float) lane.getWidth() * 0.5f, (float) lane.getHeight() - 1.0f);

    lane.mouseMove (eventAt (lane, bottomOfLane, juce::ModifierKeys()));
    const auto tip = dynamic_cast<juce::TooltipClient*> (&lane) != nullptr
                       ? dynamic_cast<juce::TooltipClient*> (&lane)->getTooltip() : juce::String();
    CHECK_MSG (tip.startsWith ("String 3 (") && tip.contains ("fret 0") && tip.contains ("click to pluck"),
               "tooltip: " + tip);

    lane.mouseDown (eventAt (lane, bottomOfLane, juce::ModifierKeys::leftButtonModifier));
    auto events = runBlock (processor);
    CHECK_MSG (hasNoteOn (events, 2, true), "no note-on on string 2 after clicking its lane");

    for (const auto& e : events)
        if (e.stringIndex == 2 && e.isNoteOn)
            CHECK_MSG (e.velocity > 0.2f && e.velocity <= 1.0f, "velocity " + juce::String (e.velocity));

    lane.mouseUp (eventAt (lane, bottomOfLane, juce::ModifierKeys()));
    events = runBlock (processor);
    CHECK_MSG (hasNoteOn (events, 2, false), "no note-off on string 2 after releasing the mouse");

    // Keyboard: Enter plucks the string the lane is for.
    CHECK (lane.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    events = runBlock (processor);
    CHECK_MSG (hasNoteOn (events, 2, true), "Enter on lane 2 did not pluck string 2");
    lane.keyStateChanged (false);

    // Every lane is a named target for a screen reader.
    for (int s = 0; s < roll.getNumLanes(); ++s)
    {
        auto& l = roll.getLane (s);
        CHECK_MSG (l.getTitle().startsWith ("String " + juce::String (s + 1)), "lane title: " + l.getTitle());
        CHECK (l.getWantsKeyboardFocus());
    }
}

//==============================================================================
/*  accessibility 5: reduced motion drops the refresh to 10 Hz. */
LUTHIER_TEST (StringRoll, reducedMotionSlowsTheTimer)
{
    LuthierAudioProcessor processor;
    auto& settings = AccessibilitySettings::get();
    const bool was = settings.isReducedMotion();

    // A component starts invisible, and a roll that cannot be seen runs no
    // timer (aHiddenRollStopsItsTimer); shown, as its host shows it.
    settings.setReducedMotion (false);
    StringRollComponent fast (processor);
    fast.setVisible (true);
    CHECK_MSG (fast.getRefreshHz() == 30, "refresh " + juce::String (fast.getRefreshHz()) + " Hz");

    settings.setReducedMotion (true);
    StringRollComponent slow (processor);
    slow.setVisible (true);
    CHECK_MSG (slow.getRefreshHz() == 10, "reduced-motion refresh " + juce::String (slow.getRefreshHz()) + " Hz");

    settings.setReducedMotion (was);
}

//==============================================================================
/*  The NOTATION tab hosts it above the live tab, with a collapse the panel
    remembers. */
LUTHIER_TEST (StringRoll, theNotationTabHostsItAndRemembersTheCollapse)
{
    LuthierAudioProcessor processor;
    const bool saved = UiPreferences::get().getBool ("notation.showStringRoll", true);

    UiPreferences::get().setBool ("notation.showStringRoll", true);

    {
        NotationPanel panel (processor);
        panel.setSize (320, panel.getPreferredHeight());

        CHECK (panel.isStringRollShown());
        CHECK (panel.getStringRoll().isVisible());
        CHECK_MSG (panel.getStringRoll().getBottom() <= panel.getHeight(), "the roll hangs below the panel");

        panel.getShowRollButton().setToggleState (false, juce::dontSendNotification);
        panel.getShowRollButton().onClick();
        CHECK (! panel.getStringRoll().isVisible());
    }

    NotationPanel again (processor);
    CHECK_MSG (! again.isStringRollShown(), "the collapse was not remembered");

    UiPreferences::get().setBool ("notation.showStringRoll", saved);
}

//==============================================================================
/*  Two rolls live in the editor; a hidden one has nothing to show, so its
    timer stops - for its own visibility and for a parent's - and starts again
    when it can be seen. */
LUTHIER_TEST (StringRoll, aHiddenRollStopsItsTimer)
{
    LuthierAudioProcessor processor;
    juce::Component parent;
    StringRollComponent roll (processor);

    // A component starts invisible: no timer until it is shown.
    CHECK_MSG (! roll.isRefreshRunning(), "a roll nobody has shown yet runs its timer");

    roll.setVisible (true);
    CHECK (roll.isRefreshRunning());

    roll.setVisible (false);
    CHECK_MSG (! roll.isRefreshRunning(), "a hidden roll keeps its timer running");

    roll.setVisible (true);
    CHECK (roll.isRefreshRunning());
    CHECK_MSG (roll.getRefreshHz() == 30 || roll.getRefreshHz() == 10, "the restarted timer lost its rate");

    parent.addAndMakeVisible (roll);
    CHECK_MSG (! roll.isRefreshRunning(), "a roll under a parent nobody has shown runs its timer");

    parent.setVisible (true);
    CHECK (roll.isRefreshRunning());

    parent.setVisible (false);
    CHECK_MSG (! roll.isRefreshRunning(), "a roll under a hidden parent keeps its timer running");

    parent.setVisible (true);
    CHECK (roll.isRefreshRunning());

    parent.removeChildComponent (&roll);
}
