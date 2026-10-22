// OWNER: WP17 (interface frozen by WP1, I-6: the public section below changes only via errata)
#pragma once

// Pop-out pedalboard window (UI_BOARD_STYLES sec 3): a non-modal DocumentWindow
// bound to the same BoardModel as the docked strip. Closing or hiding it never
// changes audio. WP1 ships the interface with a minimal body; WP17 owns the content.

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{
class LuthierAudioProcessor;
}

namespace luthier::reference
{

class BoardModel;

class BoardWindow : public juce::DocumentWindow
{
public:
    // `anchor` is the editor component the window is placed next to; it may be null.
    BoardWindow (BoardModel& model, LuthierAudioProcessor& processor, juce::Component* anchor);
    ~BoardWindow() override;

    void showWindow();
    void hideWindow();
    bool isShowing() const;

    // Test hook: builds the content without a native peer so createComponentSnapshot
    // works off-screen (UI_BOARD_STYLES sec 18).
    void forceHeadlessContent();

    void closeButtonPressed() override;

    // Fired after every visibility change with the new state, so the editor's
    // toggle button and the window's close button always agree.
    std::function<void (bool showing)> onVisibilityChanged;

private:
    BoardModel& boardModel;
    LuthierAudioProcessor& audioProcessor;
    juce::Component::SafePointer<juce::Component> anchorComponent;
};

} // namespace luthier::reference
