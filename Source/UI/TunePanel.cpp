#include "TunePanel.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneTemplates.h"
#include "../Tune/TuneExamples.h"

namespace luthier
{

namespace
{
    constexpr int kHeader = 26;
    constexpr int kRowGap = 6;
    constexpr int kStripHeight = 34;
    constexpr int kPillsHeight = 34;
    constexpr int kRollHeight = 200;
    constexpr int kErrorLine = 16;
    constexpr int kPositionLine = 18;

    // Undo grouping targets (action-and-undo 0.3): one per control, plus the
    // section index where the edit is to a section.
    constexpr int kTitleTarget = 3000;
    constexpr int kTempoTarget = 3001;
    constexpr int kProgressionTarget = 2000;
    constexpr int kFeelTarget = 4100;
    constexpr int kStrumTarget = 4200;

    // The New menu's example-tune items (onboarding 6).
    constexpr int kExampleMenuBase = 1000;

    /** The first span in play order that plays `sectionIndex`, or -1. */
    int firstSpanOf (const Tune& tune, int sectionIndex)
    {
        const auto spans = tune.getPlayOrder();

        for (size_t i = 0; i < spans.size(); ++i)
            if (spans[i].sectionIndex == sectionIndex)
                return (int) i;

        return -1;
    }

    /** The setlist repeats of a section (1 when it has no entry of its own). */
    int repeatsOf (const Tune& tune, int sectionIndex)
    {
        const auto* s = tune.getSection (sectionIndex);

        if (s == nullptr)
            return 1;

        for (const auto& e : tune.arrangement.setlist)
            if (e.section == s->name)
                return e.repeats;

        return 1;
    }

    bool isShortcut (const juce::KeyPress& key, juce::juce_wchar letter)
    {
        const auto code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());
        return key.getModifiers().isCommandDown() && code == letter;
    }
}

//==============================================================================
// Section strip
//==============================================================================
TuneSectionStrip::TuneSectionStrip (TuneSession& s) : session (s)
{
    setTooltip ("Click a section to edit it; right-click to rename, duplicate, delete, repeat or tag it");
    AccessibleSetup::configureDescriptive (*this, "Sections",
                                           "The tune's sections. Click one to edit it; right-click for its menu.");
}

juce::Colour TuneSectionStrip::colourForRole (SectionRole role)
{
    switch (role)
    {
        case SectionRole::intro:   return Palette::secondaryDim;
        case SectionRole::verse:   return Palette::accentDim;
        case SectionRole::chorus:  return Palette::accent;
        case SectionRole::bridge:  return Palette::secondary;
        case SectionRole::outro:   return Palette::edgeBright;
        case SectionRole::none:
        case SectionRole::numRoles: break;
    }

    return Palette::panelRaised;
}

juce::String TuneSectionStrip::getRoleName (SectionRole role)
{
    switch (role)
    {
        case SectionRole::none:    return "None";
        case SectionRole::intro:   return "Intro";
        case SectionRole::verse:   return "Verse";
        case SectionRole::chorus:  return "Chorus";
        case SectionRole::bridge:  return "Bridge";
        case SectionRole::outro:   return "Outro";
        case SectionRole::numRoles: break;
    }

    return {};
}

juce::Rectangle<int> TuneSectionStrip::getTabBounds (int index) const
{
    const int count = session.getTune().getNumSections() + 1;   // + the "+" tab
    const int gap = 4;
    const int width = juce::jmax (24, (getWidth() - gap * (count - 1)) / juce::jmax (1, count));

    return { index * (width + gap), 0, width, getHeight() };
}

int TuneSectionStrip::getTabAt (juce::Point<int> position) const
{
    const int count = session.getTune().getNumSections() + 1;

    for (int i = 0; i < count; ++i)
        if (getTabBounds (i).contains (position))
            return i;

    return -1;
}

void TuneSectionStrip::setPlayhead (const TunePlayhead& newPlayhead)
{
    if (newPlayhead != playhead)
    {
        playhead = newPlayhead;
        repaint();
    }
}

void TuneSectionStrip::paint (juce::Graphics& g)
{
    const auto& tune = session.getTune();

    for (int i = 0; i <= tune.getNumSections(); ++i)
    {
        const auto bounds = getTabBounds (i).toFloat().reduced (0.5f);

        if (i == tune.getNumSections())
        {
            g.setColour (Palette::panelSunken);
            g.fillRoundedRectangle (bounds, Metrics::controlCorner);
            g.setColour (Palette::edge);
            g.drawRoundedRectangle (bounds, Metrics::controlCorner, 1.0f);
            g.setColour (Palette::textMuted);
            g.setFont (Fonts::ui (14.0f, true));
            g.drawText ("+", bounds, juce::Justification::centred);
            continue;
        }

        const auto& section = tune.arrangement.sections[(size_t) i];
        const bool selected = i == session.getSelectedSection();
        const auto fill = colourForRole (section.role);

        g.setColour (selected ? fill : fill.withMultipliedAlpha (0.55f));
        g.fillRoundedRectangle (bounds, Metrics::controlCorner);
        g.setColour (selected ? Palette::textPrimary : Palette::edge);
        g.drawRoundedRectangle (bounds, Metrics::controlCorner, selected ? 1.5f : 1.0f);

        // gui-engine-dataflow 24: the playhead in the section strip.
        if (playhead.section == i)
        {
            const double beats = juce::jmax (1.0, tune.getSectionLengthBeats (i));
            const float x = bounds.getX() + bounds.getWidth() * (float) juce::jlimit (0.0, 1.0, playhead.beat / beats);

            g.setColour (Palette::textPrimary);
            g.fillRect (x - 1.0f, bounds.getY() + 2.0f, 2.0f, bounds.getHeight() - 4.0f);
        }

        const int repeats = repeatsOf (tune, i);
        auto text = section.name + " " + juce::String (section.lengthBars);

        if (repeats > 1)
            text << " x" << repeats;

        if (section.rhythmLinkedTo.isNotEmpty())
            text << " ~";

        g.setColour (fill.getPerceivedBrightness() > 0.55f && selected ? Palette::plateText : Palette::textPrimary);
        g.setFont (Fonts::ui (11.0f, selected));
        g.drawFittedText (text, bounds.toNearestInt().reduced (4, 2), juce::Justification::centred, 2);
    }
}

void TuneSectionStrip::mouseDown (const juce::MouseEvent& e)
{
    const int tab = getTabAt (e.getPosition());
    const int count = session.getTune().getNumSections();

    if (tab < 0)
        return;

    if (tab == count)
    {
        performMenuItem (-1, addItem);
        return;
    }

    session.setSelectedSection (tab);

    if (e.mods.isPopupMenu())
        buildMenu (tab).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                       [safe = juce::Component::SafePointer<TuneSectionStrip> (this), tab] (int result)
        {
            if (safe != nullptr && result != 0)
                safe->performMenuItem (tab, result);
        });

    repaint();
}

juce::PopupMenu TuneSectionStrip::buildMenu (int sectionIndex) const
{
    juce::PopupMenu menu;
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (sectionIndex);

    menu.addItem (renameItem, "Rename...");
    menu.addItem (duplicateItem, "Duplicate");
    menu.addItem (deleteItem, "Delete", section != nullptr);
    menu.addSeparator();

    // 3.3: "Repeat count (x1, x2, x4, custom)".
    juce::PopupMenu repeat;
    const int current = repeatsOf (tune, sectionIndex);
    bool standard = false;

    for (int n : { 1, 2, 3, 4, 8 })
    {
        repeat.addItem (repeatBase + n, "x" + juce::String (n), true, current == n);
        standard = standard || current == n;
    }

    repeat.addItem (customRepeatItem, standard ? juce::String ("Custom...") : "Custom (x" + juce::String (current) + ")...",
                    true, ! standard);
    menu.addSubMenu ("Repeat", repeat);

    // 3.3: "Set as intro / verse / chorus / bridge / outro (colour tag)".
    juce::PopupMenu role;

    for (int r = 0; r < (int) SectionRole::numRoles; ++r)
        role.addItem (roleBase + r, getRoleName ((SectionRole) r), true,
                      section != nullptr && (int) section->role == r);

    menu.addSubMenu ("Set as", role);

    // 3.5: "Link rhythm to X".
    juce::PopupMenu link;

    for (int i = 0; i < tune.getNumSections(); ++i)
        if (i != sectionIndex)
            link.addItem (linkBase + i, tune.arrangement.sections[(size_t) i].name, true,
                          section != nullptr && section->rhythmLinkedTo == tune.arrangement.sections[(size_t) i].name);

    link.addSeparator();
    link.addItem (unlinkItem, "Own rhythm", section != nullptr, section != nullptr && section->rhythmLinkedTo.isEmpty());
    menu.addSubMenu ("Link rhythm to", link, tune.getNumSections() > 1);

    // 8: "Section boundaries write a state boundary ... (per-section toggle)".
    menu.addItem (stateBoundaryItem, "Reset envelopes and rhythm at start", section != nullptr,
                  section != nullptr && section->stateBoundary);

    menu.addSeparator();
    menu.addItem (addItem, "Add section");
    return menu;
}

void TuneSectionStrip::performMenuItem (int sectionIndex, int itemId)
{
    if (itemId == renameItem)
    {
        promptRename (sectionIndex);
        return;
    }

    if (itemId == customRepeatItem)
    {
        promptRepeatCount (sectionIndex);
        return;
    }

    if (itemId == duplicateItem)
    {
        if (session.edit (TuneEditClass::sectionEdit, "Duplicate section",
                          [sectionIndex] (Tune& t) { return t.duplicateSection (sectionIndex) >= 0; }))
            session.setSelectedSection (sectionIndex + 1);
    }
    else if (itemId == deleteItem)
    {
        session.edit (TuneEditClass::sectionEdit, "Delete section",
                      [sectionIndex] (Tune& t) { return t.removeSection (sectionIndex); });
    }
    else if (itemId == addItem)
    {
        const auto* selected = session.getTune().getSection (session.getSelectedSection());

        TuneSection fresh;
        fresh.name = "Section";
        fresh.lengthBars = 4;

        // A new section starts with the rhythm of the one selected, so it plays.
        if (selected != nullptr)
        {
            fresh.rhythmPatternId = selected->rhythmPatternId;
            fresh.genreKitId = selected->genreKitId;
        }

        const bool added = session.edit (TuneEditClass::sectionEdit, "Add section", [fresh] (Tune& t)
        {
            if (t.addSection (fresh) < 0)
                return false;

            // With a setlist the new section would not play until it had an entry.
            if (! t.arrangement.setlist.empty())
            {
                TuneSetlistEntry entry;
                entry.section = t.arrangement.sections.back().name;
                t.arrangement.setlist.push_back (entry);
            }

            return true;
        });

        if (added)
            session.setSelectedSection (session.getTune().getNumSections() - 1);
    }
    else if (itemId > repeatBase && itemId <= repeatBase + 8)
    {
        setRepeatCount (sectionIndex, itemId - repeatBase);
    }
    else if (itemId >= roleBase && itemId < roleBase + (int) SectionRole::numRoles)
    {
        const auto role = (SectionRole) (itemId - roleBase);
        session.edit (TuneEditClass::sectionEdit, "Set section role",
                      [sectionIndex, role] (Tune& t) { return t.setSectionRole (sectionIndex, role); });
    }
    else if (itemId >= linkBase && itemId < linkBase + Tune::kMaxSections)
    {
        const auto* target = session.getTune().getSection (itemId - linkBase);

        if (target != nullptr)
        {
            const auto name = target->name;
            session.edit (TuneEditClass::sectionEdit, "Link rhythm",
                          [sectionIndex, name] (Tune& t) { return t.linkSectionRhythm (sectionIndex, name); });
        }
    }
    else if (itemId == unlinkItem)
    {
        session.edit (TuneEditClass::sectionEdit, "Unlink rhythm",
                      [sectionIndex] (Tune& t) { return t.linkSectionRhythm (sectionIndex, {}); });
    }
    else if (itemId == stateBoundaryItem)
    {
        session.edit (TuneEditClass::sectionEdit, "State boundary", [sectionIndex] (Tune& t)
        {
            auto* s = t.getSection (sectionIndex);

            if (s == nullptr)
                return false;

            s->stateBoundary = ! s->stateBoundary;
            return true;
        });
    }

    repaint();
}

bool TuneSectionStrip::renameSection (int sectionIndex, const juce::String& newName)
{
    return session.edit (TuneEditClass::sectionEdit, "Rename section",
                         [sectionIndex, newName] (Tune& t) { return t.renameSection (sectionIndex, newName.trim()); });
}

bool TuneSectionStrip::setRepeatCount (int sectionIndex, int repeats)
{
    const int clamped = juce::jlimit (1, Tune::kMaxRepeats, repeats);

    return session.edit (TuneEditClass::sectionEdit, "Repeat section",
                         [sectionIndex, clamped] (Tune& t) { return t.setRepeatCount (sectionIndex, clamped); });
}

void TuneSectionStrip::promptRename (int sectionIndex)
{
    const auto* section = session.getTune().getSection (sectionIndex);

    if (section == nullptr)
        return;

    auto* window = new juce::AlertWindow ("Rename section", "Name the section:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", section->name);
    window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // The window is deleted after its callback has run (deleteWhenDismissed).
    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<TuneSectionStrip> (this), window, sectionIndex] (int result)
    {
        if (safe != nullptr && result == 1)
            safe->renameSection (sectionIndex, window->getTextEditorContents ("name"));
    }), true);
}

void TuneSectionStrip::promptRepeatCount (int sectionIndex)
{
    if (! session.getTune().isValidSection (sectionIndex))
        return;

    auto* window = new juce::AlertWindow ("Repeat count",
                                          "How many times the section plays (1 to " + juce::String (Tune::kMaxRepeats) + "):",
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("repeats", juce::String (repeatsOf (session.getTune(), sectionIndex)));
    window->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<TuneSectionStrip> (this), window, sectionIndex] (int result)
    {
        const int repeats = window->getTextEditorContents ("repeats").getIntValue();

        if (safe != nullptr && result == 1 && repeats > 0)
            safe->setRepeatCount (sectionIndex, repeats);
    }), true);
}

//==============================================================================
// Chord pills
//==============================================================================
TuneChordPills::TuneChordPills (TuneSession& s) : session (s)
{
    AccessibleSetup::configureDescriptive (*this, "Chord pills",
                                           "The selected section's chords, coloured by their function in the key.");
}

juce::Colour TuneChordPills::colourForDegree (int degree)
{
    switch (degree)
    {
        case 1: return Palette::accent;
        case 2: return Palette::secondary;
        case 3: return Palette::success;
        case 4: return Palette::accentBright;
        case 5: return Palette::warning;
        case 6: return Palette::secondaryDim;
        case 7: return Palette::clip;
        default: break;
    }

    return Palette::edge;   // 3.2: "Non-diatonic chords use a neutral pill."
}

void TuneChordPills::setPlayhead (const TunePlayhead& newPlayhead)
{
    if (newPlayhead != playhead)
    {
        playhead = newPlayhead;
        repaint();
    }
}

void TuneChordPills::paint (juce::Graphics& g)
{
    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();
    const auto* section = tune.getSection (sectionIndex);

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::controlCorner);

    if (section == nullptr || section->chords.empty())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText ("No chords yet: type a progression above", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto spans = resolveChordSpans (*section, tune.getBeatsPerBar());
    const double total = juce::jmax (1.0, tune.getSectionLengthBeats (sectionIndex));
    const float width = (float) getWidth();

    for (const auto& span : spans)
    {
        const auto& cell = section->chords[(size_t) span.cellIndex];
        const int degree = getDiatonicDegree (cell, tune.meta.keyTonic, tune.meta.mode);
        const auto fill = colourForDegree (degree);

        juce::Rectangle<float> pill ((float) (span.startBeat / total) * width, 2.0f,
                                     (float) ((span.endBeat - span.startBeat) / total) * width, (float) getHeight() - 4.0f);
        pill = pill.reduced (1.5f, 0.0f);

        g.setColour (fill);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

        const auto label = getChordSymbol (cell, tune.preferFlats());
        const auto numeral = getRomanNumeral (cell, tune.meta.keyTonic, tune.meta.mode);

        g.setColour (fill.getPerceivedBrightness() > 0.55f ? Palette::plateText : Palette::textPrimary);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawFittedText (numeral.isEmpty() ? label : label + "  " + numeral,
                          pill.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
    }

    // gui-engine-dataflow 24: the beat marker in the chord-progression strip.
    if (playhead.section == sectionIndex)
    {
        const float x = (float) juce::jlimit (0.0, 1.0, playhead.beat / total) * width;
        g.setColour (Palette::textPrimary);
        g.fillRect (x - 1.0f, 0.0f, 2.0f, (float) getHeight());
    }
}

//==============================================================================
// Piano roll
//==============================================================================
TunePianoRoll::TunePianoRoll (TuneSession& s) : session (s)
{
    setTooltip ("Click and drag to draw a note; right-click a note to delete or lock it. C toggles chromatic.");
    AccessibleSetup::configureDescriptive (*this, "Melody piano roll",
                                           "The selected section's melody. Drag to draw a note; right-click a note for its menu.");
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

int TunePianoRoll::getLowestPitch() const
{
    int low = 48;

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
    {
        if (s->melody.has_value())
        {
            low = s->melody->rangeLow;

            for (const auto& n : s->melody->notes)
                low = juce::jmin (low, resolveNotePitch (session.getTune(), session.getSelectedSection(), n));
        }
    }

    return juce::jlimit (0, 115, low - 2);
}

int TunePianoRoll::getHighestPitch() const
{
    int high = 79;

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
    {
        if (s->melody.has_value())
        {
            high = s->melody->rangeHigh;

            for (const auto& n : s->melody->notes)
                high = juce::jmax (high, resolveNotePitch (session.getTune(), session.getSelectedSection(), n));
        }
    }

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
    const auto& tune = session.getTune();
    const auto* s = tune.getSection (session.getSelectedSection());

    if (s == nullptr || ! s->melody.has_value())
        return -1;

    // The last drawn wins where notes overlap: it is the one on top.
    for (int i = (int) s->melody->notes.size(); --i >= 0;)
    {
        const auto& n = s->melody->notes[(size_t) i];

        if (beat >= n.startBeat - 1.0e-6 && beat < n.getEndBeat() - 1.0e-6
              && resolveNotePitch (tune, session.getSelectedSection(), n) == pitch)
            return i;
    }

    return -1;
}

bool TunePianoRoll::addNote (double beat, int pitch, double durationBeats)
{
    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();

    if (! tune.isValidSection (sectionIndex))
        return false;

    // 3.4: the start snaps to the grid line at or before it, the pitch to the key.
    const double start = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);
    const double length = juce::jmax (grid, snapBeatToGrid (durationBeats, grid));
    const int snapped = chromaticMode ? pitch : snapPitchToKey (pitch, tune.meta.keyTonic, tune.meta.mode);

    if (start >= sectionBeats() - 1.0e-6)
        return false;

    const auto note = MelodyNote::make (start, juce::jmin (length, sectionBeats() - start), snapped, 100);

    return session.edit (TuneEditClass::melodyEdit, "Draw note",
                         [sectionIndex, note] (Tune& t) { return t.addMelodyNote (sectionIndex, note) >= 0; });
}

bool TunePianoRoll::deleteNoteAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);
    const int sectionIndex = session.getSelectedSection();

    if (index < 0)
        return false;

    return session.edit (TuneEditClass::melodyEdit, "Delete note",
                         [sectionIndex, index] (Tune& t) { return t.removeMelodyNotes (sectionIndex, { index }); });
}

bool TunePianoRoll::toggleLockAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);
    const int sectionIndex = session.getSelectedSection();

    if (index < 0)
        return false;

    const bool locked = session.getTune().getSection (sectionIndex)->melody->notes[(size_t) index].locked;

    return session.edit (TuneEditClass::melodyEdit, locked ? "Unlock note" : "Lock note",
                         [sectionIndex, index, locked] (Tune& t) { return t.setMelodyNoteLocked (sectionIndex, index, ! locked); },
                         index);
}

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

    if (section->melody.has_value())
    {
        for (const auto& n : section->melody->notes)
        {
            const int pitch = resolveNotePitch (tune, sectionIndex, n);
            const auto r = getNoteBounds (n.startBeat, pitch, n.durationBeats).reduced (0.5f, 1.0f);

            g.setColour (n.locked ? Palette::accentBright : Palette::secondary);
            g.fillRoundedRectangle (r, 2.0f);

            if (n.locked)
            {
                g.setColour (Palette::textPrimary);
                g.drawRoundedRectangle (r, 2.0f, 1.0f);
            }
        }

        if (section->melody->source == MelodySource::improvise)
        {
            g.setColour (Palette::textMuted);
            g.setFont (Fonts::ui (10.0f));
            g.drawText ("IMPROVISING: a new line every pass", getLocalBounds().reduced (6, 4), juce::Justification::bottomLeft);
        }
    }

    if (dragging)
    {
        const auto r = getNoteBounds (dragStart, dragPitch, juce::jmax (grid, dragEnd - dragStart));
        g.setColour (Palette::accent.withMultipliedAlpha (0.6f));
        g.fillRoundedRectangle (r.reduced (0.5f, 1.0f), 2.0f);
    }

    if (playhead.section == sectionIndex)
    {
        const float x = (float) juce::jlimit (0.0, 1.0, playhead.beat / beats) * (float) getWidth();
        g.setColour (Palette::textPrimary);
        g.fillRect (x - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }

    if (chromaticMode)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (9.0f));
        g.drawText ("CHROMATIC", getLocalBounds().reduced (4), juce::Justification::topRight);
    }
}

void TunePianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const double beat = beatAt ((float) e.x);
    const int pitch = pitchAt ((float) e.y);

    if (e.mods.isPopupMenu())
    {
        if (findNoteAt (beat, pitch) < 0)
            return;

        juce::PopupMenu menu;
        menu.addItem (1, "Delete");
        menu.addItem (2, "Lock / unlock");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<TunePianoRoll> (this), beat, pitch] (int result)
        {
            if (safe == nullptr)
                return;

            if (result == 1) safe->deleteNoteAt (beat, pitch);
            if (result == 2) safe->toggleLockAt (beat, pitch);
        });
        return;
    }

    if (! drawEnabled || ! session.getTune().isValidSection (session.getSelectedSection()))
        return;

    dragging = true;
    dragStart = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);
    dragEnd = dragStart + grid;
    dragPitch = chromaticMode ? pitch : snapPitchToKey (pitch, session.getTune().meta.keyTonic, session.getTune().meta.mode);
    repaint();
}

void TunePianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // 3.4: length sets the duration.
    dragEnd = juce::jmax (dragStart + grid, snapBeatToGrid (beatAt ((float) e.x), grid));
    repaint();
}

void TunePianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    addNote (dragStart + grid * 0.5, dragPitch, dragEnd - dragStart);
    repaint();
}

//==============================================================================
// The panel
//==============================================================================
TunePanel::TunePanel (LuthierAudioProcessor& p, TunePlayer& pl, TuneSession& s)
    : processor (p),
      player (pl),
      session (s),
      sectionStrip (s),
      chordPills (s),
      pianoRoll (s)
{
    setWantsKeyboardFocus (true);

    buildHeader();
    buildProgression();
    buildRhythm();
    buildMelody();
    buildTransport();

    // onboarding.md 8: hidden until owed.
    addChildComponent (firstHint);
    firstHint.onShownOrDismissed = [this]
    {
        setSize (getWidth(), getPreferredHeight());
        resized();
    };

    session.onChanged = [this] { refresh(); };

    // The processor normally does this; a session nobody has wired yet
    // (a test, a host that built the editor first) plays through this player.
    if (session.getPlayer() == nullptr)
    {
        session.attachPlayer (&player);
        session.attachRhythm (&processor.getEngine().getRhythmEngine(),
                              &processor.getGenreKits(), &processor.getPatternLibrary());
    }

    refresh();
    setSize (360, getPreferredHeight());
    startTimerHz (30);   // gui-engine-dataflow 24: the transport indicator drains at 30 Hz
}

TunePanel::~TunePanel()
{
    stopTimer();
    session.onChanged = nullptr;
}

//==============================================================================
void TunePanel::buildHeader()
{
    addAndMakeVisible (titleEditor);
    titleEditor.setTooltip ("The tune's title");
    AccessibleSetup::configureDescriptive (titleEditor, "Tune title", "The tune's title.");
    titleEditor.onTextChange = [this]
    {
        if (updating)
            return;

        const auto title = titleEditor.getText();
        session.edit (TuneEditClass::other, "Rename tune", [title] (Tune& t)
        {
            if (t.meta.title == title)
                return false;

            t.meta.title = title;
            return true;
        }, kTitleTarget);
    };

    for (auto* b : { &newButton, &loadButton, &saveButton, &exportButton })
        addAndMakeVisible (b);

    newButton.setTooltip ("A new tune from a template (Ctrl+T)");
    loadButton.setTooltip ("Open a .luthiertune");
    saveButton.setTooltip ("Save the tune (Ctrl+S)");
    exportButton.setTooltip ("Export the tune as a MIDI file (Ctrl+E)");

    AccessibleSetup::configureButton (newButton, "New tune", "Starts a new tune from a template.");
    AccessibleSetup::configureButton (loadButton, "Load tune");
    AccessibleSetup::configureButton (saveButton, "Save tune");
    AccessibleSetup::configureButton (exportButton, "Export tune as MIDI");

    newButton.onClick = [this] { showTemplateMenu(); };
    loadButton.onClick = [this] { chooseAndLoad(); };
    saveButton.onClick = [this] { chooseAndSave(); };
    exportButton.onClick = [this] { chooseAndExport(); };

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (Tune::kMinTempo, Tune::kMaxTempo, 1.0);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, Metrics::rowHeight);
    tempoSlider.setTooltip ("Tempo, beats per minute");
    AccessibleSetup::configureSlider (tempoSlider, "Tempo", " bpm");
    tempoSlider.onValueChange = [this]
    {
        if (updating)
            return;

        const double bpm = tempoSlider.getValue();
        session.edit (TuneEditClass::other, "Change tempo", [bpm] (Tune& t) { return t.setTempo (bpm); }, kTempoTarget);
    };

    addAndMakeVisible (keyBox);
    keyBox.setTooltip ("The key: changing it transposes the tune, melodies with it (5)");
    AccessibleSetup::configureComboBox (keyBox, "Key");

    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (tunetheory::spellPitchClass (pc, tunetheory::keyPrefersFlats (pc, TuneMode::ionian)), pc + 1);

    keyBox.onChange = [this]
    {
        if (updating || keyBox.getSelectedId() <= 0)
            return;

        const int tonic = keyBox.getSelectedId() - 1;
        int delta = tunetheory::wrapPitchClass (tonic - session.getTune().meta.keyTonic);

        if (delta > 6)
            delta -= 12;

        session.edit (TuneEditClass::chordEdit, "Transpose", [delta] (Tune& t) { return transposeTune (t, delta); });
    };

    addAndMakeVisible (modeBox);
    modeBox.setTooltip ("The mode: changing it moves the diatonic chords to the new mode (5)");
    AccessibleSetup::configureComboBox (modeBox, "Mode");

    for (int m = 0; m < (int) TuneMode::numModes; ++m)
    {
        const juce::String name (getTuneModeName ((TuneMode) m));
        modeBox.addItem (name.substring (0, 1).toUpperCase() + name.substring (1), m + 1);
    }

    modeBox.onChange = [this]
    {
        if (updating || modeBox.getSelectedId() <= 0)
            return;

        const auto mode = (TuneMode) (modeBox.getSelectedId() - 1);
        session.edit (TuneEditClass::chordEdit, "Change mode", [mode] (Tune& t) { return shiftMode (t, mode, false); });
    };
}

void TunePanel::buildProgression()
{
    addAndMakeVisible (sectionStrip);

    addAndMakeVisible (progressionEditor);
    progressionEditor.setTextToShowWhenEmpty ("Am F C G   or   [Verse] Am F C G [Chorus] F C G Am",
                                              Palette::textDisabled);
    progressionEditor.setTooltip ("Chords, one bar each: Am*2 for two bars, | Am F | for two in a bar, [Name] for a section");
    AccessibleSetup::configureDescriptive (progressionEditor, "Progression",
                                           "The section's chords in shorthand. Parsed as you type.");
    progressionEditor.onTextChange = [this] { progressionTextChanged(); };

    addAndMakeVisible (chordPills);
}

void TunePanel::buildRhythm()
{
    addAndMakeVisible (kitBox);
    kitBox.setTooltip ("The section's genre kit: its strum pattern, voicing and feel");
    AccessibleSetup::configureComboBox (kitBox, "Genre kit");

    const auto names = processor.getGenreKits().getNames();

    for (int i = 0; i < names.size(); ++i)
        kitBox.addItem (names[i], i + 1);

    kitBox.onChange = [this]
    {
        if (updating)
            return;

        const int index = kitBox.getSelectedId() - 1;
        const auto& kits = processor.getGenreKits();

        if (! juce::isPositiveAndBelow (index, kits.getNumKits()))
            return;

        const auto kit = kits.getKit (index);
        const auto pattern = ! kit.strumPatterns.isEmpty() ? kit.strumPatterns[0]
                           : (! kit.fingerpickPatterns.isEmpty() ? kit.fingerpickPatterns[0] : juce::String());
        const double density = getKitMelodyDensity (kit.name);

        editSection (TuneEditClass::sectionEdit, "Change genre kit", [kit, pattern, density] (TuneSection& s)
        {
            if (s.genreKitId == kit.name && s.rhythmPatternId == pattern)
                return false;

            s.genreKitId = kit.name;
            s.rhythmPatternId = pattern;

            // TuneMelody: the track's density follows its kit (4.1).
            if (s.melody.has_value())
                s.melody->density = density;

            return true;
        }, 4000);
    };

    for (auto* slider : { &feelSlider, &strumSlider })
    {
        addAndMakeVisible (slider);
        slider->setRange (0.0, 1.0, 0.01);
    }

    feelSlider.setTooltip ("Feel: how loosely the section is played (the kit as written in the middle)");
    strumSlider.setTooltip ("Strum: how long a strum takes to cross the strings");
    AccessibleSetup::configureSlider (feelSlider, "Feel");
    AccessibleSetup::configureSlider (strumSlider, "Strum");

    feelSlider.onValueChange = [this]
    {
        if (updating)
            return;

        const double v = tunetheory::canonical (feelSlider.getValue());
        editSection (TuneEditClass::sectionEdit, "Change feel",
                     [v] (TuneSection& s) { if (s.feel == v) return false; s.feel = v; return true; }, kFeelTarget);
    };

    strumSlider.onValueChange = [this]
    {
        if (updating)
            return;

        const double v = tunetheory::canonical (strumSlider.getValue());
        editSection (TuneEditClass::sectionEdit, "Change strum",
                     [v] (TuneSection& s) { if (s.strum == v) return false; s.strum = v; return true; }, kStrumTarget);
    };

    addAndMakeVisible (rhythmOn);
    rhythmOn.setTooltip ("The section's rhythm on or off");
    AccessibleSetup::configureButton (rhythmOn.getButton(), "Rhythm on", "Turns the section's rhythm on or off.");
    rhythmOn.getButton().onClick = [this]
    {
        const bool on = rhythmOn.getButton().getToggleState();
        const int index = session.getSelectedSection();
        session.edit (TuneEditClass::sectionEdit, "Rhythm on/off",
                      [index, on] (Tune& t) { return t.setSectionRhythmOn (index, on); });
    };
}

void TunePanel::buildMelody()
{
    addAndMakeVisible (pianoRoll);

    for (auto* b : { &autoButton, &freezeButton })
        addAndMakeVisible (b);

    for (auto* t : { &drawToggle, &recordToggle, &improviseToggle })
        addAndMakeVisible (t);

    drawToggle.getButton().setToggleState (true, juce::dontSendNotification);

    autoButton.setTooltip ("Write a melody that follows the chords; again for a new one");
    drawToggle.setTooltip ("Draw notes on the piano roll");
    recordToggle.setTooltip ("Record a take from MIDI in while the section plays; press again to quantise it in");
    improviseToggle.setTooltip ("A new melody on every pass of the loop");
    freezeButton.setTooltip ("Keep the pass just played as written notes");

    AccessibleSetup::configureButton (autoButton, "Auto melody", "Generates a melody that follows the chords.");
    AccessibleSetup::configureButton (drawToggle.getButton(), "Draw", "Draw notes on the piano roll.");
    AccessibleSetup::configureButton (recordToggle.getButton(), "Record", "Records a melody from MIDI in.");
    AccessibleSetup::configureButton (improviseToggle.getButton(), "Improvise", "A new melody on every loop pass.");
    AccessibleSetup::configureButton (freezeButton, "Freeze", "Keeps the improvised pass as written notes.");

    autoButton.onClick = [this]
    {
        const int index = session.getSelectedSection();
        const auto* s = session.getTune().getSection (index);

        if (s == nullptr)
            return;

        // 4.1: "Regenerate increments the seed"; action-and-undo 3.9 wants the
        // constraints in the description.
        const bool again = s->melody.has_value() && s->melody->source == MelodySource::autoGenerate;
        const int seed = s->melody.has_value() ? s->melody->seed + (again ? 1 : 0) : 1;
        const double density = s->melody.has_value() ? s->melody->density : getKitMelodyDensity (s->genreKitId);

        session.edit (TuneEditClass::melodyGenerate,
                      "Generate melody with seed " + juce::String (seed) + ", " + juce::String (density, 1) + " notes a bar",
                      [index, again] (Tune& t) { return again ? regenerateMelody (t, index) : generateMelody (t, index); });
    };

    drawToggle.getButton().onClick = [this] { pianoRoll.setDrawEnabled (drawToggle.getButton().getToggleState()); };

    recordToggle.getButton().onClick = [this]
    {
        if (recordToggle.getButton().getToggleState())
        {
            session.beginRecording (session.getSelectedSection());

            // A take is played against the section: start it if nothing plays.
            if (session.isRecording() && ! player.isPlaying())
                player.playFromSection (juce::jmax (0, firstSpanOf (session.getTune(), session.getSelectedSection())));
        }
        else
        {
            const auto grid = (QuantiseGrid) juce::jmax (0, quantiseBox.getSelectedId() - 1);
            const auto* s = session.getTune().getSection (session.getRecordingSection());
            const bool follow = s != nullptr && s->melody.has_value() && s->melody->followChords;
            session.finishRecording (grid, follow, ! pianoRoll.isChromatic());
        }

        refresh();
    };

    improviseToggle.getButton().onClick = [this]
    {
        const bool on = improviseToggle.getButton().getToggleState();
        const int index = session.getSelectedSection();

        session.edit (TuneEditClass::melodyGenerate, on ? "Improvise" : "Stop improvising", [index, on] (Tune& t)
        {
            auto* s = t.getSection (index);

            if (s == nullptr)
                return false;

            if (! s->melody.has_value())
                s->melody = MelodyTrack();

            const auto source = on ? MelodySource::improvise : MelodySource::draw;

            if (s->melody->source == source)
                return false;

            s->melody->source = source;
            return true;
        });
    };

    freezeButton.onClick = [this]
    {
        const int index = session.getSelectedSection();
        const int pass = player.getPass();

        session.edit (TuneEditClass::melodyRecord, "Record melody (Improvise freeze)",
                      [index, pass] (Tune& t) { return freezeImprovisedPass (t, index, pass); });
    };

    addAndMakeVisible (quantiseBox);
    quantiseBox.setTooltip ("The grid notes snap to when drawn or recorded");
    AccessibleSetup::configureComboBox (quantiseBox, "Quantise");

    for (int g = 0; g < (int) QuantiseGrid::numGrids; ++g)
        quantiseBox.addItem (getQuantiseGridName ((QuantiseGrid) g), g + 1);

    quantiseBox.setSelectedId ((int) QuantiseGrid::eighth + 1, juce::dontSendNotification);
    pianoRoll.setGridBeats (getQuantiseGridBeats (QuantiseGrid::eighth));
    quantiseBox.onChange = [this]
    {
        pianoRoll.setGridBeats (getQuantiseGridBeats ((QuantiseGrid) juce::jmax (0, quantiseBox.getSelectedId() - 1)));
    };
}

void TunePanel::buildTransport()
{
    for (auto* b : { &backButton, &playButton, &forwardButton })
        addAndMakeVisible (b);

    for (auto* t : { &loopToggle, &countInToggle, &metronomeToggle })
        addAndMakeVisible (t);

    backButton.setTooltip ("Back a section");
    playButton.setTooltip ("Play or pause (Space); Shift+Space plays this section from its start");
    forwardButton.setTooltip ("Forward a section");
    loopToggle.setTooltip ("Loop the whole tune");
    countInToggle.setTooltip ("A bar's count-in before playback");
    metronomeToggle.setTooltip ("Click on the tune's beats");

    AccessibleSetup::configureButton (backButton, "Back a section");
    AccessibleSetup::configureButton (playButton, "Play or pause", "Plays or pauses the tune. Space.");
    AccessibleSetup::configureButton (forwardButton, "Forward a section");
    AccessibleSetup::configureButton (loopToggle.getButton(), "Loop", "Loops the whole tune.");
    AccessibleSetup::configureButton (countInToggle.getButton(), "Count-in", "Counts in a bar before playback.");
    AccessibleSetup::configureButton (metronomeToggle.getButton(), "Metronome", "Clicks on the tune's beats.");

    backButton.onClick = [this] { player.skipSection (-1); };
    forwardButton.onClick = [this] { player.skipSection (1); };
    playButton.onClick = [this] { player.togglePlayPause(); updateTransport(); };

    loopToggle.getButton().setToggleState (player.isLooping(), juce::dontSendNotification);
    loopToggle.getButton().onClick = [this] { player.setLoop (loopToggle.getButton().getToggleState()); };

    countInToggle.getButton().setToggleState (player.getCountInBars() > 0, juce::dontSendNotification);
    countInToggle.getButton().onClick = [this] { player.setCountInBars (countInToggle.getButton().getToggleState() ? 1 : 0); };

    metronomeToggle.getButton().setToggleState (player.isMetronomeOn(), juce::dontSendNotification);
    metronomeToggle.getButton().onClick = [this] { player.setMetronome (metronomeToggle.getButton().getToggleState()); };
}

//==============================================================================
void TunePanel::editSection (TuneEditClass editClass, const juce::String& description,
                             const std::function<bool (TuneSection&)>& change, int targetBase)
{
    const int index = session.getSelectedSection();

    session.edit (editClass, description, [index, &change] (Tune& t)
    {
        auto* s = t.getSection (index);
        return s != nullptr && change (*s);
    }, targetBase + index);
}

void TunePanel::progressionTextChanged()
{
    if (updating)
        return;

    const auto text = progressionEditor.getText();
    const auto parsed = parseProgression (text, session.getTune().getBeatsPerBar());

    if (! parsed.ok())
    {
        // 15: a malformed progression is a named error, shown where it is, and
        // the section keeps the chords it had.
        progressionError = parsed.describe();
        progressionErrorPosition = parsed.position;
        progressionErrorLength = juce::jmax (1, parsed.token.length());
        repaint();
        return;
    }

    progressionError.clear();
    progressionErrorPosition = -1;

    const int index = session.getSelectedSection();

    session.edit (TuneEditClass::chordEdit, "Edit progression", [index, text] (Tune& t)
    {
        const auto before = t;
        applyProgressionText (t, index, text);
        return ! (t == before);
    }, kProgressionTarget + index);

    repaint();
}

//==============================================================================
void TunePanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto& tune = session.getTune();
    const int index = session.getSelectedSection();
    const auto* section = tune.getSection (index);

    if (! titleEditor.hasKeyboardFocus (true))
        titleEditor.setText (tune.meta.title, juce::dontSendNotification);

    tempoSlider.setValue (tune.meta.tempoBpm, juce::dontSendNotification);
    keyBox.setSelectedId (tune.meta.keyTonic + 1, juce::dontSendNotification);
    modeBox.setSelectedId ((int) tune.meta.mode + 1, juce::dontSendNotification);

    // The field shows the section's chords unless the player is typing in it.
    if (! progressionEditor.hasKeyboardFocus (true))
    {
        progressionEditor.setText (section != nullptr ? formatProgression (section->chords, tune.getBeatsPerBar(),
                                                                           tune.preferFlats())
                                                      : juce::String(),
                                   juce::dontSendNotification);
        progressionError.clear();
        progressionErrorPosition = -1;
    }

    kitBox.setSelectedId (section != nullptr ? processor.getGenreKits().indexOf (section->genreKitId) + 1 : 0,
                          juce::dontSendNotification);
    feelSlider.setValue (section != nullptr ? section->feel : 0.5, juce::dontSendNotification);
    strumSlider.setValue (section != nullptr ? section->strum : 0.5, juce::dontSendNotification);
    rhythmOn.getButton().setToggleState (section != nullptr && section->rhythmOn, juce::dontSendNotification);

    const bool improvising = section != nullptr && section->melody.has_value()
                               && section->melody->source == MelodySource::improvise;

    improviseToggle.getButton().setToggleState (improvising, juce::dontSendNotification);
    recordToggle.getButton().setToggleState (session.isRecording(), juce::dontSendNotification);

    for (auto* c : { static_cast<juce::Component*> (&kitBox), static_cast<juce::Component*> (&feelSlider),
                     static_cast<juce::Component*> (&strumSlider), static_cast<juce::Component*> (&rhythmOn),
                     static_cast<juce::Component*> (&autoButton), static_cast<juce::Component*> (&recordToggle),
                     static_cast<juce::Component*> (&improviseToggle) })
        c->setEnabled (section != nullptr);

    freezeButton.setEnabled (improvising);
    saveButton.setButtonText (session.isDirty() ? "SAVE *" : "SAVE");

    sectionStrip.repaint();
    chordPills.repaint();
    pianoRoll.repaint();
    repaint();
}

void TunePanel::timerCallback()
{
    updateTransport();

    if (isShowing())
        showFirstEncounterHintIfDue();
}

bool TunePanel::showFirstEncounterHintIfDue()
{
    return firstHint.showIfDue();
}

void TunePanel::updateTransport()
{
    session.service();

    // tune-builder 6: the bass plays through the instrument only when it is a bass.
    player.setBassToEngine (processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass);

    const bool playing = player.isPlaying();
    playButton.setButtonText (playing ? "PAUSE" : "PLAY");
    playButton.setToggleState (playing, juce::dontSendNotification);

    // gui-engine-dataflow 24: section index, beat in section, playing.
    const auto& tune = session.getTune();
    const auto spans = tune.getPlayOrder();
    const int span = player.getPlayingSpan();
    TunePlayhead now;

    if ((playing || player.getPositionPpq() > 0.0) && juce::isPositiveAndBelow (span, (int) spans.size()))
    {
        const auto& s = spans[(size_t) span];
        now.section = s.sectionIndex;
        now.beat = juce::jmax (0.0, player.getPositionPpq() - s.startBeat);
    }

    if (now != playhead)
    {
        playhead = now;
        sectionStrip.setPlayhead (playhead);
        chordPills.setPlayhead (playhead);
        pianoRoll.setPlayhead (playhead);
    }

    // "Verse  bar 3  beat 2" - where the playhead is, in the player's terms.
    juce::String text;

    if (player.isCountingIn())
    {
        text = "Count-in";
    }
    else if (playhead.section >= 0 && tune.isValidSection (playhead.section))
    {
        const double beatsPerBar = tune.getBeatsPerBar();
        const int bar = (int) std::floor (playhead.beat / beatsPerBar + 1.0e-9);
        const int beat = (int) std::floor (playhead.beat - bar * beatsPerBar + 1.0e-9);

        text = tune.arrangement.sections[(size_t) playhead.section].name
                 + "  bar " + juce::String (bar + 1) + "  beat " + juce::String (beat + 1);

        if (! playing)
            text << "  (paused)";
        else if (player.isFollowingHost())
            text << "  (host)";
    }
    else
    {
        text = "Stopped";
    }

    if (session.isRecording())
        text << "  REC " << session.getNumRecordedNotes();

    if (text != positionText)
    {
        positionText = text;
        repaint (positionBounds);
    }
}

bool TunePanel::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    if (key.getKeyCode() == juce::KeyPress::spaceKey && ! mods.isCommandDown())
    {
        if (mods.isShiftDown())
        {
            // 3.6: Shift+Space plays from the current section's start.
            const int span = player.isPlaying() && player.getPlayingSpan() >= 0
                               ? player.getPlayingSpan()
                               : firstSpanOf (session.getTune(), session.getSelectedSection());

            player.playFromSection (juce::jmax (0, span));
        }
        else
        {
            player.togglePlayPause();
        }

        updateTransport();
        return true;
    }

    if (isShortcut (key, 'S'))
    {
        chooseAndSave();
        return true;
    }

    if (isShortcut (key, 'E'))
    {
        chooseAndExport();
        return true;
    }

    if (isShortcut (key, 'T'))
    {
        showTemplateMenu();
        return true;
    }

    if (isShortcut (key, 'Z'))
        return mods.isShiftDown() ? session.redo() : session.undo();

    if (isShortcut (key, 'Y'))
        return session.redo();

    // 3.4: 'C' toggles chromatic. Not with a modifier: Ctrl+C is copy.
    const bool plain = ! mods.isCommandDown() && ! mods.isCtrlDown() && ! mods.isAltDown();

    if (plain && (key.getKeyCode() == 'C' || key.getKeyCode() == 'c'
                    || key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C'))
    {
        pianoRoll.setChromatic (! pianoRoll.isChromatic());
        return true;
    }

    return false;
}

//==============================================================================
bool TunePanel::saveTo (const juce::File& file, juce::String& error)
{
    return session.saveAs (file, error);
}

bool TunePanel::loadFrom (const juce::File& file, juce::String& error)
{
    player.stop();
    return session.load (file, error);
}

bool TunePanel::exportMidiTo (const juce::File& file, juce::String& error)
{
    TuneMidiFileOptions options;
    options.midi = session.getMidiOptions();
    return writeTuneMidiFile (session.getTune(), file, options, error);
}

void TunePanel::newFromTemplate (int templateIndex)
{
    const auto templates = TuneTemplateLibrary::loadFactory();

    player.stop();

    if (juce::isPositiveAndBelow (templateIndex, (int) templates.size()))
        session.newTune (TuneTemplateLibrary::instantiate (templates[(size_t) templateIndex].tune));
    else
        session.newTune (TuneTemplateLibrary::instantiate (TuneTemplateLibrary::createBlank()));
}

bool TunePanel::openExample (int exampleIndex)
{
    const auto examples = TuneExamples::loadExamples();

    if (! juce::isPositiveAndBelow (exampleIndex, (int) examples.size()))
        return false;

    player.stop();
    session.newTune (examples[(size_t) exampleIndex].tune);
    return true;
}

void TunePanel::showError (const juce::String& title, const juce::String& message)
{
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::WarningIcon)
                                           .withTitle (title)
                                           .withMessage (message)
                                           .withButton ("OK"),
                                       nullptr);
}

void TunePanel::showTemplateMenu()
{
    const auto templates = TuneTemplateLibrary::loadFactory();
    juce::PopupMenu menu;

    for (size_t i = 0; i < templates.size(); ++i)
        menu.addItem ((int) i + 1, templates[i].name);

    // Blank is built in code, so New works without the Resources folder.
    if (templates.empty())
        menu.addItem (1, "Blank");

    // onboarding 6: the example tunes, ready to play.
    const auto examples = TuneExamples::loadExamples();

    if (! examples.empty())
    {
        juce::PopupMenu exampleMenu;

        for (size_t i = 0; i < examples.size(); ++i)
            exampleMenu.addItem (kExampleMenuBase + (int) i, examples[i].name);

        menu.addSeparator();
        menu.addSubMenu ("Example tunes", exampleMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&newButton),
                        [safe = juce::Component::SafePointer<TunePanel> (this)] (int result)
    {
        if (safe != nullptr && result >= kExampleMenuBase)
            safe->openExample (result - kExampleMenuBase);
        else if (safe != nullptr && result > 0)
            safe->newFromTemplate (result - 1);
    });
}

void TunePanel::chooseAndLoad()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a tune", TuneFile::getUserDirectory(), "*.luthiertune");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<TunePanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;

        if (! safe->loadFrom (file, error))
            safe->showError ("Could not open the tune", error);
    });
}

void TunePanel::chooseAndSave()
{
    if (session.getFile() != juce::File())
    {
        juce::String error;

        if (! session.save (error))
            showError ("Could not save the tune", error);

        return;
    }

    const auto name = juce::File::createLegalFileName (session.getTune().meta.title.isNotEmpty()
                                                         ? session.getTune().meta.title : juce::String ("Untitled Tune"));

    chooser = std::make_unique<juce::FileChooser> ("Save the tune",
                                                   TuneFile::getUserDirectory().getChildFile (name + TuneFile::kFileExtension),
                                                   "*.luthiertune");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<TunePanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;

        if (! safe->saveTo (file.withFileExtension (TuneFile::kFileExtension), error))
            safe->showError ("Could not save the tune", error);
    });
}

void TunePanel::chooseAndExport()
{
    const auto name = juce::File::createLegalFileName (session.getTune().meta.title.isNotEmpty()
                                                         ? session.getTune().meta.title : juce::String ("Untitled Tune"));

    chooser = std::make_unique<juce::FileChooser> ("Export the tune as MIDI",
                                                   PresetManager::getRenderFolder().getChildFile (name + ".mid"),
                                                   "*.mid;*.midi");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<TunePanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;

        if (! safe->exportMidiTo (file, error))
            safe->showError ("Could not export the tune", error);
    });
}

//==============================================================================
int TunePanel::getPreferredHeight() const
{
    const int button = Metrics::buttonHeight;

    return Metrics::grid
         + (firstHint.isVisible() ? FirstEncounterHint::kHeight + kRowGap : 0)   // onboarding 8
         + 3 * (button + kRowGap)                                     // header: title, buttons, tempo/key
         + kHeader + kStripHeight + kRowGap                           // sections
         + kHeader + button + kErrorLine + kPillsHeight + kRowGap     // progression
         + kHeader + 2 * (button + kRowGap)                           // rhythm
         + kHeader + kRollHeight + kRowGap + 2 * (button + kRowGap)   // melody
         + kHeader + button + kRowGap + kPositionLine                 // transport
         + Metrics::grid;
}

void TunePanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    g.setFont (Fonts::ui (13.0f, true));
    g.setColour (Palette::accent);
    g.drawText ("TUNE", titleLabelBounds, juce::Justification::centredLeft);

    LuthierLookAndFeel::drawSectionHeader (g, sectionsHeader, "SECTIONS");
    LuthierLookAndFeel::drawSectionHeader (g, progressionHeader, "PROGRESSION");
    LuthierLookAndFeel::drawSectionHeader (g, rhythmHeader, "RHYTHM");
    LuthierLookAndFeel::drawSectionHeader (g, melodyHeader, "MELODY");
    LuthierLookAndFeel::drawSectionHeader (g, transportHeader, "TRANSPORT");

    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);
    g.drawText ("TEMPO", tempoLabelBounds, juce::Justification::centredLeft);
    g.drawText ("FEEL", feelLabelBounds, juce::Justification::centredLeft);
    g.drawText ("STRUM", strumLabelBounds, juce::Justification::centredLeft);
    g.drawText ("QUANTISE", quantiseLabelBounds, juce::Justification::centredLeft);

    // The parse error, under the field (2.2, 15).
    const auto errorBounds = juce::Rectangle<int> (progressionEditor.getX(), progressionEditor.getBottom(),
                                                   progressionEditor.getWidth(), kErrorLine);
    g.setFont (Fonts::ui (10.0f));
    g.setColour (progressionError.isEmpty() ? Palette::textDisabled : Palette::warning);
    g.drawText (progressionError.isEmpty() ? juce::String ("Bars: one chord each, *2 doubles, | Am F | splits")
                                           : progressionError,
                errorBounds, juce::Justification::centredLeft);

    g.setFont (Fonts::mono (11.0f));
    g.setColour (player.isPlaying() ? Palette::accent : Palette::textMuted);
    g.drawText (positionText, positionBounds, juce::Justification::centredLeft);
}

void TunePanel::paintOverChildren (juce::Graphics& g)
{
    if (progressionErrorPosition < 0)
        return;

    // Underline the offending token where it sits in the field.
    const auto rects = progressionEditor.getTextBounds ({ progressionErrorPosition,
                                                          progressionErrorPosition + progressionErrorLength });

    g.setColour (Palette::warning);

    for (const auto& r : rects)
    {
        const auto area = r.translated (progressionEditor.getX(), progressionEditor.getY());
        g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 2);
    }
}

void TunePanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid);
    const int button = Metrics::buttonHeight;

    auto row = [&bounds, button]
    {
        auto r = bounds.removeFromTop (button);
        bounds.removeFromTop (kRowGap);
        return r;
    };

    auto split = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> items, int gap = 4)
    {
        const int n = (int) items.size();
        const int w = (r.getWidth() - gap * (n - 1)) / juce::jmax (1, n);

        for (auto* c : items)
        {
            c->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (gap);
        }
    };

    // onboarding.md 8: the first-encounter hint at the very top.
    if (firstHint.isVisible())
    {
        firstHint.setBounds (bounds.removeFromTop (FirstEncounterHint::kHeight));
        bounds.removeFromTop (kRowGap);
    }

    // --- header (3.1: "TUNE [My New Tune] [Save] [Export] Tempo [120] Key [C]") ---------
    {
        auto r = row();
        titleLabelBounds = r.removeFromLeft (44);
        titleEditor.setBounds (r);
    }

    split (row(), { &newButton, &loadButton, &saveButton, &exportButton });

    {
        auto r = row();
        tempoLabelBounds = r.removeFromLeft (48);
        auto tempoArea = r.removeFromLeft (r.getWidth() / 2);
        tempoSlider.setBounds (tempoArea.withTrimmedRight (4));
        split (r, { &keyBox, &modeBox });
    }

    // --- sections ----------------------------------------------------------------------
    sectionsHeader = bounds.removeFromTop (kHeader);
    sectionStrip.setBounds (bounds.removeFromTop (kStripHeight));
    bounds.removeFromTop (kRowGap);

    // --- progression -------------------------------------------------------------------
    progressionHeader = bounds.removeFromTop (kHeader);
    progressionEditor.setBounds (bounds.removeFromTop (button));
    bounds.removeFromTop (kErrorLine);
    chordPills.setBounds (bounds.removeFromTop (kPillsHeight));
    bounds.removeFromTop (kRowGap);

    // --- rhythm ------------------------------------------------------------------------
    rhythmHeader = bounds.removeFromTop (kHeader);

    {
        auto r = row();
        rhythmOn.setBounds (r.removeFromRight (56));
        r.removeFromRight (4);
        kitBox.setBounds (r);
    }

    {
        auto r = row();
        auto left = r.removeFromLeft (r.getWidth() / 2);
        feelLabelBounds = left.removeFromLeft (44);
        feelSlider.setBounds (left.withTrimmedRight (4));
        strumLabelBounds = r.removeFromLeft (48);
        strumSlider.setBounds (r);
    }

    // --- melody ------------------------------------------------------------------------
    melodyHeader = bounds.removeFromTop (kHeader);
    pianoRoll.setBounds (bounds.removeFromTop (kRollHeight));
    bounds.removeFromTop (kRowGap);
    split (row(), { &autoButton, &drawToggle, &recordToggle, &improviseToggle, &freezeButton });

    {
        auto r = row();
        quantiseLabelBounds = r.removeFromLeft (64);
        quantiseBox.setBounds (r.removeFromLeft (96));
    }

    // --- transport ---------------------------------------------------------------------
    transportHeader = bounds.removeFromTop (kHeader);
    split (row(), { &backButton, &playButton, &forwardButton, &loopToggle, &countInToggle, &metronomeToggle });
    positionBounds = bounds.removeFromTop (kPositionLine);
}

} // namespace luthier
