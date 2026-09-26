#pragma once

/*  The shared header (build spec, "Shared header").

    Left to right: logo, guitar selector, tuning selector, preset display with
    prev/next, the file menu (Save / Save As / Open / Import / Export / Options),
    A/B compare, undo/redo, panic, and the Easy/Advanced switch.

    The output LED lives here too, in the top-left corner, as the theme requires.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "RangesUi.h"
#include "NormalizationBadge.h"   // output-normalization.md 5.1
#include "Search/MagnifierButton.h"   // global-search.md 6.1 (FEAT-SEARCH)

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class HeaderBar : public juce::Component,
                  private juce::ChangeListener,
                  private juce::Timer
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

    /** midi-export 5 (MODEL-GAPS): File -> Import -> MIDI chose this file. */
    std::function<void (const juce::File&)> onImportMidi;

    /** global-search.md 6.1 (FEAT-SEARCH): the magnifier, and File -> Search. */
    std::function<void()> onOpenSearch;
    juce::Button& getSearchButton() noexcept { return searchButton; }

    /** Below this width the magnifier goes into the File menu only (6.1). */
    static constexpr int searchButtonMinWidth = 1280;

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

    /** onboarding 3 and 4 (TUNE-HELP-ONBOARDING): the header controls the tour
        and the first-week hints point at, by id. nullptr for an unknown id. */
    juce::Component* getTourTarget (const juce::String& id) noexcept
    {
        if (id == "play")     return &led;
        if (id == "preset")   return &presetName;
        if (id == "mode")     return &modeButton;
        if (id == "workshop") return &workshopButton;
        if (id == "slide")    return &slideButton;
        if (id == "options")  return &fileMenuButton;
        if (id == "help")     return &helpButton;
        return nullptr;
    }

    void paint (juce::Graphics&) override;
    void resized() override;

    /*  action-and-undo.md 1 / 9: File -> "Undo history...". The newest 20
        entries, newest first; each item's id is 1 + the undos that reach the
        state before it, and a boundary entry sits under a separator. */
    static juce::PopupMenu buildUndoHistoryMenu (const LuthierAudioProcessor& processor);
    static void applyUndoHistoryChoice (LuthierAudioProcessor& processor, int result);
    void showUndoHistory();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void showFileMenu();
    void updateUndoRedoState();
    void updateModeButtonEnablement();
    void updateRangePadlock();

    LuthierAudioProcessor& processor;

    OutputLed led;

public:
    /** output-normalization.md 5.1: the badge beside the output LED. */
    NormalizationBadge& getNormalizationBadge() noexcept { return normalizationBadge; }

private:
    NormalizationBadge normalizationBadge { processor };

    LuthierChoice guitarSelector, tuningSelector;

    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton presetName;
    RangesUi::PadlockButton rangePadlock;
    juce::TextButton fileMenuButton { "File" };

    juce::TextButton compareA { "A" }, compareB { "B" }, copyAB { "A>B" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton panicButton { "Panic" };
    juce::TextButton midiLearnButton { "Learn" };
    juce::TextButton helpButton { "?" };
    juce::TextButton modeButton { "Advanced" };
    juce::TextButton liveButton { "Live" };

    /** slide-guitar.md 7: Slide Mode is a header toggle (shortcut S). */
    juce::TextButton slideButton { "Slide" };
    juce::TextButton workshopButton { "Workshop" };
    search::MagnifierButton searchButton;   // FEAT-SEARCH

    bool advancedMode = false;
    bool advancedAvailable = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "HeaderBar" };
};

} // namespace luthier
