#pragma once

/*  Column 4's tab strip, when there are more tabs than room (FEAT-RIFFS,
    gui-integration.md 4.4).

    With RIFFS, TECHNIQUES, JAM and the rest the strip holds about fifteen
    tabs, which no longer fit a 480-point workspace at a readable size. The
    strip lays the tab buttons out itself:

      - when every tab fits at its natural width (its label plus padding), the
        tabs share the width evenly, as the strip always did;
      - otherwise the strip scrolls: a left and a right arrow step it one tab
        at a time, an overflow button (the last control) lists every tab in a
        menu with the current one ticked, and the selected tab is always
        scrolled into view. Tabs that do not fit whole are hidden, never
        clipped.

    The arrows and the menu button are ordinary focusable buttons with
    accessible names, so the strip is keyboard reachable; Ctrl+[ / Ctrl+] (the
    editor's tab steps) scroll it by selecting. The strip does not own the tab
    buttons - AdvancedPanel does - and knows nothing about what they show:
    adding a tab to AdvancedPanel::buildWorkspace's list needs no layout
    change here. The remembered tab stays remembered by name (AdvancedPanel).
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class WorkspaceTabStrip : public juce::Component
{
public:
    static constexpr int kArrowWidth = 22;
    static constexpr int kMenuWidth = 26;
    static constexpr int kMinTabWidth = 44;
    static constexpr int kTabPadding = 12;

    WorkspaceTabStrip();
    ~WorkspaceTabStrip() override;

    /** The tab buttons, in strip order. They become this strip's children. */
    void setTabs (const juce::Array<juce::Button*>& buttons);
    int getNumTabs() const noexcept { return tabs.size(); }

    /** Scrolls so the tab is whole and on screen. */
    void setSelectedIndex (int index);
    int getSelectedIndex() const noexcept { return selected; }

    /** Called when the overflow menu picks a tab (the button is also clicked). */
    std::function<void (int)> onTabChosen;

    /** Steps the first visible tab; clamped. */
    void scrollBy (int tabs);

    //==========================================================================
    bool isOverflowing() const noexcept { return overflowing; }
    int getFirstVisibleIndex() const noexcept { return firstVisible; }
    bool isTabVisible (int index) const;
    int getNaturalWidth (int index) const;

    juce::Button& getLeftArrow() noexcept   { return leftArrow; }
    juce::Button& getRightArrow() noexcept  { return rightArrow; }
    juce::Button& getMenuButton() noexcept  { return menuButton; }

    /** The overflow menu's entries (every tab's name), for tests. */
    juce::StringArray getMenuItems() const;

    /** Picks a tab as the menu does. */
    void chooseFromMenu (int index);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void layout (bool revealSelected = true);
    void showMenu();

    juce::Array<juce::Component::SafePointer<juce::Button>> tabs;
    juce::TextButton leftArrow { juce::String::fromUTF8 ("\xe2\x80\xb9") };   // single left-pointing angle
    juce::TextButton rightArrow { juce::String::fromUTF8 ("\xe2\x80\xba") };
    juce::TextButton menuButton { juce::String::fromUTF8 ("\xe2\x80\xa6") };  // ellipsis

    int selected = 0;
    int firstVisible = 0;
    bool overflowing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorkspaceTabStrip)
};

} // namespace luthier
