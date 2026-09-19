#pragma once

/*  The shared header (build spec, "Shared header").

    Left to right: logo, guitar selector, tuning selector, preset display with
    prev/next, the file menu (Save / Save As / Open / Import / Export / Options),
    A/B compare, undo/redo, panic, and the Easy/Advanced switch.

    The output LED lives here too, in the top-left corner, as the theme requires.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

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
    std::function<void()> onOpenOptions;
    std::function<void()> onOpenExport;
    std::function<void()> onOpenPresetBrowser;
    std::function<void()> onSaveAs;

    void setAdvancedMode (bool advanced);
    bool isAdvancedMode() const noexcept { return advancedMode; }

    void refreshPresetDisplay();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void showFileMenu();
    void updateUndoRedoState();

    LuthierAudioProcessor& processor;

    OutputLed led;

    LuthierChoice guitarSelector, tuningSelector;

    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton presetName;
    juce::TextButton fileMenuButton { "File" };

    juce::TextButton compareA { "A" }, compareB { "B" }, copyAB { "A>B" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton panicButton { "Panic" };
    juce::TextButton helpButton { "?" };
    juce::TextButton modeButton { "Advanced" };
    juce::TextButton liveButton { "Live" };

    bool advancedMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};

} // namespace luthier
