#pragma once

/*  The shared header (build spec, "Shared header").

    Left to right: logo, guitar selector, tuning selector, preset display with
    prev/next, the file menu (Save / Save As / Open / Import / Export / Options),
    A/B compare, undo/redo, panic, and the Easy/Advanced switch.

    The output LED lives here too, in the top-left corner, as the theme requires.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "RangesUi.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** A TextButton whose label breaks at its " & " onto two lines, so RESET & STOP
    takes the width of one header button rather than two. The look and feel
    still draws the background, the colours and the state. */
class TwoLineTextButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override;
};

//==============================================================================
class HeaderBar : public juce::Component,
                  private juce::ChangeListener,
                  private juce::Timer,
                  private juce::AudioProcessorListener
{
public:
    explicit HeaderBar (LuthierAudioProcessor& processor);
    ~HeaderBar() override;

    //==========================================================================
    std::function<void (bool advanced)> onModeChanged;
    std::function<void()> onOpenHelp;

    /** The wrench (gui-integration.md 6): the Workshop tab, or the Easy overlay. */
    std::function<void()> onOpenWorkshop;
    std::function<void()> onOpenOptions;

    /** gui-integration 2: the range-lock padlock opens Options -> Ranges. */
    std::function<void()> onOpenRanges;

    /** For tests: whether the range-lock padlock is showing. */
    bool isRangePadlockShowing() const noexcept { return rangePadlock.isVisible(); }

    /** Flips Slide Mode as one undoable action - the header button and the S shortcut. */
    static void toggleSlideMode (LuthierAudioProcessor& processor);
    std::function<void()> onOpenExport;
    std::function<void()> onOpenPresetBrowser;
    std::function<void()> onSaveAs;

    /** gui-integration 19: File -> New Tune... (the TUNE tab's template picker)
        and File -> Import MIDI... (midi-export 5, into the Tune Builder). The
        editor owns the TUNE tab, so both go out to it; unset, the items are
        disabled rather than missing. */
    std::function<void()> onNewTune;
    std::function<void (const juce::File&)> onImportMidi;

    /** gui-integration 19: the header MIDI Learn button. */
    std::function<void (bool)> onMidiLearnArmChanged;

    void setAdvancedMode (bool advanced);

    /*  gui-integration 4.5: below 1000 points the Advanced toggle is disabled
        rather than merely refusing when pressed. Two separate things lock it -
        this and Live Mode - so neither sets `enabled` directly; both go through
        updateModeButtonEnablement, which was how one of them used to silently
        unlock the other. */
    void setAdvancedModeAvailable (bool available);

    /** Reflects Live Mode in the header. Called whenever it changes, including
        from the shortcut - which used to change the mode and leave the pill
        showing the old state. */
    void setLiveMode (bool live);

    /** Reflects the arm state; the editor owns it. */
    void setMidiLearnArmed (bool armed);
    bool isAdvancedMode() const noexcept { return advancedMode; }

    void refreshPresetDisplay();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    /*  AudioProcessorListener, for one thing: the guitar selector's gesture.
        A guitar the player picks - here, or in a host's generic editor - ends
        a gesture; a snapshot, a setlist entry or automation moving guitar_type
        does not. The pass that loads a picked guitar sets use_fingers from
        it (ParameterBridge::followGuitarHandOnNextLoad); the others keep the
        use_fingers they carry. Value changes arrive on any thread, including
        the audio thread, and are ignored. */
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int parameterIndex) override;

    void showFileMenu();
    void updateUndoRedoState();
    void updateModeButtonEnablement();
    void updateRangePadlock();

    LuthierAudioProcessor& processor;

    OutputLed led;

    LuthierChoice guitarSelector, tuningSelector;

    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton presetName;
    RangesUi::PadlockButton rangePadlock;
    juce::TextButton fileMenuButton { "File" };

    juce::TextButton compareA { "A" }, compareB { "B" }, copyAB { "A>B" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton panicButton { "Panic" };

    /** RESET & STOP: everything off and every setting back to default. */
    TwoLineTextButton resetStopButton;
    juce::TextButton midiLearnButton { "Learn" };
    juce::TextButton helpButton { "?" };
    juce::TextButton modeButton { "Advanced" };
    juce::TextButton liveButton { "Live" };

    /** slide-guitar.md 7: Slide Mode is a header toggle (shortcut S). */
    juce::TextButton slideButton { "Slide" };

    /*  Lit while the rhythm engine is on. Picking a genre in Easy mode switches
        it on silently, and then held chords are strummed by a pattern instead
        of ringing; this makes that visible and one click undoes it. */
    juce::TextButton rhythmButton { "Rhythm" };
    bool rhythmWasDriving = false;

public:
    juce::TextButton& getRhythmButton() noexcept { return rhythmButton; }

    /** What the 6 Hz timer does, now: undo/redo, padlock, Slide and Rhythm. */
    void refreshIndicators() { timerCallback(); }

private:
    juce::TextButton workshopButton { "Workshop" };

    bool advancedMode = false;
    bool advancedAvailable = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};

} // namespace luthier
