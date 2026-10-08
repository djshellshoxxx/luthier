#include "PracticeSetupPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

namespace
{
    constexpr int kHeader = 26;
    constexpr int kRowGap = Metrics::gridHalf;
    constexpr int kRow = Metrics::buttonHeight + kRowGap;
    constexpr int kLine = 16;
    constexpr int kCaptionWidth = 92;
    constexpr int kListRow = 20;

    constexpr int kRoutineRows = 5;
    constexpr int kEntryRows = 5;
    constexpr int kLoopRows = 4;
    constexpr int kSessionRows = 3;
    constexpr int kTabRows = 3;

    constexpr int kChartHeight = 64;
    constexpr int kSparkHeight = 44;
    constexpr int kMaxPhrasesShown = 5;
    constexpr int kWarningHeight = 46;
    constexpr int kMaxAccentButtons = 16;

    const char* const kKeyNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    /** Every scale but custom: a custom scale is an interval list, edited where
        it is used (the drawer's SCALE tab), so it is not a thing to default to. */
    constexpr int kNumScaleToggles = (int) ScaleType::custom;

    const char* const kScaleToggleNames[kNumScaleToggles] = {
        "IONIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXO", "AEOLIAN",
        "LOCRIAN", "HARM MIN", "MEL MIN", "MAJ PENT", "MIN PENT", "BLUES"
    };

    /** practice-tools 1's signatures; any other in a file is shown as well. */
    struct Signature { int numerator, denominator; };
    constexpr Signature kSignatures[] = { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 },
                                          { 7, 4 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };

    const char* const kToolNames[] = { "Metronome", "Looper", "Backing track", "Scale trainer",
                                       "Ear trainer", "Tab reader", "Progression", "Session recorder" };

    const char* const kScaleModeNames[] = { "Explore", "Quiz", "Interval trainer", "Chord tones" };
    const char* const kExerciseNames[] = { "Intervals", "Chord quality", "Progressions" };

    /** Count-in choices, by combo id - 1. */
    constexpr int kCountInBars[] = { 0, 1, 2, 4 };

    juce::String minutesAndSeconds (double seconds)
    {
        const int s = juce::roundToInt (juce::jmax (0.0, seconds));
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    double parseMinutesAndSeconds (const juce::String& text)
    {
        if (text.containsChar (':'))
            return text.upToFirstOccurrenceOf (":", false, false).getIntValue() * 60.0
                 + text.fromFirstOccurrenceOf (":", false, false).getIntValue();

        return text.getDoubleValue();
    }

    juce::String describeMinutes (double seconds)
    {
        const int minutes = juce::roundToInt (juce::jmax (0.0, seconds) / 60.0);

        if (minutes >= 60)
            return juce::String (minutes / 60) + " h " + juce::String (minutes % 60) + " min";

        return juce::String (minutes) + " min";
    }

    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, note), true, true, 4);
    }

    /** "E2", "F#3", "Bb1" or a plain number, for the range sliders' text boxes. */
    double parseNoteNumber (const juce::String& text)
    {
        const auto t = text.trim();

        if (t.isEmpty() || ! juce::CharacterFunctions::isLetter (t[0]))
            return t.getDoubleValue();

        static const int naturals[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);

        if (letter < 'A' || letter > 'G')
            return t.getDoubleValue();

        int pc = naturals[(int) (letter - 'A')];
        int at = 1;

        if (t[at] == '#') { ++pc; ++at; }
        else if (t[at] == 'b') { --pc; ++at; }

        const int octave = t.substring (at).getIntValue();
        return juce::jlimit (0, 127, (octave + 1) * 12 + pc);
    }

    /** "ear.chord_quality" -> "Ear: chord quality"; "scale.dorian.quiz" -> "Scale: dorian, quiz". */
    juce::String describeExercise (const juce::String& key)
    {
        auto parts = juce::StringArray::fromTokens (key.replaceCharacter ('_', ' '), ".", {});

        if (parts.isEmpty())
            return key;

        auto head = parts[0];
        parts.remove (0);
        return head.substring (0, 1).toUpperCase() + head.substring (1) + (parts.isEmpty() ? juce::String()
                                                                                            : ": " + parts.joinIntoString (", "));
    }

    juce::String accentText (BeatAccent a)
    {
        switch (a)
        {
            case BeatAccent::accent:    return "A";
            case BeatAccent::normal:    return "n";
            case BeatAccent::ghost:     return "g";
            case BeatAccent::silent:
            case BeatAccent::numLevels: break;
        }

        return "-";
    }

    //==========================================================================
    /** One of a tool's own settings, offered as a choice in the entry editor
        (the keys are RoutineEntry's, PracticeRoutine.h). */
    struct OptionSpec
    {
        const char* key;
        const char* caption;
        std::vector<std::pair<juce::String, juce::String>> items;   // file key, what the menu says
    };

    std::vector<OptionSpec> optionsFor (PracticeTool tool)
    {
        std::vector<OptionSpec> specs;

        switch (tool)
        {
            case PracticeTool::metronome:
            {
                OptionSpec subdivision { "subdivision", "Subdivision", {} };

                for (int i = 0; i < (int) ClickSubdivision::numSubdivisions; ++i)
                    subdivision.items.emplace_back (practicekeys::subdivision ((ClickSubdivision) i),
                                                    getClickSubdivisionName ((ClickSubdivision) i));

                OptionSpec signature { "time_sig", "Time signature", {} };

                for (const auto& s : kSignatures)
                {
                    const auto text = juce::String (s.numerator) + "/" + juce::String (s.denominator);
                    signature.items.emplace_back (text, text);
                }

                OptionSpec sound { "sound", "Click", {} };

                for (int i = 0; i < (int) ClickSound::numSounds; ++i)
                    sound.items.emplace_back (practicekeys::sound ((ClickSound) i), getClickSoundName ((ClickSound) i));

                specs = { subdivision, signature, sound };
                break;
            }

            case PracticeTool::looper:
            {
                OptionSpec mode { "layer_mode", "Layer mode", {} };

                for (int i = 0; i < (int) LayerMode::numModes; ++i)
                    mode.items.emplace_back (practicekeys::layerMode ((LayerMode) i), getLayerModeName ((LayerMode) i));

                specs = { mode };
                break;
            }

            case PracticeTool::scaleTrainer:
            {
                OptionSpec key { "key", "Key", {} };

                for (auto* name : kKeyNames)
                    key.items.emplace_back (name, name);

                OptionSpec scale { "scale", "Scale", {} };

                for (int i = 0; i < kNumScaleToggles; ++i)
                    scale.items.emplace_back (practicekeys::scale ((ScaleType) i), getScaleTypeName ((ScaleType) i));

                OptionSpec mode { "mode", "Mode", {} };

                for (int i = 0; i < (int) ScaleTrainer::Mode::numModes; ++i)
                    mode.items.emplace_back (practicekeys::scaleMode ((ScaleTrainer::Mode) i), kScaleModeNames[i]);

                specs = { key, scale, mode };
                break;
            }

            case PracticeTool::earTrainer:
            {
                OptionSpec exercise { "exercise", "Exercise", {} };

                for (int i = 0; i < (int) EarTrainer::Exercise::numExercises; ++i)
                    exercise.items.emplace_back (practicekeys::earExercise ((EarTrainer::Exercise) i), kExerciseNames[i]);

                specs = { exercise };
                break;
            }

            case PracticeTool::backingTrack:
            case PracticeTool::tabReader:
            case PracticeTool::progression:
            case PracticeTool::sessionRecorder:
            case PracticeTool::numTools:
                break;
        }

        return specs;
    }

    /** The one free-text setting a tool takes, if any: its file, or the
        progression's chord symbols. */
    std::pair<juce::String, juce::String> textSettingFor (PracticeTool tool)
    {
        switch (tool)
        {
            case PracticeTool::backingTrack: return { "file", "Track file" };
            case PracticeTool::tabReader:    return { "file", "Tab file" };
            case PracticeTool::progression:  return { "text", "Progression" };

            case PracticeTool::metronome:
            case PracticeTool::looper:
            case PracticeTool::scaleTrainer:
            case PracticeTool::earTrainer:
            case PracticeTool::sessionRecorder:
            case PracticeTool::numTools:
                break;
        }

        return {};
    }

    const juce::var* entrySetting (const RoutineEntry& entry, const juce::String& key)
    {
        if (auto* object = entry.settings.getDynamicObject())
            if (object->hasProperty (key))
                return &object->getProperty (key);

        return nullptr;
    }

    /** A settings object to change: a copy, never the one the entry shares
        with the library's routine (juce::var objects are shared by reference). */
    std::unique_ptr<juce::DynamicObject> settingsCopy (const RoutineEntry& entry)
    {
        if (auto* object = entry.settings.getDynamicObject())
            return object->clone();

        return std::make_unique<juce::DynamicObject>();
    }

    void cloneSettings (PracticeRoutine& routine)
    {
        for (auto& entry : routine.entries)
            entry.settings = juce::var (settingsCopy (entry).release());
    }
}

//==============================================================================
/*  A list of names with a muted detail on the right, and a sentence of its own
    when it is empty (11.4: "Saved loops appear here" with the folder button
    still offered beside it). */
class PracticeSetupPanel::ItemList : public juce::ListBox,
                                     private juce::ListBoxModel
{
public:
    ItemList (const juce::String& name, const juce::String& description)
        : juce::ListBox (name)
    {
        setModel (this);
        setRowHeight (kListRow);
        setOutlineThickness (1);
        setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
        setColour (juce::ListBox::outlineColourId, Palette::edge);
        AccessibleSetup::configureDescriptive (*this, name, description);
    }

    ~ItemList() override
    {
        setModel (nullptr);
    }

    std::function<int()> numRows;
    std::function<juce::String (int)> rowText, rowDetail;
    std::function<void (int)> onSelect;
    std::function<juce::String()> emptyText;

    int countRows() const { return numRows != nullptr ? juce::jmax (0, numRows()) : 0; }

    void paintOverChildren (juce::Graphics& g) override
    {
        juce::ListBox::paintOverChildren (g);

        const auto text = emptyText != nullptr ? emptyText() : juce::String();

        if (countRows() > 0 || text.isEmpty())
            return;

        g.setFont (Fonts::ui (11.0f));
        g.setColour (Palette::textDisabled);
        g.drawFittedText (text, getLocalBounds().reduced (Metrics::grid, 2), juce::Justification::centred, 2);
    }

private:
    int getNumRows() override { return countRows(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, countRows()))
            return;

        if (selected)
        {
            g.setColour (Palette::accentDim.withAlpha (0.55f));
            g.fillRect (0, 0, width, height);
        }

        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (Metrics::gridHalf + 2, 0);
        const auto detail = rowDetail != nullptr ? rowDetail (row) : juce::String();

        if (detail.isNotEmpty())
        {
            g.setFont (Fonts::ui (10.0f));
            g.setColour (Palette::textMuted);
            g.drawFittedText (detail, area.removeFromRight (juce::jmin (width / 2, 128)),
                              juce::Justification::centredRight, 1);
        }

        g.setFont (Fonts::ui (11.0f, selected));
        g.setColour (Palette::textPrimary);
        g.drawFittedText (rowText != nullptr ? rowText (row) : juce::String(), area,
                          juce::Justification::centredLeft, 1);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (onSelect != nullptr)
            onSelect (lastRowSelected);
    }
};

//==============================================================================
PracticeSetupLocations PracticeSetupLocations::inside (const juce::File& root)
{
    const auto practice = root.getChildFile ("Practice");

    PracticeSetupLocations l;
    l.statsFile      = practice.getChildFile ("stats.json");
    l.routinesFolder = practice.getChildFile ("Routines");
    l.defaultsFile   = practice.getChildFile ("defaults.json");
    l.libraryFile    = practice.getChildFile ("library.json");
    l.loopsFolder    = root.getChildFile ("Loops");
    l.sessionsFolder = root.getChildFile ("Sessions");
    return l;
}

PracticeSetupContext PracticeSetupPanel::contextFor (LuthierAudioProcessor& processor)
{
    PracticeSetupContext c;
    c.targets.metronome       = &processor.getMetronome();
    c.targets.looper          = &processor.getLooper();
    c.targets.backingTrack    = &processor.getBackingTrack();
    c.targets.scaleTrainer    = &processor.getScaleTrainer();
    c.targets.earTrainer      = &processor.getEarTrainer();
    c.targets.progression     = &processor.getProgressionLooper();
    c.targets.sessionRecorder = &processor.getSessionRecorder();
    c.sampleRate = [&processor] { return processor.getSampleRate(); };

    // The runner and the history belong to the processor, so the drawer's
    // timer (which advances the one and adds minutes to the other) and this
    // tab see the same objects.
    c.runner = &processor.getPracticeRoutineRunner();
    c.stats  = &processor.getPracticeStats();
    c.showInDrawer = [&processor] (PracticeTool tool) { processor.requestPracticeDrawer (tool); };
    return c;
}

//==============================================================================
PracticeSetupPanel::PracticeSetupPanel (LuthierAudioProcessor& p)
    : PracticeSetupPanel (contextFor (p))
{
}

PracticeSetupPanel::PracticeSetupPanel (PracticeSetupContext c)
    : context (std::move (c)),
      routines (context.locations.routinesFolder)
{
    if (context.runner == nullptr)
    {
        ownRunner = std::make_unique<PracticeRoutineRunner> (context.targets);
        context.runner = ownRunner.get();
    }

    if (context.stats == nullptr)
    {
        ownStats = std::make_unique<PracticeStats>();
        context.stats = ownStats.get();
    }

    build();
    setSize (360, getPreferredHeight());
    refresh();
    startTimerHz (2);
}

PracticeSetupPanel::~PracticeSetupPanel()
{
    stopTimer();
}

//==============================================================================
void PracticeSetupPanel::build()
{
    auto makeToggle = [this] (const juce::String& text, const juce::String& tip, std::function<void()> onClick)
    {
        auto toggle = std::make_unique<LuthierToggle> (text);
        toggle->setTooltip (tip);
        toggle->getButton().setClickingTogglesState (true);
        toggle->getButton().onClick = std::move (onClick);
        AccessibleSetup::configureButton (toggle->getButton(), text, tip);
        addAndMakeVisible (*toggle);
        return toggle;
    };

    auto setUpButton = [this] (juce::TextButton& button, const juce::String& label, const juce::String& tip,
                               std::function<void()> onClick)
    {
        button.setTooltip (tip);
        button.onClick = std::move (onClick);
        AccessibleSetup::configureButton (button, label, tip);
        addAndMakeVisible (button);
    };

    auto setUpBox = [this] (juce::ComboBox& box, const juce::String& label, const juce::String& tip)
    {
        box.setTooltip (tip);
        AccessibleSetup::configureComboBox (box, label);
        addAndMakeVisible (box);
    };

    auto setUpSlider = [this] (juce::Slider& slider, double lowest, double highest, double step,
                               const juce::String& label, const juce::String& suffix, const juce::String& tip)
    {
        slider.setRange (lowest, highest, step);
        slider.setTextValueSuffix (suffix);
        slider.setTooltip (tip);
        AccessibleSetup::configureSlider (slider, label, suffix);
        addAndMakeVisible (slider);
    };

    auto setUpEditor = [this] (juce::TextEditor& editor, const juce::String& label, const juce::String& tip)
    {
        editor.setFont (Fonts::ui (12.0f));
        editor.setTooltip (tip);
        editor.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
        editor.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
        editor.setColour (juce::TextEditor::outlineColourId, Palette::edge);
        AccessibleSetup::configureDescriptive (editor, label, tip);
        addAndMakeVisible (editor);
    };

    // --- progress (11.2) ------------------------------------------------------------
    accuracyBox.onChange = [this] { if (! updating) refreshProgress(); };
    setUpBox (accuracyBox, "Trainer exercise", "Which trainer exercise the accuracy chart shows.");

    setUpButton (exportCsvButton, "Export as CSV",
                 "Write the history as a spreadsheet: one row per day with practice.",
                 [this] { exportCsvWithChooser(); });
    setUpButton (clearHistoryButton, "Clear history",
                 "Forget the practice history. Asks first. Saved loops and sessions are kept.",
                 [this] { askToClearHistory(); });

    // --- routines (11.2) ------------------------------------------------------------
    routineList = std::make_unique<ItemList> ("Routines", "The factory routines and your own.");
    routineList->numRows = [this] { return routines.getNumRoutines(); };
    routineList->rowText = [this] (int row) { return routines.getRoutine (row).name; };
    routineList->rowDetail = [this] (int row)
    {
        const auto& r = routines.getRoutine (row);
        return minutesAndSeconds (r.getTotalSeconds()) + (r.factory ? "  factory" : "");
    };
    routineList->onSelect = [this] (int row) { if (! updating && row >= 0) selectRoutine (row); };
    addAndMakeVisible (*routineList);

    setUpButton (startButton, "Start routine in the drawer",
                 "Hand this routine to the drawer, which loads each entry's settings, runs its timer and "
                 "moves on. Pause, skip and stop are in the drawer.",
                 [this] { startSelected(); });
    setUpButton (newRoutineButton, "New routine", "Make an empty routine of your own.", [this] { newRoutine(); });
    setUpButton (duplicateButton, "Duplicate routine",
                 "Copy this routine under a new name. Copy a factory routine to change it.",
                 [this] { duplicateRoutine(); });
    setUpButton (deleteRoutineButton, "Delete routine", "Delete this routine. Asks first.",
                 [this] { askToDeleteRoutine(); });

    setUpEditor (nameEditor, "Routine name", "The routine's name. Press Return to rename it.");
    nameEditor.onReturnKey = [this] { renameRoutine(); };
    nameEditor.onFocusLost = [this] { renameRoutine(); };
    nameEditor.onEscapeKey = [this] { nameEditor.setText (editing.name, false); };

    entryList = std::make_unique<ItemList> ("Routine entries", "The routine's exercises, in order.");
    entryList->numRows = [this] { return (int) editing.entries.size(); };
    entryList->rowText = [this] (int row)
    {
        const auto& e = editing.entries[(size_t) row];
        return juce::String (row + 1) + ". " + getPracticeToolTabLabel (e.tool) + "  " + e.title;
    };
    entryList->rowDetail = [this] (int row)
    {
        const auto& e = editing.entries[(size_t) row];
        return e.isTimed() ? minutesAndSeconds (e.durationSeconds) : juce::String (e.repetitions) + " reps";
    };
    entryList->onSelect = [this] (int row) { if (! updating && row >= 0) selectEntry (row); };
    addAndMakeVisible (*entryList);

    setUpButton (addEntryButton, "Add entry", "Add an exercise after the selected one.", [this]
    {
        if (! canEditSelected())
            return;

        RoutineEntry entry;

        if (const auto* current = currentEntry())
        {
            entry = *current;
            entry.settings = juce::var (settingsCopy (*current).release());
        }
        else
        {
            entry.title = getPracticeToolTabLabel (entry.tool);
            entry.settings = juce::var (new juce::DynamicObject());
        }

        const auto at = juce::jlimit (0, (int) editing.entries.size(), selectedEntry + 1);
        editing.entries.insert (editing.entries.begin() + at, entry);
        saveEditing();
        selectEntry (at);
    });

    setUpButton (removeEntryButton, "Remove entry", "Remove the selected exercise.", [this]
    {
        if (! canEditSelected() || currentEntry() == nullptr)
            return;

        editing.entries.erase (editing.entries.begin() + selectedEntry);
        saveEditing();
        selectEntry (juce::jmin (selectedEntry, (int) editing.entries.size() - 1));
    });

    auto moveEntry = [this] (int delta)
    {
        const int to = selectedEntry + delta;

        if (! canEditSelected() || currentEntry() == nullptr || ! juce::isPositiveAndBelow (to, (int) editing.entries.size()))
            return;

        std::swap (editing.entries[(size_t) selectedEntry], editing.entries[(size_t) to]);
        saveEditing();
        selectEntry (to);
    };

    setUpButton (moveUpButton, "Move entry up", "Move the selected exercise earlier.", [moveEntry] { moveEntry (-1); });
    setUpButton (moveDownButton, "Move entry down", "Move the selected exercise later.", [moveEntry] { moveEntry (1); });

    for (int t = 0; t < (int) PracticeTool::numTools; ++t)
        entryToolBox.addItem (kToolNames[t], t + 1);

    entryToolBox.onChange = [this]
    {
        if (! updating && entryToolBox.getSelectedId() > 0)
            setEntryTool ((PracticeTool) (entryToolBox.getSelectedId() - 1));
    };
    setUpBox (entryToolBox, "Entry tool", "The drawer tab this exercise runs on.");

    setUpEditor (entryTitle, "Entry title", "What the drawer calls this exercise.");
    entryTitle.onReturnKey = [this] { writeEntryFromControls (true); };
    entryTitle.onFocusLost = [this] { writeEntryFromControls (true); };

    entryModeBox.addItem ("Timed", 1);
    entryModeBox.addItem ("Repetitions", 2);
    entryModeBox.onChange = [this] { writeEntryFromControls (true); };
    setUpBox (entryModeBox, "Entry length", "Move on after a time, or after a number of clean passes or answers.");

    setUpSlider (entryDuration, 10.0, 3600.0, 5.0, "Entry duration", {}, "How long the drawer's timer runs.");
    entryDuration.textFromValueFunction = [] (double v) { return minutesAndSeconds (v); };
    entryDuration.valueFromTextFunction = [] (const juce::String& text) { return parseMinutesAndSeconds (text); };
    entryDuration.onValueChange = [this] { writeEntryFromControls (! entryDuration.isMouseButtonDown()); };
    entryDuration.onDragEnd = [this] { writeEntryFromControls (true); };

    setUpSlider (entryRepetitions, 1.0, 100.0, 1.0, "Entry repetitions", " reps",
                 "Passes through a loop, or questions answered, before the drawer moves on.");
    entryRepetitions.onValueChange = [this] { writeEntryFromControls (! entryRepetitions.isMouseButtonDown()); };
    entryRepetitions.onDragEnd = [this] { writeEntryFromControls (true); };

    setUpSlider (entryTempo, 0.0, 300.0, 1.0, "Entry tempo", {},
                 "The practice tempo, set on the metronome. Below 20: the tool keeps its own.");
    entryTempo.textFromValueFunction = [] (double v) { return v < 20.0 ? juce::String ("own tempo")
                                                                       : juce::String (juce::roundToInt (v)) + " bpm"; };
    entryTempo.valueFromTextFunction = [] (const juce::String& text) { return text.getDoubleValue(); };
    entryTempo.onValueChange = [this] { writeEntryFromControls (! entryTempo.isMouseButtonDown()); };
    entryTempo.onDragEnd = [this] { writeEntryFromControls (true); };

    entryCountInBox.addItem ("No count-in", 1);
    entryCountInBox.addItem ("1 bar count-in", 2);
    entryCountInBox.addItem ("2 bar count-in", 3);
    entryCountInBox.addItem ("4 bar count-in", 4);
    entryCountInBox.onChange = [this] { writeEntryFromControls (true); };
    setUpBox (entryCountInBox, "Entry count-in", "Metronome bars before the timer starts.");

    entryClick = makeToggle ("CLICK", "Keep the metronome running under this exercise.",
                             [this] { writeEntryFromControls (true); });

    for (int i = 0; i < kNumEntryOptions; ++i)
    {
        entryOptions[i].onChange = [this, i] { writeEntryOption (i); };
        setUpBox (entryOptions[i], "Entry setting " + juce::String (i + 1), "One of this tool's settings for the exercise.");
    }

    setUpEditor (entryText, "Entry text", "A file for the track or tab, or the chord symbols of a progression.");
    entryText.onReturnKey = [this] { writeEntrySetting (entryTextKey, entryText.getText().trim().isEmpty()
                                                                         ? juce::var() : juce::var (entryText.getText().trim())); };
    entryText.onFocusLost = entryText.onReturnKey;

    // --- defaults (11.2) ------------------------------------------------------------
    setUpSlider (defaultTempo, 20.0, 300.0, 1.0, "Default tempo", " bpm", "The metronome's tempo when the plugin opens.");
    defaultTempo.onValueChange = [this] { writeDefaults (! defaultTempo.isMouseButtonDown()); };
    defaultTempo.onDragEnd = [this] { saveDefaults(); };

    timeSigBox.onChange = [this] { writeDefaults (true); };
    setUpBox (timeSigBox, "Default time signature", "The metronome's time signature when the plugin opens.");

    for (int i = 0; i < (int) ClickSubdivision::numSubdivisions; ++i)
        subdivisionBox.addItem (getClickSubdivisionName ((ClickSubdivision) i), i + 1);

    subdivisionBox.onChange = [this] { writeDefaults (true); };
    setUpBox (subdivisionBox, "Default subdivision", "The metronome's subdivision when the plugin opens.");

    // 11.2 "click sample choice from Resources/Practice/Clicks/": the six
    // sounds of practice-tools 1, which are those files.
    for (int i = 0; i < (int) ClickSound::numSounds; ++i)
        soundBox.addItem (getClickSoundName ((ClickSound) i), i + 1);

    soundBox.onChange = [this] { writeDefaults (true); };
    setUpBox (soundBox, "Default click sound", "The click sample the metronome starts with.");

    setUpSlider (loopLength, 0.0, Looper::kMaxLoopSeconds, 1.0, "Default loop length", " s",
                 "The length the first recording stops at. 0: the first recording sets it.");
    loopLength.textFromValueFunction = [] (double v) { return v < 1.0 ? juce::String ("first take")
                                                                      : juce::String (juce::roundToInt (v)) + " s"; };
    loopLength.valueFromTextFunction = [] (const juce::String& text) { return text.getDoubleValue(); };
    loopLength.onValueChange = [this] { writeDefaults (! loopLength.isMouseButtonDown()); };
    loopLength.onDragEnd = [this] { saveDefaults(); };

    for (int bars = 0; bars <= 4; ++bars)
        loopCountInBox.addItem (bars == 0 ? juce::String ("No count-in")
                                          : juce::String (bars) + " bar count-in", bars + 1);

    loopCountInBox.onChange = [this] { writeDefaults (true); };
    setUpBox (loopCountInBox, "Default loop count-in", "Metronome bars before the looper records.");

    for (int i = 0; i < (int) LayerMode::numModes; ++i)
        overdubBox.addItem (getLayerModeName ((LayerMode) i), i + 1);

    overdubBox.onChange = [this] { writeDefaults (true); };
    setUpBox (overdubBox, "Default overdub mode", "How a new layer records over the loop.");

    for (int k = 0; k < 12; ++k)
        trainerKeyBox.addItem (kKeyNames[k], k + 1);

    trainerKeyBox.onChange = [this] { writeDefaults (true); };
    setUpBox (trainerKeyBox, "Default trainer key", "The key the scale trainer starts in.");

    for (int s = 0; s < kNumScaleToggles; ++s)
    {
        scaleToggles.add (makeToggle (kScaleToggleNames[s],
                                      juce::String ("Include ") + getScaleTypeName ((ScaleType) s)
                                        + " in the trainers' scale set. The first one chosen is where they start.",
                                      [this, s] { writeScaleSet ((ScaleType) s, scaleToggles[s]->getButton().getToggleState()); })
                            .release());
    }

    for (auto* slider : { &rangeLow, &rangeHigh })
    {
        setUpSlider (*slider, 0.0, 127.0, 1.0, slider == &rangeLow ? "Lowest note" : "Highest note", {},
                     "The trainers ask about notes in this range.");
        slider->textFromValueFunction = [] (double v) { return noteName (juce::roundToInt (v)); };
        slider->valueFromTextFunction = [] (const juce::String& text) { return parseNoteNumber (text); };
        slider->onValueChange = [this, slider] { writeDefaults (! slider->isMouseButtonDown()); };
        slider->onDragEnd = [this] { saveDefaults(); };
    }

    setUpSlider (questionCount, 1.0, 200.0, 1.0, "Questions per session", " questions",
                 "How many questions a trainer session asks.");
    questionCount.onValueChange = [this] { writeDefaults (! questionCount.isMouseButtonDown()); };
    questionCount.onDragEnd = [this] { saveDefaults(); };

    shuffleToggle = makeToggle ("SHUFFLE", "Play the backing-track folder in random order.",
                                [this] { writeDefaults (true); });

    setUpSlider (backingLevel, -60.0, 12.0, 0.5, "Default backing level", " dB", "The backing track's level when it loads.");
    backingLevel.onValueChange = [this] { writeDefaults (! backingLevel.isMouseButtonDown()); };
    backingLevel.onDragEnd = [this] { saveDefaults(); };

    // --- library (11.2) -------------------------------------------------------------
    loopList = std::make_unique<ItemList> ("Saved loops", "Loops you saved from the looper.");
    loopList->numRows = [this] { return (int) loopItems.size(); };
    loopList->rowText = [this] (int row) { return loopItems[(size_t) row].name; };
    loopList->rowDetail = [this] (int row)
    {
        const auto& item = loopItems[(size_t) row];
        return item.modified.formatted ("%d %b %Y") + "  " + SessionRecorderSetup::describeBytes (item.sizeBytes);
    };
    loopList->emptyText = [] { return juce::String (PracticeEmptyStates::noLoops); };
    loopList->onSelect = [this] (int) { updateLibraryButtons(); };
    addAndMakeVisible (*loopList);

    setUpButton (loadLoopButton, "Load loop",
                 "Load the loop into the looper and open the drawer's LOOP tab, where it plays.",
                 [this] { loadLoop (loopList->getSelectedRow()); });
    setUpButton (deleteLoopButton, "Delete loop", "Delete the saved loop from disk. Asks first.",
                 [this] { askToDeleteLoop(); });
    setUpButton (openLoopsButton, "Open loops folder", "Show the Loops folder.",
                 [this] { openFolder (context.locations.loopsFolder); });

    sessionList = std::make_unique<ItemList> ("Saved sessions", "Takes saved from the session recorder.");
    sessionList->numRows = [this] { return (int) sessionItems.size(); };
    sessionList->rowText = [this] (int row) { return sessionItems[(size_t) row].name; };
    sessionList->rowDetail = [this] (int row)
    {
        const auto& item = sessionItems[(size_t) row];
        return item.modified.formatted ("%d %b %Y") + "  " + SessionRecorderSetup::describeBytes (item.sizeBytes);
    };
    sessionList->emptyText = [] { return juce::String (PracticeEmptyStates::noSessions); };
    sessionList->onSelect = [this] (int) { updateLibraryButtons(); };
    addAndMakeVisible (*sessionList);

    setUpButton (revealSessionButton, "Reveal session", "Show the saved take in its folder.", [this]
    {
        const int row = sessionList->getSelectedRow();

        if (juce::isPositiveAndBelow (row, (int) sessionItems.size()))
            sessionItems[(size_t) row].file.revealToUser();
    });
    setUpButton (openSessionsButton, "Open sessions folder", "Show the Sessions folder.",
                 [this] { openFolder (context.locations.sessionsFolder); });

    setUpButton (chooseBackingButton, "Choose backing-tracks folder",
                 "The folder the TRACK tab's file chooser opens in.", [this] { chooseBackingFolder(); });
    setUpButton (openBackingButton, "Open backing-tracks folder", "Show the backing-tracks folder.", [this]
    {
        const juce::File folder (defaults.backingFolder);

        if (juce::File::isAbsolutePath (defaults.backingFolder) && folder.isDirectory())
            folder.revealToUser();
    });

    tabList = std::make_unique<ItemList> ("Recent tabs", "Tab files opened recently in the drawer's TAB tab.");
    tabList->numRows = [this] { return library.getRecentTabs().size(); };
    tabList->rowText = [this] (int row) { return library.getRecentTabs()[row].getFileName(); };
    tabList->rowDetail = [this] (int row) { return library.getRecentTabs()[row].getParentDirectory().getFileName(); };
    tabList->emptyText = [] { return juce::String ("Tab files you open appear here"); };
    tabList->onSelect = [this] (int) { updateLibraryButtons(); };
    addAndMakeVisible (*tabList);

    setUpButton (openTabFolderButton, "Open tab folder", "Show the selected tab file in its folder.", [this]
    {
        const int row = tabList->getSelectedRow();

        if (juce::isPositiveAndBelow (row, library.getRecentTabs().size()))
            library.getRecentTabs()[row].revealToUser();
    });

    // --- session recorder (11.2) ----------------------------------------------------
    setUpSlider (ringMinutes, 1.0, 240.0, 1.0, "Ring length", " min",
                 "How many minutes the session recorder keeps. The memory is taken when the recorder is on.");
    ringMinutes.onValueChange = [this] { writeSessionSetup (! ringMinutes.isMouseButtonDown()); };
    ringMinutes.onDragEnd = [this] { writeSessionSetup (true); };

    recordWhatBox.addItem ("Audio and MIDI", 1);
    recordWhatBox.addItem ("Audio only", 2);
    recordWhatBox.addItem ("MIDI only", 3);
    recordWhatBox.onChange = [this] { writeSessionSetup (false); };
    setUpBox (recordWhatBox, "Record", "What the session recorder keeps.");

    autoSaveToggle = makeToggle ("AUTO-SAVE", "Save the take as a WAV and MIDI file whenever the recorder stops.",
                                 [this] { writeSessionSetup (false); });
}

//==============================================================================
PracticeStats& PracticeSetupPanel::getStats() noexcept             { return *context.stats; }
PracticeRoutineRunner& PracticeSetupPanel::getRunner() noexcept    { return *context.runner; }
juce::ListBox& PracticeSetupPanel::getRoutineList() noexcept       { return *routineList; }
juce::ListBox& PracticeSetupPanel::getEntryList() noexcept         { return *entryList; }
juce::ListBox& PracticeSetupPanel::getLoopList() noexcept          { return *loopList; }
juce::ListBox& PracticeSetupPanel::getSessionList() noexcept       { return *sessionList; }
juce::ListBox& PracticeSetupPanel::getRecentTabList() noexcept     { return *tabList; }

juce::ComboBox& PracticeSetupPanel::getEntryOptionBox (int index) noexcept
{
    return entryOptions[juce::jlimit (0, kNumEntryOptions - 1, index)];
}

juce::Button& PracticeSetupPanel::getScaleToggle (ScaleType scale) noexcept
{
    return scaleToggles[juce::jlimit (0, scaleToggles.size() - 1, (int) scale)]->getButton();
}

//==============================================================================
void PracticeSetupPanel::refresh()
{
    if (ownStats != nullptr)
        ownStats->load (context.locations.statsFile);

    refreshProgress();
    refreshRoutineList();
    loadDefaults();
    showDefaults();
    showSessionSetup();
    refreshLibrary();
    refreshRunnerStatus();
    fitHeight();
    repaint();
}

void PracticeSetupPanel::timerCallback()
{
    // A tab that is not on screen reads nothing: the viewport shows one panel
    // at a time, and the others' timers idle.
    if (! isShowing())
        return;

    // The drawer adds minutes and moves the routine on; show both as they go.
    // The folders are read less often, as reading them walks the disk.
    if (ownStats != nullptr && ticks % 4 == 0)
        ownStats->load (context.locations.statsFile);

    refreshProgress();
    refreshRunnerStatus();

    if (++ticks % 4 == 0)
        refreshLibrary();
}

//==============================================================================
// Progress (11.2): read-only, from stats.json.
//==============================================================================
void PracticeSetupPanel::refreshProgress()
{
    const auto& stats = getStats();
    const auto today = PracticeStats::today();

    statsEmpty = stats.isEmpty();

    dayMinutes.clear();
    toolSessions.fill (0);
    double total = 0.0;

    for (const auto& day : stats.getRecentDays (today))
    {
        dayMinutes.push_back (day.getTotalSeconds() / 60.0);
        total += day.getTotalSeconds();

        for (size_t t = 0; t < toolSessions.size(); ++t)
            toolSessions[t] += day.sessions[t];
    }

    toolSeconds = stats.getToolTotals (today);

    // 11.4, word for word, when there is nothing yet.
    if (statsEmpty)
    {
        progressSummary = PracticeEmptyStates::noStats;
    }
    else
    {
        const int streak = stats.getStreak (today);
        progressSummary = "Streak: " + juce::String (streak) + (streak == 1 ? " day" : " days")
                        + ".  Today: " + describeMinutes (stats.getSeconds (today))
                        + ".  Last 90 days: " + describeMinutes (total) + ".";
    }

    // Accuracy over time, per exercise: the box lists what has results.
    const auto found = stats.getExercises();

    if (found != exercises)
    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        const auto shown = exercises[accuracyBox.getSelectedId() - 1];

        exercises = found;
        accuracyBox.clear (juce::dontSendNotification);

        for (int i = 0; i < exercises.size(); ++i)
            accuracyBox.addItem (describeExercise (exercises[i]), i + 1);

        const int keep = exercises.indexOf (shown);
        accuracyBox.setSelectedId (keep >= 0 ? keep + 1 : (exercises.isEmpty() ? 0 : 1), juce::dontSendNotification);
    }

    const auto exercise = exercises[accuracyBox.getSelectedId() - 1];
    accuracy = exercise.isNotEmpty() ? stats.getAccuracyHistory (exercise)
                                     : std::vector<std::pair<juce::String, double>>();

    phrases = stats.getTempoProgress();

    accuracyBox.setVisible (! statsEmpty);
    accuracyBox.setEnabled (! exercises.isEmpty());
    exportCsvButton.setEnabled (! statsEmpty);
    clearHistoryButton.setEnabled (! statsEmpty);

    // The section only; the timer calls this twice a second.
    fitHeight();
    repaint (progressHeader.getUnion (progressStatusBounds));
}

bool PracticeSetupPanel::exportCsvTo (const juce::File& destination, juce::String* error)
{
    if (getStats().isEmpty())
    {
        if (error != nullptr)
            *error = "There is no practice history to export yet.";

        return false;
    }

    if (! destination.getParentDirectory().createDirectory().wasOk()
        || ! destination.replaceWithText (getStats().toCsv(), false, false, "\n"))
    {
        if (error != nullptr)
            *error = "Could not write " + destination.getFullPathName();

        return false;
    }

    return true;
}

void PracticeSetupPanel::exportCsvWithChooser()
{
    const auto start = context.locations.statsFile.getParentDirectory().getChildFile (
        "Practice history " + PracticeStats::today() + ".csv");

    chooser = std::make_unique<juce::FileChooser> ("Export the practice history", start, "*.csv");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<PracticeSetupPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;
        const bool ok = safe->exportCsvTo (file.withFileExtension (".csv"), &error);

        safe->progressStatus = ok ? "Exported to " + file.withFileExtension (".csv").getFullPathName() : error;
        safe->repaint();
    });
}

void PracticeSetupPanel::askToClearHistory()
{
    // 11.2: "a Clear history that confirms".
    juce::NativeMessageBox::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Clear practice history?")
            .withMessage ("This forgets every day of practice time, trainer score and best tempo.\n\n"
                          "Your saved loops and sessions are NOT deleted.\n\nThis cannot be undone.")
            .withButton ("Clear history")
            .withButton ("Cancel")
            .withAssociatedComponent (this),
        [safe = juce::Component::SafePointer<PracticeSetupPanel> (this)] (int result)
        {
            if (safe != nullptr && result == 0)   // plain index: 0 confirms, 1 is Cancel
                safe->clearHistoryConfirmed();
        });
}

void PracticeSetupPanel::clearHistoryConfirmed()
{
    // 12.1: "on confirm, empties stats.json without deleting saved loops or
    // sessions". Only the history key goes; the ear trainer's keys in the same
    // file stay (PracticeStats::save).
    getStats().clear();

    juce::String error;
    progressStatus = getStats().save (context.locations.statsFile, error)
                       ? juce::String ("History cleared.")
                       : "Cleared here, but stats.json could not be written: " + error;

    refreshProgress();
}

//==============================================================================
// Routines (11.2)
//==============================================================================
bool PracticeSetupPanel::canEditSelected() const noexcept
{
    return ! editing.factory && editing.name.isNotEmpty();
}

RoutineEntry* PracticeSetupPanel::currentEntry() noexcept
{
    return juce::isPositiveAndBelow (selectedEntry, (int) editing.entries.size())
             ? &editing.entries[(size_t) selectedEntry] : nullptr;
}

juce::String PracticeSetupPanel::getUserRoutinesText() const
{
    // 11.4: the factory three are always there, so only the user's part can
    // be empty.
    return routines.getNumUserRoutines() == 0 ? juce::String (PracticeEmptyStates::noUserRoutines) : juce::String();
}

void PracticeSetupPanel::refreshRoutineList()
{
    const auto shown = editing.name;

    routines.refresh();
    routineList->updateContent();

    const int index = routines.indexOf (shown);
    selectRoutine (index >= 0 ? index : juce::jlimit (0, routines.getNumRoutines() - 1, selectedRoutine));

    if (! routines.getLoadErrors().isEmpty())
        routineStatus = "Skipped: " + routines.getLoadErrors().joinIntoString ("; ");
}

void PracticeSetupPanel::selectRoutine (int index)
{
    if (routines.getNumRoutines() <= 0)
        return;

    selectedRoutine = juce::jlimit (0, routines.getNumRoutines() - 1, index);
    editing = routines.getRoutine (selectedRoutine);
    cloneSettings (editing);
    selectedEntry = editing.entries.empty() ? -1 : 0;

    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        routineList->updateContent();
        routineList->selectRow (selectedRoutine);
    }

    refreshRoutineEditor();
}

void PracticeSetupPanel::selectEntry (int index)
{
    selectedEntry = editing.entries.empty() ? -1 : juce::jlimit (0, (int) editing.entries.size() - 1, index);

    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        entryList->updateContent();

        if (selectedEntry >= 0)
            entryList->selectRow (selectedEntry);
        else
            entryList->deselectAllRows();
    }

    refreshEntryEditor();
}

void PracticeSetupPanel::refreshRoutineEditor()
{
    const bool editable = canEditSelected();

    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        nameEditor.setText (editing.name, false);
        nameEditor.setReadOnly (! editable);
        entryList->updateContent();

        if (selectedEntry >= 0)
            entryList->selectRow (selectedEntry);
    }

    startButton.setEnabled (! editing.entries.empty());
    deleteRoutineButton.setEnabled (editable);
    addEntryButton.setEnabled (editable);

    refreshEntryEditor();
    repaint();
}

void PracticeSetupPanel::refreshEntryEditor()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const auto* entry = currentEntry();
    const bool editable = canEditSelected() && entry != nullptr;

    removeEntryButton.setEnabled (editable);
    moveUpButton.setEnabled (editable && selectedEntry > 0);
    moveDownButton.setEnabled (editable && selectedEntry < (int) editing.entries.size() - 1);

    const std::initializer_list<juce::Component*> entryControls { &entryToolBox, &entryTitle, &entryModeBox,
                                                                  &entryDuration, &entryRepetitions, &entryTempo,
                                                                  &entryCountInBox, entryClick.get(), &entryText };

    for (auto* c : entryControls)
        c->setEnabled (editable);

    // Factory routines show their settings, read-only (duplicate to change).
    entryTitle.setReadOnly (! editable);
    entryText.setReadOnly (! editable);

    if (entry == nullptr)
    {
        entryToolBox.setSelectedId (0, juce::dontSendNotification);
        entryTitle.setText ({}, false);
        entryText.setVisible (false);
        entryClick->setVisible (false);

        for (int i = 0; i < kNumEntryOptions; ++i)
        {
            entryOptions[i].setVisible (false);
            entryOptionCaptions[i] = {};
            entryOptionKeys[i] = {};
        }

        entryTextKey = entryTextCaption = {};
        resized();
        repaint();
        return;
    }

    entryToolBox.setSelectedId ((int) entry->tool + 1, juce::dontSendNotification);
    entryTitle.setText (entry->title, false);

    entryModeBox.setSelectedId (entry->isTimed() ? 1 : 2, juce::dontSendNotification);
    entryDuration.setValue (entry->durationSeconds, juce::dontSendNotification);
    entryRepetitions.setValue (juce::jmax (1, entry->repetitions), juce::dontSendNotification);
    entryDuration.setVisible (entry->isTimed());
    entryRepetitions.setVisible (! entry->isTimed());

    const auto* tempo = entrySetting (*entry, "tempo_bpm");
    entryTempo.setValue (tempo != nullptr ? (double) *tempo : 0.0, juce::dontSendNotification);

    int countInId = 1;

    for (int i = 0; i < (int) (sizeof (kCountInBars) / sizeof (kCountInBars[0])); ++i)
        if (kCountInBars[i] <= entry->countInBars)
            countInId = i + 1;

    entryCountInBox.setSelectedId (countInId, juce::dontSendNotification);

    // "click" keeps the metronome under another tool; on the metronome it is the tool.
    const auto* click = entrySetting (*entry, "click");
    entryClick->setVisible (entry->tool != PracticeTool::metronome);
    entryClick->getButton().setToggleState (click != nullptr && (bool) *click, juce::dontSendNotification);

    // The tool's own settings. A value the menu does not know (a newer file's)
    // is shown as it is and kept, not replaced (file-formats 0.3).
    const auto specs = optionsFor (entry->tool);

    for (int i = 0; i < kNumEntryOptions; ++i)
    {
        auto& box = entryOptions[i];
        box.clear (juce::dontSendNotification);

        const bool used = i < (int) specs.size();
        box.setVisible (used);
        entryOptionKeys[i] = used ? juce::String (specs[(size_t) i].key) : juce::String();
        entryOptionCaptions[i] = used ? juce::String (specs[(size_t) i].caption) : juce::String();

        if (! used)
            continue;

        const auto& spec = specs[(size_t) i];
        box.addItem ("Tool's own", 1);

        for (size_t item = 0; item < spec.items.size(); ++item)
            box.addItem (spec.items[item].second, (int) item + 2);

        const auto* value = entrySetting (*entry, spec.key);
        int selected = 1;

        if (value != nullptr)
        {
            selected = 0;

            for (size_t item = 0; item < spec.items.size(); ++item)
                if (spec.items[item].first == value->toString())
                    selected = (int) item + 2;

            if (selected == 0)
            {
                selected = (int) spec.items.size() + 2;
                box.addItem (value->toString(), selected);
            }
        }

        box.setSelectedId (selected, juce::dontSendNotification);
        box.setEnabled (editable);
    }

    const auto text = textSettingFor (entry->tool);
    entryTextKey = text.first;
    entryTextCaption = text.second;
    entryText.setVisible (entryTextKey.isNotEmpty());

    const auto* textValue = entryTextKey.isNotEmpty() ? entrySetting (*entry, entryTextKey) : nullptr;
    entryText.setText (textValue != nullptr ? textValue->toString() : juce::String(), false);

    resized();
    repaint();
}

void PracticeSetupPanel::refreshRunnerStatus()
{
    const auto& runner = getRunner();
    juce::String text;

    if (runner.isActive())
    {
        text = "In the drawer: " + runner.getRoutine().name + ", " + juce::String (runner.getEntryIndex() + 1)
             + " of " + juce::String ((int) runner.getRoutine().entries.size())
             + " (" + getPracticeToolTabLabel (runner.getActiveTool()) + ")"
             + (runner.getPhase() == PracticeRoutineRunner::Phase::paused ? ", paused." : ".");
    }
    else if (runner.getPhase() == PracticeRoutineRunner::Phase::finished)
    {
        text = "Finished: " + runner.getRoutine().name + ".";
    }
    else
    {
        text = "START hands the routine to the drawer, which runs it.";
    }

    if (text != runnerStatus)
    {
        runnerStatus = text;
        repaint (routineStatusBounds);
    }
}

void PracticeSetupPanel::saveEditing()
{
    if (! canEditSelected())
        return;

    juce::String error;

    if (routines.save (editing, error))
    {
        selectedRoutine = juce::jmax (0, routines.indexOf (editing.name));
        routineStatus = {};
    }
    else
    {
        routineStatus = "Could not save: " + error;
    }

    routineList->updateContent();
    routineList->repaint();
    entryList->updateContent();
    entryList->repaint();
    repaint (routineStatusBounds.getUnion (userHintBounds));
}

void PracticeSetupPanel::startSelected()
{
    /*  11.2 puts starting a routine here and 11.3 keeps every transport in the
        drawer. START does not run anything itself: it gives the routine to the
        runner the drawer advances and opens the drawer on the first entry.
        Pausing, skipping and stopping are the drawer's, and this tab offers
        none of them. */
    auto& runner = getRunner();

    if (! runner.start (editing))
    {
        routineStatus = "This routine has no entries to run.";
        repaint (routineStatusBounds);
        return;
    }

    routineStatus = runner.getLastApplyProblems().isEmpty()
                      ? juce::String()
                      : "Some settings did not take: " + runner.getLastApplyProblems().joinIntoString ("; ");

    if (context.showInDrawer != nullptr)
        context.showInDrawer (runner.getActiveTool());

    refreshRunnerStatus();
    repaint (routineStatusBounds);
}

juce::String PracticeSetupPanel::uniqueRoutineName (const juce::String& base) const
{
    auto taken = [this] (const juce::String& name)
    {
        return routines.indexOf (name) >= 0 || routines.getFileFor (name).exists();
    };

    if (! taken (base))
        return base;

    int n = 2;

    while (taken (base + " " + juce::String (n)))
        ++n;

    return base + " " + juce::String (n);
}

void PracticeSetupPanel::newRoutine()
{
    PracticeRoutine routine;
    routine.name = uniqueRoutineName ("My routine");

    // One entry to start from: the default tempo and subdivision, two minutes.
    RoutineEntry entry;
    entry.tool = PracticeTool::metronome;
    entry.title = getPracticeToolTabLabel (entry.tool);
    entry.durationSeconds = 120.0;

    auto* settings = new juce::DynamicObject();
    settings->setProperty ("tempo_bpm", defaults.metronomeTempo);
    settings->setProperty ("subdivision", practicekeys::subdivision (defaults.subdivision));
    entry.settings = juce::var (settings);
    routine.entries.push_back (entry);

    juce::String error;

    if (! routines.save (routine, error))
    {
        routineStatus = "Could not make a routine: " + error;
        repaint (routineStatusBounds);
        return;
    }

    routineList->updateContent();
    selectRoutine (routines.indexOf (routine.name));
}

void PracticeSetupPanel::duplicateRoutine()
{
    auto copy = editing;
    copy.factory = false;
    copy.name = uniqueRoutineName (editing.name + " copy");
    cloneSettings (copy);

    juce::String error;

    if (! routines.save (copy, error))
    {
        routineStatus = "Could not copy: " + error;
        repaint (routineStatusBounds);
        return;
    }

    routineList->updateContent();
    selectRoutine (routines.indexOf (copy.name));
}

void PracticeSetupPanel::askToDeleteRoutine()
{
    if (! canEditSelected())
        return;

    juce::NativeMessageBox::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Delete this routine?")
            .withMessage ("\"" + editing.name + "\" will be deleted from disk. This cannot be undone.")
            .withButton ("Delete")
            .withButton ("Cancel")
            .withAssociatedComponent (this),
        [safe = juce::Component::SafePointer<PracticeSetupPanel> (this)] (int result)
        {
            if (safe != nullptr && result == 0)   // plain index: 0 confirms, 1 is Cancel
                safe->deleteRoutineConfirmed();
        });
}

void PracticeSetupPanel::deleteRoutineConfirmed()
{
    if (! canEditSelected())
        return;

    const auto name = editing.name;

    if (! routines.remove (name))
    {
        routineStatus = "Could not delete \"" + name + "\".";
        repaint (routineStatusBounds);
        return;
    }

    routineStatus = "Deleted \"" + name + "\".";
    editing = {};
    routineList->updateContent();
    selectRoutine (juce::jmin (selectedRoutine, routines.getNumRoutines() - 1));
}

void PracticeSetupPanel::renameRoutine()
{
    if (updating || ! canEditSelected())
        return;

    const auto oldName = editing.name;
    const auto newName = nameEditor.getText().trim();

    if (newName.isEmpty() || newName == oldName)
    {
        nameEditor.setText (oldName, false);
        return;
    }

    if (routines.indexOf (newName) >= 0)
    {
        routineStatus = "There is already a routine called \"" + newName + "\".";
        nameEditor.setText (oldName, false);
        repaint (routineStatusBounds);
        return;
    }

    auto renamed = editing;
    renamed.name = newName;

    juce::String error;

    if (! routines.save (renamed, error))
    {
        routineStatus = "Could not rename: " + error;
        nameEditor.setText (oldName, false);
        repaint (routineStatusBounds);
        return;
    }

    // Two names can make one file name ("Riffs?" and "Riffs"); removing the old
    // name would then delete the file just written, so re-read the folder instead.
    if (routines.getFileFor (oldName) == routines.getFileFor (newName))
        routines.refresh();
    else
        routines.remove (oldName);

    editing = renamed;
    routineStatus = {};
    routineList->updateContent();
    selectRoutine (routines.indexOf (newName));
}

void PracticeSetupPanel::setEntryTool (PracticeTool tool)
{
    auto* entry = currentEntry();

    if (entry == nullptr || ! canEditSelected() || entry->tool == tool)
        return;

    // The old tool's settings mean nothing to the new one. The practice tempo
    // and the click are anybody's, so they stay.
    const bool defaultTitle = entry->title.isEmpty() || entry->title == getPracticeToolTabLabel (entry->tool);
    auto settings = std::make_unique<juce::DynamicObject>();

    if (const auto* old = entry->settings.getDynamicObject())
        for (auto* key : { "tempo_bpm", "click" })
            if (old->hasProperty (key))
                settings->setProperty (key, old->getProperty (key));

    entry->tool = tool;
    entry->settings = juce::var (settings.release());

    if (defaultTitle)
        entry->title = getPracticeToolTabLabel (tool);

    saveEditing();
    refreshEntryEditor();
}

void PracticeSetupPanel::writeEntrySetting (const juce::String& key, const juce::var& value)
{
    auto* entry = currentEntry();

    if (updating || entry == nullptr || ! canEditSelected() || key.isEmpty())
        return;

    auto settings = settingsCopy (*entry);

    if (value.isVoid())
        settings->removeProperty (key);
    else
        settings->setProperty (key, value);

    entry->settings = juce::var (settings.release());
    saveEditing();
}

void PracticeSetupPanel::writeEntryFromControls (bool save)
{
    auto* entry = currentEntry();

    if (updating || entry == nullptr || ! canEditSelected())
        return;

    entry->title = entryTitle.getText().trim();

    const bool repetitions = entryModeBox.getSelectedId() == 2;
    entry->repetitions = repetitions ? (int) entryRepetitions.getValue() : 0;
    entry->durationSeconds = entryDuration.getValue();
    entry->countInBars = kCountInBars[(size_t) juce::jlimit (0, 3, entryCountInBox.getSelectedId() - 1)];

    auto settings = settingsCopy (*entry);

    if (entryTempo.getValue() >= 20.0)
        settings->setProperty ("tempo_bpm", entryTempo.getValue());
    else
        settings->removeProperty ("tempo_bpm");

    if (entry->tool != PracticeTool::metronome && entryClick->getButton().getToggleState())
        settings->setProperty ("click", true);
    else
        settings->removeProperty ("click");

    entry->settings = juce::var (settings.release());

    entryDuration.setVisible (! repetitions);
    entryRepetitions.setVisible (repetitions);

    if (save)
        saveEditing();
    else
        entryList->repaint();
}

void PracticeSetupPanel::writeEntryOption (int index)
{
    auto* entry = currentEntry();

    if (updating || entry == nullptr || ! juce::isPositiveAndBelow (index, kNumEntryOptions))
        return;

    const auto specs = optionsFor (entry->tool);

    if (! juce::isPositiveAndBelow (index, (int) specs.size()))
        return;

    const auto& spec = specs[(size_t) index];
    const int id = entryOptions[index].getSelectedId();

    if (id == 1)
        writeEntrySetting (spec.key, {});
    else if (juce::isPositiveAndBelow (id - 2, (int) spec.items.size()))
        writeEntrySetting (spec.key, spec.items[(size_t) (id - 2)].first);

    // Anything else is the file's own value, shown and left as it was.
}

//==============================================================================
// Defaults (11.2): starting settings, loaded into the tools when the plugin
// opens. Editing them starts nothing and changes no running tool.
//==============================================================================
void PracticeSetupPanel::loadDefaults()
{
    const auto& file = context.locations.defaultsFile;
    PracticeDefaults loaded;
    juce::String error;

    defaultsStatus = {};

    if (! file.existsAsFile())
        defaults = PracticeDefaults();
    else if (PracticeDefaults::load (file, loaded, error))
        defaults = loaded;
    else
        defaultsStatus = "defaults.json could not be read (" + error + "). Showing the built-in defaults.";

    sessionSetup = SessionRecorderSetup::fromVar (defaults.extra[kSessionSetupKey]);
}

void PracticeSetupPanel::showDefaults()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    defaultTempo.setValue (defaults.metronomeTempo, juce::dontSendNotification);

    timeSigBox.clear (juce::dontSendNotification);
    const auto current = juce::String (defaults.timeSigNumerator) + "/" + juce::String (defaults.timeSigDenominator);
    int selected = 0;

    for (int i = 0; i < (int) (sizeof (kSignatures) / sizeof (kSignatures[0])); ++i)
    {
        const auto text = juce::String (kSignatures[i].numerator) + "/" + juce::String (kSignatures[i].denominator);
        timeSigBox.addItem (text, i + 1);

        if (text == current)
            selected = i + 1;
    }

    if (selected == 0)
    {
        selected = 100;
        timeSigBox.addItem (current, selected);
    }

    timeSigBox.setSelectedId (selected, juce::dontSendNotification);
    subdivisionBox.setSelectedId ((int) defaults.subdivision + 1, juce::dontSendNotification);
    soundBox.setSelectedId ((int) defaults.sound + 1, juce::dontSendNotification);

    loopLength.setValue (defaults.loopLengthSeconds, juce::dontSendNotification);
    loopCountInBox.setSelectedId (juce::jlimit (0, 4, defaults.loopCountInBars) + 1, juce::dontSendNotification);
    overdubBox.setSelectedId ((int) defaults.overdubMode + 1, juce::dontSendNotification);

    trainerKeyBox.setSelectedId (juce::jlimit (0, 11, defaults.trainerKey) + 1, juce::dontSendNotification);

    for (int s = 0; s < scaleToggles.size(); ++s)
        scaleToggles[s]->getButton().setToggleState (std::find (defaults.scaleSet.begin(), defaults.scaleSet.end(),
                                                                (ScaleType) s) != defaults.scaleSet.end(),
                                                     juce::dontSendNotification);

    rangeLow.setValue (defaults.rangeLowNote, juce::dontSendNotification);
    rangeHigh.setValue (defaults.rangeHighNote, juce::dontSendNotification);
    questionCount.setValue (defaults.questionCount, juce::dontSendNotification);

    shuffleToggle->getButton().setToggleState (defaults.shuffle, juce::dontSendNotification);
    backingLevel.setValue (defaults.backingLevelDb, juce::dontSendNotification);

    rebuildAccentButtons();
}

void PracticeSetupPanel::writeDefaults (bool save)
{
    if (updating)
        return;

    defaults.metronomeTempo = defaultTempo.getValue();

    const auto sig = timeSigBox.getText();
    const int numerator = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue();
    const int denominator = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
    const bool newBar = numerator >= 1 && numerator != defaults.timeSigNumerator;

    if (numerator >= 1 && denominator >= 1)
    {
        defaults.timeSigNumerator = numerator;
        defaults.timeSigDenominator = denominator;
    }

    // One accent per beat of the new bar; the beats that were there keep theirs.
    if (newBar)
        defaults.accents.resize ((size_t) numerator, BeatAccent::normal);

    defaults.subdivision = (ClickSubdivision) juce::jlimit (0, (int) ClickSubdivision::numSubdivisions - 1,
                                                            subdivisionBox.getSelectedId() - 1);
    defaults.sound = (ClickSound) juce::jlimit (0, (int) ClickSound::numSounds - 1, soundBox.getSelectedId() - 1);

    defaults.loopLengthSeconds = loopLength.getValue();
    defaults.loopCountInBars = juce::jlimit (0, 4, loopCountInBox.getSelectedId() - 1);
    defaults.overdubMode = (LayerMode) juce::jlimit (0, (int) LayerMode::numModes - 1, overdubBox.getSelectedId() - 1);

    defaults.trainerKey = juce::jlimit (0, 11, trainerKeyBox.getSelectedId() - 1);
    defaults.rangeLowNote = (int) rangeLow.getValue();
    defaults.rangeHighNote = juce::jmax (defaults.rangeLowNote, (int) rangeHigh.getValue());
    defaults.questionCount = (int) questionCount.getValue();

    if (rangeHigh.getValue() < defaults.rangeHighNote)
    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        rangeHigh.setValue (defaults.rangeHighNote, juce::dontSendNotification);
    }

    defaults.shuffle = shuffleToggle->getButton().getToggleState();
    defaults.backingLevelDb = backingLevel.getValue();

    if (newBar)
    {
        rebuildAccentButtons();
        resized();
    }

    if (save)
        saveDefaults();
}

void PracticeSetupPanel::writeScaleSet (ScaleType scale, bool included)
{
    if (updating)
        return;

    // The set keeps its order, so the first scale - where the trainers start
    // (PracticeDefaults::applyTo) - stays first while others come and go.
    auto& set = defaults.scaleSet;
    set.erase (std::remove (set.begin(), set.end(), scale), set.end());

    if (included)
        set.push_back (scale);

    saveDefaults();
}

void PracticeSetupPanel::saveDefaults()
{
    juce::String error;
    const auto before = defaultsStatus;

    defaultsStatus = defaults.save (context.locations.defaultsFile, error) ? juce::String()
                                                                           : "Could not save the defaults: " + error;

    if (defaultsStatus != before)
        repaint (defaultsStatusBounds);
}

void PracticeSetupPanel::rebuildAccentButtons()
{
    const int beats = juce::jlimit (1, kMaxAccentButtons, defaults.timeSigNumerator);

    while (accentButtons.size() > beats)
        accentButtons.removeLast();

    while (accentButtons.size() < beats)
    {
        const int beat = accentButtons.size();
        auto* button = accentButtons.add (new juce::TextButton());

        button->setTooltip ("Beat " + juce::String (beat + 1) + ": click to cycle accent (A), normal (n), "
                            "ghost (g) and silent (-).");

        // accent -> normal -> ghost -> silent -> accent, as the drawer's buttons go.
        button->onClick = [this, beat]
        {
            if (defaults.accents.size() <= (size_t) beat)
                defaults.accents.resize ((size_t) beat + 1, BeatAccent::normal);

            auto& a = defaults.accents[(size_t) beat];
            a = (BeatAccent) (((int) a + (int) BeatAccent::numLevels - 1) % (int) BeatAccent::numLevels);

            rebuildAccentButtons();
            saveDefaults();
        };

        AccessibleSetup::configureButton (*button, "Beat " + juce::String (beat + 1) + " accent",
                                          "Cycles the default accent of this beat.");
        addAndMakeVisible (button);
    }

    for (int beat = 0; beat < accentButtons.size(); ++beat)
    {
        const auto level = (size_t) beat < defaults.accents.size() ? defaults.accents[(size_t) beat] : BeatAccent::normal;
        auto* button = accentButtons[beat];
        button->setButtonText (accentText (level));
        button->setToggleState (level == BeatAccent::accent, juce::dontSendNotification);
    }
}

//==============================================================================
// Library (11.2)
//==============================================================================
void PracticeSetupPanel::refreshLibrary()
{
    loopItems = PracticeLibrary::listLoops (context.locations.loopsFolder);
    sessionItems = PracticeLibrary::listSessions (context.locations.sessionsFolder);

    // The drawer's TAB tab notes the files it opens into library.json.
    if (! library.load (context.locations.libraryFile))
        library = PracticeLibrary();

    library.pruneMissing();

    loopList->updateContent();
    sessionList->updateContent();
    tabList->updateContent();
    loopList->repaint();
    sessionList->repaint();
    tabList->repaint();

    updateLibraryButtons();
}

void PracticeSetupPanel::updateLibraryButtons()
{
    const bool loop = juce::isPositiveAndBelow (loopList->getSelectedRow(), (int) loopItems.size());
    loadLoopButton.setEnabled (loop && context.targets.looper != nullptr);
    deleteLoopButton.setEnabled (loop);

    revealSessionButton.setEnabled (juce::isPositiveAndBelow (sessionList->getSelectedRow(), (int) sessionItems.size()));
    openTabFolderButton.setEnabled (juce::isPositiveAndBelow (tabList->getSelectedRow(), library.getRecentTabs().size()));
    openBackingButton.setEnabled (juce::File::isAbsolutePath (defaults.backingFolder)
                                  && juce::File (defaults.backingFolder).isDirectory());
}

juce::String PracticeSetupPanel::getLoopsText() const
{
    return loopItems.empty() ? juce::String (PracticeEmptyStates::noLoops) : juce::String();
}

juce::String PracticeSetupPanel::getSessionsText() const
{
    return sessionItems.empty() ? juce::String (PracticeEmptyStates::noSessions) : juce::String();
}

juce::String PracticeSetupPanel::getRecentTabsText() const
{
    return library.getRecentTabs().isEmpty() ? tabList->emptyText() : juce::String();
}

bool PracticeSetupPanel::loadLoop (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) loopItems.size()) || context.targets.looper == nullptr)
        return false;

    auto& looper = *context.targets.looper;
    const auto& item = loopItems[(size_t) row];

    /*  11.2 lists "play" for a saved loop; 11.3 and its 12.1 test keep every
        transport in the drawer. Loading is the setup half: the loop goes into
        the looper, stopped, and the drawer opens on LOOP where Play is. A
        looper that is running is left alone - replacing its layers under the
        audio thread, or stopping it from here, would both be this tab
        reaching into the transport. */
    if (looper.getState() != Looper::State::stopped)
    {
        libraryStatus = "The looper is running. Stop it in the drawer, then load.";
        repaint (libraryStatusBounds);
        return false;
    }

    const bool ok = looper.load (item.file);

    libraryStatus = ok ? "\"" + item.name + "\" is in the looper. Play it from the drawer's LOOP tab."
                       : "Could not load \"" + item.name + "\".";

    if (ok && context.showInDrawer != nullptr)
        context.showInDrawer (PracticeTool::looper);

    repaint (libraryStatusBounds);
    return ok;
}

void PracticeSetupPanel::askToDeleteLoop()
{
    const int row = loopList->getSelectedRow();

    if (! juce::isPositiveAndBelow (row, (int) loopItems.size()))
        return;

    juce::NativeMessageBox::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Delete this loop?")
            .withMessage ("\"" + loopItems[(size_t) row].name + "\" and its audio will be deleted from disk. "
                          "This cannot be undone.")
            .withButton ("Delete")
            .withButton ("Cancel")
            .withAssociatedComponent (this),
        [safe = juce::Component::SafePointer<PracticeSetupPanel> (this), row] (int result)
        {
            if (safe != nullptr && result == 0)   // plain index: 0 confirms, 1 is Cancel
                safe->deleteLoopConfirmed (row);
        });
}

bool PracticeSetupPanel::deleteLoopConfirmed (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) loopItems.size()))
        return false;

    const auto item = loopItems[(size_t) row];
    const bool ok = PracticeLibrary::deleteLoop (item);

    libraryStatus = ok ? "Deleted \"" + item.name + "\"." : "Could not delete \"" + item.name + "\".";
    refreshLibrary();
    repaint (libraryStatusBounds);
    return ok;
}

void PracticeSetupPanel::chooseBackingFolder()
{
    const juce::File current (defaults.backingFolder);
    const auto start = juce::File::isAbsolutePath (defaults.backingFolder) && current.isDirectory()
                         ? current : juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Choose the backing-tracks folder", start);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<PracticeSetupPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();

        if (safe != nullptr && folder != juce::File())
            safe->setBackingFolder (folder);
    });
}

void PracticeSetupPanel::setBackingFolder (const juce::File& folder)
{
    // One setting, two bullets of 11.2: the Defaults' "default folder" and the
    // Library's "Backing tracks folder picker" are PracticeDefaults::backingFolder.
    defaults.backingFolder = folder.getFullPathName();
    saveDefaults();
    updateLibraryButtons();
    repaint (backingPathBounds);
}

void PracticeSetupPanel::openFolder (const juce::File& folder)
{
    folder.createDirectory();
    folder.revealToUser();
}

//==============================================================================
// Session recorder setup (11.2): the settings, not the transport.
//==============================================================================
double PracticeSetupPanel::currentSampleRate() const
{
    const double sr = context.sampleRate != nullptr ? context.sampleRate() : 0.0;
    return sr > 0.0 ? sr : 48000.0;
}

void PracticeSetupPanel::showSessionSetup()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    ringMinutes.setValue (sessionSetup.ringMinutes, juce::dontSendNotification);
    recordWhatBox.setSelectedId (! sessionSetup.recordMidi ? 2 : (! sessionSetup.recordAudio ? 3 : 1),
                                 juce::dontSendNotification);
    autoSaveToggle->getButton().setToggleState (sessionSetup.autoSaveOnStop, juce::dontSendNotification);
}

void PracticeSetupPanel::writeSessionSetup (bool applyToRecorder)
{
    if (updating)
        return;

    sessionSetup.ringMinutes = ringMinutes.getValue();
    sessionSetup.recordAudio = recordWhatBox.getSelectedId() != 3;
    sessionSetup.recordMidi = recordWhatBox.getSelectedId() != 2;
    sessionSetup.autoSaveOnStop = autoSaveToggle->getButton().getToggleState();

    // Kept in defaults.json beside the other starting settings; PracticeDefaults
    // carries keys it does not know, so this survives its own saves.
    defaults.extra.set (kSessionSetupKey, sessionSetup.toVar());
    saveDefaults();

    /*  The ring is only sized when the recorder is on: off by default, it takes
        no memory (performance-budget 3), and switching it on in the drawer is
        what allocates it. Only a changed length reallocates, because
        reallocating drops what the ring holds. */
    sessionStatus = {};

    if (auto* recorder = context.targets.sessionRecorder)
    {
        if (applyToRecorder && recorder->isEnabled()
            && std::abs (recorder->getCapacityMinutes() - sessionSetup.ringMinutes) > 0.01)
        {
            if (! sessionSetup.applyTo (*recorder, currentSampleRate()))
                sessionStatus = "Only " + juce::String (recorder->getCapacityMinutes(), 1)
                              + " minutes could be allocated.";
        }
    }

    repaint (warningBounds.getUnion (sessionStatusBounds));
}

//==============================================================================
// Layout. Each section asks for its own height, and resized() lays each one out
// inside exactly that much, so a section's height and its layout cannot drift
// into the next one.
//==============================================================================
int PracticeSetupPanel::progressHeight() const
{
    if (statsEmpty)
        return kHeader + 40 + kRow + kLine + Metrics::grid;

    const int shown = juce::jmin (kMaxPhrasesShown, (int) phrases.size());
    const int phraseLines = juce::jmax (1, shown) + ((int) phrases.size() > kMaxPhrasesShown ? 1 : 0);

    return kHeader + 20
         + kLine + kChartHeight + kRowGap
         + 4 * kLine + kRowGap
         + kRow + kSparkHeight + kRowGap
         + kLine + phraseLines * kLine + kRowGap
         + kRow + kLine + Metrics::grid;
}

int PracticeSetupPanel::routinesHeight() const
{
    return kHeader
         + kRoutineRows * kListRow + kRowGap + kLine
         + kRow + kRow + 2 * kLine
         + kLine + kEntryRows * kListRow + kRowGap + kRow
         + 5 * kRow + kNumEntryOptions * kRow + kRow
         + Metrics::grid;
}

int PracticeSetupPanel::defaultsHeight() const
{
    return kHeader
         + kLine + 4 * kRow
         + kLine + 2 * kRow
         + kLine + 7 * kRow
         + kLine + kRow
         + kLine + Metrics::grid;
}

int PracticeSetupPanel::libraryHeight() const
{
    return kHeader
         + kLine + kLoopRows * kListRow + kRowGap + kRow
         + kLine + kSessionRows * kListRow + kRowGap + kRow
         + kLine + kLine + kRow
         + kLine + kTabRows * kListRow + kRowGap + kRow
         + kLine + Metrics::grid;
}

int PracticeSetupPanel::sessionHeight() const
{
    return kHeader + kRow + kWarningHeight + kRow + kRow + kLine + Metrics::grid;
}

int PracticeSetupPanel::getPreferredHeight() const
{
    return progressHeight() + routinesHeight() + defaultsHeight() + libraryHeight() + sessionHeight();
}

void PracticeSetupPanel::fitHeight()
{
    const int wanted = getPreferredHeight();

    if (getHeight() != wanted)
        setSize (juce::jmax (1, getWidth()), wanted);
}

void PracticeSetupPanel::addCaption (juce::Rectangle<int> area, const juce::String& text)
{
    captions.emplace_back (area, text);
}

void PracticeSetupPanel::resized()
{
    captions.clear();

    auto bounds = getLocalBounds().reduced (Metrics::grid, 0);

    auto row = [] (juce::Rectangle<int>& area)
    {
        auto r = area.removeFromTop (Metrics::buttonHeight);
        area.removeFromTop (kRowGap);
        return r;
    };

    auto split = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> parts)
    {
        const int w = juce::jmax (1, r.getWidth() / (int) parts.size());

        for (auto* c : parts)
            c->setBounds (r.removeFromLeft (w).reduced (1));
    };

    auto labelled = [this] (juce::Rectangle<int> r, const juce::String& caption)
    {
        addCaption (r.removeFromLeft (kCaptionWidth), caption);
        return r.reduced (1);
    };

    auto list = [] (juce::Rectangle<int>& area, juce::Component& c, int rows)
    {
        c.setBounds (area.removeFromTop (rows * kListRow));
        area.removeFromTop (kRowGap);
    };

    // --- progress -------------------------------------------------------------------
    {
        auto area = bounds.removeFromTop (progressHeight());
        progressHeader = area.removeFromTop (kHeader);

        if (statsEmpty)
        {
            summaryBounds = area.removeFromTop (40);
            chartBounds = breakdownBounds = sparkBounds = tempoBounds = {};
        }
        else
        {
            summaryBounds = area.removeFromTop (20);

            addCaption (area.removeFromTop (kLine), "Minutes per day, last 90 days");
            chartBounds = area.removeFromTop (kChartHeight);
            area.removeFromTop (kRowGap);

            breakdownBounds = area.removeFromTop (4 * kLine);
            area.removeFromTop (kRowGap);

            accuracyBox.setBounds (labelled (row (area), "Accuracy"));
            sparkBounds = area.removeFromTop (kSparkHeight);
            area.removeFromTop (kRowGap);

            const int shown = juce::jmin (kMaxPhrasesShown, (int) phrases.size());
            const int phraseLines = juce::jmax (1, shown) + ((int) phrases.size() > kMaxPhrasesShown ? 1 : 0);
            addCaption (area.removeFromTop (kLine), "Best clean tempo, per phrase");
            tempoBounds = area.removeFromTop (phraseLines * kLine);
            area.removeFromTop (kRowGap);
        }

        split (row (area), { &exportCsvButton, &clearHistoryButton });
        progressStatusBounds = area.removeFromTop (kLine);
    }

    // --- routines -------------------------------------------------------------------
    {
        auto area = bounds.removeFromTop (routinesHeight());
        routinesHeader = area.removeFromTop (kHeader);

        list (area, *routineList, kRoutineRows);
        userHintBounds = area.removeFromTop (kLine);

        // START's text is the longest; it gets the room it needs.
        {
            auto r = row (area);
            startButton.setBounds (r.removeFromLeft (r.getWidth() * 2 / 5).reduced (1));
            split (r, { &newRoutineButton, &duplicateButton, &deleteRoutineButton });
        }

        nameEditor.setBounds (labelled (row (area), "Name"));
        routineStatusBounds = area.removeFromTop (2 * kLine);

        addCaption (area.removeFromTop (kLine), "Entries");
        list (area, *entryList, kEntryRows);
        split (row (area), { &addEntryButton, &removeEntryButton, &moveUpButton, &moveDownButton });

        entryToolBox.setBounds (labelled (row (area), "Tool"));
        entryTitle.setBounds (labelled (row (area), "Title"));

        {
            auto r = labelled (row (area), "Length");
            entryModeBox.setBounds (r.removeFromLeft (r.getWidth() * 2 / 5).reduced (1, 0));
            entryDuration.setBounds (r);
            entryRepetitions.setBounds (r);
        }

        entryTempo.setBounds (labelled (row (area), "Tempo"));

        {
            auto r = labelled (row (area), "Count-in");
            entryCountInBox.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (1, 0));
            entryClick->setBounds (r.reduced (1, 0));
        }

        for (int i = 0; i < kNumEntryOptions; ++i)
        {
            auto r = row (area);

            if (entryOptions[i].isVisible())
                entryOptions[i].setBounds (labelled (r, entryOptionCaptions[i]));
        }

        {
            auto r = row (area);

            if (entryText.isVisible())
                entryText.setBounds (labelled (r, entryTextCaption));
        }
    }

    // --- defaults -------------------------------------------------------------------
    {
        auto area = bounds.removeFromTop (defaultsHeight());
        defaultsHeader = area.removeFromTop (kHeader);

        addCaption (area.removeFromTop (kLine), "METRONOME");
        defaultTempo.setBounds (labelled (row (area), "Tempo"));

        {
            auto r = labelled (row (area), "Bar");
            timeSigBox.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (1, 0));
            subdivisionBox.setBounds (r.reduced (1, 0));
        }

        soundBox.setBounds (labelled (row (area), "Click"));

        {
            auto r = labelled (row (area), "Accents");
            const int w = juce::jmin (28, r.getWidth() / juce::jmax (1, accentButtons.size()));

            for (auto* button : accentButtons)
                button->setBounds (r.removeFromLeft (w).reduced (1));
        }

        addCaption (area.removeFromTop (kLine), "LOOPER");
        loopLength.setBounds (labelled (row (area), "Length"));

        {
            auto r = labelled (row (area), "Count-in");
            loopCountInBox.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (1, 0));
            overdubBox.setBounds (r.reduced (1, 0));
        }

        addCaption (area.removeFromTop (kLine), "TRAINERS");
        trainerKeyBox.setBounds (labelled (row (area), "Key"));

        for (int first = 0; first < scaleToggles.size(); first += 4)
        {
            auto r = row (area);
            addCaption (r.removeFromLeft (kCaptionWidth), first == 0 ? "Scale set" : juce::String());
            const int w = juce::jmax (1, r.getWidth() / 4);

            for (int s = first; s < juce::jmin (first + 4, scaleToggles.size()); ++s)
                scaleToggles[s]->setBounds (r.removeFromLeft (w).reduced (1));
        }

        rangeLow.setBounds (labelled (row (area), "Lowest note"));
        rangeHigh.setBounds (labelled (row (area), "Highest note"));
        questionCount.setBounds (labelled (row (area), "Questions"));

        addCaption (area.removeFromTop (kLine), "BACKING TRACK");

        {
            auto r = labelled (row (area), "Level");
            shuffleToggle->setBounds (r.removeFromRight (juce::jmin (96, r.getWidth() / 3)).reduced (1, 0));
            backingLevel.setBounds (r);
        }

        defaultsStatusBounds = area.removeFromTop (kLine);
    }

    // --- library --------------------------------------------------------------------
    {
        auto area = bounds.removeFromTop (libraryHeight());
        libraryHeader = area.removeFromTop (kHeader);

        addCaption (area.removeFromTop (kLine), "SAVED LOOPS");
        list (area, *loopList, kLoopRows);
        split (row (area), { &loadLoopButton, &deleteLoopButton, &openLoopsButton });

        addCaption (area.removeFromTop (kLine), "SAVED SESSIONS");
        list (area, *sessionList, kSessionRows);
        split (row (area), { &revealSessionButton, &openSessionsButton });

        addCaption (area.removeFromTop (kLine), "BACKING TRACKS FOLDER");
        backingPathBounds = area.removeFromTop (kLine);
        split (row (area), { &chooseBackingButton, &openBackingButton });

        addCaption (area.removeFromTop (kLine), "RECENT TABS");
        list (area, *tabList, kTabRows);
        split (row (area), { &openTabFolderButton });

        libraryStatusBounds = area.removeFromTop (kLine);
    }

    // --- session recorder -----------------------------------------------------------
    {
        auto area = bounds.removeFromTop (sessionHeight());
        sessionHeader = area.removeFromTop (kHeader);

        // 11.2: the size warning sits next to the ring control, in plain words.
        ringMinutes.setBounds (labelled (row (area), "Ring length"));
        warningBounds = area.removeFromTop (kWarningHeight).withTrimmedLeft (kCaptionWidth);
        recordWhatBox.setBounds (labelled (row (area), "Record"));
        autoSaveToggle->setBounds (labelled (row (area), "When it stops"));
        sessionStatusBounds = area.removeFromTop (kLine);
    }
}

//==============================================================================
void PracticeSetupPanel::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawSectionHeader (g, progressHeader, "PROGRESS");
    LuthierLookAndFeel::drawSectionHeader (g, routinesHeader, "ROUTINES");
    LuthierLookAndFeel::drawSectionHeader (g, defaultsHeader, "DEFAULTS");
    LuthierLookAndFeel::drawSectionHeader (g, libraryHeader, "LIBRARY");
    LuthierLookAndFeel::drawSectionHeader (g, sessionHeader, "SESSION RECORDER");

    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);

    for (const auto& caption : captions)
        g.drawFittedText (caption.second, caption.first, juce::Justification::centredLeft, 1);

    // --- progress -------------------------------------------------------------------
    g.setFont (Fonts::ui (statsEmpty ? 11.0f : 12.0f, ! statsEmpty));
    g.setColour (statsEmpty ? Palette::textMuted : Palette::textPrimary);
    g.drawFittedText (progressSummary, summaryBounds, juce::Justification::centredLeft, statsEmpty ? 3 : 1);

    if (! statsEmpty)
    {
        // Minutes per day, oldest on the left; today's bar is the bright one.
        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (chartBounds.toFloat(), Metrics::controlCorner);

        const auto plot = chartBounds.reduced (2).withTrimmedTop (12).toFloat();
        const double most = juce::jmax (1.0, dayMinutes.empty() ? 1.0 : *std::max_element (dayMinutes.begin(), dayMinutes.end()));
        const float barWidth = plot.getWidth() / (float) juce::jmax ((size_t) 1, dayMinutes.size());

        for (size_t d = 0; d < dayMinutes.size(); ++d)
        {
            if (dayMinutes[d] <= 0.0)
                continue;

            const float h = juce::jmax (1.0f, (float) (dayMinutes[d] / most) * plot.getHeight());
            g.setColour (d + 1 == dayMinutes.size() ? Palette::accentBright : Palette::accent);
            g.fillRect (plot.getX() + barWidth * (float) d, plot.getBottom() - h, juce::jmax (1.0f, barWidth - 1.0f), h);
        }

        g.setColour (Palette::edge);
        g.fillRect (plot.getX(), plot.getBottom(), plot.getWidth(), 1.0f);

        g.setFont (Fonts::ui (10.0f));
        g.setColour (Palette::textMuted);
        g.drawText ("most " + describeMinutes (most * 60.0), chartBounds.reduced (4, 1).removeFromTop (12),
                    juce::Justification::centredRight);

        // The per-tool breakdown: minutes as bars against the busiest tool.
        const PracticeTool timed[] = { PracticeTool::metronome, PracticeTool::looper, PracticeTool::backingTrack };
        double busiest = 1.0;

        for (auto t : timed)
            busiest = juce::jmax (busiest, toolSeconds[(size_t) t]);

        auto rows = breakdownBounds;

        for (auto t : timed)
        {
            auto r = rows.removeFromTop (kLine);
            g.setColour (Palette::textMuted);
            g.drawFittedText (kToolNames[(int) t], r.removeFromLeft (kCaptionWidth), juce::Justification::centredLeft, 1);

            const auto value = describeMinutes (toolSeconds[(size_t) t]);
            g.setColour (Palette::textPrimary);
            g.drawFittedText (value, r.removeFromRight (64), juce::Justification::centredRight, 1);

            const auto bar = r.reduced (0, 4).toFloat();
            g.setColour (Palette::panelSunken);
            g.fillRect (bar);
            g.setColour (Palette::secondary);
            g.fillRect (bar.withWidth (bar.getWidth() * (float) (toolSeconds[(size_t) t] / busiest)));
        }

        {
            auto r = rows.removeFromTop (kLine);
            const int scale = toolSessions[(size_t) PracticeTool::scaleTrainer];
            const int ear = toolSessions[(size_t) PracticeTool::earTrainer];

            g.setColour (Palette::textMuted);
            g.drawFittedText ("Trainers", r.removeFromLeft (kCaptionWidth), juce::Justification::centredLeft, 1);
            g.setColour (Palette::textPrimary);
            g.drawFittedText (juce::String (scale) + " scale, " + juce::String (ear) + " ear sessions",
                              r, juce::Justification::centredLeft, 1);
        }

        // Accuracy over time for the chosen exercise, one point per day.
        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (sparkBounds.toFloat(), Metrics::controlCorner);

        if (accuracy.empty())
        {
            g.setFont (Fonts::ui (11.0f));
            g.setColour (Palette::textDisabled);
            g.drawFittedText ("No trainer scores yet. The scale and ear trainers record theirs here.",
                              sparkBounds.reduced (Metrics::grid, 2), juce::Justification::centred, 2);
        }
        else
        {
            auto plotArea = sparkBounds.reduced (4);
            const auto label = plotArea.removeFromRight (44);
            const auto p = plotArea.toFloat();

            g.setColour (Palette::edge);
            g.fillRect (p.getX(), p.getCentreY(), p.getWidth(), 1.0f);

            juce::Path line;
            const float step = accuracy.size() > 1 ? p.getWidth() / (float) (accuracy.size() - 1) : 0.0f;

            for (size_t i = 0; i < accuracy.size(); ++i)
            {
                const juce::Point<float> at (accuracy.size() > 1 ? p.getX() + step * (float) i : p.getCentreX(),
                                             p.getBottom() - (float) juce::jlimit (0.0, 1.0, accuracy[i].second) * p.getHeight());

                if (i == 0)
                    line.startNewSubPath (at);
                else
                    line.lineTo (at);

                g.setColour (Palette::secondary);
                g.fillEllipse (at.x - 2.0f, at.y - 2.0f, 4.0f, 4.0f);
            }

            g.setColour (Palette::secondary);
            g.strokePath (line, juce::PathStrokeType (1.5f));

            g.setFont (Fonts::mono (11.0f));
            g.setColour (Palette::textPrimary);
            g.drawFittedText (juce::String (juce::roundToInt (accuracy.back().second * 100.0)) + "%",
                              label, juce::Justification::centredRight, 1);
        }

        // Best clean tempo per phrase: the best, and where it started.
        g.setFont (Fonts::ui (11.0f));
        auto lines = tempoBounds;

        if (phrases.empty())
        {
            g.setColour (Palette::textDisabled);
            g.drawFittedText ("No phrases yet. The speed trainer records the fastest clean pass.",
                              lines.removeFromTop (kLine), juce::Justification::centredLeft, 1);
        }

        for (int i = 0; i < juce::jmin (kMaxPhrasesShown, (int) phrases.size()); ++i)
        {
            const auto& phrase = phrases[(size_t) i];
            auto r = lines.removeFromTop (kLine);

            auto value = juce::String (juce::roundToInt (phrase.bestBpm)) + " bpm";

            if (phrase.history.size() > 1)
                value << "  (from " << juce::roundToInt (phrase.history.front().second) << ")";

            g.setColour (Palette::textPrimary);
            g.drawFittedText (value, r.removeFromRight (juce::jmin (140, r.getWidth() / 2)),
                              juce::Justification::centredRight, 1);
            g.setColour (Palette::textMuted);
            g.drawFittedText (phrase.phrase, r, juce::Justification::centredLeft, 1);
        }

        if ((int) phrases.size() > kMaxPhrasesShown)
        {
            g.setColour (Palette::textDisabled);
            g.drawFittedText ("and " + juce::String ((int) phrases.size() - kMaxPhrasesShown)
                                + " more in the CSV export",
                              lines.removeFromTop (kLine), juce::Justification::centredLeft, 1);
        }
    }

    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);
    g.drawFittedText (progressStatus, progressStatusBounds, juce::Justification::centredLeft, 1);

    // --- routines -------------------------------------------------------------------
    g.setColour (Palette::textDisabled);
    g.drawFittedText (getUserRoutinesText(), userHintBounds, juce::Justification::centredLeft, 1);

    {
        auto r = routineStatusBounds;
        g.setColour (Palette::textMuted);
        g.drawFittedText (runnerStatus, r.removeFromTop (kLine), juce::Justification::centredLeft, 1);

        const auto message = routineStatus.isNotEmpty() ? routineStatus
                           : (canEditSelected() ? juce::String()
                                                : juce::String ("A factory routine: duplicate it to change it."));
        g.setColour (routineStatus.isNotEmpty() ? Palette::warning : Palette::textDisabled);
        g.drawFittedText (message, r, juce::Justification::centredLeft, 1);
    }

    // --- defaults -------------------------------------------------------------------
    g.setColour (Palette::warning);
    g.drawFittedText (defaultsStatus, defaultsStatusBounds, juce::Justification::centredLeft, 1);

    // --- library --------------------------------------------------------------------
    g.setColour (defaults.backingFolder.isEmpty() ? Palette::textDisabled : Palette::textPrimary);
    g.drawFittedText (defaults.backingFolder.isEmpty() ? juce::String ("No folder chosen. The TRACK tab's file chooser opens here.")
                                                       : defaults.backingFolder,
                      backingPathBounds, juce::Justification::centredLeft, 1);

    g.setColour (Palette::textMuted);
    g.drawFittedText (libraryStatus, libraryStatusBounds, juce::Justification::centredLeft, 1);

    // --- session recorder -----------------------------------------------------------
    g.setColour (Palette::warning);
    g.drawFittedText (getSizeWarning(), warningBounds.reduced (1, 2), juce::Justification::topLeft, 3);

    g.setColour (Palette::textMuted);
    g.drawFittedText (sessionStatus, sessionStatusBounds, juce::Justification::centredLeft, 1);
}

} // namespace luthier
