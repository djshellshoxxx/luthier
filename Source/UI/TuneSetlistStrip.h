#pragma once

/*  The setlist timeline (tune-builder.md 3.3: "The setlist for the whole tune
    (repeats and section order) is edited by dragging tabs into the timeline
    strip at the top of the section list"). TUNE-HELP-ONBOARDING workstream.

    One block per setlist entry, "Verse x2", coloured by the section's role,
    in play order. A section tab dropped here inserts an entry where it lands;
    a block dragged along the strip moves; right-click removes it or sets its
    repeats. With no setlist the tune plays every section once, in order, and
    the strip says so. Every change is one `tune-section-edit`.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Tune/TuneSession.h"

namespace luthier
{

class TuneSetlistStrip : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    enum MenuItem { removeItem = 1, clearItem, repeatBase = 100 };

    explicit TuneSetlistStrip (TuneSession& session);

    static constexpr int kHeight = 24;

    juce::Rectangle<int> getEntryBounds (int entryIndex) const;
    int getEntryAt (juce::Point<int> position) const;

    /** The slot a drop at x lands in: 0 .. number of entries. */
    int insertIndexAt (int x) const;

    /** A section tab dropped here. */
    bool dropSection (int sectionIndex, int insertAt);

    bool moveEntry (int fromIndex, int toSlot);
    bool removeEntry (int entryIndex);
    bool setEntryRepeats (int entryIndex, int repeats);

    juce::PopupMenu buildMenu (int entryIndex) const;
    void performMenuItem (int entryIndex, int itemId);

    /** Where a section tab being dragged would land, drawn as a marker; -1 hides it. */
    void showDropMarker (int slot);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool changeSetlist (const juce::String& description, const std::function<void (std::vector<TuneSetlistEntry>&)>& edit);

    TuneSession& session;
    int dragEntry = -1, dropSlot = -1;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSetlistStrip)
};

} // namespace luthier
