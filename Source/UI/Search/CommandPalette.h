#pragma once

/*  global-search.md 6: the palette.

    A child of the editor above the OverlayHost and below the MIDI-learn arm
    layer. Not an OverlayPanel, so it opens over Options without dismissing
    it. A 40% scrim; clicking it closes the palette.

        +--------------------------------------------------------------+
        | [magnifier] treble bl|                               [Esc]   |  field 40
        | All  Controls  Places  Commands  Content  Help               |  chips 24
        +--------------------------------------------------------------+
        | [~] Treble bleed        Adv > Col 2 > Circuit        None  > |  rows 36 (44 in Live Mode)
        +--------------------------------------------------------------+
        | Enter go | Alt+Enter actions | type a value to set | Esc     |  footer 20
        +--------------------------------------------------------------+

    Keyboard first (6.3): the field keeps focus while typing, so single-key
    shortcuts cannot fire; the arrows, Page keys, Ctrl+Home/End, the Enter
    family and Alt+arrows are intercepted from it; Tab moves between the field,
    the chips and the list.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "SearchNavigator.h"
#include "../AnimationPolicy.h"   // cpu-quality-modes 6 (INTEGRATE-2)

namespace luthier::search
{

class CommandPalette : public juce::Component,
                       private juce::KeyListener,
                       private juce::Timer
{
public:
    explicit CommandPalette (SearchNavigator& navigator);
    ~CommandPalette() override;

    //==========================================================================
    void open (const juce::String& initialText = {});
    void close (bool restoreFocus = true);
    bool isOpen() const noexcept { return isVisible(); }

    void setQuery (const juce::String& text);
    juce::String getQuery() const { return field.getText(); }

    void setScope (SearchIndex::Scope scope);
    SearchIndex::Scope getScope() const noexcept { return scope; }

    /** Re-runs the query (the index may have re-collected), keeping the
        selection by id (12: a host preset load while open). */
    void refresh();

    //==========================================================================
    struct Row
    {
        enum class Type { header, item, recentQuery, hint, noResults, didYouMean, searchHelp };

        Type type = Type::item;
        const SearchItem* item = nullptr;
        juce::String itemId;           ///< kept across refreshes
        juce::String text;             ///< header / query / message text
        Availability availability = Availability::available;
        bool valueReading = false;     ///< the row the typed value applies to (4.4)

        bool isSelectable() const noexcept { return type != Type::header && type != Type::hint && type != Type::noResults; }
    };

    const std::vector<Row>& getRows() const noexcept { return rows; }
    int getSelectedRow() const noexcept { return selected; }
    void selectRow (int row);
    const Row* getSelected() const;

    /** The value reading behind the value row, if any. */
    const ValueReading& getValueReading() const noexcept { return reading; }

    /** The footer line (hint, or an error in the warning colour). */
    juce::String getFooterText() const { return footerText; }
    bool isFooterWarning() const noexcept { return footerWarning; }
    void setFooter (const juce::String& text, bool warning);

    /** The subtitle under a row: "Opens in Advanced", a confirm prompt... */
    juce::String getSubtitle (int row) const;

    /** "{title}, {kind}, {breadcrumb}, {value}, {lock}" (14). */
    juce::String getAccessibleRowTitle (int row) const;

    //==========================================================================
    /** Everything 6.3's keys do; the field's key listener and keyPressed both
        land here. Returns true if the key was used. */
    bool handleKey (const juce::KeyPress& key);

    /** Enter and its modifiers on the selection (4.4, 5). */
    void activateSelected (ActivationKind kind);

    /** The secondary actions for a row (5): for a parameter, exactly
        buildParameterContextMenu; otherwise the provider's plus Copy id and
        Remove from recent. */
    juce::PopupMenu buildSecondaryMenu (int row);
    void runSecondary (int row, int menuResult);
    void showSecondaryMenu (int row);

    juce::TextEditor& getField() noexcept { return field; }
    juce::ListBox& getList() noexcept     { return list; }
    juce::Button& getChip (int index)     { return *chips[index]; }
    int getNumChips() const noexcept      { return chips.size(); }

    /** The box the palette draws in (the rest is scrim). */
    juce::Rectangle<int> getBoxBounds() const noexcept { return box; }
    int getRowHeight() const;

    /** 14, GS-36: announcements are also sent here. */
    std::function<void (const juce::String&)> onAnnouncement;

    /** Runs the "400 ms after typing stops" announcement now (tests). */
    void announceResultsNow();

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    void timerCallback() override;

    void announce (const juce::String& text);
    void rebuildRows();
    void buildEmptyState();
    void updateLayout();
    void moveSelection (int delta, bool wrap);
    void confirmOrActivate (ActivationKind kind);
    void handleOutcome (const SearchNavigator::Outcome& outcome, const SearchItem* item);
    void paintRow (juce::Graphics&, int row, int width, int height, bool selectedRow);

    class RowComponent;
    class Model;

    SearchNavigator& navigator;

    juce::TextEditor field;
    juce::OwnedArray<juce::TextButton> chips;
    juce::ListBox list;
    std::unique_ptr<Model> model;

    SearchIndex::Scope scope = SearchIndex::Scope::all;
    std::vector<Row> rows;
    int selected = -1;
    ValueReading reading;

    juce::String footerText;
    bool footerWarning = false;

    /** A row that asked for a second Enter, by id, with what it said. */
    juce::String pendingConfirmId, pendingConfirmText;
    juce::String allowInlineId;

    juce::Rectangle<int> box, fieldArea, chipArea, listArea, footerArea;

    juce::Component::SafePointer<juce::Component> focusBeforeOpen;

    double lastTypedMs = 0.0;
    bool announcePending = false;
    int valueRefreshTicks = 0;
    bool updatingField = false;

    // cpu-quality-modes 6: the rows' live values are a readout (10 Hz, stepped at Low).
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "CommandPalette" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CommandPalette)
};

} // namespace luthier::search
