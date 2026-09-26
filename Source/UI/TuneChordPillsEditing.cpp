/*  The progression editor's mouse and menu (tune-builder.md 3.2).
    TUNE-HELP-ONBOARDING workstream. The pills' drawing is in TunePanel.cpp.

    Click opens the popover, dragging a pill reorders it, dragging its right
    edge changes its length, right-click has insert / duplicate / delete /
    copy / paste and "Suggest substitution". Every action is one
    `tune-chord-edit`.
*/

#include "TunePanel.h"
#include "TuneChordEditor.h"
#include "../Tune/TuneHarmony.h"

#include <optional>

namespace luthier
{

namespace
{
    /** 3.2's Copy / Paste: one cell, shared by every pill strip in the process. */
    std::optional<ChordCell>& chordClipboard()
    {
        static std::optional<ChordCell> clip;
        return clip;
    }

    constexpr int kEdgeGrab = 5;           // pixels either side of a pill's right edge
    constexpr int kDragThreshold = 5;
    constexpr double kDurationSnap = 0.5;  // beats
}

bool TuneChordPills::hasClipboard() noexcept
{
    return chordClipboard().has_value();
}

double TuneChordPills::sectionBeats() const
{
    return juce::jmax (1.0, session.getTune().getSectionLengthBeats (session.getSelectedSection()));
}

juce::Rectangle<int> TuneChordPills::getCellBounds (int cellIndex) const
{
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (session.getSelectedSection());

    if (section == nullptr)
        return {};

    const auto total = sectionBeats();

    for (const auto& span : resolveChordSpans (*section, tune.getBeatsPerBar()))
    {
        if (span.cellIndex != cellIndex)
            continue;

        const int x0 = juce::roundToInt (span.startBeat / total * getWidth());
        const int x1 = juce::roundToInt (span.endBeat / total * getWidth());
        return { x0, 0, juce::jmax (4, x1 - x0), getHeight() };
    }

    return {};
}

int TuneChordPills::getCellAt (juce::Point<int> position) const
{
    const auto* section = session.getTune().getSection (session.getSelectedSection());

    if (section == nullptr)
        return -1;

    for (int i = 0; i < (int) section->chords.size(); ++i)
        if (getCellBounds (i).expanded (0, 1).contains (position))
            return i;

    return -1;
}

bool TuneChordPills::isOnRightEdge (int cellIndex, juce::Point<int> position) const
{
    const auto bounds = getCellBounds (cellIndex);
    return ! bounds.isEmpty() && std::abs (position.x - bounds.getRight()) <= kEdgeGrab;
}

double TuneChordPills::beatsForEdgeAt (int cellIndex, int x) const
{
    const auto bounds = getCellBounds (cellIndex);
    const double beats = (double) (x - bounds.getX()) / juce::jmax (1, getWidth()) * sectionBeats();
    return juce::jmax (kDurationSnap, std::round (beats / kDurationSnap) * kDurationSnap);
}

bool TuneChordPills::resizeCell (int cellIndex, double beats)
{
    const int section = session.getSelectedSection();

    return session.edit (TuneEditClass::chordEdit, "Chord duration",
                         [section, cellIndex, beats] (Tune& t) { return t.setChordDuration (section, cellIndex, beats); },
                         5000 + section * 256 + cellIndex);
}

int TuneChordPills::dropIndexAt (int x) const
{
    const auto* section = session.getTune().getSection (session.getSelectedSection());

    if (section == nullptr)
        return -1;

    // The slot before the first pill whose centre is to the right of x.
    for (int i = 0; i < (int) section->chords.size(); ++i)
        if (x < getCellBounds (i).getCentreX())
            return i;

    return (int) section->chords.size();
}

bool TuneChordPills::moveCell (int fromIndex, int toIndex)
{
    const int section = session.getSelectedSection();
    const auto* s = session.getTune().getSection (section);

    if (s == nullptr)
        return false;

    // A drop slot after the cell's own position means "after", so it moves one less.
    const int count = (int) s->chords.size();
    const int target = juce::jlimit (0, count - 1, toIndex > fromIndex ? toIndex - 1 : toIndex);

    if (target == fromIndex)
        return false;

    return session.edit (TuneEditClass::chordEdit, "Move chord",
                         [section, fromIndex, target] (Tune& t) { return t.moveChord (section, fromIndex, target); });
}

juce::PopupMenu TuneChordPills::buildMenu (int cellIndex) const
{
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (session.getSelectedSection());
    const bool valid = section != nullptr && juce::isPositiveAndBelow (cellIndex, (int) section->chords.size());

    juce::PopupMenu menu;
    menu.addItem (editItem, "Edit...", valid);
    menu.addSeparator();
    menu.addItem (insertBeforeItem, "Insert before", valid);
    menu.addItem (insertAfterItem, "Insert after", section != nullptr);
    menu.addItem (duplicateItem, "Duplicate", valid);
    menu.addItem (deleteItem, "Delete", valid);
    menu.addSeparator();
    menu.addItem (copyItem, "Copy", valid);
    menu.addItem (pasteItem, "Paste", section != nullptr && hasClipboard());

    // 3.2: "Suggest substitution (tritone sub, ii-V insertion, relative minor)".
    juce::PopupMenu subs;

    if (valid)
    {
        const auto options = suggestSubstitutions (section->chords, cellIndex, tune.meta.keyTonic, tune.meta.mode);

        for (int i = 0; i < (int) options.size(); ++i)
        {
            juce::StringArray symbols;

            for (const auto& c : options[(size_t) i].cells)
                symbols.add (getChordSymbol (c, tune.preferFlats()));

            subs.addItem (substitutionBase + i, options[(size_t) i].name + ": " + symbols.joinIntoString (" "));
        }
    }

    menu.addSeparator();
    menu.addSubMenu ("Suggest substitution", subs, subs.getNumItems() > 0);
    return menu;
}

void TuneChordPills::performMenuItem (int cellIndex, int itemId)
{
    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();
    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return;

    const bool valid = juce::isPositiveAndBelow (cellIndex, (int) section->chords.size());

    // A new cell is the one it sits beside, a bar long; on an empty section, the tonic.
    auto neighbour = [&]
    {
        auto c = valid ? section->chords[(size_t) cellIndex]
                       : ChordCell::make (tune.meta.keyTonic, tune.meta.mode == TuneMode::aeolian ? "m" : "", tune.getBeatsPerBar());
        c.durationBeats = tune.getBeatsPerBar();
        return c;
    };

    switch (itemId)
    {
        case editItem:
            if (valid)
                openEditor (cellIndex);
            break;

        case insertBeforeItem:
        case insertAfterItem:
        {
            const int at = itemId == insertBeforeItem ? juce::jmax (0, cellIndex)
                                                      : (valid ? cellIndex + 1 : -1);
            const auto cell = neighbour();
            session.edit (TuneEditClass::chordEdit, "Insert chord",
                          [sectionIndex, at, cell] (Tune& t) { return t.insertChord (sectionIndex, at, cell); });
            break;
        }

        case duplicateItem:
            if (valid)
            {
                const auto cell = section->chords[(size_t) cellIndex];
                session.edit (TuneEditClass::chordEdit, "Duplicate chord",
                              [sectionIndex, cellIndex, cell] (Tune& t) { return t.insertChord (sectionIndex, cellIndex + 1, cell); });
            }
            break;

        case deleteItem:
            if (valid)
                session.edit (TuneEditClass::chordEdit, "Delete chord",
                              [sectionIndex, cellIndex] (Tune& t) { return t.removeChord (sectionIndex, cellIndex); });
            break;

        case copyItem:
            if (valid)
                chordClipboard() = section->chords[(size_t) cellIndex];
            break;

        case pasteItem:
            if (hasClipboard())
            {
                // Pasted after the cell clicked, or at the end.
                const auto cell = *chordClipboard();
                const int at = valid ? cellIndex + 1 : -1;
                session.edit (TuneEditClass::chordEdit, "Paste chord",
                              [sectionIndex, at, cell] (Tune& t) { return t.insertChord (sectionIndex, at, cell); });
            }
            break;

        default:
            if (itemId >= substitutionBase && valid)
            {
                const auto options = suggestSubstitutions (section->chords, cellIndex, tune.meta.keyTonic, tune.meta.mode);
                const int k = itemId - substitutionBase;

                if (juce::isPositiveAndBelow (k, (int) options.size()))
                {
                    const auto sub = options[(size_t) k];

                    session.edit (TuneEditClass::chordEdit, "Substitute: " + sub.name,
                                  [sectionIndex, cellIndex, sub] (Tune& t)
                    {
                        auto* s = t.getSection (sectionIndex);

                        if (s == nullptr || sub.cells.empty())
                            return false;

                        auto cells = s->chords;
                        cells.erase (cells.begin() + cellIndex);
                        cells.insert (cells.begin() + cellIndex, sub.cells.begin(), sub.cells.end());
                        return t.setChords (sectionIndex, cells);
                    });
                }
            }
            break;
    }

    repaint();
}

void TuneChordPills::openEditor (int cellIndex)
{
    const auto bounds = getCellBounds (cellIndex);

    if (bounds.isEmpty() || ! isShowing())
        return;

    auto editor = std::make_unique<TuneChordEditor> (session, session.getSelectedSection(), cellIndex,
                                                     getPatternNames != nullptr ? getPatternNames() : juce::StringArray());
    juce::CallOutBox::launchAsynchronously (std::move (editor), localAreaToGlobal (bounds), nullptr);
}

//==============================================================================
void TuneChordPills::mouseMove (const juce::MouseEvent& e)
{
    const int cell = getCellAt (e.getPosition());
    setMouseCursor (cell >= 0 && isOnRightEdge (cell, e.getPosition()) ? juce::MouseCursor::LeftRightResizeCursor
                                                                         : juce::MouseCursor::NormalCursor);
}

void TuneChordPills::mouseDown (const juce::MouseEvent& e)
{
    const int cell = getCellAt (e.getPosition());

    if (e.mods.isPopupMenu())
    {
        buildMenu (cell).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                        [safe = juce::Component::SafePointer<TuneChordPills> (this), cell] (int result)
        {
            if (safe != nullptr && result != 0)
                safe->performMenuItem (cell, result);
        });
        return;
    }

    // The right edge of the pill to the left wins over the pill it touches.
    int edgeCell = -1;

    for (int i : { cell - 1, cell })
        if (i >= 0 && isOnRightEdge (i, e.getPosition()))
            edgeCell = i;

    dragCell = edgeCell >= 0 ? edgeCell : cell;
    drag = edgeCell >= 0 ? Drag::resize : (cell >= 0 ? Drag::pending : Drag::none);
    dropIndex = -1;

    if (drag == Drag::resize)
        dragBeats = beatsForEdgeAt (dragCell, e.x);
}

void TuneChordPills::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::resize)
    {
        dragBeats = beatsForEdgeAt (dragCell, e.x);
        repaint();
        return;
    }

    if (drag == Drag::pending && e.getDistanceFromDragStart() > kDragThreshold)
        drag = Drag::move;

    if (drag == Drag::move)
    {
        dropIndex = dropIndexAt (e.x);
        repaint();
    }
}

void TuneChordPills::mouseUp (const juce::MouseEvent&)
{
    const auto was = drag;
    drag = Drag::none;

    if (was == Drag::resize)
        resizeCell (dragCell, dragBeats);
    else if (was == Drag::move && dropIndex >= 0)
        moveCell (dragCell, dropIndex);
    else if (was == Drag::pending && dragCell >= 0)
        openEditor (dragCell);

    dropIndex = -1;
    repaint();
}

} // namespace luthier
