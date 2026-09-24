#pragma once

/*  The PRACTICE workspace tab (gui-integration.md 4.4 and 10; practice-tools.md 11).

    The setup surface. The drawer (PracticePanel, practice-tools 9) is for
    during practice; this tab is for before and after (11.1): arranging what
    practice will be, and looking at what it was. Five sections, in 11.2's
    order:

      PROGRESS - practice time per day for the last 90 days, the per-tool
          breakdown, trainer accuracy over time per exercise, the best clean
          tempo per phrase and the streak. Read-only, with Export as CSV and a
          Clear history that confirms.

      ROUTINES - the three factory routines and the user's own; start one
          (the drawer then runs it through PracticeRoutineRunner), make, copy
          and delete them, and edit their entries: tool, settings, and a
          duration or a repetition count. Factory routines are read-only;
          duplicate one to change it.

      DEFAULTS - per-tool starting settings (PracticeDefaults). They load
          into the tools when the plugin opens; editing them here starts
          nothing and does not touch a tool that is already running.

      LIBRARY - saved loops (load into the looper, delete), saved sessions
          (reveal), the backing-tracks folder, the tab files opened recently,
          and an open-folder button for each.

      SESSION RECORDER - ring length with the plain-words size warning beside
          it, audio / MIDI / both, and auto-save on stop.

    11.3, deliberately not here: any transport. The metronome, looper, track
    and recorder are started and stopped in the drawer and nowhere else, so
    the drawer stays the single source of truth about what is running. The
    two places 11.2 comes close are resolved in the .cpp: a saved loop is
    loaded, not played, and START hands a routine to the drawer's runner.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Practice/PracticeRoutine.h"
#include "../Practice/PracticeRoutineProgress.h"
#include "../Practice/PracticeRoutineSetup.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** Where the tab's files live (practice-tools 10). The defaults are the
    user's own folders; tests point every one of them at a temp folder. */
struct PracticeSetupLocations
{
    juce::File statsFile      = PracticeStats::getStatsFile();
    juce::File routinesFolder = PracticeRoutineLibrary::getUserDirectory();
    juce::File defaultsFile   = PracticeDefaults::getDefaultsFile();
    juce::File libraryFile    = PracticeLibrary::getLibraryFile();
    juce::File loopsFolder    = Looper::getUserDirectory();
    juce::File sessionsFolder = SessionRecorder::getSessionDirectory();

    /** Every location inside `root`, laid out as under Documents/Luthier. */
    static PracticeSetupLocations inside (const juce::File& root);
};

/** What the tab works on. Built from the processor by contextFor(); built by
    hand in tests. */
struct PracticeSetupContext
{
    PracticeTargets targets;

    /** The runner the drawer advances. Null: the tab owns one, which nothing
        advances (tests, or before the processor owns one). */
    PracticeRoutineRunner* runner = nullptr;

    /** The history the activity tracker adds to. Null: the tab owns one,
        loaded from locations.statsFile on every refresh. */
    PracticeStats* stats = nullptr;

    /** For sizing the session recorder's ring. */
    std::function<double()> sampleRate;

    PracticeSetupLocations locations;

    /** Opens the drawer on a tool: after START (the routine's first entry)
        and after a loop is loaded (LOOP). */
    std::function<void (PracticeTool)> showInDrawer;
};

//==============================================================================
class PracticeSetupPanel : public juce::Component,
                           private juce::Timer
{
public:
    /** The context for the plugin: the processor's tools, and its runner and
        history once it owns them. */
    static PracticeSetupContext contextFor (LuthierAudioProcessor& processor);

    explicit PracticeSetupPanel (LuthierAudioProcessor& processor);
    explicit PracticeSetupPanel (PracticeSetupContext context);
    ~PracticeSetupPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getPreferredHeight() const;

    /** Re-reads everything from the model and the disk into the controls. */
    void refresh();

    //==========================================================================
    // For tests: the controls, by what they do, and the confirmed paths of the
    // actions that ask first.

    // --- progress -----------------------------------------------------------
    juce::String getProgressSummary() const          { return progressSummary; }
    bool isShowingNoStats() const noexcept           { return statsEmpty; }
    juce::Button& getExportCsvButton() noexcept      { return exportCsvButton; }
    juce::Button& getClearHistoryButton() noexcept   { return clearHistoryButton; }
    juce::ComboBox& getAccuracyBox() noexcept        { return accuracyBox; }

    /** What Export as CSV writes, without the file chooser. */
    bool exportCsvTo (const juce::File& destination, juce::String* error = nullptr);

    /** Clear history, after the player confirmed. Loops and sessions stay. */
    void clearHistoryConfirmed();

    PracticeStats& getStats() noexcept;

    // --- routines -----------------------------------------------------------
    juce::ListBox& getRoutineList() noexcept;
    juce::ListBox& getEntryList() noexcept;
    void selectRoutine (int index);
    int getSelectedRoutineIndex() const noexcept     { return selectedRoutine; }
    const PracticeRoutine& getSelectedRoutine() const noexcept { return editing; }
    void selectEntry (int index);
    int getSelectedEntryIndex() const noexcept       { return selectedEntry; }
    PracticeRoutineLibrary& getRoutineLibrary() noexcept { return routines; }
    PracticeRoutineRunner& getRunner() noexcept;

    juce::Button& getStartButton() noexcept          { return startButton; }
    juce::Button& getNewRoutineButton() noexcept     { return newRoutineButton; }
    juce::Button& getDuplicateButton() noexcept      { return duplicateButton; }
    juce::Button& getDeleteRoutineButton() noexcept  { return deleteRoutineButton; }
    juce::TextEditor& getNameEditor() noexcept       { return nameEditor; }

    /** Delete the selected user routine, after the player confirmed. */
    void deleteRoutineConfirmed();

    juce::Button& getAddEntryButton() noexcept       { return addEntryButton; }
    juce::Button& getRemoveEntryButton() noexcept    { return removeEntryButton; }
    juce::Button& getMoveUpButton() noexcept         { return moveUpButton; }
    juce::Button& getMoveDownButton() noexcept       { return moveDownButton; }
    juce::ComboBox& getEntryToolBox() noexcept       { return entryToolBox; }
    juce::TextEditor& getEntryTitleEditor() noexcept { return entryTitle; }
    juce::ComboBox& getEntryModeBox() noexcept       { return entryModeBox; }
    juce::Slider& getEntryDurationSlider() noexcept  { return entryDuration; }
    juce::Slider& getEntryRepetitionsSlider() noexcept { return entryRepetitions; }
    juce::Slider& getEntryTempoSlider() noexcept     { return entryTempo; }
    juce::ComboBox& getEntryCountInBox() noexcept    { return entryCountInBox; }
    juce::Button& getEntryClickToggle() noexcept     { return entryClick->getButton(); }

    /** The tool's own settings: up to three choices, then one text field. */
    static constexpr int kNumEntryOptions = 3;
    juce::ComboBox& getEntryOptionBox (int index) noexcept;
    juce::TextEditor& getEntryTextEditor() noexcept  { return entryText; }

    /** 11.4: "Your own routines appear here." while there are none. */
    juce::String getUserRoutinesText() const;
    juce::String getRoutineStatus() const            { return routineStatus; }

    /** What the drawer's runner is doing, read-only (the transport is the drawer's). */
    juce::String getRunnerStatus() const             { return runnerStatus; }

    // --- defaults -----------------------------------------------------------
    const PracticeDefaults& getDefaults() const noexcept { return defaults; }

    juce::Slider& getDefaultTempoSlider() noexcept   { return defaultTempo; }
    juce::ComboBox& getTimeSignatureBox() noexcept   { return timeSigBox; }
    juce::ComboBox& getSubdivisionBox() noexcept     { return subdivisionBox; }
    juce::ComboBox& getClickSoundBox() noexcept      { return soundBox; }
    juce::Button* getAccentButton (int beat) noexcept { return accentButtons[beat]; }
    juce::Slider& getLoopLengthSlider() noexcept     { return loopLength; }
    juce::ComboBox& getLoopCountInBox() noexcept     { return loopCountInBox; }
    juce::ComboBox& getOverdubBox() noexcept         { return overdubBox; }
    juce::ComboBox& getTrainerKeyBox() noexcept      { return trainerKeyBox; }
    juce::Button& getScaleToggle (ScaleType scale) noexcept;
    juce::Slider& getRangeLowSlider() noexcept       { return rangeLow; }
    juce::Slider& getRangeHighSlider() noexcept      { return rangeHigh; }
    juce::Slider& getQuestionsSlider() noexcept      { return questionCount; }
    juce::Button& getShuffleToggle() noexcept        { return shuffleToggle->getButton(); }
    juce::Slider& getBackingLevelSlider() noexcept   { return backingLevel; }

    // --- library ------------------------------------------------------------
    juce::ListBox& getLoopList() noexcept;
    juce::ListBox& getSessionList() noexcept;
    juce::ListBox& getRecentTabList() noexcept;
    const std::vector<PracticeLibrary::Item>& getLoopItems() const noexcept    { return loopItems; }
    const std::vector<PracticeLibrary::Item>& getSessionItems() const noexcept { return sessionItems; }

    juce::Button& getLoadLoopButton() noexcept       { return loadLoopButton; }
    juce::Button& getDeleteLoopButton() noexcept     { return deleteLoopButton; }

    /** Loads a saved loop into the looper, stopped, and shows the drawer's
        LOOP tab, where it is played. */
    bool loadLoop (int row);

    /** Delete a saved loop, after the player confirmed. */
    bool deleteLoopConfirmed (int row);

    /** The backing-tracks folder (the chooser's result; PracticeDefaults). */
    void setBackingFolder (const juce::File& folder);

    /** What each list shows when it is empty, or nothing when it is not. */
    juce::String getLoopsText() const;
    juce::String getSessionsText() const;
    juce::String getRecentTabsText() const;
    juce::String getLibraryStatus() const            { return libraryStatus; }

    // --- session recorder ---------------------------------------------------
    juce::Slider& getRingMinutesSlider() noexcept    { return ringMinutes; }
    juce::ComboBox& getRecordWhatBox() noexcept      { return recordWhatBox; }
    juce::Button& getAutoSaveToggle() noexcept       { return autoSaveToggle->getButton(); }
    const SessionRecorderSetup& getSessionSetup() const noexcept { return sessionSetup; }
    juce::String getSizeWarning() const              { return sessionSetup.getSizeWarning (currentSampleRate()); }

    /** The key in defaults.json that carries the session setup. */
    static constexpr const char* kSessionSetupKey = "session_recorder";

private:
    class ItemList;

    void timerCallback() override;
    void build();

    // --- progress
    void refreshProgress();
    void exportCsvWithChooser();
    void askToClearHistory();

    // --- routines
    void refreshRoutineList();
    void refreshRoutineEditor();
    void refreshEntryEditor();
    void refreshRunnerStatus();
    void saveEditing();
    void startSelected();
    void newRoutine();
    void duplicateRoutine();
    void askToDeleteRoutine();
    void renameRoutine();
    juce::String uniqueRoutineName (const juce::String& base) const;
    bool canEditSelected() const noexcept;
    RoutineEntry* currentEntry() noexcept;
    void writeEntrySetting (const juce::String& key, const juce::var& value);
    void writeEntryFromControls (bool save);
    void writeEntryOption (int index);
    void setEntryTool (PracticeTool tool);

    // --- defaults
    void loadDefaults();
    void showDefaults();
    void writeDefaults (bool save);
    void saveDefaults();
    void rebuildAccentButtons();
    void writeScaleSet (ScaleType scale, bool included);

    // --- library
    void refreshLibrary();
    void updateLibraryButtons();
    void askToDeleteLoop();
    void chooseBackingFolder();
    static void openFolder (const juce::File& folder);

    // --- session recorder
    void showSessionSetup();
    void writeSessionSetup (bool applyToRecorder);
    double currentSampleRate() const;

    // --- layout
    int progressHeight() const;
    int routinesHeight() const;
    int defaultsHeight() const;
    int libraryHeight() const;
    int sessionHeight() const;
    void fitHeight();

    void addCaption (juce::Rectangle<int> area, const juce::String& text);

    PracticeSetupContext context;
    std::unique_ptr<PracticeRoutineRunner> ownRunner;
    std::unique_ptr<PracticeStats> ownStats;
    bool updating = false;
    int ticks = 0;

    // --- progress -------------------------------------------------------------------
    bool statsEmpty = true;
    juce::String progressSummary, progressStatus;
    std::vector<double> dayMinutes;                                          // 90, oldest first
    std::array<double, (size_t) PracticeTool::numTools> toolSeconds {};
    std::array<int, (size_t) PracticeTool::numTools> toolSessions {};
    std::vector<std::pair<juce::String, double>> accuracy;                   // for the chosen exercise
    std::vector<PhraseTempoRecord> phrases;
    juce::StringArray exercises;

    juce::ComboBox accuracyBox;
    juce::TextButton exportCsvButton { "EXPORT CSV..." }, clearHistoryButton { "CLEAR HISTORY..." };

    // --- routines -------------------------------------------------------------------
    PracticeRoutineLibrary routines;
    PracticeRoutine editing;
    int selectedRoutine = 0;
    int selectedEntry = -1;
    juce::String routineStatus, runnerStatus;

    std::unique_ptr<ItemList> routineList, entryList;
    juce::TextButton startButton { "START IN DRAWER" }, newRoutineButton { "NEW" },
                     duplicateButton { "DUPLICATE" }, deleteRoutineButton { "DELETE..." };
    juce::TextEditor nameEditor;

    juce::TextButton addEntryButton { "ADD" }, removeEntryButton { "REMOVE" },
                     moveUpButton { "UP" }, moveDownButton { "DOWN" };
    juce::ComboBox entryToolBox, entryModeBox, entryCountInBox;
    juce::TextEditor entryTitle, entryText;
    juce::Slider entryDuration { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider entryRepetitions { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider entryTempo { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<LuthierToggle> entryClick;
    juce::ComboBox entryOptions[kNumEntryOptions];
    juce::String entryOptionKeys[kNumEntryOptions];
    juce::String entryOptionCaptions[kNumEntryOptions];
    juce::String entryTextKey, entryTextCaption;

    // --- defaults -------------------------------------------------------------------
    PracticeDefaults defaults;
    juce::String defaultsStatus;
    juce::Slider defaultTempo { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox timeSigBox, subdivisionBox, soundBox;
    juce::OwnedArray<juce::TextButton> accentButtons;
    juce::Slider loopLength { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox loopCountInBox, overdubBox, trainerKeyBox;
    juce::OwnedArray<LuthierToggle> scaleToggles;
    juce::Slider rangeLow { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider rangeHigh { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider questionCount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<LuthierToggle> shuffleToggle;
    juce::Slider backingLevel { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // --- library --------------------------------------------------------------------
    PracticeLibrary library;
    std::vector<PracticeLibrary::Item> loopItems, sessionItems;
    juce::String libraryStatus;

    std::unique_ptr<ItemList> loopList, sessionList, tabList;
    juce::TextButton loadLoopButton { "LOAD" }, deleteLoopButton { "DELETE..." }, openLoopsButton { "OPEN FOLDER" };
    juce::TextButton revealSessionButton { "REVEAL" }, openSessionsButton { "OPEN FOLDER" };
    juce::TextButton chooseBackingButton { "CHOOSE..." }, openBackingButton { "OPEN FOLDER" };
    juce::TextButton openTabFolderButton { "OPEN FOLDER" };

    // --- session recorder -----------------------------------------------------------
    SessionRecorderSetup sessionSetup;
    juce::String sessionStatus;
    juce::Slider ringMinutes { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox recordWhatBox;
    std::unique_ptr<LuthierToggle> autoSaveToggle;

    // --- painted parts, laid out in resized() ------------------------------------------
    juce::Rectangle<int> progressHeader, routinesHeader, defaultsHeader, libraryHeader, sessionHeader;
    juce::Rectangle<int> summaryBounds, chartBounds, breakdownBounds, sparkBounds, tempoBounds, progressStatusBounds;
    juce::Rectangle<int> userHintBounds, routineStatusBounds, backingPathBounds, libraryStatusBounds, warningBounds;
    juce::Rectangle<int> defaultsStatusBounds, sessionStatusBounds;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeSetupPanel)
};

} // namespace luthier
