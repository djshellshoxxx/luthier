#include "TuneSetlistStrip.h"
#include "TunePanel.h"
#include "Theme.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

TuneSetlistStrip::TuneSetlistStrip (TuneSession& s) : session (s)
{
    setTooltip ("The order the sections play in. Drag a section tab here to add it; drag a block to move it; "
                "right-click for repeats or to remove it");
    AccessibleSetup::configureDescriptive (*this, "Setlist", "The tune's play order: sections and their repeats.");
}

juce::Rectangle<int> TuneSetlistStrip::getEntryBounds (int entryIndex) const
{
    const int count = (int) session.getTune().arrangement.setlist.size();

    if (! juce::isPositiveAndBelow (entryIndex, count))
        return {};

    const int gap = 2;
    const int width = juce::jmax (18, (getWidth() - gap * (count - 1)) / count);
    return { entryIndex * (width + gap), 0, width, getHeight() };
}

int TuneSetlistStrip::getEntryAt (juce::Point<int> position) const
{
    for (int i = 0; i < (int) session.getTune().arrangement.setlist.size(); ++i)
        if (getEntryBounds (i).contains (position))
            return i;

    return -1;
}

int TuneSetlistStrip::insertIndexAt (int x) const
{
    const int count = (int) session.getTune().arrangement.setlist.size();

    for (int i = 0; i < count; ++i)
        if (x < getEntryBounds (i).getCentreX())
            return i;

    return count;
}

bool TuneSetlistStrip::changeSetlist (const juce::String& description,
                                      const std::function<void (std::vector<TuneSetlistEntry>&)>& edit)
{
    auto setlist = session.getTune().arrangement.setlist;
    edit (setlist);

    const bool changed = session.edit (TuneEditClass::sectionEdit, description,
                                       [setlist] (Tune& t) { return t.setSetlist (setlist); });
    repaint();
    return changed;
}

bool TuneSetlistStrip::dropSection (int sectionIndex, int insertAt)
{
    const auto* section = session.getTune().getSection (sectionIndex);

    if (section == nullptr)
        return false;

    const auto name = section->name;

    return changeSetlist ("Add to setlist", [name, insertAt] (std::vector<TuneSetlistEntry>& setlist)
    {
        TuneSetlistEntry entry;
        entry.section = name;
        setlist.insert (setlist.begin() + juce::jlimit (0, (int) setlist.size(), insertAt), entry);
    });
}

bool TuneSetlistStrip::moveEntry (int fromIndex, int toSlot)
{
    const int count = (int) session.getTune().arrangement.setlist.size();

    if (! juce::isPositiveAndBelow (fromIndex, count))
        return false;

    const int target = juce::jlimit (0, count - 1, toSlot > fromIndex ? toSlot - 1 : toSlot);

    if (target == fromIndex)
        return false;

    return changeSetlist ("Move in setlist", [fromIndex, target] (std::vector<TuneSetlistEntry>& setlist)
    {
        const auto entry = setlist[(size_t) fromIndex];
        setlist.erase (setlist.begin() + fromIndex);
        setlist.insert (setlist.begin() + target, entry);
    });
}

bool TuneSetlistStrip::removeEntry (int entryIndex)
{
    if (! juce::isPositiveAndBelow (entryIndex, (int) session.getTune().arrangement.setlist.size()))
        return false;

    return changeSetlist ("Remove from setlist", [entryIndex] (std::vector<TuneSetlistEntry>& setlist)
                          { setlist.erase (setlist.begin() + entryIndex); });
}

bool TuneSetlistStrip::setEntryRepeats (int entryIndex, int repeats)
{
    if (! juce::isPositiveAndBelow (entryIndex, (int) session.getTune().arrangement.setlist.size()))
        return false;

    return changeSetlist ("Setlist repeats", [entryIndex, repeats] (std::vector<TuneSetlistEntry>& setlist)
                          { setlist[(size_t) entryIndex].repeats = juce::jlimit (1, Tune::kMaxRepeats, repeats); });
}

juce::PopupMenu TuneSetlistStrip::buildMenu (int entryIndex) const
{
    juce::PopupMenu menu;
    const auto& setlist = session.getTune().arrangement.setlist;
    const bool valid = juce::isPositiveAndBelow (entryIndex, (int) setlist.size());

    juce::PopupMenu repeats;

    for (int n : { 1, 2, 3, 4, 8 })
        repeats.addItem (repeatBase + n, "x" + juce::String (n), valid,
                         valid && setlist[(size_t) entryIndex].repeats == n);

    menu.addSubMenu ("Repeats", repeats, valid);
    menu.addItem (removeItem, "Remove from setlist", valid);
    menu.addSeparator();
    menu.addItem (clearItem, "Clear setlist (play every section once)", ! setlist.empty());
    return menu;
}

void TuneSetlistStrip::performMenuItem (int entryIndex, int itemId)
{
    if (itemId == removeItem)
        removeEntry (entryIndex);
    else if (itemId == clearItem)
        changeSetlist ("Clear setlist", [] (std::vector<TuneSetlistEntry>& s) { s.clear(); });
    else if (itemId > repeatBase && itemId <= repeatBase + Tune::kMaxRepeats)
        setEntryRepeats (entryIndex, itemId - repeatBase);
}

void TuneSetlistStrip::showDropMarker (int slot)
{
    if (slot != dropSlot)
    {
        dropSlot = slot;
        repaint();
    }
}

void TuneSetlistStrip::paint (juce::Graphics& g)
{
    const auto& tune = session.getTune();
    const auto& setlist = tune.arrangement.setlist;

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::controlCorner);

    if (setlist.empty())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.5f));
        g.drawText ("SETLIST: every section once, in order. Drag a section here to arrange.",
                    getLocalBounds().reduced (6, 0), juce::Justification::centredLeft);
    }

    for (int i = 0; i < (int) setlist.size(); ++i)
    {
        const auto bounds = getEntryBounds (i).toFloat().reduced (0.5f, 1.5f);
        const int sectionIndex = tune.findSection (setlist[(size_t) i].section);
        const auto role = sectionIndex >= 0 ? tune.arrangement.sections[(size_t) sectionIndex].role : SectionRole::none;
        const auto fill = TuneSectionStrip::colourForRole (role);

        g.setColour (i == dragEntry && dragging ? fill.withMultipliedAlpha (0.4f) : fill.withMultipliedAlpha (0.8f));
        g.fillRoundedRectangle (bounds, Metrics::controlCorner);
        g.setColour (sectionIndex < 0 ? Palette::warning : Palette::edge);
        g.drawRoundedRectangle (bounds, Metrics::controlCorner, 1.0f);

        auto text = setlist[(size_t) i].section;

        if (setlist[(size_t) i].repeats > 1)
            text << " x" << setlist[(size_t) i].repeats;

        g.setColour (Palette::textPrimary);
        g.setFont (Fonts::ui (10.0f));
        g.drawFittedText (text, bounds.toNearestInt().reduced (3, 0), juce::Justification::centred, 1);
    }

    if (dropSlot >= 0)
    {
        const int count = (int) setlist.size();
        const int x = count == 0 ? 2 : (dropSlot >= count ? getEntryBounds (count - 1).getRight() + 1
                                                          : getEntryBounds (dropSlot).getX() - 1);
        g.setColour (Palette::accentBright);
        g.fillRect (x - 1, 0, 3, getHeight());
    }
}

void TuneSetlistStrip::mouseDown (const juce::MouseEvent& e)
{
    const int entry = getEntryAt (e.getPosition());

    if (e.mods.isPopupMenu())
    {
        buildMenu (entry).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                         [safe = juce::Component::SafePointer<TuneSetlistStrip> (this), entry] (int r)
        {
            if (safe != nullptr && r != 0)
                safe->performMenuItem (entry, r);
        });
        return;
    }

    dragEntry = entry;
    dragging = false;
}

void TuneSetlistStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEntry < 0)
        return;

    dragging = dragging || e.getDistanceFromDragStart() > 5;

    if (dragging)
        showDropMarker (insertIndexAt (e.x));
}

void TuneSetlistStrip::mouseUp (const juce::MouseEvent& e)
{
    if (dragging && dragEntry >= 0)
    {
        // Dragged off the strip: out of the setlist.
        if (! getLocalBounds().expanded (0, 12).contains (e.getPosition()))
            removeEntry (dragEntry);
        else
            moveEntry (dragEntry, insertIndexAt (e.x));
    }

    dragEntry = -1;
    dragging = false;
    showDropMarker (-1);
}

} // namespace luthier
