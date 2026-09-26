#include "TunePanel.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneTemplates.h"
#include "../Tune/TuneExamples.h"
#include "../Tune/TuneHarmony.h"
#include "TuneExportDialog.h"
#include "../Support/TuneExport.h"

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

    /** The key bound to an action in the shortcut registry (gui-integration 17:
        every shortcut is rebindable), so a rebound save or new-tune key is the
        one the tab answers. */
    bool isBound (const juce::KeyPress& key, const char* actionId)
    {
        const auto* binding = AccessibilitySettings::get().findShortcut (actionId);
        return binding != nullptr && binding->key == key;
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

    // 3.3: where a dragged tab will land.
    if (draggingTab && dropSlot >= 0)
    {
        const int x = dropSlot >= tune.getNumSections() ? getTabBounds (tune.getNumSections()).getX() - 2
                                                       : getTabBounds (dropSlot).getX() - 2;
        g.setColour (Palette::accentBright);
        g.fillRect (x - 1, 0, 3, getHeight());
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

    // 3.3: a left press may become a drag (TuneSectionStripEditing.cpp).
    dragTab = e.mods.isPopupMenu() ? -1 : tab;
    draggingTab = false;

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
    menu.addItem (varyItem, "Vary", section != nullptr);   // 3.3
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

    if (itemId == varyItem)
    {
        varySection (sectionIndex);
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
    setTooltip ("Click a chord to edit it; drag it to reorder, drag its right edge to change its length; "
                "right-click for insert, duplicate, delete, copy, paste and substitutions");
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

    // 3.2: what a drag in progress will do.
    if (drag == Drag::resize && dragCell >= 0)
    {
        const auto cell = getCellBounds (dragCell);
        const float x = (float) cell.getX() + (float) (dragBeats / total) * width;
        g.setColour (Palette::accentBright);
        g.fillRect (x - 1.5f, 0.0f, 3.0f, (float) getHeight());
        g.setFont (Fonts::mono (10.0f));
        g.drawText (juce::String (dragBeats, 1), juce::Rectangle<float> (x + 3.0f, 0.0f, 40.0f, 12.0f),
                    juce::Justification::centredLeft);
    }
    else if (drag == Drag::move && dropIndex >= 0)
    {
        const int count = (int) section->chords.size();
        const int x = dropIndex >= count ? getCellBounds (count - 1).getRight() : getCellBounds (dropIndex).getX();
        g.setColour (Palette::accentBright);
        g.fillRect ((float) x - 1.5f, 0.0f, 3.0f, (float) getHeight());
    }
}

//==============================================================================
// The panel
//==============================================================================
TunePanel::TunePanel (LuthierAudioProcessor& p, TunePlayer& pl, TuneSession& s)
    : processor (p),
      player (pl),
      session (s),
      setlistStrip (s),
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
    motion.startTimerHz (*this, 30);   // gui-engine-dataflow 24: the transport indicator drains at 30 Hz
}

TunePanel::~TunePanel()
{
    motion.stopTimer();

    if (looperWorker != nullptr)
        looperWorker->stopThread (30000);
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
    exportButton.setTooltip ("Export the tune: audio (with stems), MIDI, notation or the project (Ctrl+E)");

    AccessibleSetup::configureButton (newButton, "New tune", "Starts a new tune from a template.");
    AccessibleSetup::configureButton (loadButton, "Load tune");
    AccessibleSetup::configureButton (saveButton, "Save tune");
    AccessibleSetup::configureButton (exportButton, "Export tune", "Opens the export dialog: audio, MIDI, notation or project.");

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

    // 3.3: tabs dragged into the setlist timeline above them.
    addAndMakeVisible (setlistStrip);

    sectionStrip.onTabDragged = [this] (int, juce::Point<int> screen)
    {
        const auto local = setlistStrip.getLocalPoint (nullptr, screen);
        setlistStrip.showDropMarker (setlistStrip.getLocalBounds().expanded (0, 6).contains (local)
                                       ? setlistStrip.insertIndexAt (local.x) : -1);
    };

    sectionStrip.onTabDropped = [this] (int sectionIndex, juce::Point<int> screen)
    {
        const auto local = setlistStrip.getLocalPoint (nullptr, screen);
        setlistStrip.showDropMarker (-1);

        if (! setlistStrip.getLocalBounds().expanded (0, 6).contains (local))
            return false;

        setlistStrip.dropSection (sectionIndex, setlistStrip.insertIndexAt (local.x));
        return true;
    };

    addAndMakeVisible (progressionEditor);
    progressionEditor.setTextToShowWhenEmpty ("Am F C G   or   [Verse] Am F C G [Chorus] F C G Am",
                                              Palette::textDisabled);
    progressionEditor.setTooltip ("Chords, one bar each: Am*2 for two bars, | Am F | for two in a bar, [Name] for a section");
    AccessibleSetup::configureDescriptive (progressionEditor, "Progression",
                                           "The section's chords in shorthand. Parsed as you type.");
    progressionEditor.onTextChange = [this] { progressionTextChanged(); };

    addAndMakeVisible (chordPills);

    // tune-builder 5: palette, suggest, reharmonize, transpose, modal shift.
    addAndMakeVisible (toolsButton);
    toolsButton.setTooltip ("Chord tools: the diatonic palette, suggest next chord, reharmonize, transpose, modal shift");
    AccessibleSetup::configureButton (toolsButton, "Chord tools");
    toolsButton.onClick = [this]
    {
        buildToolsMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolsButton),
                                        [safe = juce::Component::SafePointer<TunePanel> (this)] (int r)
        {
            if (safe != nullptr && r != 0)
                safe->performToolsItem (r);
        });
    };

    // 3.2: the popover's strum overrides are the pattern library's patterns.
    chordPills.getPatternNames = [this]
    {
        juce::StringArray names;
        const auto& library = processor.getPatternLibrary();

        for (int i = 0; i < library.getNumPatterns(); ++i)
            names.add (library.getPattern (i).getName());

        return names;
    };
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

        const int sectionIndex = session.getSelectedSection();

        session.edit (TuneEditClass::sectionEdit, "Change genre kit", [kit, pattern, density, sectionIndex] (Tune& t)
        {
            auto* s = t.getSection (sectionIndex);

            if (s == nullptr || (s->genreKitId == kit.name && s->rhythmPatternId == pattern))
                return false;

            // 2.1 (TUNE-HELP-ONBOARDING): the kit's suggested tempo and swing come
            // with it while the tune is still on the old kit's (or the default) tempo,
            // and its feel always does. A tempo the player chose is never overwritten.
            const auto previous = TuneKits::getSuggestion (s->genreKitId);
            const auto suggested = TuneKits::getSuggestion (kit.name);
            const bool tempoUntouched = std::abs (t.meta.tempoBpm - 120.0) < 1.0e-6
                                          || (s->genreKitId.isNotEmpty() && std::abs (t.meta.tempoBpm - previous.tempoBpm) < 1.0e-6);

            if (tempoUntouched)
            {
                t.setTempo (suggested.tempoBpm);
                t.meta.swingPercent = suggested.swingPercent;
            }

            s->genreKitId = kit.name;
            s->rhythmPatternId = pattern;
            s->feel = tunetheory::canonical (suggested.feel);

            // TuneMelody: the track's density follows its kit (4.1).
            if (s->melody.has_value())
                s->melody->density = density;

            return true;
        }, 4000 + sectionIndex);
    };

    // 2.1: the kit's chord palette - click a chord to append it to the section.
    addAndMakeVisible (paletteButton);
    paletteButton.setTooltip ("The genre kit's chord palette, in this key: click one to add it to the section");
    AccessibleSetup::configureButton (paletteButton, "Chord palette", "Adds a chord from the kit's palette.");
    paletteButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const auto cells = getPaletteChords();

        for (size_t i = 0; i < cells.size(); ++i)
            menu.addItem ((int) i + 1, getChordSymbol (cells[i], session.getTune().preferFlats()));

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&paletteButton),
                            [safe = juce::Component::SafePointer<TunePanel> (this)] (int r)
        {
            if (safe != nullptr && r > 0)
                safe->appendPaletteChord (r - 1);
        });
    };

    addAndMakeVisible (kitTempoButton);
    kitTempoButton.setTooltip ("Set the tempo and swing to the kit's suggestion");
    AccessibleSetup::configureButton (kitTempoButton, "Kit tempo", "Sets the tune's tempo to the kit's suggestion.");
    kitTempoButton.onClick = [this] { applyKitTempo(); };

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

    // 4.5: "Play this melody like a bluegrass fiddle" - phrasing only.
    addAndMakeVisible (styleBox);
    styleBox.setTooltip ("Style transfer: plays the melody with another instrument's phrasing (never its pitches)");
    AccessibleSetup::configureComboBox (styleBox, "Melody style");

    for (int st = 0; st < (int) MelodyStyle::numStyles; ++st)
        styleBox.addItem (getMelodyStyleDisplayName ((MelodyStyle) st), st + 1);

    styleBox.onChange = [this]
    {
        if (updating || styleBox.getSelectedId() <= 0)
            return;

        const auto style = (MelodyStyle) (styleBox.getSelectedId() - 1);
        editSection (TuneEditClass::melodyEdit, "Melody style",
                     [style] (TuneSection& s) { if (s.style == style) return false; s.style = style; return true; }, 4600);
    };

    // 4.3: "Held notes across chord changes are optionally re-fitted to the new chord".
    addAndMakeVisible (followChordsToggle);
    followChordsToggle.setTooltip ("Record: a note held across a chord change moves to the new chord's nearest chord tone");
    AccessibleSetup::configureButton (followChordsToggle.getButton(), "Follow chord changes");
    followChordsToggle.getButton().onClick = [this]
    {
        const bool on = followChordsToggle.getButton().getToggleState();
        editSection (TuneEditClass::melodyEdit, "Follow chord changes", [on] (TuneSection& s)
        {
            if (! s.melody.has_value())
                s.melody = MelodyTrack();

            if (s.melody->followChords == on)
                return false;

            s.melody->followChords = on;
            return true;
        }, 4700);
    };

    // 13: "a big Sing button on the Melody strip when audio in is present".
    addChildComponent (singToggle);
    singToggle.setTooltip ("Sing or hum the melody into the audio input; press again to write it down (snapped to the key "
                           "and the grid)");
    AccessibleSetup::configureButton (singToggle.getButton(), "Sing", "Captures a sung melody from the audio input.");
    singToggle.getButton().onClick = [this]
    {
        if (singToggle.getButton().getToggleState())
            startSinging();
        else
            stopSinging();

        refresh();
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

    // 6 and 7: the roll is also the bass and countermelody editor.
    addAndMakeVisible (rollTargetBox);
    rollTargetBox.addItem ("Melody", 1);
    rollTargetBox.addItem ("Bass", 2);
    rollTargetBox.addItem ("Countermelody", 3);
    rollTargetBox.setSelectedId (1, juce::dontSendNotification);
    rollTargetBox.setTooltip ("What the piano roll edits: the melody, the bass line or the countermelody layer");
    AccessibleSetup::configureComboBox (rollTargetBox, "Piano roll edits");
    rollTargetBox.onChange = [this]
    {
        pianoRoll.setTarget ((TunePianoRoll::Target) juce::jmax (0, rollTargetBox.getSelectedId() - 1));
    };

    {
        juce::StringArray fingerpicks;
        const auto& library = processor.getPatternLibrary();

        for (int i = 0; i < library.getNumPatterns(); ++i)
            if (library.getPattern (i).getKind() == RhythmPattern::Kind::fingerpick)
                fingerpicks.add (library.getPattern (i).getName());

        layersStrip = std::make_unique<TuneLayersStrip> (session, fingerpicks);
        addAndMakeVisible (*layersStrip);
    }

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

    // tune-builder 14: the looper captures a whole render to practise over.
    addAndMakeVisible (toLooperButton);
    toLooperButton.setTooltip ("Render the tune once through into a looper layer, to practise over it "
                               "(the practice drawer's looper)");
    AccessibleSetup::configureButton (toLooperButton, "Send to looper", "Renders the tune into a looper layer.");
    toLooperButton.onClick = [this] { sendToLooper (false); };

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
    kitTempoButton.setButtonText ("KIT " + juce::String (juce::roundToInt (TuneKits::getSuggestion (section != nullptr ? section->genreKitId
                                                                                                     : juce::String()).tempoBpm)));

    styleBox.setSelectedId (section != nullptr ? (int) section->style + 1 : 0, juce::dontSendNotification);
    followChordsToggle.getButton().setToggleState (section != nullptr && section->melody.has_value()
                                                     && section->melody->followChords, juce::dontSendNotification);

    const bool improvising = section != nullptr && section->melody.has_value()
                               && section->melody->source == MelodySource::improvise;

    improviseToggle.getButton().setToggleState (improvising, juce::dontSendNotification);
    recordToggle.getButton().setToggleState (session.isRecording(), juce::dontSendNotification);
    singToggle.getButton().setToggleState (processor.getHumCapture().isArmed(), juce::dontSendNotification);

    for (auto* c : { static_cast<juce::Component*> (&kitBox), static_cast<juce::Component*> (&feelSlider),
                     static_cast<juce::Component*> (&strumSlider), static_cast<juce::Component*> (&rhythmOn),
                     static_cast<juce::Component*> (&autoButton), static_cast<juce::Component*> (&recordToggle),
                     static_cast<juce::Component*> (&improviseToggle) })
        c->setEnabled (section != nullptr);

    freezeButton.setEnabled (improvising);
    saveButton.setButtonText (session.isDirty() ? "SAVE *" : "SAVE");

    if (layersStrip != nullptr)
        layersStrip->refresh();

    sectionStrip.repaint();
    setlistStrip.repaint();
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

    // 13: the sung take is analysed as it arrives; Sing shows only with an audio input.
    if (processor.getHumCapture().isArmed())
        processor.getHumCapture().process();

    const bool audioIn = processor.hasSidechainInput() || processor.getHumCapture().isArmed();

    if (singToggle.isVisible() != audioIn)
    {
        singToggle.setVisible (audioIn);
        resized();
    }

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

    if (isBound (key, "save"))
    {
        chooseAndSave();
        return true;
    }

    if (isBound (key, "export"))
    {
        chooseAndExport();
        return true;
    }

    if (isBound (key, "newTune"))
    {
        showTemplateMenu();
        return true;
    }

    // The tune's own undo stack, on whatever undo and redo are bound to.
    if (isBound (key, "redo"))
        return session.redo();

    if (isBound (key, "undo"))
        return session.undo();

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
bool TunePanel::startSinging()
{
    const int index = session.getSelectedSection();

    if (! session.getTune().isValidSection (index))
        return false;

    // The take lines up with the section: where it is playing now, or from its start.
    double beat = 0.0;
    const auto spans = session.getTune().getPlayOrder();
    const int span = player.getPlayingSpan();

    if (player.isPlaying() && juce::isPositiveAndBelow (span, (int) spans.size()) && spans[(size_t) span].sectionIndex == index)
        beat = juce::jmax (0.0, player.getPositionPpq() - spans[(size_t) span].startBeat);
    else
        player.playFromSection (juce::jmax (0, firstSpanOf (session.getTune(), index)));

    auto& capture = processor.getHumCapture();
    capture.begin (session.getTune().meta.tempoBpm, beat);
    capture.setArmed (true);
    singingSection = index;
    return true;
}

bool TunePanel::stopSinging()
{
    auto& capture = processor.getHumCapture();

    if (! capture.isArmed())
        return false;

    capture.setArmed (false);
    const int index = singingSection;
    singingSection = -1;

    const auto grid = (QuantiseGrid) juce::jmax (0, quantiseBox.getSelectedId() - 1);
    const auto notes = capture.finish (session.getTune(), index, grid, ! pianoRoll.isChromatic());

    return session.edit (TuneEditClass::melodyRecord, "Record melody (Sing)", [index, notes] (Tune& t)
    {
        return t.setMelodyNotes (index, notes, MelodySource::sing);
    });
}

int TunePanel::sendToLooper (bool synchronous)
{
    if (looperWorker != nullptr && looperWorker->isThreadRunning())
        return 0;

    const auto state = processor.captureStateBlock();
    const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    auto importInto = [safeProcessor = &processor] (const TuneExport::Render& render)
    {
        auto& looper = safeProcessor->getLooper();
        int layer = 0;

        for (int i = 0; i < looper.getNumLayers(); ++i)
            if (! looper.getLayer (i).hasContent())
            {
                layer = i;
                break;
            }

        return looper.importLayer (layer, render.main);
    };

    if (synchronous)
    {
        TuneExport::Render render;
        return TuneExport::renderAudio (state, rate, 512, 0.0, false, render) ? importInto (render) : 0;
    }

    struct Worker : juce::Thread
    {
        Worker (std::function<void()> f) : juce::Thread ("Tune to looper"), job (std::move (f)) {}
        void run() override { job(); }
        std::function<void()> job;
    };

    toLooperButton.setEnabled (false);
    toLooperButton.setButtonText ("RENDERING...");

    looperWorker = std::make_unique<Worker> ([state, rate, importInto, safe = juce::Component::SafePointer<TunePanel> (this)]
    {
        auto render = std::make_shared<TuneExport::Render>();
        const bool ok = TuneExport::renderAudio (state, rate, 512, 0.0, false, *render);

        juce::MessageManager::callAsync ([safe, render, ok, importInto]
        {
            if (safe == nullptr)
                return;

            if (ok)
                importInto (*render);

            safe->toLooperButton.setEnabled (true);
            safe->toLooperButton.setButtonText ("TO LOOPER");
        });
    });

    looperWorker->startThread();
    return 0;
}

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
    // tune-builder 2.6 and 9: one screen, four destinations (TUNE-HELP-ONBOARDING).
    if (isShowing())
    {
        TuneExportDialog::launch (processor, this);
        return;
    }
}

void TunePanel::chooseAndExportMidiFile()
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
//==============================================================================
std::vector<ChordCell> TunePanel::getPaletteChords() const
{
    const auto& tune = session.getTune();
    const auto* s = tune.getSection (session.getSelectedSection());
    const auto suggestion = TuneKits::getSuggestion (s != nullptr ? s->genreKitId : juce::String());
    return TuneKits::resolvePalette (suggestion.palette, tune.meta.keyTonic, tune.meta.mode, tune.getBeatsPerBar());
}

bool TunePanel::appendPaletteChord (int paletteIndex)
{
    const auto cells = getPaletteChords();
    const int index = session.getSelectedSection();

    if (! juce::isPositiveAndBelow (paletteIndex, (int) cells.size()) || ! session.getTune().isValidSection (index))
        return false;

    const auto cell = cells[(size_t) paletteIndex];
    return session.edit (TuneEditClass::chordEdit, "Add chord from palette",
                         [index, cell] (Tune& t) { return t.insertChord (index, -1, cell); });
}

bool TunePanel::applyKitTempo()
{
    const auto* s = session.getTune().getSection (session.getSelectedSection());

    if (s == nullptr)
        return false;

    const auto suggestion = TuneKits::getSuggestion (s->genreKitId);

    return session.edit (TuneEditClass::other, "Kit tempo", [suggestion] (Tune& t)
    {
        const bool tempo = t.setTempo (suggestion.tempoBpm);
        const bool swing = t.meta.swingPercent != suggestion.swingPercent;
        t.meta.swingPercent = suggestion.swingPercent;
        return tempo || swing;
    });
}

int TunePanel::getPreferredHeight() const
{
    const int button = Metrics::buttonHeight;

    return Metrics::grid
         + (firstHint.isVisible() ? FirstEncounterHint::kHeight + kRowGap : 0)   // onboarding 8
         + 3 * (button + kRowGap)                                     // header: title, buttons, tempo/key
         + kHeader + TuneSetlistStrip::kHeight + 4 + kStripHeight + kRowGap   // setlist and sections
         + kHeader + button + kErrorLine + kPillsHeight + kRowGap     // progression
         + kHeader + 2 * (button + kRowGap)                           // rhythm
         + kHeader + kRollHeight + kRowGap + 3 * (button + kRowGap)   // melody
         + kHeader + (layersStrip != nullptr ? layersStrip->getPreferredHeight() : 0) + kRowGap   // bass and layers
         + kHeader + 2 * (button + kRowGap) + kPositionLine           // transport and TO LOOPER
         + Metrics::grid;
}

void TunePanel::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    g.fillAll (Palette::background);

    g.setFont (Fonts::ui (13.0f, true));
    g.setColour (Palette::accent);
    g.drawText ("TUNE", titleLabelBounds, juce::Justification::centredLeft);

    LuthierLookAndFeel::drawSectionHeader (g, sectionsHeader, "SECTIONS");
    LuthierLookAndFeel::drawSectionHeader (g, progressionHeader, "PROGRESSION");
    LuthierLookAndFeel::drawSectionHeader (g, rhythmHeader, "RHYTHM");
    LuthierLookAndFeel::drawSectionHeader (g, melodyHeader, "MELODY");
    LuthierLookAndFeel::drawSectionHeader (g, transportHeader, "TRANSPORT");
    LuthierLookAndFeel::drawSectionHeader (g, layersHeader, "BASS AND LAYERS");

    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);
    g.drawText ("TEMPO", tempoLabelBounds, juce::Justification::centredLeft);
    g.drawText ("FEEL", feelLabelBounds, juce::Justification::centredLeft);
    g.drawText ("STRUM", strumLabelBounds, juce::Justification::centredLeft);
    g.drawText ("QUANTISE", quantiseLabelBounds, juce::Justification::centredLeft);
    g.drawText ("EDIT", rollTargetLabelBounds, juce::Justification::centredLeft);

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
    setlistStrip.setBounds (bounds.removeFromTop (TuneSetlistStrip::kHeight));
    bounds.removeFromTop (4);
    sectionStrip.setBounds (bounds.removeFromTop (kStripHeight));
    bounds.removeFromTop (kRowGap);

    // --- progression -------------------------------------------------------------------
    progressionHeader = bounds.removeFromTop (kHeader);
    {
        auto r = bounds.removeFromTop (button);
        toolsButton.setBounds (r.removeFromRight (64));
        r.removeFromRight (4);
        progressionEditor.setBounds (r);
    }
    bounds.removeFromTop (kErrorLine);
    chordPills.setBounds (bounds.removeFromTop (kPillsHeight));
    bounds.removeFromTop (kRowGap);

    // --- rhythm ------------------------------------------------------------------------
    rhythmHeader = bounds.removeFromTop (kHeader);

    {
        auto r = row();
        rhythmOn.setBounds (r.removeFromRight (56));
        r.removeFromRight (4);
        kitTempoButton.setBounds (r.removeFromRight (80));
        r.removeFromRight (4);
        paletteButton.setBounds (r.removeFromRight (72));
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
    if (singToggle.isVisible())
        split (row(), { &autoButton, &drawToggle, &recordToggle, &improviseToggle, &singToggle, &freezeButton });
    else
        split (row(), { &autoButton, &drawToggle, &recordToggle, &improviseToggle, &freezeButton });

    {
        auto r = row();
        quantiseLabelBounds = r.removeFromLeft (64);
        quantiseBox.setBounds (r.removeFromLeft (96));
        r.removeFromLeft (Metrics::grid);
        rollTargetLabelBounds = r.removeFromLeft (36);
        rollTargetBox.setBounds (r.removeFromLeft (juce::jmin (140, r.getWidth())));
    }

    {
        auto r = row();
        styleBox.setBounds (r.removeFromLeft (r.getWidth() * 2 / 3).withTrimmedRight (4));
        followChordsToggle.setBounds (r);
    }

    // --- bass and layers (6, 7) --------------------------------------------------------
    layersHeader = bounds.removeFromTop (kHeader);
    layersStrip->setBounds (bounds.removeFromTop (layersStrip->getPreferredHeight()));
    bounds.removeFromTop (kRowGap);

    // --- transport ---------------------------------------------------------------------
    transportHeader = bounds.removeFromTop (kHeader);
    split (row(), { &backButton, &playButton, &forwardButton, &loopToggle, &countInToggle, &metronomeToggle });
    toLooperButton.setBounds (row().removeFromLeft (120));
    positionBounds = bounds.removeFromTop (kPositionLine);
}

} // namespace luthier
