/*  The section strip's drags and Vary (tune-builder.md 3.3).
    TUNE-HELP-ONBOARDING workstream. The strip's drawing and menu are in
    TunePanel.cpp.
*/

#include "TunePanel.h"
#include "../Tune/TuneVary.h"

namespace luthier
{

int TuneSectionStrip::dropSlotAt (int x) const
{
    const int count = session.getTune().getNumSections();

    for (int i = 0; i < count; ++i)
        if (x < getTabBounds (i).getCentreX())
            return i;

    return count;
}

bool TuneSectionStrip::moveSectionTo (int fromIndex, int toSlot)
{
    const int count = session.getTune().getNumSections();

    if (! juce::isPositiveAndBelow (fromIndex, count))
        return false;

    const int target = juce::jlimit (0, count - 1, toSlot > fromIndex ? toSlot - 1 : toSlot);

    if (target == fromIndex)
        return false;

    const bool moved = session.edit (TuneEditClass::sectionEdit, "Move section",
                                     [fromIndex, target] (Tune& t) { return t.moveSection (fromIndex, target); });

    if (moved)
        session.setSelectedSection (target);

    repaint();
    return moved;
}

int TuneSectionStrip::varySection (int sectionIndex)
{
    int made = -1;
    const int seed = varySeed++;

    session.edit (TuneEditClass::sectionEdit, "Vary section", [sectionIndex, seed, &made] (Tune& t)
    {
        made = createSectionVariation (t, sectionIndex, seed);
        return made >= 0;
    });

    if (made >= 0)
        session.setSelectedSection (made);

    repaint();
    return made;
}

void TuneSectionStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTab < 0)
        return;

    draggingTab = draggingTab || e.getDistanceFromDragStart() > 6;

    if (! draggingTab)
        return;

    dropSlot = getLocalBounds().contains (e.getPosition()) ? dropSlotAt (e.x) : -1;

    if (onTabDragged != nullptr)
        onTabDragged (dragTab, e.getScreenPosition());

    repaint();
}

void TuneSectionStrip::mouseUp (const juce::MouseEvent& e)
{
    if (draggingTab && dragTab >= 0)
    {
        const bool taken = onTabDropped != nullptr && onTabDropped (dragTab, e.getScreenPosition());

        if (! taken && getLocalBounds().contains (e.getPosition()))
            moveSectionTo (dragTab, dropSlotAt (e.x));
    }

    dragTab = -1;
    dropSlot = -1;
    draggingTab = false;
    repaint();
}

} // namespace luthier
