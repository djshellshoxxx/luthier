#pragma once

/*  action-and-undo.md 9: the Undo History dropdown.

    A search box over a scrollable list of the processor's undo entries, newest
    first. A boundary entry (section 5) is drawn under a horizontal rule with
    its description as the subtitle. Clicking a row undoes back to before that
    entry - across boundaries, since choosing an older entry is itself the
    explicit request. Opened from the header's File menu.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Support/UndoHistory.h"

namespace luthier
{

class LuthierAudioProcessor;

class UndoHistoryPanel : public juce::Component,
                         private juce::ListBoxModel
{
public:
    explicit UndoHistoryPanel (LuthierAudioProcessor& processor);
    ~UndoHistoryPanel() override;

    /** Re-reads the history, keeping only entries whose description contains
        the search text (case-insensitive). */
    void refresh();

    void setSearchText (const juce::String& text);

    /** The rows being shown, newest first (tests). */
    const juce::Array<UndoHistory::Item>& getShownItems() const noexcept { return shown; }

    /** What a click on a shown row does (tests call it directly). */
    void chooseRow (int row);

    std::function<void()> onChosen;

    void resized() override;
    void paint (juce::Graphics&) override;

    static constexpr int kRowHeight = 22;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    LuthierAudioProcessor& processor;
    juce::TextEditor search;
    juce::ListBox list;
    juce::Array<UndoHistory::Item> shown;
};

} // namespace luthier
