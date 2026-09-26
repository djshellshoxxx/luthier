#pragma once

/*  The HELP workspace tab (gui-integration.md 4.4; include.md "Help File";
    accessibility.md 2 and 7).

      TOPICS - the manual, one topic at a time (HelpContent.h), chosen from the
          list or pinned from outside. showTopicFor() is the entry point for
          "Docs" on a panel's menu (16), a panel's ? (20) and F1 on the focused
          control (accessibility 2): each passes the name of the panel it is
          in, and the topic whose alias that is comes up.

      SHORTCUTS - 4.4's "live keyboard-shortcut cheat sheet": every binding in
          AccessibilitySettings as it is now, grouped, searchable (accessibility
          2), with a rebound key marked in words as well as colour (0.2).
          Rebinding stays where accessibility 9 puts it, in Options ->
          ACCESSIBILITY; Rebind... and a double-click on a row go there.

      FOOTER - include.md's debug button and its three links. The version is on
          the header row, so it is on screen whichever topic is showing.

    The same component is the Help overlay's content in Easy mode, so the tab
    and the overlay are one surface in two places rather than two surfaces
    that drift. Neither host is known to it: the debug window and the rebind
    table are overlays, which only the editor can show, so both requests come
    out through callbacks - the same arrangement as OptionsPanel's
    onShowDebugWindow.

    760 points and wider (the overlay, or a wide column 4) the cheat sheet sits
    beside the topic; narrower, below it. Column 4's 480-point minimum is the
    narrow case.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "HelpContent.h"

#include <vector>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class HelpTab : public juce::Component,
                private juce::ChangeListener
{
public:
    explicit HelpTab (LuthierAudioProcessor& processor);
    ~HelpTab() override;

    //==========================================================================
    /** Pins Help to the topic a panel, tab, section or topic name belongs to
        (HelpContent::findTopic). Returns false, and leaves the topic showing as
        it was, when nothing answers to the name - so a panel without docs
        opens Help where the user left it rather than on the wrong page. */
    bool showTopicFor (const juce::String& panelOrTopicName);

    /** Shows a topic by its index in HelpContent. Out-of-range is clamped. */
    void showTopic (int index);

    int getShownTopic() const noexcept { return shownTopic; }
    juce::String getShownTopicId() const;

    /** The body as shown: the topic's text with every {key:...} resolved and
        the live parts (folders, version, licence state) appended. */
    juce::String getBodyText() const { return body.getText(); }

    //==========================================================================
    /** Re-reads the registry into the cheat sheet, and the body's keys with it.
        Called on every AccessibilitySettings change and whenever the tab is
        shown; public for a caller that changed the registry without a change
        message. */
    void refreshShortcuts();

    /** The cheat sheet as it stands, filter applied, one row per binding. */
    const std::vector<ShortcutRow>& getShownShortcuts() const noexcept { return shownShortcuts; }

    /** The same, as printable text. */
    juce::String getShortcutText() const;

    /** "Luthier 1.0.0", as the header row shows it. */
    static juce::String getVersionText();

    //==========================================================================
    /** include.md: the debug button. The editor shows DebugPanel. */
    std::function<void()> onOpenDebug;

    /** Rebind...: the editor opens Options on the shortcut table. */
    std::function<void()> onOpenShortcutTable;

    /** onboarding 2: "user can restart the tour from Help -> Take the tour"
        (TUNE-HELP-ONBOARDING). The editor runs it. */
    std::function<void()> onTakeTour;
    juce::Button& getTourButton() noexcept          { return tourButton; }

    int getPreferredHeight() const noexcept { return 640; }

    //==========================================================================
    // For tests.
    juce::ListBox& getTopicList() noexcept          { return topicList; }
    juce::TextEditor& getShortcutSearch() noexcept  { return searchBox; }
    juce::Button& getDebugButton() noexcept         { return debugButton; }
    juce::Button& getRebindButton() noexcept        { return rebindButton; }
    juce::Button& getSourceButton() noexcept        { return sourceButton; }
    juce::Button& getHomepageButton() noexcept      { return homepageButton; }
    juce::Button& getSupportButton() noexcept       { return supportButton; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** The body for a topic: resolved keys, plus what only this machine knows. */
    juce::String composeBody (int index) const;

    void layoutFooter (juce::Rectangle<int> footer);

    LuthierAudioProcessor& processor;

    // --- topics ---------------------------------------------------------------------
    juce::ListBox topicList;
    juce::TextEditor body;
    int shownTopic = 0;

    // --- shortcuts ------------------------------------------------------------------
    juce::TextEditor searchBox;
    juce::TextButton rebindButton { "Rebind..." };
    juce::ListBox shortcutList;

    std::vector<ShortcutRow> shownShortcuts;

    /*  What the list shows: a heading line wherever the group changes, then the
        group's bindings. `row` indexes shownShortcuts; -1 marks a heading. */
    struct SheetLine
    {
        int row = -1;
        juce::String heading;
    };

    std::vector<SheetLine> sheetLines;

    // --- footer ---------------------------------------------------------------------
    juce::TextButton debugButton { "Open Debug Tools" };
    juce::TextButton sourceButton { "GitHub" };
    juce::TextButton homepageButton { "Homepage" };
    juce::TextButton supportButton { "Email Support" };
    juce::TextButton tourButton { "Take the tour" };

    juce::Rectangle<int> headerBounds, versionBounds, shortcutHeaderBounds;

    //==========================================================================
    class TopicModel : public juce::ListBoxModel
    {
    public:
        explicit TopicModel (HelpTab& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRow) override;
        juce::String getNameForRow (int row) override;

    private:
        HelpTab& owner;
    };

    class ShortcutModel : public juce::ListBoxModel
    {
    public:
        explicit ShortcutModel (HelpTab& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        juce::String getNameForRow (int row) override;

    private:
        HelpTab& owner;
    };

    TopicModel topicModel { *this };
    ShortcutModel shortcutModel { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpTab)
};

} // namespace luthier
