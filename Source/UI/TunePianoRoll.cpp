/*  The TUNE tab's piano roll (tune-builder.md 3.4, 6, 7). Declared in
    TunePanel.h. TUNE-HELP-ONBOARDING workstream: the note menu, multi-select,
    the clipboard, nudging and moving, and the bass / countermelody targets.
*/

#include "TunePanel.h"
#include "../Accessibility/Accessibility.h"
#include "../Tune/TuneHarmony.h"
#include "../Tune/TuneMelody.h"

#include <algorithm>

namespace luthier
{

namespace
{
    /** 3.4's cut / copy / paste: absolute pitches, starts from the first note. */
    struct NoteClipboard
    {
        std::vector<MelodyNote> notes;
        double span = 0.0;
    };

    NoteClipboard& clipboard()
    {
        static NoteClipboard clip;
        return clip;
    }

    constexpr int kNoteEditTarget = 7000;   // + section: nudges of one section group within 200 ms

    const char* articulationLabel (NoteArticulation a)
    {
        switch (a)
        {
            case NoteArticulation::inherit:   return "Track default";
            case NoteArticulation::natural:   return "Natural";
            case NoteArticulation::legato:    return "Legato";
            case NoteArticulation::staccato:  return "Staccato";
            case NoteArticulation::palmMuted: return "Palm-muted";
            case NoteArticulation::letRing:   return "Let ring";
            case NoteArticulation::numArticulations: break;
        }

        return "";
    }

    const char* techniqueLabel (NoteTechnique t)
    {
        switch (t)
        {
            case NoteTechnique::none:     return "None";
            case NoteTechnique::bend:     return "Bend";
            case NoteTechnique::slide:    return "Slide";
            case NoteTechnique::hammerOn: return "Hammer-on";
            case NoteTechnique::pullOff:  return "Pull-off";
            case NoteTechnique::vibrato:  return "Vibrato";
            case NoteTechnique::harmonic: return "Harmonic";
            case NoteTechnique::numTechniques: break;
        }

        return "";
    }
}

//==============================================================================
TunePianoRoll::TunePianoRoll (TuneSession& s) : session (s)
{
    setWantsKeyboardFocus (true);
    setTooltip ("Drag to draw a note; click a note to select it (Shift adds), drag to move; Shift-drag selects a box. "
                "Right-click a note for velocity, articulation, technique and lock. Arrows nudge, Ctrl+X/C/V, "
                "Delete. C toggles chromatic.");
    AccessibleSetup::configureDescriptive (*this, "Piano roll",
                                           "The selected section's melody, bass or countermelody. Drag to draw a note; "
                                           "right-click a note for its menu.");
}

void TunePianoRoll::setTarget (Target newTarget)
{
    if (target != newTarget)
    {
        target = newTarget;
        clearSelection();
        repaint();
    }
}

double TunePianoRoll::sectionBeats() const
{
    return juce::jmax (1.0, session.getTune().getSectionLengthBeats (session.getSelectedSection()));
}

void TunePianoRoll::setPlayhead (const TunePlayhead& newPlayhead)
{
    if (newPlayhead != playhead)
    {
        playhead = newPlayhead;
        repaint();
    }
}

bool TunePianoRoll::isShowingGeneratedBass() const
{
    const auto* s = session.getTune().getSection (session.getSelectedSection());
    return target == Target::bass && s != nullptr && s->bass.mode != BassMode::manual;
}

std::vector<MelodyNote> TunePianoRoll::getShownNotes() const
{
    const auto& tune = session.getTune();
    const int index = session.getSelectedSection();
    const auto* s = tune.getSection (index);

    if (s == nullptr)
        return {};

    switch (target)
    {
        case Target::melody:        return s->melody.has_value() ? s->melody->notes : std::vector<MelodyNote>();
        case Target::bass:          return s->bass.mode == BassMode::manual ? s->bass.notes : generateBassLine (tune, index);
        case Target::countermelody: if (const auto* l = s->findLayer (LayerType::countermelody)) return l->notes; break;
    }

    return {};
}

int TunePianoRoll::pitchOf (const MelodyNote& note) const
{
    return resolveNotePitch (session.getTune(), session.getSelectedSection(), note);
}

int TunePianoRoll::getLowestPitch() const
{
    int low = target == Target::bass ? 28 : (target == Target::countermelody ? 43 : 48);

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
        if (target == Target::melody && s->melody.has_value())
            low = s->melody->rangeLow;

    for (const auto& n : getShownNotes())
        low = juce::jmin (low, pitchOf (n));

    return juce::jlimit (0, 115, low - 2);
}

int TunePianoRoll::getHighestPitch() const
{
    int high = target == Target::bass ? 55 : (target == Target::countermelody ? 74 : 79);

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
        if (target == Target::melody && s->melody.has_value())
            high = s->melody->rangeHigh;

    for (const auto& n : getShownNotes())
        high = juce::jmax (high, pitchOf (n));

    return juce::jlimit (getLowestPitch() + 12, 127, high + 2);
}

double TunePianoRoll::beatAt (float x) const
{
    return juce::jlimit (0.0, sectionBeats(), (double) x / juce::jmax (1.0, (double) getWidth()) * sectionBeats());
}

int TunePianoRoll::pitchAt (float y) const
{
    const int low = getLowestPitch();
    const int rows = getHighestPitch() - low + 1;
    const float rowHeight = (float) getHeight() / (float) rows;
    return juce::jlimit (low, low + rows - 1, low + rows - 1 - (int) std::floor (y / juce::jmax (1.0f, rowHeight)));
}

juce::Rectangle<float> TunePianoRoll::getNoteBounds (double beat, int pitch, double durationBeats) const
{
    const int low = getLowestPitch();
    const int rows = getHighestPitch() - low + 1;
    const float rowHeight = (float) getHeight() / (float) rows;
    const float x = (float) (beat / sectionBeats()) * (float) getWidth();
    const float w = (float) (durationBeats / sectionBeats()) * (float) getWidth();

    return { x, (float) (low + rows - 1 - pitch) * rowHeight, juce::jmax (2.0f, w), rowHeight };
}

int TunePianoRoll::findNoteAt (double beat, int pitch) const
{
    const auto notes = getShownNotes();

    // The last drawn wins where notes overlap: it is the one on top.
    for (int i = (int) notes.size(); --i >= 0;)
    {
        const auto& n = notes[(size_t) i];

        if (beat >= n.startBeat - 1.0e-6 && beat < n.getEndBeat() - 1.0e-6 && pitchOf (n) == pitch)
            return i;
    }

    return -1;
}

//==============================================================================
bool TunePianoRoll::editNotes (TuneEditClass editClass, const juce::String& description,
                               const std::function<bool (std::vector<MelodyNote>&, std::vector<int>&)>& change,
                               int groupTarget)
{
    const int index = session.getSelectedSection();
    const auto which = target;
    std::vector<int> keep;

    const bool changed = session.edit (editClass, description, [index, which, &change, &keep] (Tune& t)
    {
        auto* s = t.getSection (index);

        if (s == nullptr)
            return false;

        switch (which)
        {
            case Target::melody:
            {
                auto notes = s->melody.has_value() ? s->melody->notes : std::vector<MelodyNote>();
                const auto source = s->melody.has_value() ? s->melody->source : MelodySource::draw;

                if (! change (notes, keep))
                    return false;

                return t.setMelodyNotes (index, notes, source);
            }

            case Target::bass:
            {
                // Editing a generated line makes it the manual line it was (6: "Manual").
                auto notes = s->bass.mode == BassMode::manual ? s->bass.notes : generateBassLine (t, index);

                if (! change (notes, keep))
                    return false;

                return t.setBassNotes (index, notes);
            }

            case Target::countermelody:
            {
                TuneLayer layer;
                layer.type = LayerType::countermelody;

                if (const auto* existing = s->findLayer (LayerType::countermelody))
                    layer = *existing;

                if (! change (layer.notes, keep))
                    return false;

                return t.setLayer (index, layer);
            }
        }

        return false;
    }, groupTarget);

    if (changed)
    {
        selection = keep;
        selectionSection = index;
    }

    repaint();
    return changed;
}

bool TunePianoRoll::addNote (double beat, int pitch, double durationBeats)
{
    const auto& tune = session.getTune();

    if (! tune.isValidSection (session.getSelectedSection()))
        return false;

    // 3.4: the start snaps to the grid line at or before it, the pitch to the key.
    const double start = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);
    const double length = juce::jmax (grid, snapBeatToGrid (durationBeats, grid));
    const int snapped = chromaticMode ? pitch : snapPitchToKey (pitch, tune.meta.keyTonic, tune.meta.mode);

    if (start >= sectionBeats() - 1.0e-6)
        return false;

    auto note = MelodyNote::make (start, juce::jmin (length, sectionBeats() - start), snapped, 100);
    note.locked = true;

    return editNotes (TuneEditClass::melodyEdit, "Draw note", [note] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        notes.push_back (note);
        keep = { (int) notes.size() - 1 };
        return true;
    });
}

bool TunePianoRoll::deleteNoteAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);

    if (index < 0)
        return false;

    return editNotes (TuneEditClass::melodyEdit, "Delete note", [index] (std::vector<MelodyNote>& notes, std::vector<int>&)
    {
        if (! juce::isPositiveAndBelow (index, (int) notes.size()))
            return false;

        notes.erase (notes.begin() + index);
        return true;
    });
}

bool TunePianoRoll::toggleLockAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);

    if (index < 0)
        return false;

    const bool locked = getShownNotes()[(size_t) index].locked;

    return editNotes (TuneEditClass::melodyEdit, locked ? "Unlock note" : "Lock note",
                      [index, locked] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        notes[(size_t) index].locked = ! locked;
        keep = { index };
        return true;
    }, kNoteEditTarget + index);
}

//==============================================================================
void TunePianoRoll::selectNote (int noteIndex, bool addToSelection)
{
    if (selectionSection != session.getSelectedSection())
        selection.clear();

    selectionSection = session.getSelectedSection();

    if (! juce::isPositiveAndBelow (noteIndex, (int) getShownNotes().size()))
        return;

    const auto it = std::find (selection.begin(), selection.end(), noteIndex);

    if (addToSelection)
    {
        if (it != selection.end())
            selection.erase (it);
        else
            selection.push_back (noteIndex);
    }
    else
    {
        selection = { noteIndex };
    }

    repaint();
}

void TunePianoRoll::selectInBox (juce::Rectangle<float> box, bool addToSelection)
{
    if (! addToSelection || selectionSection != session.getSelectedSection())
        selection.clear();

    selectionSection = session.getSelectedSection();
    const auto notes = getShownNotes();

    for (int i = 0; i < (int) notes.size(); ++i)
        if (box.intersects (getNoteBounds (notes[(size_t) i].startBeat, pitchOf (notes[(size_t) i]),
                                           notes[(size_t) i].durationBeats))
              && std::find (selection.begin(), selection.end(), i) == selection.end())
            selection.push_back (i);

    repaint();
}

void TunePianoRoll::selectAll()
{
    selection.clear();
    selectionSection = session.getSelectedSection();

    for (int i = 0; i < (int) getShownNotes().size(); ++i)
        selection.push_back (i);

    repaint();
}

void TunePianoRoll::clearSelection()
{
    selection.clear();
    repaint();
}

bool TunePianoRoll::hasClipboard() noexcept
{
    return ! clipboard().notes.empty();
}

bool TunePianoRoll::copySelected()
{
    const auto notes = getShownNotes();
    std::vector<MelodyNote> copied;

    for (int i : selection)
        if (juce::isPositiveAndBelow (i, (int) notes.size()))
            copied.push_back (notes[(size_t) i]);

    if (copied.empty())
        return false;

    double first = 1.0e9, last = 0.0;

    for (auto& n : copied)
    {
        first = juce::jmin (first, n.startBeat);
        last = juce::jmax (last, n.getEndBeat());
    }

    for (auto& n : copied)
    {
        // The clipboard holds what was heard: relative pitches resolved here.
        n.pitch = MelodyPitch::absolute (pitchOf (n));
        n.startBeat -= first;
    }

    clipboard() = { copied, last - first };
    pasteBeat = last;
    return true;
}

bool TunePianoRoll::deleteSelected()
{
    auto doomed = selection;

    if (doomed.empty())
        return false;

    std::sort (doomed.begin(), doomed.end());

    return editNotes (TuneEditClass::melodyEdit, doomed.size() > 1 ? "Delete notes" : "Delete note",
                      [doomed] (std::vector<MelodyNote>& notes, std::vector<int>&)
    {
        bool any = false;

        for (auto it = doomed.rbegin(); it != doomed.rend(); ++it)
            if (juce::isPositiveAndBelow (*it, (int) notes.size()))
            {
                notes.erase (notes.begin() + *it);
                any = true;
            }

        return any;
    });
}

bool TunePianoRoll::cutSelected()
{
    return copySelected() && deleteSelected();
}

bool TunePianoRoll::paste (double beat)
{
    if (! hasClipboard())
        return false;

    const double at = snapBeatToGrid (juce::jmax (0.0, beat >= 0.0 ? beat : juce::jmax (0.0, pasteBeat)), grid);
    const double length = sectionBeats();
    const auto clip = clipboard();

    const bool pasted = editNotes (TuneEditClass::melodyEdit, "Paste notes",
                                   [clip, at, length] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        for (auto n : clip.notes)
        {
            n.startBeat += at;

            if (n.startBeat >= length - 1.0e-6)
                continue;

            n.durationBeats = juce::jmin (n.durationBeats, length - n.startBeat);
            n.locked = true;
            notes.push_back (n);
            keep.push_back ((int) notes.size() - 1);
        }

        return ! keep.empty();
    });

    // Pasting again lands after this one.
    if (pasted)
        pasteBeat = at + clip.span;

    return pasted;
}

int TunePianoRoll::transposeStep (int pitch, int steps, bool large) const
{
    if (large)
        return pitch + 12 * steps;

    if (chromaticMode)
        return pitch + steps;

    const auto& tune = session.getTune();
    return tunetheory::moveByScaleSteps (pitch, steps, tune.meta.keyTonic, tune.meta.mode);
}

bool TunePianoRoll::nudgeSelected (int gridSteps, int pitchSteps, bool large)
{
    if (selection.empty())
        return false;

    const double deltaBeats = gridSteps * (large ? session.getTune().getBeatsPerBar() : grid);
    const double length = sectionBeats();
    const auto chosen = selection;
    std::vector<int> pitches;
    const auto shown = getShownNotes();

    // The new pitches are worked out here, where the key and the chord are known.
    for (int i : chosen)
        pitches.push_back (juce::isPositiveAndBelow (i, (int) shown.size())
                             ? juce::jlimit (0, 127, transposeStep (pitchOf (shown[(size_t) i]), pitchSteps, large)) : 0);

    return editNotes (TuneEditClass::melodyEdit, "Nudge notes",
                      [chosen, pitches, deltaBeats, pitchSteps, length] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        bool any = false;

        for (size_t k = 0; k < chosen.size(); ++k)
        {
            const int i = chosen[k];

            if (! juce::isPositiveAndBelow (i, (int) notes.size()))
                continue;

            auto& n = notes[(size_t) i];
            const auto before = n;
            n.startBeat = juce::jlimit (0.0, juce::jmax (0.0, length - n.durationBeats), n.startBeat + deltaBeats);

            // Relative pitches keep their meaning and only move in time.
            if (pitchSteps != 0 && n.pitch.isAbsolute())
                n.pitch.value = pitches[k];

            n.locked = true;
            any = any || n != before;
            keep.push_back (i);
        }

        return any;
    }, kNoteEditTarget + session.getSelectedSection());
}

bool TunePianoRoll::setSelectedVelocity (int velocity)
{
    const auto chosen = selection;
    const int v = juce::jlimit (1, 127, velocity);

    return ! chosen.empty() && editNotes (TuneEditClass::melodyEdit, "Note velocity",
                                          [chosen, v] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        for (int i : chosen)
            if (juce::isPositiveAndBelow (i, (int) notes.size()))
            {
                notes[(size_t) i].velocity = v;
                notes[(size_t) i].locked = true;
                keep.push_back (i);
            }

        return ! keep.empty();
    });
}

bool TunePianoRoll::setSelectedArticulation (NoteArticulation articulation)
{
    const auto chosen = selection;

    return ! chosen.empty() && editNotes (TuneEditClass::melodyEdit, "Note articulation",
                                          [chosen, articulation] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        for (int i : chosen)
            if (juce::isPositiveAndBelow (i, (int) notes.size()))
            {
                notes[(size_t) i].articulation = articulation;
                notes[(size_t) i].locked = true;
                keep.push_back (i);
            }

        return ! keep.empty();
    });
}

bool TunePianoRoll::setSelectedTechnique (NoteTechnique technique)
{
    const auto chosen = selection;

    return ! chosen.empty() && editNotes (TuneEditClass::melodyEdit, "Note technique",
                                          [chosen, technique] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        for (int i : chosen)
            if (juce::isPositiveAndBelow (i, (int) notes.size()))
            {
                notes[(size_t) i].technique = technique;
                notes[(size_t) i].locked = true;
                keep.push_back (i);
            }

        return ! keep.empty();
    });
}

bool TunePianoRoll::setSelectedLocked (bool locked)
{
    const auto chosen = selection;

    return ! chosen.empty() && editNotes (TuneEditClass::melodyEdit, locked ? "Lock notes" : "Unlock notes",
                                          [chosen, locked] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
    {
        for (int i : chosen)
            if (juce::isPositiveAndBelow (i, (int) notes.size()))
            {
                notes[(size_t) i].locked = locked;
                keep.push_back (i);
            }

        return ! keep.empty();
    });
}

//==============================================================================
juce::PopupMenu TunePianoRoll::buildNoteMenu() const
{
    const auto notes = getShownNotes();
    const bool any = ! selection.empty();
    const MelodyNote* first = any && juce::isPositiveAndBelow (selection.front(), (int) notes.size())
                                ? &notes[(size_t) selection.front()] : nullptr;

    juce::PopupMenu velocity;

    for (const auto& [label, v] : { std::pair<const char*, int> { "Ghost (40)", 40 }, { "Soft (64)", 64 },
                                    { "Medium (90)", 90 }, { "Firm (110)", 110 }, { "Full (127)", 127 } })
        velocity.addItem (velocityBase + v, label, any, first != nullptr && first->velocity == v);

    juce::PopupMenu articulation;

    for (int a = 0; a < (int) NoteArticulation::numArticulations; ++a)
        articulation.addItem (articulationBase + a, articulationLabel ((NoteArticulation) a), any,
                              first != nullptr && (int) first->articulation == a);

    juce::PopupMenu technique;

    for (int t = 0; t < (int) NoteTechnique::numTechniques; ++t)
        technique.addItem (techniqueBase + t, techniqueLabel ((NoteTechnique) t), any,
                           first != nullptr && (int) first->technique == t);

    juce::PopupMenu menu;
    menu.addSubMenu ("Velocity", velocity, any);
    menu.addSubMenu ("Articulation", articulation, any);
    menu.addSubMenu ("Technique", technique, any);
    menu.addSeparator();

    // 3.4's "unlock": a regenerate leaves a locked note alone.
    const bool anyLocked = std::any_of (selection.begin(), selection.end(), [&notes] (int i)
                                        { return juce::isPositiveAndBelow (i, (int) notes.size()) && notes[(size_t) i].locked; });
    menu.addItem (anyLocked ? unlockItem : lockItem, anyLocked ? "Unlock (regenerate may change it)" : "Lock", any);
    menu.addItem (deleteItem, "Delete", any);
    menu.addSeparator();
    menu.addItem (cutItem, "Cut", any);
    menu.addItem (copyItem, "Copy", any);
    menu.addItem (pasteItem, "Paste", hasClipboard());
    menu.addItem (selectAllItem, "Select all", ! notes.empty());
    return menu;
}

void TunePianoRoll::performNoteMenuItem (int itemId)
{
    switch (itemId)
    {
        case deleteItem:    deleteSelected(); return;
        case lockItem:      setSelectedLocked (true); return;
        case unlockItem:    setSelectedLocked (false); return;
        case cutItem:       cutSelected(); return;
        case copyItem:      copySelected(); return;
        case pasteItem:     paste(); return;
        case selectAllItem: selectAll(); return;
        default: break;
    }

    if (itemId >= techniqueBase && itemId < techniqueBase + (int) NoteTechnique::numTechniques)
        setSelectedTechnique ((NoteTechnique) (itemId - techniqueBase));
    else if (itemId >= articulationBase && itemId < articulationBase + (int) NoteArticulation::numArticulations)
        setSelectedArticulation ((NoteArticulation) (itemId - articulationBase));
    else if (itemId > velocityBase && itemId <= velocityBase + 127)
        setSelectedVelocity (itemId - velocityBase);
}

//==============================================================================
void TunePianoRoll::paint (juce::Graphics& g)
{
    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();
    const int low = getLowestPitch();
    const int high = getHighestPitch();
    const int rows = high - low + 1;
    const float rowHeight = (float) getHeight() / (float) rows;

    g.setColour (Palette::panelSunken);
    g.fillRect (getLocalBounds());

    // 3.4: key-of-scale rows shaded, the tonic a shade brighter.
    for (int pitch = low; pitch <= high; ++pitch)
    {
        if (! tunetheory::isInScale (pitch, tune.meta.keyTonic, tune.meta.mode))
            continue;

        const float y = (float) (high - pitch) * rowHeight;
        g.setColour (tunetheory::wrapPitchClass (pitch - tune.meta.keyTonic) == 0 ? Palette::panelRaised : Palette::panel);
        g.fillRect (0.0f, y, (float) getWidth(), rowHeight);
    }

    // Beat and bar lines.
    const double beats = sectionBeats();
    const double bar = tune.getBeatsPerBar();

    for (double b = 0.0; b <= beats + 1.0e-9; b += 1.0)
    {
        const float x = (float) (b / beats) * (float) getWidth();
        const double intoBar = std::fmod (b, bar);
        const bool isBar = intoBar < 1.0e-6 || bar - intoBar < 1.0e-6;
        g.setColour (isBar ? Palette::edgeBright : Palette::edge.withMultipliedAlpha (0.6f));
        g.drawVerticalLine (juce::jmin ((int) x, getWidth() - 1), 0.0f, (float) getHeight());
    }

    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText ("Add a section to write a melody", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto notes = getShownNotes();
    const bool generated = isShowingGeneratedBass();

    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        const bool selected = selectionSection == sectionIndex
                                && std::find (selection.begin(), selection.end(), i) != selection.end();
        auto r = getNoteBounds (n.startBeat, pitchOf (n), n.durationBeats).reduced (0.5f, 1.0f);

        // A move in progress shows where the selection will land.
        if (selected && drag == Drag::move)
            r = r.translated ((float) (moveBeats / beats) * (float) getWidth(), -(float) movePitchRows * rowHeight);

        auto fill = n.locked ? Palette::accentBright : Palette::secondary;

        // Velocity reads as depth of colour, so a ghost note looks like one.
        fill = fill.withMultipliedAlpha (generated ? 0.35f : 0.45f + 0.55f * (float) n.velocity / 127.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 2.0f);

        if (n.locked || selected)
        {
            g.setColour (selected ? Palette::textPrimary : Palette::textPrimary.withMultipliedAlpha (0.7f));
            g.drawRoundedRectangle (r, 2.0f, selected ? 2.0f : 1.0f);
        }

        // A technique is marked with its initial, not colour alone (accessibility 0.2).
        if (n.technique != NoteTechnique::none && r.getWidth() > 10.0f)
        {
            g.setColour (Palette::plateText);
            g.setFont (Fonts::ui (juce::jmin (10.0f, r.getHeight()), true));
            g.drawText (juce::String (techniqueLabel (n.technique)).substring (0, 1), r.reduced (2.0f, 0.0f),
                        juce::Justification::centredLeft);
        }
    }

    if (target == Target::melody && section->melody.has_value() && section->melody->source == MelodySource::improvise)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawText ("IMPROVISING: a new line every pass", getLocalBounds().reduced (6, 4), juce::Justification::bottomLeft);
    }

    if (generated && section->bass.mode != BassMode::off)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawText ("Generated bass: edit a note to make it a manual line", getLocalBounds().reduced (6, 4),
                    juce::Justification::bottomLeft);
    }

    if (drag == Drag::draw)
    {
        const auto r = getNoteBounds (dragStart, dragPitch, juce::jmax (grid, dragEnd - dragStart));
        g.setColour (Palette::accent.withMultipliedAlpha (0.6f));
        g.fillRoundedRectangle (r.reduced (0.5f, 1.0f), 2.0f);
    }
    else if (drag == Drag::box)
    {
        const auto box = juce::Rectangle<float> (boxStart, boxEnd);
        g.setColour (Palette::accent.withAlpha (0.15f));
        g.fillRect (box);
        g.setColour (Palette::accent);
        g.drawRect (box, 1.0f);
    }

    if (playhead.section == sectionIndex)
    {
        const float x = (float) juce::jlimit (0.0, 1.0, playhead.beat / beats) * (float) getWidth();
        g.setColour (Palette::textPrimary);
        g.fillRect (x - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (9.0f));
    const char* targetName = target == Target::melody ? "MELODY" : (target == Target::bass ? "BASS" : "COUNTERMELODY");
    g.drawText (juce::String (targetName) + (chromaticMode ? "  CHROMATIC" : ""), getLocalBounds().reduced (4),
                juce::Justification::topRight);
}

//==============================================================================
void TunePianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    const double beat = beatAt ((float) e.x);
    const int pitch = pitchAt ((float) e.y);
    const int hit = findNoteAt (beat, pitch);

    if (selectionSection != session.getSelectedSection())
        selection.clear();

    if (e.mods.isPopupMenu())
    {
        // Right-click a note: its menu, for the selection it joins.
        if (hit >= 0 && std::find (selection.begin(), selection.end(), hit) == selection.end())
            selectNote (hit, false);

        if (hit < 0 && ! hasClipboard())
            return;

        if (hit < 0)
            pasteBeat = snapBeatToGrid (beat, grid);

        buildNoteMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                       [safe = juce::Component::SafePointer<TunePianoRoll> (this)] (int result)
        {
            if (safe != nullptr && result != 0)
                safe->performNoteMenuItem (result);
        });
        return;
    }

    if (! session.getTune().isValidSection (session.getSelectedSection()))
        return;

    if (hit >= 0)
    {
        if (e.mods.isShiftDown())
        {
            selectNote (hit, true);
            drag = Drag::none;
            return;
        }

        if (std::find (selection.begin(), selection.end(), hit) == selection.end())
            selectNote (hit, false);

        drag = Drag::move;
        moveBeats = 0.0;
        movePitchRows = 0;
        return;
    }

    pasteBeat = snapBeatToGrid (beat, grid);

    if (e.mods.isShiftDown() || ! drawEnabled)
    {
        drag = Drag::box;
        boxAdds = e.mods.isShiftDown();
        boxStart = boxEnd = e.position;
        repaint();
        return;
    }

    clearSelection();
    drag = Drag::draw;
    dragStart = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);
    dragEnd = dragStart + grid;
    dragPitch = chromaticMode ? pitch : snapPitchToKey (pitch, session.getTune().meta.keyTonic, session.getTune().meta.mode);
    repaint();
}

void TunePianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::draw:
            // 3.4: length sets the duration.
            dragEnd = juce::jmax (dragStart + grid, snapBeatToGrid (beatAt ((float) e.x), grid));
            break;

        case Drag::box:
            boxEnd = e.position;
            break;

        case Drag::move:
        {
            const double beats = sectionBeats();
            moveBeats = snapBeatToGrid ((double) e.getDistanceFromDragStartX() / juce::jmax (1, getWidth()) * beats, grid);
            const int rows = getHighestPitch() - getLowestPitch() + 1;
            movePitchRows = -juce::roundToInt ((float) e.getDistanceFromDragStartY() / ((float) getHeight() / (float) rows));
            break;
        }

        case Drag::none:
            return;
    }

    repaint();
}

void TunePianoRoll::mouseUp (const juce::MouseEvent&)
{
    const auto was = drag;
    drag = Drag::none;

    if (was == Drag::draw)
    {
        addNote (dragStart + grid * 0.5, dragPitch, dragEnd - dragStart);
    }
    else if (was == Drag::box)
    {
        selectInBox (juce::Rectangle<float> (boxStart, boxEnd), boxAdds);
    }
    else if (was == Drag::move && (std::abs (moveBeats) > 1.0e-9 || movePitchRows != 0))
    {
        // A drag moves by grid steps and by semitone rows, snapped to the key.
        const auto shown = getShownNotes();
        const auto chosen = selection;
        const auto& tune = session.getTune();
        const double length = sectionBeats();
        const double delta = moveBeats;
        std::vector<int> pitches;

        for (int i : chosen)
        {
            const int moved = juce::isPositiveAndBelow (i, (int) shown.size()) ? pitchOf (shown[(size_t) i]) + movePitchRows : 60;
            pitches.push_back (juce::jlimit (0, 127, chromaticMode ? moved : snapPitchToKey (moved, tune.meta.keyTonic, tune.meta.mode)));
        }

        const bool pitchMoved = movePitchRows != 0;

        editNotes (TuneEditClass::melodyEdit, "Move notes",
                   [chosen, pitches, delta, length, pitchMoved] (std::vector<MelodyNote>& notes, std::vector<int>& keep)
        {
            for (size_t k = 0; k < chosen.size(); ++k)
            {
                const int i = chosen[k];

                if (! juce::isPositiveAndBelow (i, (int) notes.size()))
                    continue;

                auto& n = notes[(size_t) i];
                n.startBeat = juce::jlimit (0.0, juce::jmax (0.0, length - n.durationBeats), n.startBeat + delta);

                if (pitchMoved)
                    n.pitch = MelodyPitch::absolute (pitches[k]);

                n.locked = true;
                keep.push_back (i);
            }

            return ! keep.empty();
        });
    }

    moveBeats = 0.0;
    movePitchRows = 0;
    repaint();
}

bool TunePianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey
          || code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        if (selection.empty())
            return false;

        const int dx = code == juce::KeyPress::leftKey ? -1 : (code == juce::KeyPress::rightKey ? 1 : 0);
        const int dy = code == juce::KeyPress::upKey ? 1 : (code == juce::KeyPress::downKey ? -1 : 0);
        nudgeSelected (dx, dy, mods.isShiftDown());
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        return deleteSelected();

    if (code == juce::KeyPress::escapeKey && ! selection.empty())
    {
        clearSelection();
        return true;
    }

    if (mods.isCommandDown())
    {
        const auto letter = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);

        if (letter == 'C') return copySelected();
        if (letter == 'X') return cutSelected();
        if (letter == 'V') return paste();
        if (letter == 'A') { selectAll(); return true; }
    }

    return false;
}

} // namespace luthier
