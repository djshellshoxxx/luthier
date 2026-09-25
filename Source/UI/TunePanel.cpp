#include "TunePanel.h"
#include "TuneExportPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Localisation.h"
#include "../Tune/TuneImport.h"
#include "../Tune/TuneTemplates.h"

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
    setTooltip (tr ("tune.sections.tooltip"));
    AccessibleSetup::configureDescriptive (*this, tr ("tune.sections"), tr ("tune.sections.tooltip"));
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
        auto fill = colourForRole (section.role);

        if (dragging && i == dragTab)
            fill = fill.withMultipliedAlpha (0.4f);

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

    // 3.3 "Drag sections to reorder": where the dragged tab would land.
    if (dragging && dropTarget >= 0 && dropTarget != dragTab && dropTarget < tune.getNumSections())
    {
        const auto slot = getTabBounds (dropTarget).toFloat();
        const float x = dropTarget > dragTab ? slot.getRight() + 2.0f : slot.getX() - 2.0f;
        g.setColour (Palette::textPrimary);
        g.fillRect (x - 1.5f, 0.0f, 3.0f, (float) getHeight());
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
    {
        buildMenu (tab).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                       [safe = juce::Component::SafePointer<TuneSectionStrip> (this), tab] (int result)
        {
            if (safe != nullptr && result != 0)
                safe->performMenuItem (tab, result);
        });
    }
    else
    {
        dragTab = tab;
        dropTarget = tab;
        dragging = false;
    }

    repaint();
}

void TuneSectionStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTab < 0)
        return;

    if (! dragging && e.getDistanceFromDragStart() > 6)
        dragging = true;

    if (! dragging)
        return;

    const int over = getTabAt ({ e.x, juce::jlimit (0, getHeight() - 1, e.y) });

    if (over >= 0 && over < session.getTune().getNumSections())
        dropTarget = over;

    repaint();
}

void TuneSectionStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dropTarget >= 0 && dropTarget != dragTab)
        reorder (dragTab, dropTarget);

    dragging = false;
    dragTab = -1;
    dropTarget = -1;
    repaint();
}

bool TuneSectionStrip::reorder (int fromIndex, int toIndex)
{
    const bool moved = session.edit (TuneEditClass::sectionEdit, tr ("tune.sections.edit.move"), [fromIndex, toIndex] (Tune& t)
    {
        if (! t.moveSection (fromIndex, toIndex))
            return false;

        // The tabs are the play order: a setlist follows them, keeping each
        // entry's repeats, so what is seen is what is heard.
        if (! t.arrangement.setlist.empty())
        {
            auto setlist = t.arrangement.setlist;
            std::stable_sort (setlist.begin(), setlist.end(), [&t] (const TuneSetlistEntry& a, const TuneSetlistEntry& b)
            {
                return t.findSection (a.section) < t.findSection (b.section);
            });
            t.arrangement.setlist = std::move (setlist);
        }

        return true;
    });

    if (moved)
        session.setSelectedSection (toIndex);

    return moved;
}

int TuneSectionStrip::vary (int sectionIndex)
{
    const auto patterns = session.getKitPatterns (sectionIndex);
    int created = -1;

    session.edit (TuneEditClass::sectionEdit, tr ("tune.sections.edit.vary"), [sectionIndex, patterns, &created] (Tune& t)
    {
        created = varySection (t, sectionIndex, patterns);
        return created >= 0;
    });

    if (created >= 0)
        session.setSelectedSection (created);

    return created;
}

juce::PopupMenu TuneSectionStrip::buildMenu (int sectionIndex) const
{
    juce::PopupMenu menu;
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (sectionIndex);

    menu.addItem (renameItem, "Rename...");
    menu.addItem (duplicateItem, "Duplicate");
    menu.addItem (varyItem, tr ("tune.sections.menu.vary"), section != nullptr);
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
    else if (itemId == varyItem)
    {
        vary (sectionIndex);
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
// Chord editor (the popover)
//==============================================================================
TuneChordEditor::TuneChordEditor (TuneSession& s, int section, int cell)
    : session (s), sectionIndex (section), cellIndex (cell)
{
    setSize (kWidth, kHeight);
    AccessibleSetup::configureDescriptive (*this, tr ("tune.chord.editor"), tr ("tune.chord.editor.tooltip"));

    const bool flats = session.getTune().preferFlats();

    addAndMakeVisible (rootBox);
    rootBox.setTooltip (tr ("tune.chord.root.tooltip"));
    AccessibleSetup::configureComboBox (rootBox, tr ("tune.chord.root"));

    for (int pc = 0; pc < 12; ++pc)
        rootBox.addItem (tunetheory::spellPitchClass (pc, flats), pc + 1);

    rootBox.onChange = [this]
    {
        if (updating || rootBox.getSelectedId() <= 0)
            return;

        const int root = rootBox.getSelectedId() - 1;
        apply (tr ("tune.chord.edit.root"), [root] (ChordCell& c) { if (c.root == root) return false; c.root = root; return true; });
    };

    addAndMakeVisible (qualityBox);
    qualityBox.setTooltip (tr ("tune.chord.quality.tooltip"));
    AccessibleSetup::configureComboBox (qualityBox, tr ("tune.chord.quality"));

    const auto qualities = getQualityChoices();

    for (int i = 0; i < qualities.size(); ++i)
        qualityBox.addItem (qualities[i].isEmpty() ? tr ("tune.chord.quality.major") : qualities[i], i + 1);

    qualityBox.onChange = [this, qualities]
    {
        if (updating || qualityBox.getSelectedId() <= 0)
            return;

        const auto quality = qualities[qualityBox.getSelectedId() - 1];
        apply (tr ("tune.chord.edit.quality"), [quality] (ChordCell& c) { if (c.quality == quality) return false; c.quality = quality; return true; });
    };

    addAndMakeVisible (bassBox);
    bassBox.setTooltip (tr ("tune.chord.bass.tooltip"));
    AccessibleSetup::configureComboBox (bassBox, tr ("tune.chord.bass"));
    bassBox.addItem (tr ("tune.chord.bass.root"), 1);

    for (int pc = 0; pc < 12; ++pc)
        bassBox.addItem ("/" + tunetheory::spellPitchClass (pc, flats), pc + 2);

    bassBox.onChange = [this]
    {
        if (updating || bassBox.getSelectedId() <= 0)
            return;

        const int bass = bassBox.getSelectedId() - 2;
        apply (tr ("tune.chord.edit.bass"), [bass] (ChordCell& c) { if (c.bass == bass) return false; c.bass = bass; return true; });
    };

    addAndMakeVisible (beatsSlider);
    beatsSlider.setRange (0.5, 32.0, 0.5);
    beatsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    beatsSlider.setTooltip (tr ("tune.chord.beats.tooltip"));
    AccessibleSetup::configureSlider (beatsSlider, tr ("tune.chord.beats"));
    beatsSlider.onValueChange = [this]
    {
        if (updating)
            return;

        const double beats = tunetheory::canonical (beatsSlider.getValue());
        apply (tr ("tune.chord.edit.beats"), [beats] (ChordCell& c) { if (c.durationBeats == beats) return false; c.durationBeats = beats; return true; });
    };

    addAndMakeVisible (fillToggle);
    fillToggle.setButtonText (tr ("tune.chord.fill"));
    fillToggle.setTooltip (tr ("tune.chord.fill.tooltip"));
    AccessibleSetup::configureButton (fillToggle, tr ("tune.chord.fill"), tr ("tune.chord.fill.tooltip"));
    fillToggle.onClick = [this]
    {
        if (updating)
            return;

        const bool fill = fillToggle.getToggleState();
        const double beats = tunetheory::canonical (beatsSlider.getValue());
        apply (tr ("tune.chord.edit.beats"), [fill, beats] (ChordCell& c)
        {
            const double wanted = fill ? 0.0 : juce::jmax (0.5, beats);

            if (c.durationBeats == wanted)
                return false;

            c.durationBeats = wanted;
            return true;
        });
    };

    addAndMakeVisible (extensionsEditor);
    extensionsEditor.setTooltip (tr ("tune.chord.extensions.tooltip"));
    AccessibleSetup::configureDescriptive (extensionsEditor, tr ("tune.chord.extensions"), tr ("tune.chord.extensions.tooltip"));
    extensionsEditor.onReturnKey = [this] { extensionsEditor.onFocusLost(); };
    extensionsEditor.onFocusLost = [this]
    {
        if (updating)
            return;

        juce::StringArray extensions;
        extensions.addTokens (extensionsEditor.getText(), ", ", "");
        extensions.trim();
        extensions.removeEmptyStrings();

        for (const auto& e : extensions)
            if (tunetheory::getExtensionSemitones (e) < 0)
                return refresh();   // an unknown extension: the field goes back to what the cell has

        apply (tr ("tune.chord.edit.extensions"), [extensions] (ChordCell& c)
        {
            if (c.extensions == extensions)
                return false;

            c.extensions = extensions;
            return true;
        });
    };

    addAndMakeVisible (emphasisBox);
    emphasisBox.setTooltip (tr ("tune.chord.emphasis.tooltip"));
    AccessibleSetup::configureComboBox (emphasisBox, tr ("tune.chord.emphasis"));

    for (int e = 0; e < (int) ChordEmphasis::numEmphases; ++e)
        emphasisBox.addItem (tr (juce::String ("tune.chord.emphasis.") + getChordEmphasisName ((ChordEmphasis) e)), e + 1);

    emphasisBox.onChange = [this]
    {
        if (updating || emphasisBox.getSelectedId() <= 0)
            return;

        const auto emphasis = (ChordEmphasis) (emphasisBox.getSelectedId() - 1);
        apply (tr ("tune.chord.edit.emphasis"), [emphasis] (ChordCell& c) { if (c.emphasis == emphasis) return false; c.emphasis = emphasis; return true; });
    };

    addAndMakeVisible (lockToggle);
    lockToggle.setButtonText (tr ("tune.chord.lock"));
    lockToggle.setTooltip (tr ("tune.chord.lock.tooltip"));
    AccessibleSetup::configureButton (lockToggle, tr ("tune.chord.lock"), tr ("tune.chord.lock.tooltip"));
    lockToggle.onClick = [this]
    {
        if (updating)
            return;

        const bool locked = lockToggle.getToggleState();
        apply (locked ? tr ("tune.chord.edit.lock") : tr ("tune.chord.edit.unlock"),
               [locked] (ChordCell& c) { if (c.locked == locked) return false; c.locked = locked; return true; });
    };

    refresh();
}

juce::StringArray TuneChordEditor::getQualityChoices()
{
    static const juce::StringArray candidates { "", "m", "7", "maj7", "m7", "m7b5", "dim", "dim7", "aug", "sus2", "sus4",
                                               "7sus4", "6", "m6", "9", "maj9", "m9", "add9", "5", "6/9", "11", "13",
                                               "7b9", "7#9", "mMaj7" };
    juce::StringArray choices;

    for (const auto& candidate : candidates)
    {
        juce::String canonical;

        if (candidate.isEmpty())
            choices.add ("");
        else if (tunetheory::resolveQuality (candidate, canonical))
            choices.addIfNotAlreadyThere (canonical);
    }

    return choices;
}

const ChordCell* TuneChordEditor::cell() const
{
    const auto* s = session.getTune().getSection (sectionIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (cellIndex, (int) s->chords.size()))
        return nullptr;

    return &s->chords[(size_t) cellIndex];
}

void TuneChordEditor::apply (const juce::String& description, const std::function<bool (ChordCell&)>& change)
{
    const int section = sectionIndex, index = cellIndex;

    // action-and-undo 3.9: same chord cell within 200 ms is one entry.
    session.edit (TuneEditClass::chordEdit, description, [section, index, &change] (Tune& t)
    {
        auto* s = t.getSection (section);

        if (s == nullptr || ! juce::isPositiveAndBelow (index, (int) s->chords.size()))
            return false;

        return change (s->chords[(size_t) index]);
    }, 2500 + section * 64 + index);

    refresh();
}

void TuneChordEditor::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto* c = cell();

    if (c == nullptr)
    {
        setEnabled (false);
        return;
    }

    setEnabled (true);
    rootBox.setSelectedId (c->root + 1, juce::dontSendNotification);

    const auto qualities = getQualityChoices();
    const int quality = qualities.indexOf (c->quality);
    qualityBox.setSelectedId (quality >= 0 ? quality + 1 : 0, juce::dontSendNotification);

    if (quality < 0)
        qualityBox.setText (c->quality, juce::dontSendNotification);

    bassBox.setSelectedId (c->bass >= 0 ? c->bass + 2 : 1, juce::dontSendNotification);
    fillToggle.setToggleState (c->holdsToFill(), juce::dontSendNotification);
    beatsSlider.setEnabled (! c->holdsToFill());

    if (! c->holdsToFill())
        beatsSlider.setValue (c->durationBeats, juce::dontSendNotification);

    if (! extensionsEditor.hasKeyboardFocus (true))
        extensionsEditor.setText (c->extensions.joinIntoString (", "), false);

    emphasisBox.setSelectedId ((int) c->emphasis + 1, juce::dontSendNotification);
    lockToggle.setToggleState (c->locked, juce::dontSendNotification);
    repaint();
}

void TuneChordEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    const auto* c = cell();
    g.setFont (Fonts::ui (13.0f, true));
    g.setColour (Palette::accent);
    g.drawText (c != nullptr ? getChordSymbol (*c, session.getTune().preferFlats()) : juce::String(),
                getLocalBounds().removeFromTop (24).reduced (8, 0), juce::Justification::centredLeft);

    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);
    g.drawText (tr ("tune.chord.root"), rootLabel, juce::Justification::centredLeft);
    g.drawText (tr ("tune.chord.quality"), qualityLabel, juce::Justification::centredLeft);
    g.drawText (tr ("tune.chord.bass"), bassLabel, juce::Justification::centredLeft);
    g.drawText (tr ("tune.chord.beats"), beatsLabel, juce::Justification::centredLeft);
    g.drawText (tr ("tune.chord.extensions"), extensionsLabel, juce::Justification::centredLeft);
    g.drawText (tr ("tune.chord.emphasis"), emphasisLabel, juce::Justification::centredLeft);
}

void TuneChordEditor::resized()
{
    auto bounds = getLocalBounds().reduced (8, 4);
    bounds.removeFromTop (22);

    auto row = [&bounds]
    {
        auto r = bounds.removeFromTop (Metrics::rowHeight);
        bounds.removeFromTop (4);
        return r;
    };

    {
        auto r = row();
        rootLabel = r.removeFromLeft (64);
        rootBox.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (6);
        qualityLabel = r.removeFromLeft (52);
        qualityBox.setBounds (r);
    }

    {
        auto r = row();
        bassLabel = r.removeFromLeft (64);
        bassBox.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (6);
        emphasisLabel = r.removeFromLeft (52);
        emphasisBox.setBounds (r);
    }

    {
        auto r = row();
        beatsLabel = r.removeFromLeft (64);
        fillToggle.setBounds (r.removeFromRight (64));
        beatsSlider.setBounds (r);
    }

    {
        auto r = row();
        extensionsLabel = r.removeFromLeft (64);
        extensionsEditor.setBounds (r);
    }

    lockToggle.setBounds (row());
}

//==============================================================================
// Chord pills
//==============================================================================
std::optional<ChordCell> TuneChordPills::clipboard;

TuneChordPills::TuneChordPills (TuneSession& s) : session (s)
{
    setTooltip (tr ("tune.pills.tooltip"));
    AccessibleSetup::configureDescriptive (*this, tr ("tune.pills"), tr ("tune.pills.tooltip"));
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

double TuneChordPills::beatsPerPixel() const
{
    const auto& tune = session.getTune();
    return juce::jmax (1.0, tune.getSectionLengthBeats (session.getSelectedSection())) / juce::jmax (1.0, (double) getWidth());
}

std::vector<TuneChordPills::Pill> TuneChordPills::layoutPills() const
{
    std::vector<Pill> pills;
    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();
    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr || section->chords.empty())
        return pills;

    const auto spans = resolveChordSpans (*section, tune.getBeatsPerBar());
    const double total = juce::jmax (1.0, tune.getSectionLengthBeats (sectionIndex));
    const float width = (float) getWidth();

    for (const auto& span : spans)
    {
        Pill pill;
        pill.cellIndex = span.cellIndex;
        pill.bounds = juce::Rectangle<float> ((float) (span.startBeat / total) * width, 2.0f,
                                              (float) ((span.endBeat - span.startBeat) / total) * width,
                                              (float) getHeight() - 4.0f).reduced (1.5f, 0.0f);
        pills.push_back (pill);
    }

    return pills;
}

int TuneChordPills::getCellAt (juce::Point<int> position) const
{
    for (const auto& pill : layoutPills())
        if (pill.bounds.contains (position.toFloat()))
            return pill.cellIndex;

    return -1;
}

juce::Rectangle<float> TuneChordPills::getPillBounds (int cellIndex) const
{
    for (const auto& pill : layoutPills())
        if (pill.cellIndex == cellIndex)
            return pill.bounds;

    return {};
}

bool TuneChordPills::isOnRightEdge (juce::Point<int> position) const
{
    for (const auto& pill : layoutPills())
        if (pill.bounds.contains (position.toFloat()))
            return position.x >= pill.bounds.getRight() - 6.0f && pill.bounds.getWidth() > 16.0f;

    return false;
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
        g.drawText (tr ("tune.pills.empty"), getLocalBounds(), juce::Justification::centred);
        return;
    }

    const double total = juce::jmax (1.0, tune.getSectionLengthBeats (sectionIndex));
    const float width = (float) getWidth();

    for (const auto& pill : layoutPills())
    {
        const auto& cell = section->chords[(size_t) pill.cellIndex];
        const int degree = getDiatonicDegree (cell, tune.meta.keyTonic, tune.meta.mode);
        auto fill = colourForDegree (degree);
        auto bounds = pill.bounds;

        // The duration drag shows the pill at the length it is being dragged to.
        if (dragMode == DragMode::duration && pill.cellIndex == dragCell)
            bounds.setWidth (juce::jmax (8.0f, (float) (dragBeats / total) * width));

        if (dragMode == DragMode::reorder && pill.cellIndex == dragCell)
            fill = fill.withMultipliedAlpha (0.5f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

        // Ground rule accessibility 0.2: the lock is a mark, not a colour.
        if (cell.locked)
        {
            g.setColour (Palette::textPrimary);
            g.drawRoundedRectangle (bounds.reduced (0.75f), bounds.getHeight() * 0.5f, 1.5f);
        }

        const auto label = getChordSymbol (cell, tune.preferFlats());
        const auto numeral = getRomanNumeral (cell, tune.meta.keyTonic, tune.meta.mode);
        auto text = numeral.isEmpty() ? label : label + "  " + numeral;

        if (cell.locked)
            text = "* " + text;

        g.setColour (fill.getPerceivedBrightness() > 0.55f ? Palette::plateText : Palette::textPrimary);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawFittedText (text, bounds.toNearestInt().reduced (4, 0), juce::Justification::centred, 1);
    }

    // The drop slot of a reorder.
    if (dragMode == DragMode::reorder && dropTarget >= 0)
    {
        const auto pills = layoutPills();
        float x = width;

        for (const auto& pill : pills)
        {
            if (pill.cellIndex == dropTarget)
            {
                x = dropTarget > dragCell ? pill.bounds.getRight() : pill.bounds.getX();
                break;
            }
        }

        g.setColour (Palette::textPrimary);
        g.fillRect (x - 1.5f, 0.0f, 3.0f, (float) getHeight());
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
ChordCell TuneChordPills::newCellLike (const ChordCell& cell) const
{
    const auto& tune = session.getTune();
    auto fresh = makeTonicChord (tune.meta.keyTonic, tune.meta.mode);
    fresh.durationBeats = cell.holdsToFill() ? tune.getBeatsPerBar() : cell.durationBeats;
    fresh.strumOverride = cell.strumOverride;
    fresh.emphasis = cell.emphasis;
    return fresh;
}

juce::PopupMenu TuneChordPills::buildMenu (int cellIndex) const
{
    juce::PopupMenu menu;
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (session.getSelectedSection());
    const bool valid = section != nullptr && juce::isPositiveAndBelow (cellIndex, (int) section->chords.size());
    const auto* cell = valid ? &section->chords[(size_t) cellIndex] : nullptr;

    menu.addItem (editItem, tr ("tune.pills.menu.edit"), valid);
    menu.addSeparator();
    menu.addItem (insertBeforeItem, tr ("tune.pills.menu.insertBefore"), valid);
    menu.addItem (insertAfterItem, tr ("tune.pills.menu.insertAfter"), valid);
    menu.addItem (duplicateItem, tr ("tune.pills.menu.duplicate"), valid);
    menu.addItem (deleteItem, tr ("tune.pills.menu.delete"), valid);
    menu.addSeparator();
    menu.addItem (copyItem, tr ("tune.pills.menu.copy"), valid);
    menu.addItem (pasteItem, tr ("tune.pills.menu.paste"), clipboard.has_value());
    menu.addItem (lockItem, tr ("tune.pills.menu.lock"), valid, cell != nullptr && cell->locked);

    if (cell != nullptr)
    {
        // 3.2 "Suggest substitution"; a locked cell offers none.
        juce::PopupMenu substitutions;
        const auto offers = suggestSubstitutions (section->chords, cellIndex, tune.meta.keyTonic, tune.meta.mode);

        for (size_t i = 0; i < offers.size(); ++i)
        {
            juce::StringArray symbols;

            for (const auto& c : offers[i].cells)
                symbols.add (getChordSymbol (c, tune.preferFlats()));

            substitutions.addItem (substitutionBase + (int) i, offers[i].name + ": " + symbols.joinIntoString (" "));
        }

        menu.addSeparator();
        menu.addSubMenu (tr ("tune.pills.menu.substitute"), substitutions, ! cell->locked && ! offers.empty());

        // 5 "Suggest next chord": appended after this cell.
        juce::PopupMenu next;
        const auto suggestions = suggestNextChords (cell, tune.meta.keyTonic, tune.meta.mode, section->genreKitId,
                                                    cell->holdsToFill() ? tune.getBeatsPerBar() : cell->durationBeats);

        for (size_t i = 0; i < suggestions.size(); ++i)
            next.addItem (suggestBase + (int) i, getChordSymbol (suggestions[i], tune.preferFlats()));

        menu.addSubMenu (tr ("tune.pills.menu.suggestNext"), next, ! suggestions.empty());
    }

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

    if (itemId == pasteItem && clipboard.has_value())
    {
        // Paste replaces the cell under the mouse, or appends with nothing there.
        const auto pasted = *clipboard;
        session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.paste"), [sectionIndex, cellIndex, pasted, valid] (Tune& t)
        {
            return valid ? t.setChord (sectionIndex, cellIndex, pasted) : t.insertChord (sectionIndex, -1, pasted);
        });
        return;
    }

    if (! valid)
        return;

    const auto cell = section->chords[(size_t) cellIndex];

    switch (itemId)
    {
        case editItem:
            openEditor (cellIndex);
            return;

        case insertBeforeItem:
        case insertAfterItem:
        {
            const int at = itemId == insertBeforeItem ? cellIndex : cellIndex + 1;
            const auto fresh = newCellLike (cell);

            if (session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.insert"),
                              [sectionIndex, at, fresh] (Tune& t) { return t.insertChord (sectionIndex, at, fresh); }))
                openEditor (at);

            return;
        }

        case duplicateItem:
        {
            auto copy = cell;
            copy.locked = false;
            session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.duplicate"),
                          [sectionIndex, cellIndex, copy] (Tune& t) { return t.insertChord (sectionIndex, cellIndex + 1, copy); });
            return;
        }

        case deleteItem:
            session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.delete"),
                          [sectionIndex, cellIndex] (Tune& t) { return t.removeChord (sectionIndex, cellIndex); });
            return;

        case copyItem:
            clipboard = cell;
            return;

        case lockItem:
        {
            const bool locked = ! cell.locked;
            session.edit (TuneEditClass::chordEdit, locked ? tr ("tune.chord.edit.lock") : tr ("tune.chord.edit.unlock"),
                          [sectionIndex, cellIndex, locked] (Tune& t) { return t.setChordLocked (sectionIndex, cellIndex, locked); });
            return;
        }

        default:
            break;
    }

    if (itemId >= substitutionBase && itemId < suggestBase)
    {
        const auto offers = suggestSubstitutions (section->chords, cellIndex, tune.meta.keyTonic, tune.meta.mode);
        const size_t which = (size_t) (itemId - substitutionBase);

        if (which >= offers.size() || cell.locked)
            return;

        const auto cells = offers[which].cells;
        session.edit (TuneEditClass::chordEdit, offers[which].name, [sectionIndex, cellIndex, cells] (Tune& t)
        {
            auto* s = t.getSection (sectionIndex);

            if (s == nullptr || ! juce::isPositiveAndBelow (cellIndex, (int) s->chords.size()))
                return false;

            auto chords = s->chords;
            chords.erase (chords.begin() + cellIndex);
            chords.insert (chords.begin() + cellIndex, cells.begin(), cells.end());
            return t.setChords (sectionIndex, std::move (chords));
        });
        return;
    }

    if (itemId >= suggestBase)
    {
        const auto suggestions = suggestNextChords (&cell, tune.meta.keyTonic, tune.meta.mode, section->genreKitId,
                                                    cell.holdsToFill() ? tune.getBeatsPerBar() : cell.durationBeats);
        const size_t which = (size_t) (itemId - suggestBase);

        if (which >= suggestions.size())
            return;

        const auto next = suggestions[which];
        session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.suggestNext"),
                      [sectionIndex, cellIndex, next] (Tune& t) { return t.insertChord (sectionIndex, cellIndex + 1, next); });
    }
}

bool TuneChordPills::reorder (int fromIndex, int toIndex)
{
    const int sectionIndex = session.getSelectedSection();

    return session.edit (TuneEditClass::chordEdit, tr ("tune.pills.edit.move"),
                         [sectionIndex, fromIndex, toIndex] (Tune& t) { return t.moveChord (sectionIndex, fromIndex, toIndex); });
}

bool TuneChordPills::setDuration (int cellIndex, double beats)
{
    const int sectionIndex = session.getSelectedSection();
    const double snapped = tunetheory::canonical (juce::jmax (0.5, std::round (beats * 2.0) / 2.0));

    return session.edit (TuneEditClass::chordEdit, tr ("tune.chord.edit.beats"),
                         [sectionIndex, cellIndex, snapped] (Tune& t) { return t.setChordDuration (sectionIndex, cellIndex, snapped); },
                         2500 + sectionIndex * 64 + cellIndex);
}

void TuneChordPills::openEditor (int cellIndex)
{
    const auto bounds = getPillBounds (cellIndex);

    // A CallOutBox is a window of its own: only for pills that are on screen
    // (the console test runner drives TuneChordEditor directly).
    if (bounds.isEmpty() || ! isShowing())
        return;

    auto editor = std::make_unique<TuneChordEditor> (session, session.getSelectedSection(), cellIndex);
    juce::CallOutBox::launchAsynchronously (std::move (editor), localAreaToGlobal (bounds.toNearestInt()), nullptr);
}

//==============================================================================
void TuneChordPills::mouseDown (const juce::MouseEvent& e)
{
    const int cell = getCellAt (e.getPosition());
    dragMode = DragMode::none;
    dragCell = cell;
    dropTarget = -1;
    dragOrigin = e.getPosition();

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

    if (cell < 0)
        return;

    if (isOnRightEdge (e.getPosition()))
    {
        const auto* section = session.getTune().getSection (session.getSelectedSection());
        const auto& c = section->chords[(size_t) cell];
        dragMode = DragMode::duration;
        dragBeats = c.holdsToFill() ? juce::jmax (0.5, getPillBounds (cell).getWidth() * beatsPerPixel()) : c.durationBeats;
        return;
    }

    dragMode = DragMode::pending;
}

void TuneChordPills::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none || dragCell < 0)
        return;

    if (dragMode == DragMode::duration)
    {
        const auto bounds = getPillBounds (dragCell);
        dragBeats = juce::jmax (0.5, std::round (((float) e.x - bounds.getX()) * beatsPerPixel() * 2.0) / 2.0);
        repaint();
        return;
    }

    if (dragMode == DragMode::pending && e.getDistanceFromDragStart() > 6)
        dragMode = DragMode::reorder;

    if (dragMode == DragMode::reorder)
    {
        const int over = getCellAt ({ e.x, juce::jlimit (0, getHeight() - 1, e.y) });
        dropTarget = over >= 0 ? over : dropTarget;
        repaint();
    }
}

void TuneChordPills::mouseUp (const juce::MouseEvent& e)
{
    const auto mode = dragMode;
    dragMode = DragMode::none;

    if (mode == DragMode::duration)
    {
        setDuration (dragCell, dragBeats);
    }
    else if (mode == DragMode::reorder)
    {
        if (dropTarget >= 0 && dropTarget != dragCell)
            reorder (dragCell, dropTarget);
    }
    else if (mode == DragMode::pending && dragCell >= 0 && ! e.mods.isPopupMenu())
    {
        // A click: 3.2 "Click a cell to open a popover".
        openEditor (dragCell);
    }

    dropTarget = -1;
    repaint();
}

void TuneChordPills::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isOnRightEdge (e.getPosition()) ? juce::MouseCursor::LeftRightResizeCursor
                                                    : juce::MouseCursor::NormalCursor);
}

//==============================================================================
// Piano roll
//==============================================================================
const int TunePianoRoll::kVelocityPresets[TunePianoRoll::kNumVelocityPresets] = { 40, 64, 80, 100, 127 };
std::vector<MelodyNote> TunePianoRoll::clipboard;

TunePianoRoll::TunePianoRoll (TuneSession& s) : session (s)
{
    setWantsKeyboardFocus (true);
    setTooltip (tr ("tune.roll.tooltip"));
    AccessibleSetup::configureDescriptive (*this, tr ("tune.roll"), tr ("tune.roll.tooltip"));
}

double TunePianoRoll::sectionBeats() const
{
    return juce::jmax (1.0, session.getTune().getSectionLengthBeats (session.getSelectedSection()));
}

juce::Rectangle<float> TunePianoRoll::rollArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) kKeyboardWidth);
}

void TunePianoRoll::setPlayhead (const TunePlayhead& newPlayhead)
{
    if (newPlayhead != playhead)
    {
        playhead = newPlayhead;
        repaint();
    }
}

//==============================================================================
void TunePianoRoll::setPart (TuneNotePart newPart)
{
    if (part == newPart)
        return;

    part = newPart;
    selection.clear();
    dragMode = DragMode::none;
    repaint();
}

const std::vector<MelodyNote>* TunePianoRoll::getNotes() const
{
    return session.getTune().getPartNotes (session.getSelectedSection(), part);
}

bool TunePianoRoll::isShowingDerivedBass() const
{
    const auto* s = session.getTune().getSection (session.getSelectedSection());
    return part == TuneNotePart::bass && s != nullptr && s->bass.mode != BassMode::manual && s->bass.mode != BassMode::off;
}

std::vector<MelodyNote> TunePianoRoll::getShownNotes() const
{
    if (isShowingDerivedBass())
        return generateBassLine (session.getTune(), session.getSelectedSection());

    if (const auto* notes = getNotes())
        return *notes;

    return {};
}

bool TunePianoRoll::editNotes (const juce::String& description,
                               const std::function<bool (std::vector<MelodyNote>&)>& change, int target)
{
    const int sectionIndex = session.getSelectedSection();

    if (! session.getTune().isValidSection (sectionIndex))
        return false;

    auto notes = getShownNotes();

    if (! change (notes))
        return false;

    const auto editClass = part == TuneNotePart::melody ? TuneEditClass::melodyEdit : TuneEditClass::sectionEdit;
    const auto which = part;

    return session.edit (editClass, description, [sectionIndex, which, notes] (Tune& t)
    {
        return t.setPartNotes (sectionIndex, which, notes);
    }, target);
}

void TunePianoRoll::clampSelection()
{
    const int count = (int) getShownNotes().size();

    for (auto it = selection.begin(); it != selection.end();)
        it = (*it < 0 || *it >= count) ? selection.erase (it) : std::next (it);
}

//==============================================================================
int TunePianoRoll::getLowestPitch() const
{
    int low = 48;

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
    {
        switch (part)
        {
            case TuneNotePart::melody:        low = s->melody.has_value() ? s->melody->rangeLow : 48; break;
            case TuneNotePart::bass:          low = 28; break;   // E1
            case TuneNotePart::countermelody: low = (s->melody.has_value() ? s->melody->rangeLow : 48) - 5; break;
            case TuneNotePart::numParts:      break;
        }

        for (const auto& n : getShownNotes())
            low = juce::jmin (low, resolveNotePitch (session.getTune(), session.getSelectedSection(), n));
    }

    return juce::jlimit (0, 115, low - 2);
}

int TunePianoRoll::getHighestPitch() const
{
    int high = 79;

    if (const auto* s = session.getTune().getSection (session.getSelectedSection()))
    {
        switch (part)
        {
            case TuneNotePart::melody:        high = s->melody.has_value() ? s->melody->rangeHigh : 79; break;
            case TuneNotePart::bass:          high = 55; break;   // G3
            case TuneNotePart::countermelody: high = (s->melody.has_value() ? s->melody->rangeLow : 48) + 19; break;
            case TuneNotePart::numParts:      break;
        }

        for (const auto& n : getShownNotes())
            high = juce::jmax (high, resolveNotePitch (session.getTune(), session.getSelectedSection(), n));
    }

    return juce::jlimit (getLowestPitch() + 12, 127, high + 2);
}

double TunePianoRoll::beatAt (float x) const
{
    const auto area = rollArea();
    const double fraction = (double) (x - area.getX()) / juce::jmax (1.0, (double) area.getWidth());
    return juce::jlimit (0.0, sectionBeats(), viewStart + fraction * visibleBeats);
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
    const auto area = rollArea();
    const float x = area.getX() + (float) ((beat - viewStart) / visibleBeats) * area.getWidth();
    const float w = (float) (durationBeats / visibleBeats) * area.getWidth();

    return { x, (float) (low + rows - 1 - pitch) * rowHeight, juce::jmax (2.0f, w), rowHeight };
}

int TunePianoRoll::findNoteAt (double beat, int pitch) const
{
    const auto& tune = session.getTune();
    const auto notes = getShownNotes();

    // The last drawn wins where notes overlap: it is the one on top.
    for (int i = (int) notes.size(); --i >= 0;)
    {
        const auto& n = notes[(size_t) i];

        if (beat >= n.startBeat - 1.0e-6 && beat < n.getEndBeat() - 1.0e-6
              && resolveNotePitch (tune, session.getSelectedSection(), n) == pitch)
            return i;
    }

    return -1;
}

//==============================================================================
void TunePianoRoll::setView (double startBeat, double beatsVisible)
{
    const double total = sectionBeats();
    visibleBeats = juce::jlimit (1.0, juce::jmax (1.0, total), beatsVisible);
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, total - visibleBeats), startBeat);
    repaint();
}

void TunePianoRoll::zoom (double factor)
{
    const double centre = viewStart + visibleBeats * 0.5;
    const double beats = visibleBeats / juce::jmax (0.01, factor);
    setView (centre - beats * 0.5, beats);
}

void TunePianoRoll::scrollBy (double beats)
{
    setView (viewStart + beats, visibleBeats);
}

void TunePianoRoll::showBeat (double beat)
{
    if (beat < viewStart)
        setView (beat, visibleBeats);
    else if (beat >= viewStart + visibleBeats)
        setView (beat - visibleBeats + 1.0, visibleBeats);
}

//==============================================================================
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

    auto note = MelodyNote::make (start, juce::jmin (length, sectionBeats() - start), snapped, 100);
    note.locked = true;   // a manual edit: regenerate leaves it alone

    int index = -1;

    if (! editNotes (tr ("tune.roll.edit.draw"), [&note, &index] (std::vector<MelodyNote>& notes)
    {
        notes.push_back (note);
        index = (int) notes.size() - 1;
        return true;
    }))
        return false;

    selection = { index };
    return true;
}

bool TunePianoRoll::deleteNoteAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);

    if (index < 0)
        return false;

    selection = { index };
    return deleteSelected();
}

bool TunePianoRoll::toggleLockAt (double beat, int pitch)
{
    const int index = findNoteAt (beat, pitch);

    if (index < 0)
        return false;

    const bool locked = getShownNotes()[(size_t) index].locked;
    selection = { index };
    return setSelectedLocked (! locked);
}

//==============================================================================
void TunePianoRoll::select (int noteIndex, bool addToSelection)
{
    if (! addToSelection)
        selection.clear();

    if (noteIndex >= 0 && noteIndex < (int) getShownNotes().size())
    {
        // Shift-click on a selected note takes it out again.
        if (addToSelection && selection.count (noteIndex) > 0)
            selection.erase (noteIndex);
        else
            selection.insert (noteIndex);
    }

    repaint();
}

void TunePianoRoll::selectAll()
{
    selection.clear();
    const int count = (int) getShownNotes().size();

    for (int i = 0; i < count; ++i)
        selection.insert (i);

    repaint();
}

void TunePianoRoll::clearSelection()
{
    selection.clear();
    repaint();
}

void TunePianoRoll::selectInBox (double fromBeat, double toBeat, int lowPitch, int highPitch, bool addToSelection)
{
    if (! addToSelection)
        selection.clear();

    const auto& tune = session.getTune();
    const auto notes = getShownNotes();
    const double b0 = juce::jmin (fromBeat, toBeat), b1 = juce::jmax (fromBeat, toBeat);
    const int p0 = juce::jmin (lowPitch, highPitch), p1 = juce::jmax (lowPitch, highPitch);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const int pitch = resolveNotePitch (tune, session.getSelectedSection(), n);

        // A note is in the box when any of it is.
        if (n.getEndBeat() > b0 - 1.0e-6 && n.startBeat < b1 + 1.0e-6 && pitch >= p0 && pitch <= p1)
            selection.insert ((int) i);
    }

    repaint();
}

bool TunePianoRoll::deleteSelected()
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto doomed = selection;

    const bool done = editNotes (tr ("tune.roll.edit.delete"), [&doomed] (std::vector<MelodyNote>& notes)
    {
        std::vector<MelodyNote> kept;

        for (size_t i = 0; i < notes.size(); ++i)
            if (doomed.count ((int) i) == 0)
                kept.push_back (notes[i]);

        notes = std::move (kept);
        return true;
    });

    selection.clear();
    return done;
}

bool TunePianoRoll::setSelectedLocked (bool locked)
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto which = selection;
    const int target = selection.size() == 1 ? 5000 + *selection.begin() : -1;

    return editNotes (locked ? tr ("tune.roll.edit.lock") : tr ("tune.roll.edit.unlock"), [&which, locked] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            if (notes[(size_t) i].locked != locked)
            {
                notes[(size_t) i].locked = locked;
                changed = true;
            }
        }

        return changed;
    }, target);
}

int TunePianoRoll::stepPitch (int pitch, int steps) const
{
    if (chromaticMode)
        return juce::jlimit (0, 127, pitch + steps);

    const auto& tune = session.getTune();
    return tunetheory::moveByScaleSteps (pitch, steps, tune.meta.keyTonic, tune.meta.mode);
}

bool TunePianoRoll::nudgeSelected (double deltaBeats, int deltaSteps)
{
    clampSelection();

    if (selection.empty() || (deltaBeats == 0.0 && deltaSteps == 0))
        return false;

    const auto which = selection;
    const double total = sectionBeats();
    const int target = selection.size() == 1 ? 5000 + *selection.begin() : -1;

    return editNotes (tr ("tune.roll.edit.nudge"), [&, this] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            auto& n = notes[(size_t) i];
            const auto before = n;

            n.startBeat = juce::jlimit (0.0, juce::jmax (0.0, total - grid), n.startBeat + deltaBeats);
            n.durationBeats = juce::jmin (n.durationBeats, total - n.startBeat);

            // Relative pitches keep their meaning and move only in time (TuneModel).
            if (deltaSteps != 0 && n.pitch.isAbsolute())
                n.pitch.value = stepPitch (n.pitch.value, deltaSteps);

            n.locked = true;
            changed = changed || n != before;
        }

        return changed;
    }, target);
}

bool TunePianoRoll::setSelectedVelocity (int velocity)
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto which = selection;
    const int v = juce::jlimit (1, 127, velocity);
    const int target = selection.size() == 1 ? 5000 + *selection.begin() : -1;

    return editNotes (tr ("tune.roll.edit.velocity"), [&which, v] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            if (notes[(size_t) i].velocity != v)
            {
                notes[(size_t) i].velocity = v;
                notes[(size_t) i].locked = true;
                changed = true;
            }
        }

        return changed;
    }, target);
}

bool TunePianoRoll::adjustSelectedVelocity (int delta)
{
    clampSelection();

    if (selection.empty() || delta == 0)
        return false;

    const auto which = selection;
    const int target = selection.size() == 1 ? 5000 + *selection.begin() : -1;

    return editNotes (tr ("tune.roll.edit.velocity"), [&which, delta] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            const int v = juce::jlimit (1, 127, notes[(size_t) i].velocity + delta);

            if (notes[(size_t) i].velocity != v)
            {
                notes[(size_t) i].velocity = v;
                notes[(size_t) i].locked = true;
                changed = true;
            }
        }

        return changed;
    }, target);
}

bool TunePianoRoll::setSelectedArticulation (NoteArticulation articulation)
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto which = selection;

    return editNotes (tr ("tune.roll.edit.articulation"), [&which, articulation] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            if (notes[(size_t) i].articulation != articulation)
            {
                notes[(size_t) i].articulation = articulation;
                notes[(size_t) i].locked = true;
                changed = true;
            }
        }

        return changed;
    });
}

bool TunePianoRoll::setSelectedTechnique (NoteTechnique technique)
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto which = selection;

    return editNotes (tr ("tune.roll.edit.technique"), [&which, technique] (std::vector<MelodyNote>& notes)
    {
        bool changed = false;

        for (int i : which)
        {
            if (notes[(size_t) i].technique != technique)
            {
                notes[(size_t) i].technique = technique;
                notes[(size_t) i].locked = true;
                changed = true;
            }
        }

        return changed;
    });
}

//==============================================================================
bool TunePianoRoll::copySelected()
{
    clampSelection();

    if (selection.empty())
        return false;

    const auto notes = getShownNotes();
    clipboard.clear();

    for (int i : selection)
        clipboard.push_back (notes[(size_t) i]);

    std::stable_sort (clipboard.begin(), clipboard.end(),
                      [] (const MelodyNote& a, const MelodyNote& b) { return a.startBeat < b.startBeat; });
    return true;
}

bool TunePianoRoll::cutSelected()
{
    return copySelected() && deleteSelected();
}

bool TunePianoRoll::paste()
{
    if (clipboard.empty())
        return false;

    const double offset = snapBeatToGrid (cursorBeat, grid) - clipboard.front().startBeat;
    const double total = sectionBeats();
    std::vector<int> pasted;

    const bool done = editNotes (tr ("tune.roll.edit.paste"), [&, this] (std::vector<MelodyNote>& notes)
    {
        for (auto n : clipboard)
        {
            n.startBeat = n.startBeat + offset;

            if (n.startBeat >= total - 1.0e-6)
                continue;

            n.durationBeats = juce::jmin (n.durationBeats, total - n.startBeat);
            n.locked = true;
            notes.push_back (n);
            pasted.push_back ((int) notes.size() - 1);
        }

        return ! pasted.empty();
    });

    if (done)
        selection = std::set<int> (pasted.begin(), pasted.end());

    repaint();
    return done;
}

//==============================================================================
juce::PopupMenu TunePianoRoll::buildNoteMenu (int noteIndex) const
{
    juce::PopupMenu menu;
    const auto notes = getShownNotes();
    const bool valid = juce::isPositiveAndBelow (noteIndex, (int) notes.size());

    if (! valid)
    {
        menu.addItem (pasteItem, tr ("tune.roll.menu.paste"), ! clipboard.empty());
        menu.addItem (selectAllItem, tr ("tune.roll.menu.selectAll"), ! notes.empty());
        return menu;
    }

    const auto& n = notes[(size_t) noteIndex];

    juce::PopupMenu velocity;

    for (int i = 0; i < kNumVelocityPresets; ++i)
        velocity.addItem (velocityBase + i, juce::String (kVelocityPresets[i]), true, n.velocity == kVelocityPresets[i]);

    menu.addSubMenu (tr ("tune.roll.menu.velocity", { { "v", juce::String (n.velocity) } }), velocity);

    juce::PopupMenu articulation;

    for (int a = 0; a < (int) NoteArticulation::numArticulations; ++a)
        articulation.addItem (articulationBase + a, tr (juce::String ("tune.roll.articulation.") + getNoteArticulationName ((NoteArticulation) a)),
                              true, (int) n.articulation == a);

    menu.addSubMenu (tr ("tune.roll.menu.articulation"), articulation);

    juce::PopupMenu technique;

    for (int t = 0; t < (int) NoteTechnique::numTechniques; ++t)
        technique.addItem (techniqueBase + t, tr (juce::String ("tune.roll.technique.") + getNoteTechniqueName ((NoteTechnique) t)),
                           true, (int) n.technique == t);

    menu.addSubMenu (tr ("tune.roll.menu.technique"), technique);
    menu.addSeparator();
    menu.addItem (lockItem, n.locked ? tr ("tune.roll.menu.unlock") : tr ("tune.roll.menu.lock"));
    menu.addItem (deleteItem, tr ("tune.roll.menu.delete"));
    menu.addSeparator();
    menu.addItem (pasteItem, tr ("tune.roll.menu.paste"), ! clipboard.empty());
    menu.addItem (selectAllItem, tr ("tune.roll.menu.selectAll"));
    return menu;
}

void TunePianoRoll::performNoteMenuItem (int noteIndex, int itemId)
{
    if (itemId == pasteItem)
    {
        paste();
        return;
    }

    if (itemId == selectAllItem)
    {
        selectAll();
        return;
    }

    // The menu acts on the selection, which the note under the mouse joins.
    if (noteIndex >= 0 && selection.count (noteIndex) == 0)
        selection = { noteIndex };

    if (selection.empty())
        return;

    if (itemId == deleteItem)
    {
        deleteSelected();
    }
    else if (itemId == lockItem)
    {
        const auto notes = getShownNotes();
        const int first = *selection.begin();
        setSelectedLocked (juce::isPositiveAndBelow (first, (int) notes.size()) ? ! notes[(size_t) first].locked : true);
    }
    else if (itemId >= velocityBase && itemId < velocityBase + kNumVelocityPresets)
    {
        setSelectedVelocity (kVelocityPresets[itemId - velocityBase]);
    }
    else if (itemId >= articulationBase && itemId < articulationBase + (int) NoteArticulation::numArticulations)
    {
        setSelectedArticulation ((NoteArticulation) (itemId - articulationBase));
    }
    else if (itemId >= techniqueBase && itemId < techniqueBase + (int) NoteTechnique::numTechniques)
    {
        setSelectedTechnique ((NoteTechnique) (itemId - techniqueBase));
    }

    repaint();
}

//==============================================================================
void TunePianoRoll::paint (juce::Graphics& g)
{
    clampSelection();

    const auto& tune = session.getTune();
    const int sectionIndex = session.getSelectedSection();
    const int low = getLowestPitch();
    const int high = getHighestPitch();
    const int rows = high - low + 1;
    const float rowHeight = (float) getHeight() / (float) rows;
    const auto area = rollArea();

    g.setColour (Palette::panelSunken);
    g.fillRect (getLocalBounds());

    // 3.4: key-of-scale rows shaded, the tonic a shade brighter.
    for (int pitch = low; pitch <= high; ++pitch)
    {
        const float y = (float) (high - pitch) * rowHeight;

        if (tunetheory::isInScale (pitch, tune.meta.keyTonic, tune.meta.mode))
        {
            g.setColour (tunetheory::wrapPitchClass (pitch - tune.meta.keyTonic) == 0 ? Palette::panelRaised : Palette::panel);
            g.fillRect (area.getX(), y, area.getWidth(), rowHeight);
        }

        // The keyboard column: white and black keys, C named.
        const int pc = tunetheory::wrapPitchClass (pitch);
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour (black ? Palette::backgroundDeep : Palette::textMuted);
        g.fillRect (0.0f, y, (float) kKeyboardWidth - 2.0f, rowHeight);
        g.setColour (Palette::edge);
        g.drawHorizontalLine ((int) y, 0.0f, (float) kKeyboardWidth - 2.0f);

        if (pc == 0 && rowHeight >= 7.0f)
        {
            g.setColour (Palette::plateText);
            g.setFont (Fonts::ui (juce::jmin (9.0f, rowHeight - 1.0f)));
            g.drawText ("C" + juce::String (pitch / 12 - 1), 2, (int) y, kKeyboardWidth - 4, (int) std::ceil (rowHeight),
                        juce::Justification::centredLeft);
        }
    }

    // Beat and bar lines, over the visible window.
    const double beats = sectionBeats();
    const double bar = tune.getBeatsPerBar();
    const double firstBeat = std::floor (viewStart);

    for (double b = firstBeat; b <= juce::jmin (beats, viewStart + visibleBeats) + 1.0e-9; b += 1.0)
    {
        if (b < viewStart - 1.0e-9)
            continue;

        const float x = area.getX() + (float) ((b - viewStart) / visibleBeats) * area.getWidth();
        const double intoBar = std::fmod (b, bar);
        const bool isBar = intoBar < 1.0e-6 || bar - intoBar < 1.0e-6;
        g.setColour (isBar ? Palette::edgeBright : Palette::edge.withMultipliedAlpha (0.6f));
        g.drawVerticalLine (juce::jmin ((int) x, getWidth() - 1), 0.0f, (float) getHeight());

        if (isBar && visibleBeats <= 64.0)
        {
            g.setColour (Palette::textMuted);
            g.setFont (Fonts::ui (9.0f));
            g.drawText (juce::String ((int) std::lround (b / bar) + 1), (int) x + 3, 1, 24, 11, juce::Justification::centredLeft);
        }
    }

    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText (tr ("tune.roll.noSection"), area.toNearestInt(), juce::Justification::centred);
        return;
    }

    const bool derived = isShowingDerivedBass();
    const auto notes = getShownNotes();
    const bool moving = dragMode == DragMode::move && (moveBeats != 0.0 || moveSteps != 0);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const bool selected = selection.count ((int) i) > 0;
        double start = n.startBeat;
        int pitch = resolveNotePitch (tune, sectionIndex, n);

        if (moving && selected)
        {
            start += moveBeats;
            pitch += moveSteps;
        }

        if (start + n.durationBeats < viewStart || start > viewStart + visibleBeats)
            continue;

        const auto r = getNoteBounds (start, pitch, n.durationBeats).reduced (0.5f, 1.0f);

        // Velocity as brightness (3.4 "velocity"); a derived bass line is grey
        // until it is edited, when it becomes a Manual line.
        auto fill = derived ? Palette::textDisabled : (n.locked ? Palette::accentBright : Palette::secondary);
        fill = fill.withMultipliedAlpha (0.45f + 0.55f * (float) n.velocity / 127.0f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 2.0f);

        if (n.locked && ! derived)
        {
            g.setColour (Palette::textPrimary);
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }

        if (selected)
        {
            g.setColour (Palette::textPrimary);
            g.drawRoundedRectangle (r.expanded (1.0f), 2.5f, 1.5f);
        }

        if (n.technique != NoteTechnique::none && r.getWidth() > 12.0f && rowHeight >= 8.0f)
        {
            g.setColour (Palette::plateText);
            g.setFont (Fonts::ui (juce::jmin (9.0f, rowHeight - 1.0f)));
            g.drawText (juce::String (getNoteTechniqueName (n.technique)).substring (0, 1).toUpperCase(),
                        r.toNearestInt(), juce::Justification::centredLeft);
        }
    }

    if (section->melody.has_value() && part == TuneNotePart::melody && section->melody->source == MelodySource::improvise)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawText (tr ("tune.roll.improvising"), area.toNearestInt().reduced (6, 4), juce::Justification::bottomLeft);
    }

    if (part == TuneNotePart::bass)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawText (derived ? tr ("tune.roll.bassDerived") : (section->bass.mode == BassMode::off ? tr ("tune.roll.bassOff")
                                                                                                    : tr ("tune.roll.bassManual")),
                    area.toNearestInt().reduced (6, 4), juce::Justification::bottomLeft);
    }

    if (dragMode == DragMode::draw)
    {
        const auto r = getNoteBounds (dragStart, dragPitch, juce::jmax (grid, dragEnd - dragStart));
        g.setColour (Palette::accent.withMultipliedAlpha (0.6f));
        g.fillRoundedRectangle (r.reduced (0.5f, 1.0f), 2.0f);
    }

    if (dragMode == DragMode::box)
    {
        const auto a = getNoteBounds (juce::jmin (dragStart, dragEnd), juce::jmax (dragPitch, dragPitchEnd), 0.0);
        const auto b = getNoteBounds (juce::jmax (dragStart, dragEnd), juce::jmin (dragPitch, dragPitchEnd), 0.0);
        const auto box = juce::Rectangle<float> (a.getX(), a.getY(), b.getX() - a.getX(), b.getBottom() - a.getY());
        g.setColour (Palette::textPrimary.withMultipliedAlpha (0.15f));
        g.fillRect (box);
        g.setColour (Palette::textPrimary);
        g.drawRect (box, 1.0f);
    }

    if (playhead.section == sectionIndex && playhead.beat >= viewStart && playhead.beat <= viewStart + visibleBeats)
    {
        const float x = area.getX() + (float) ((playhead.beat - viewStart) / visibleBeats) * area.getWidth();
        g.setColour (Palette::textPrimary);
        g.fillRect (x - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }

    if (chromaticMode)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (9.0f));
        g.drawText (tr ("tune.roll.chromatic"), getLocalBounds().reduced (4), juce::Justification::topRight);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accent.withMultipliedAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }
}

//==============================================================================
void TunePianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.x < kKeyboardWidth)
        return;

    const double beat = beatAt ((float) e.x);
    const int pitch = pitchAt ((float) e.y);
    const int hit = findNoteAt (beat, pitch);
    dragMode = DragMode::none;
    dragOrigin = e.getPosition();
    cursorBeat = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);

    if (e.mods.isPopupMenu())
    {
        if (hit >= 0 && selection.count (hit) == 0)
            select (hit, e.mods.isShiftDown());

        buildNoteMenu (hit).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                           [safe = juce::Component::SafePointer<TunePianoRoll> (this), hit] (int result)
        {
            if (safe != nullptr && result != 0)
                safe->performNoteMenuItem (hit, result);
        });
        return;
    }

    if (! session.getTune().isValidSection (session.getSelectedSection()))
        return;

    if (hit >= 0)
    {
        // 3.4: click selects, shift-click adds; a drag then moves the selection.
        if (e.mods.isShiftDown())
            select (hit, true);
        else if (selection.count (hit) == 0)
            select (hit, false);

        dragMode = DragMode::move;
        dragStart = beat;
        dragPitch = pitch;
        moveBeats = 0.0;
        moveSteps = 0;
        repaint();
        return;
    }

    // Empty space: draw, or box-select when drawing is off or Shift is held.
    if (drawEnabled && ! e.mods.isShiftDown())
    {
        dragMode = DragMode::draw;
        dragStart = snapBeatToGrid (juce::jmax (0.0, beat - grid * 0.5 + 1.0e-9), grid);
        dragEnd = dragStart + grid;
        dragPitch = chromaticMode ? pitch : snapPitchToKey (pitch, session.getTune().meta.keyTonic, session.getTune().meta.mode);
    }
    else
    {
        if (! e.mods.isShiftDown())
            selection.clear();

        dragMode = DragMode::box;
        dragStart = dragEnd = beat;
        dragPitch = dragPitchEnd = pitch;
    }

    repaint();
}

void TunePianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    switch (dragMode)
    {
        case DragMode::draw:
            // 3.4: length sets the duration.
            dragEnd = juce::jmax (dragStart + grid, snapBeatToGrid (beatAt ((float) e.x), grid));
            break;

        case DragMode::box:
            dragEnd = beatAt ((float) e.x);
            dragPitchEnd = pitchAt ((float) e.y);
            break;

        case DragMode::move:
            moveBeats = snapBeatToGrid (beatAt ((float) e.x) - dragStart, grid);
            moveSteps = pitchAt ((float) e.y) - dragPitch;
            break;

        case DragMode::none:
            return;
    }

    repaint();
}

void TunePianoRoll::mouseUp (const juce::MouseEvent& e)
{
    const auto mode = dragMode;
    dragMode = DragMode::none;

    switch (mode)
    {
        case DragMode::draw:
            addNote (dragStart + grid * 0.5, dragPitch, dragEnd - dragStart);
            break;

        case DragMode::box:
            if (e.getDistanceFromDragStart() > 3)
                selectInBox (dragStart, dragEnd, dragPitch, dragPitchEnd, e.mods.isShiftDown());
            break;

        case DragMode::move:
            if (moveBeats != 0.0 || moveSteps != 0)
            {
                // Rows are semitones, whatever the snap: the note lands where it was dropped.
                const bool wasChromatic = chromaticMode;
                chromaticMode = true;
                nudgeSelected (moveBeats, moveSteps);
                chromaticMode = wasChromatic;
            }
            break;

        case DragMode::none:
            break;
    }

    moveBeats = 0.0;
    moveSteps = 0;
    repaint();
}

void TunePianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl zooms, a plain wheel scrolls along the bars (3.1 "scrollable").
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        zoom (wheel.deltaY > 0.0f ? 1.25 : 0.8);
        return;
    }

    const float delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : -wheel.deltaY;
    scrollBy ((double) delta * visibleBeats * 0.5);
}

bool TunePianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool command = mods.isCommandDown() || mods.isCtrlDown();
    const auto code = key.getKeyCode();
    const double bar = session.getTune().getBeatsPerBar();

    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        // 3.4: "Nudge with arrow keys; larger nudge with Shift" - a grid step, or a bar.
        const double step = mods.isShiftDown() ? bar : grid;
        return nudgeSelected (code == juce::KeyPress::leftKey ? -step : step, 0);
    }

    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        const int direction = code == juce::KeyPress::upKey ? 1 : -1;

        if (command)
            return adjustSelectedVelocity (10 * direction);

        // An octave is twelve semitones, or seven steps of a seven-note mode.
        const int steps = mods.isShiftDown() ? (chromaticMode ? 12 : 7) : 1;
        return nudgeSelected (0.0, steps * direction);
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        return deleteSelected();

    if (code == juce::KeyPress::escapeKey)
    {
        if (selection.empty())
            return false;

        clearSelection();
        return true;
    }

    if (command && ! mods.isAltDown())
    {
        const auto letter = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);

        if (letter == 'C') return copySelected();
        if (letter == 'X') return cutSelected();
        if (letter == 'V') return paste();

        if (letter == 'A')
        {
            selectAll();
            return true;
        }
    }

    return false;
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

    session.onChanged = [this] { refresh(); };

    // onboarding 8: hidden until owed; it takes its row above the header.
    addChildComponent (firstEncounterHint);
    firstEncounterHint.onShownOrDismissed = [this]
    {
        setSize (getWidth(), getPreferredHeight());
        resized();
    };

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

    // The window holds a raw pointer to the panel: it goes first.
    exportWindow.reset();
    exportPanel.reset();
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

    for (auto* b : { &newButton, &loadButton, &importButton, &saveButton, &exportButton })
        addAndMakeVisible (b);

    newButton.setTooltip ("A new tune from a template (Ctrl+T)");
    loadButton.setTooltip ("Open a .luthiertune");
    importButton.setTooltip ("Import a MIDI file (.mid) as a new tune: its chords are strummed by "
                             "each section's rhythm pattern, its melody and bass play as written");
    saveButton.setTooltip ("Save the tune (Ctrl+S)");
    exportButton.setTooltip (tr ("tune.export.button.tooltip"));

    AccessibleSetup::configureButton (newButton, "New tune", "Starts a new tune from a template.");
    AccessibleSetup::configureButton (loadButton, "Load tune");
    AccessibleSetup::configureButton (importButton, "Import MIDI file",
                                      "Imports a MIDI file as a new tune. Chords are strummed by the rhythm pattern.");
    AccessibleSetup::configureButton (saveButton, "Save tune");
    AccessibleSetup::configureButton (exportButton, tr ("tune.export.button"), tr ("tune.export.button.tooltip"));

    newButton.onClick = [this] { showTemplateMenu(); };
    loadButton.onClick = [this] { chooseAndLoad(); };
    importButton.onClick = [this] { chooseAndImportMidi(); };
    saveButton.onClick = [this] { chooseAndSave(); };
    exportButton.onClick = [this] { showExportDialog(); };

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

        /*  2.1: "Every genre kit ships with a suggested tempo". The kit sets the
            tune's tempo when the tempo has not been chosen by hand: it is still
            the default, or the previous kit's suggestion. A tempo the user set
            stays. */
        const auto* current = session.getTune().getSection (session.getSelectedSection());
        const double now = session.getTune().meta.tempoBpm;
        const double previous = current != nullptr ? getKitSuggestedTempo (current->genreKitId) : 120.0;
        const bool tempoUntouched = std::abs (now - 120.0) < 1.0e-6 || std::abs (now - previous) < 1.0e-6;
        const double suggested = getKitSuggestedTempo (kit.name);
        const int sectionIndex = session.getSelectedSection();

        session.edit (TuneEditClass::sectionEdit, "Change genre kit",
                      [sectionIndex, kit, pattern, density, tempoUntouched, suggested] (Tune& t)
        {
            auto* s = t.getSection (sectionIndex);

            if (s == nullptr || (s->genreKitId == kit.name && s->rhythmPatternId == pattern))
                return false;

            s->genreKitId = kit.name;
            s->rhythmPatternId = pattern;

            // TuneMelody: the track's density follows its kit (4.1).
            if (s->melody.has_value())
                s->melody->density = density;

            if (tempoUntouched)
                t.setTempo (suggested);

            return true;
        }, 4000 + sectionIndex);
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

    // 6: the bass line's mode. Manual is what the roll's BASS part edits.
    addAndMakeVisible (bassModeBox);
    bassModeBox.setTooltip (tr ("tune.bass.tooltip"));
    AccessibleSetup::configureComboBox (bassModeBox, tr ("tune.bass"));

    for (int m = 0; m < (int) BassMode::numModes; ++m)
        bassModeBox.addItem (tr (juce::String ("tune.bass.mode.") + getBassModeName ((BassMode) m)), m + 1);

    bassModeBox.onChange = [this]
    {
        if (updating || bassModeBox.getSelectedId() <= 0)
            return;

        const auto mode = (BassMode) (bassModeBox.getSelectedId() - 1);
        const int index = session.getSelectedSection();
        const auto* s = session.getTune().getSection (index);

        // Switching to Manual keeps the line the old mode played, to edit from.
        const auto seed = s != nullptr && mode == BassMode::manual && s->bass.mode != BassMode::manual
                            ? generateBassLine (session.getTune(), index) : std::vector<MelodyNote>();

        session.edit (TuneEditClass::sectionEdit, tr ("tune.bass.edit"), [index, mode, seed] (Tune& t)
        {
            if (! seed.empty())
                return t.setBassNotes (index, seed);

            return t.setBassMode (index, mode);
        });
    };

    // 7: the four layers, each on or off. A layer that is switched on for the
    // first time is created with the section's kit and a fresh countermelody.
    for (int i = 0; i < (int) LayerType::numTypes; ++i)
    {
        const auto type = (LayerType) i;
        const juce::String key = juce::String ("tune.layer.") + getLayerTypeName (type);
        auto toggle = std::make_unique<LuthierToggle> (tr (key));
        toggle->setTooltip (tr (key + ".tooltip"));
        toggle->getButton().setTooltip (tr (key + ".tooltip"));
        AccessibleSetup::configureButton (toggle->getButton(), tr (key), tr (key + ".tooltip"));
        addAndMakeVisible (*toggle);

        toggle->getButton().onClick = [this, type]
        {
            const bool on = layerToggles[(size_t) type]->getButton().getToggleState();
            const int index = session.getSelectedSection();

            session.edit (TuneEditClass::sectionEdit, on ? tr ("tune.layer.edit.on") : tr ("tune.layer.edit.off"), [index, type, on] (Tune& t)
            {
                auto* s = t.getSection (index);

                if (s == nullptr)
                    return false;

                TuneLayer layer;

                if (const auto* existing = s->findLayer (type))
                    layer = *existing;
                else
                {
                    layer.type = type;

                    if (type == LayerType::arpeggio)
                        layer.patternId = s->rhythmPatternId;

                    if (type == LayerType::countermelody)
                        layer.notes = generateCountermelody (t, index, layer.seed);
                }

                layer.enabled = on;
                return t.setLayer (index, layer);
            });
        };

        layerToggles[(size_t) i] = std::move (toggle);
    }
}

void TunePanel::buildMelody()
{
    addAndMakeVisible (pianoRoll);

    // 6 and 7: which notes the roll edits.
    for (int i = 0; i < (int) TuneNotePart::numParts; ++i)
    {
        const auto part = (TuneNotePart) i;
        const juce::String key = juce::String ("tune.part.") + getTuneNotePartName (part);
        auto button = std::make_unique<juce::TextButton> (tr (key));
        button->setClickingTogglesState (true);
        button->setRadioGroupId (7601);
        button->setTooltip (tr (key + ".tooltip"));
        AccessibleSetup::configureButton (*button, tr (key), tr (key + ".tooltip"));
        button->onClick = [this, part] { setPart (part); };
        addAndMakeVisible (*button);
        partButtons[(size_t) i] = std::move (button);
    }

    partButtons[0]->setToggleState (true, juce::dontSendNotification);

    for (auto* b : { &zoomInButton, &zoomOutButton })
        addAndMakeVisible (b);

    zoomInButton.setTooltip (tr ("tune.roll.zoomIn.tooltip"));
    zoomOutButton.setTooltip (tr ("tune.roll.zoomOut.tooltip"));
    AccessibleSetup::configureButton (zoomInButton, tr ("tune.roll.zoomIn"), tr ("tune.roll.zoomIn.tooltip"));
    AccessibleSetup::configureButton (zoomOutButton, tr ("tune.roll.zoomOut"), tr ("tune.roll.zoomOut.tooltip"));
    zoomInButton.onClick = [this] { pianoRoll.zoom (1.5); };
    zoomOutButton.onClick = [this] { pianoRoll.zoom (1.0 / 1.5); };

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
    for (auto* b : { &backButton, &playButton, &stopButton, &forwardButton })
        addAndMakeVisible (b);

    stopButton.setTooltip (tr ("tune.transport.stop.tooltip"));
    AccessibleSetup::configureButton (stopButton, tr ("tune.transport.stop"), tr ("tune.transport.stop.tooltip"));
    stopButton.onClick = [this] { player.stop(); updateTransport(); };

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

    bassModeBox.setSelectedId (section != nullptr ? (int) section->bass.mode + 1 : 0, juce::dontSendNotification);
    bassModeBox.setEnabled (section != nullptr);

    for (int i = 0; i < (int) LayerType::numTypes; ++i)
    {
        const auto* layer = section != nullptr ? section->findLayer ((LayerType) i) : nullptr;
        layerToggles[(size_t) i]->getButton().setToggleState (layer != nullptr && layer->enabled, juce::dontSendNotification);
        layerToggles[(size_t) i]->setEnabled (section != nullptr);
    }

    for (auto& b : partButtons)
        b->setEnabled (section != nullptr);

    partButtons[(size_t) pianoRoll.getPart()]->setToggleState (true, juce::dontSendNotification);

    sectionStrip.repaint();
    chordPills.repaint();
    pianoRoll.repaint();
    repaint();
}

void TunePanel::timerCallback()
{
    updateTransport();
    showFirstEncounterHintIfDue();
}

void TunePanel::showFirstEncounterHintIfDue()
{
    firstEncounterHint.showIfDue();
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
        showExportDialog();
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

    // The roll's editing keys (3.4) work with the panel focused too.
    if (! pianoRoll.hasKeyboardFocus (false) && pianoRoll.keyPressed (key))
        return true;

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

bool TunePanel::importMidiFrom (const juce::File& file, juce::String& error)
{
    player.stop();

    Tune imported;
    juce::StringArray warnings;
    TuneImportOptions options;
    options.midi = session.getMidiOptions();   // the channels our own export used

    // error-recovery 1: a refused file changes nothing, so the import goes
    // into a tune of its own and only a success reaches the session.
    if (! importMidiFile (file, imported, options, error, &warnings))
        return false;

    // A new tune, not an edit: the undo stack belongs to one tune (TuneSession).
    session.newTune (imported);

    // Say plainly what an import is (midi-export 5): the chords are re-strummed,
    // the file's own chord track is a layer. The importer's warnings say which
    // tracks became what and what was defaulted.
    juce::StringArray lines;
    lines.add ("Imported " + file.getFileName() + " into the Tune Builder: "
               + juce::String (imported.getNumSections()) + " section(s), "
               + juce::String (imported.meta.tempoBpm, 0) + " bpm. Chords are strummed by each section's "
               "rhythm pattern; the melody and bass play as written.");
    lines.addArray (warnings);

    notify (lines.joinIntoString ("\n"), ! warnings.isEmpty());
    return true;
}

void TunePanel::notify (const juce::String& message, bool warning)
{
    if (onNotification != nullptr)
    {
        onNotification (message, warning);
        return;
    }

    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (warning ? juce::MessageBoxIconType::WarningIcon
                                                                  : juce::MessageBoxIconType::InfoIcon)
                                           .withTitle ("MIDI imported")
                                           .withMessage (message)
                                           .withButton ("OK"),
                                       nullptr);
}

void TunePanel::chooseAndImportMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a MIDI file as a tune",
                                                   PresetManager::getRenderFolder(), "*.mid;*.midi");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<TunePanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;

        if (! safe->importMidiFrom (file, error))
            safe->showError ("Could not import the MIDI file", error);
    });
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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&newButton),
                        [safe = juce::Component::SafePointer<TunePanel> (this)] (int result)
    {
        if (safe != nullptr && result > 0)
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

void TunePanel::setPart (TuneNotePart part)
{
    pianoRoll.setPart (part);
    partButtons[(size_t) part]->setToggleState (true, juce::dontSendNotification);
    repaint();
}

TuneExportPanel& TunePanel::getExportPanel()
{
    if (exportPanel == nullptr)
    {
        exportPanel = std::make_unique<TuneExportPanel> (processor, session);
        exportPanel->onFinished = [this] (const TuneExportReport& report)
        {
            notify (report.ok() ? tr ("tune.export.notice.done", { { "n", juce::String (report.files.size()) } })
                                : tr ("tune.export.notice.failed", { { "errors", report.errors.joinIntoString ("; ") } }),
                    ! report.ok());
        };
    }

    return *exportPanel;
}

void TunePanel::showExportDialog()
{
    auto& dialog = getExportPanel();

    // The editor's overlay host shows it like the header's Export (Overlays.h:
    // only the editor can put an overlay up). Without one, a window of its own.
    if (onShowOverlay != nullptr)
    {
        onShowOverlay (&dialog);
        return;
    }

    if (exportWindow != nullptr)
    {
        exportWindow->toFront (true);
        return;
    }

    dialog.setSize (dialog.getPreferredSize().x, dialog.getPreferredSize().y);
    dialog.onDismiss = [this] { exportWindow.reset(); };

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = tr ("tune.export.title");
    options.content.setNonOwned (&dialog);
    options.componentToCentreAround = getTopLevelComponent();
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = true;
    exportWindow.reset (options.launchAsync());
    dialog.overlayShown();
}

//==============================================================================
int TunePanel::getPreferredHeight() const
{
    const int button = Metrics::buttonHeight;

    return Metrics::grid
         + (firstEncounterHint.isVisible() ? FirstEncounterHint::kHeight + kRowGap : 0)   // onboarding 8
         + 3 * (button + kRowGap)                                     // header: title, buttons, tempo/key
         + kHeader + kStripHeight + kRowGap                           // sections
         + kHeader + button + kErrorLine + kPillsHeight + kRowGap     // progression
         + kHeader + 3 * (button + kRowGap)                           // rhythm, bass and layers
         + kHeader + kRollHeight + kRowGap + 3 * (button + kRowGap)   // melody: parts, generators, quantise
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
    g.drawText (tr ("tune.bass.label"), bassLabelBounds, juce::Justification::centredLeft);
    g.drawText (tr ("tune.layer.label"), layersLabelBounds, juce::Justification::centredLeft);
    g.drawText (tr ("tune.part.label"), partLabelBounds, juce::Justification::centredLeft);
    g.drawText (tr ("tune.roll.view"), viewLabelBounds, juce::Justification::centredLeft);

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

    // onboarding 8: the first-session hint sits above everything.
    if (firstEncounterHint.isVisible())
    {
        firstEncounterHint.setBounds (bounds.removeFromTop (FirstEncounterHint::kHeight));
        bounds.removeFromTop (kRowGap);
    }

    // --- header (3.1: "TUNE [My New Tune] [Save] [Export] Tempo [120] Key [C]") ---------
    {
        auto r = row();
        titleLabelBounds = r.removeFromLeft (44);
        titleEditor.setBounds (r);
    }

    split (row(), { &newButton, &loadButton, &importButton, &saveButton, &exportButton });

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

    {
        // 6 and 7: BASS [mode] LAYERS [PAD] [ARP] [COUNTER] [PERC]
        auto r = row();
        bassLabelBounds = r.removeFromLeft (40);
        bassModeBox.setBounds (r.removeFromLeft (juce::jmax (72, r.getWidth() * 3 / 10)));
        r.removeFromLeft (6);
        layersLabelBounds = r.removeFromLeft (52);

        std::initializer_list<juce::Component*> layers { layerToggles[0].get(), layerToggles[1].get(),
                                                         layerToggles[2].get(), layerToggles[3].get() };
        split (r, layers);
    }

    // --- melody ------------------------------------------------------------------------
    melodyHeader = bounds.removeFromTop (kHeader);
    pianoRoll.setBounds (bounds.removeFromTop (kRollHeight));
    bounds.removeFromTop (kRowGap);

    {
        // PART [MELODY] [BASS] [LAYER]   VIEW [-] [+]
        auto r = row();
        partLabelBounds = r.removeFromLeft (40);
        zoomInButton.setBounds (r.removeFromRight (28));
        r.removeFromRight (4);
        zoomOutButton.setBounds (r.removeFromRight (28));
        r.removeFromRight (4);
        viewLabelBounds = r.removeFromRight (36);
        split (r.withTrimmedRight (4), { partButtons[0].get(), partButtons[1].get(), partButtons[2].get() });
    }

    split (row(), { &autoButton, &drawToggle, &recordToggle, &improviseToggle, &freezeButton });

    {
        auto r = row();
        quantiseLabelBounds = r.removeFromLeft (64);
        quantiseBox.setBounds (r.removeFromLeft (96));
    }

    // --- transport ---------------------------------------------------------------------
    transportHeader = bounds.removeFromTop (kHeader);
    split (row(), { &backButton, &playButton, &stopButton, &forwardButton, &loopToggle, &countInToggle, &metronomeToggle });
    positionBounds = bounds.removeFromTop (kPositionLine);
}

} // namespace luthier
