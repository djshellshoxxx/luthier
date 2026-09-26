#include "TuneChordEditor.h"
#include "Theme.h"
#include "../Accessibility/Accessibility.h"
#include "../Tune/TuneHarmony.h"

namespace luthier
{

namespace
{
    constexpr int kChordEditTarget = 5000;   // + section * 256 + cell: 200 ms grouping per cell

    juce::String durationLabel (double beats)
    {
        if (beats <= 0.0)
            return "Hold to fill";

        return juce::String (beats, beats == std::floor (beats) ? 0 : 1) + (beats == 1.0 ? " beat" : " beats");
    }
}

const juce::StringArray& TuneChordEditor::getQualityChoices()
{
    static const juce::StringArray choices = []
    {
        juce::StringArray all { "", "m", "7", "maj7", "m7", "m7b5", "dim", "dim7", "aug", "sus2", "sus4",
                                "7sus4", "6", "m6", "9", "maj9", "m9", "add9", "5", "mMaj7", "7#5" };
        juce::StringArray known;

        // Only what the chord vocabulary knows: a quality the model would refuse is never offered.
        for (const auto& q : all)
            if (tunetheory::isKnownQuality (q))
                known.add (q);

        return known;
    }();

    return choices;
}

const juce::Array<double>& TuneChordEditor::getDurationChoices()
{
    static const juce::Array<double> choices { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 0.0 };
    return choices;
}

TuneChordEditor::TuneChordEditor (TuneSession& s, int section, int cell, juce::StringArray patternNames)
    : session (s), sectionIndex (section), cellIndex (cell), patterns (std::move (patternNames))
{
    const bool flats = session.getTune().preferFlats();

    for (int pc = 0; pc < 12; ++pc)
        rootBox.addItem (tunetheory::spellPitchClass (pc, flats), pc + 1);

    for (int i = 0; i < getQualityChoices().size(); ++i)
    {
        const auto& q = getQualityChoices()[i];
        qualityBox.addItem (q.isEmpty() ? juce::String ("maj") : (q == "m" ? juce::String ("min") : q), i + 1);
    }

    bassBox.addItem ("Root", 1);

    for (int pc = 0; pc < 12; ++pc)
        bassBox.addItem ("/" + tunetheory::spellPitchClass (pc, flats), pc + 2);

    for (int i = 0; i < getDurationChoices().size(); ++i)
        durationBox.addItem (durationLabel (getDurationChoices()[i]), i + 1);

    strumBox.addItem ("Section pattern", 1);

    for (int i = 0; i < patterns.size(); ++i)
        strumBox.addItem (patterns[i], i + 2);

    emphasisBox.addItem ("Normal", 1);
    emphasisBox.addItem ("Accent", 2);
    emphasisBox.addItem ("Ghost", 3);

    const struct { juce::ComboBox* box; const char* name; } combos[] =
    {
        { &rootBox, "Root" }, { &qualityBox, "Quality" }, { &bassBox, "Bass" }, { &durationBox, "Duration" },
        { &strumBox, "Strum override" }, { &emphasisBox, "Emphasis" }
    };

    for (const auto& c : combos)
    {
        addAndMakeVisible (c.box);
        AccessibleSetup::configureComboBox (*c.box, c.name);
    }

    addAndMakeVisible (extensions);
    extensions.setTextToShowWhenEmpty ("9, #11, add9 ...", Palette::textDisabled);
    AccessibleSetup::configureDescriptive (extensions, "Extensions", "Comma-separated chord extensions.");

    rootBox.onChange = [this] { if (! updating) setRoot (rootBox.getSelectedId() - 1); };
    qualityBox.onChange = [this] { if (! updating && qualityBox.getSelectedId() > 0) setQuality (getQualityChoices()[qualityBox.getSelectedId() - 1]); };
    bassBox.onChange = [this] { if (! updating) setBass (bassBox.getSelectedId() - 2); };
    durationBox.onChange = [this] { if (! updating && durationBox.getSelectedId() > 0) setDuration (getDurationChoices()[durationBox.getSelectedId() - 1]); };
    strumBox.onChange = [this] { if (! updating) setStrumOverride (strumBox.getSelectedId() > 1 ? patterns[strumBox.getSelectedId() - 2] : juce::String()); };
    emphasisBox.onChange = [this] { if (! updating) setEmphasis ((ChordEmphasis) juce::jmax (0, emphasisBox.getSelectedId() - 1)); };
    extensions.onReturnKey = [this] { setExtensions (extensions.getText()); };
    extensions.onFocusLost = [this] { setExtensions (extensions.getText()); };

    setSize (300, 214);
    refresh();
}

const ChordCell* TuneChordEditor::getCell() const
{
    const auto* s = session.getTune().getSection (sectionIndex);
    return s != nullptr && juce::isPositiveAndBelow (cellIndex, (int) s->chords.size()) ? &s->chords[(size_t) cellIndex] : nullptr;
}

void TuneChordEditor::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto* cell = getCell();

    for (auto* c : getChildren())
        c->setEnabled (cell != nullptr);

    if (cell == nullptr)
        return;

    rootBox.setSelectedId (cell->root + 1, juce::dontSendNotification);
    qualityBox.setSelectedId (getQualityChoices().indexOf (cell->quality) + 1, juce::dontSendNotification);
    bassBox.setSelectedId (cell->bass + 2, juce::dontSendNotification);

    int durationId = 0;

    for (int i = 0; i < getDurationChoices().size(); ++i)
        if (std::abs (getDurationChoices()[i] - juce::jmax (0.0, cell->durationBeats)) < 1.0e-6)
            durationId = i + 1;

    durationBox.setSelectedId (durationId, juce::dontSendNotification);

    if (durationId == 0)
        durationBox.setText (durationLabel (cell->durationBeats), juce::dontSendNotification);

    strumBox.setSelectedId (cell->strumOverride.isEmpty() ? 1 : patterns.indexOf (cell->strumOverride) + 2,
                            juce::dontSendNotification);
    emphasisBox.setSelectedId ((int) cell->emphasis + 1, juce::dontSendNotification);

    if (! extensions.hasKeyboardFocus (true))
        extensions.setText (cell->extensions.joinIntoString (", "), juce::dontSendNotification);

    repaint();
}

bool TuneChordEditor::change (const juce::String& description, const std::function<void (ChordCell&)>& edit)
{
    const auto* current = getCell();

    if (current == nullptr)
        return false;

    auto cell = *current;
    edit (cell);

    if (! cell.isValid() || cell == *current)
        return false;

    const int section = sectionIndex, index = cellIndex;
    const bool changed = session.edit (TuneEditClass::chordEdit, description,
                                       [section, index, cell] (Tune& t) { return t.setChord (section, index, cell); },
                                       kChordEditTarget + section * 256 + index);
    refresh();
    return changed;
}

bool TuneChordEditor::setRoot (int pitchClass)
{
    return juce::isPositiveAndBelow (pitchClass, 12) && change ("Chord root", [pitchClass] (ChordCell& c) { c.root = pitchClass; });
}

bool TuneChordEditor::setQuality (const juce::String& q)
{
    return change ("Chord quality", [q] (ChordCell& c) { c.quality = q; });
}

bool TuneChordEditor::setBass (int pitchClass)
{
    return pitchClass >= -1 && pitchClass < 12 && change ("Slash bass", [pitchClass] (ChordCell& c) { c.bass = pitchClass; });
}

bool TuneChordEditor::setExtensions (const juce::String& text)
{
    auto list = juce::StringArray::fromTokens (text, ", ", {});
    list.trim();
    list.removeEmptyStrings();
    list.removeDuplicates (false);

    for (const auto& e : list)
    {
        if (tunetheory::getExtensionSemitones (e) < 0)
        {
            // Named where it is, like the progression field's errors.
            extensionError = "Unknown extension: " + e;
            repaint();
            return false;
        }
    }

    extensionError.clear();
    repaint();
    return change ("Chord extensions", [list] (ChordCell& c) { c.extensions = list; });
}

bool TuneChordEditor::setDuration (double beats)
{
    return change ("Chord duration", [beats] (ChordCell& c) { c.durationBeats = juce::jmax (0.0, beats); });
}

bool TuneChordEditor::setStrumOverride (const juce::String& pattern)
{
    return change ("Strum override", [pattern] (ChordCell& c) { c.strumOverride = pattern; });
}

bool TuneChordEditor::setEmphasis (ChordEmphasis emphasis)
{
    return change ("Chord emphasis", [emphasis] (ChordCell& c) { c.emphasis = emphasis; });
}

void TuneChordEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    const char* labels[] = { "ROOT", "QUALITY", "BASS", "EXTENSIONS", "DURATION", "STRUM", "EMPHASIS" };
    auto area = getLocalBounds().reduced (8);

    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);

    for (auto* label : labels)
    {
        auto row = area.removeFromTop (26);
        g.drawText (label, row.removeFromLeft (84), juce::Justification::centredLeft);
        area.removeFromTop (2);
    }

    if (extensionError.isNotEmpty())
    {
        g.setColour (Palette::warning);
        g.setFont (Fonts::ui (10.0f));
        g.drawText (extensionError, getLocalBounds().removeFromBottom (14).reduced (8, 0), juce::Justification::centredLeft);
    }
}

void TuneChordEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    juce::Component* rows[] = { &rootBox, &qualityBox, &bassBox, &extensions, &durationBox, &strumBox, &emphasisBox };

    for (auto* c : rows)
    {
        auto row = area.removeFromTop (26);
        row.removeFromLeft (84);
        c->setBounds (row.reduced (0, 1));
        area.removeFromTop (2);
    }
}

} // namespace luthier
