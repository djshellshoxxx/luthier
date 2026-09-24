/*  The PRACTICE workspace tab (gui-integration.md 4.4; practice-tools.md 11 and
    its 12.1 setup-surface tests).

    The tab runs against the real practice tools, built standalone as the
    processor owns them, and every file it reads or writes goes to a temp
    folder through PracticeSetupLocations::inside - never to the user's
    Documents/Luthier. Controls are driven by calling onClick / onChange /
    onValueChange directly, as a click would; the actions that ask first are
    driven through their confirmed paths.
*/

#include "TestFramework.h"

#include "../UI/PracticeSetupPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** One of each tool, the runner and history the processor will own, and a
        temp folder standing in for Documents/Luthier. */
    struct Rig
    {
        Metronome metronome;
        Looper looper;
        BackingTrackPlayer backingTrack;
        SessionRecorder recorder;
        ScaleTrainer scaleTrainer;
        EarTrainer earTrainer;
        ProgressionLooper progression;

        PracticeRoutineRunner runner;
        PracticeStats stats;

        juce::File root;

        PracticeTool drawerTool = PracticeTool::numTools;
        int drawerRequests = 0;

        explicit Rig (const juce::String& name)
            : root (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierPracticeSetup" + name))
        {
            root.deleteRecursively();
            root.createDirectory();

            metronome.prepare (48000.0, 512);
            looper.prepare (48000.0, 2.0);   // two seconds: enough to load a test loop, cheap on memory
            runner.setTargets (targets());
        }

        ~Rig()
        {
            root.deleteRecursively();
        }

        PracticeTargets targets()
        {
            PracticeTargets t;
            t.metronome = &metronome;
            t.looper = &looper;
            t.backingTrack = &backingTrack;
            t.scaleTrainer = &scaleTrainer;
            t.earTrainer = &earTrainer;
            t.progression = &progression;
            t.sessionRecorder = &recorder;
            return t;
        }

        PracticeSetupLocations locations() const { return PracticeSetupLocations::inside (root); }

        /** As the processor will build it: shared runner and history. With
            `shareStats` false the tab keeps its own history, read from stats.json. */
        PracticeSetupContext context (bool shareStats = true)
        {
            PracticeSetupContext c;
            c.targets = targets();
            c.runner = &runner;
            c.stats = shareStats ? &stats : nullptr;
            c.sampleRate = [] { return 48000.0; };
            c.locations = locations();
            c.showInDrawer = [this] (PracticeTool tool) { drawerTool = tool; ++drawerRequests; };
            return c;
        }

        /** A folder Looper::save would have written. */
        juce::File makeLoop (const juce::String& name)
        {
            auto folder = locations().loopsFolder.getChildFile (name);
            folder.createDirectory();
            folder.getChildFile ("loop.json").replaceWithText ("{ \"loopLength\": 100, \"layers\": [] }");
            return folder;
        }

        juce::File makeSession (const juce::String& name)
        {
            auto file = locations().sessionsFolder.getChildFile (name + ".wav");
            file.getParentDirectory().createDirectory();
            file.replaceWithText ("RIFF");
            return file;
        }

        bool anyTransportRunning() const
        {
            return metronome.isEnabled() || looper.getState() != Looper::State::stopped
                || backingTrack.isPlaying() || recorder.isEnabled() || runner.isActive();
        }
    };

    /** What a click does: flips a toggle and runs the same callback. Refuses a
        disabled control. */
    bool press (juce::Button& button)
    {
        if (! button.isEnabled() || button.onClick == nullptr)
            return false;

        if (button.getClickingTogglesState())
            button.setToggleState (button.getRadioGroupId() != 0 || ! button.getToggleState(),
                                   juce::dontSendNotification);

        button.onClick();
        return true;
    }

    void choose (juce::ComboBox& box, int id)
    {
        box.setSelectedId (id, juce::dontSendNotification);

        if (box.onChange != nullptr)
            box.onChange();
    }

    bool chooseText (juce::ComboBox& box, const juce::String& text)
    {
        for (int i = 0; i < box.getNumItems(); ++i)
        {
            if (box.getItemText (i) == text)
            {
                choose (box, box.getItemId (i));
                return true;
            }
        }

        return false;
    }

    void drag (juce::Slider& slider, double value)
    {
        slider.setValue (value, juce::dontSendNotification);

        if (slider.onValueChange != nullptr)
            slider.onValueChange();
    }

    void type (juce::TextEditor& editor, const juce::String& text)
    {
        editor.setText (text, false);

        if (editor.onReturnKey != nullptr)
            editor.onReturnKey();
    }

    void findButtons (juce::Component& root, juce::Array<juce::Button*>& found)
    {
        if (auto* b = dynamic_cast<juce::Button*> (&root))
            found.add (b);

        for (auto* child : root.getChildren())
            findButtons (*child, found);
    }

    /** A little of everything, for the progress tests and the picture. */
    void practiseAWeek (PracticeStats& stats)
    {
        const auto today = PracticeStats::today();

        for (int d = 0; d < 6; ++d)
        {
            const auto date = PracticeStats::daysBefore (today, d);
            stats.addSeconds (date, PracticeTool::metronome, 600.0 + 120.0 * d);
            stats.addSeconds (date, PracticeTool::looper, 300.0);
        }

        stats.addSeconds (PracticeStats::daysBefore (today, 20), PracticeTool::backingTrack, 900.0);
        stats.addTrainerSession (PracticeStats::daysBefore (today, 2), PracticeTool::earTrainer, "ear.interval", 10, 6);
        stats.addTrainerSession (PracticeStats::daysBefore (today, 1), PracticeTool::earTrainer, "ear.interval", 10, 8);
        stats.addTrainerSession (today, PracticeTool::earTrainer, "ear.interval", 10, 9);
        stats.addTrainerSession (today, PracticeTool::scaleTrainer, "scale.dorian.quiz", 12, 10);
        stats.recordCleanTempo (PracticeStats::daysBefore (today, 3), "Solo intro", 92.0);
        stats.recordCleanTempo (today, "Solo intro", 104.0);
        stats.recordCleanTempo (today, "Chorus riff", 126.0);
    }
}

//==============================================================================
// Progress (11.2, 11.4, 12.1)
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, progressReadsStatsJsonBackAndSaysSoWhenEmpty)
{
    Rig rig ("Progress");
    const auto statsFile = rig.locations().statsFile;

    // The tab's own history, read from stats.json: 12.1 "the tab reads it back".
    PracticeSetupPanel panel (rig.context (false));

    // 11.4, word for word.
    CHECK (panel.isShowingNoStats());
    CHECK (panel.getProgressSummary()
             == "No practice recorded yet. The metronome, looper and trainers all count time once you start them.");
    CHECK_MSG (! panel.getExportCsvButton().isEnabled(), "there is nothing to export");
    CHECK_MSG (! panel.getClearHistoryButton().isEnabled(), "there is nothing to clear");

    // 12.1: sixty seconds of metronome, counted by the tracker as the drawer
    // will, land against today and the tab reads them back.
    PracticeStats written;
    PracticeActivityTracker tracker;
    rig.metronome.setEnabled (true);

    for (int second = 0; second < 60; ++second)
        tracker.update (rig.targets(), true, 1.0, written, PracticeStats::today());

    rig.metronome.setEnabled (false);

    juce::String error;
    CHECK_MSG (written.save (statsFile, error), error);

    panel.refresh();
    CHECK (! panel.isShowingNoStats());
    CHECK_NEAR (panel.getStats().getSeconds (PracticeStats::today(), PracticeTool::metronome), 60.0, 1.0e-9);
    CHECK_MSG (panel.getProgressSummary().contains ("Streak: 1 day"), panel.getProgressSummary());
    CHECK_MSG (panel.getProgressSummary().contains ("Today: 1 min"), panel.getProgressSummary());

    // Shared with the processor: what the tracker adds shows on the next refresh.
    PracticeSetupPanel shared (rig.context (true));
    practiseAWeek (rig.stats);
    shared.refresh();

    CHECK_MSG (shared.getProgressSummary().contains ("Streak: 6 days"), shared.getProgressSummary());
    CHECK (shared.getAccuracyBox().getNumItems() == 2);   // ear.interval, scale.dorian.quiz
    CHECK (shared.getExportCsvButton().isEnabled());

    // Export as CSV: the model's CSV, as a file.
    const auto csv = rig.root.getChildFile ("export").getChildFile ("history.csv");
    CHECK_MSG (shared.exportCsvTo (csv, &error), error);
    CHECK (csv.loadFileAsString() == rig.stats.toCsv());
    CHECK (csv.loadFileAsString().startsWith ("date,total_minutes"));
}

LUTHIER_TEST (PracticeSetupPanel, clearHistoryEmptiesStatsButKeepsLoopsAndSessions)
{
    // 12.1: "Clear history confirms and, on confirm, empties stats.json without
    // deleting saved loops or sessions." The confirmation is a dialog; this is
    // the path its "Clear history" button takes.
    Rig rig ("Clear");
    const auto statsFile = rig.locations().statsFile;

    // The ear trainer's own keys share the file and must survive the clear.
    statsFile.getParentDirectory().createDirectory();
    statsFile.replaceWithText ("{ \"difficulty\": 3 }");

    practiseAWeek (rig.stats);
    juce::String error;
    CHECK_MSG (rig.stats.save (statsFile, error), error);

    const auto loop = rig.makeLoop ("Riff");
    const auto session = rig.makeSession ("session-2026-09-23");

    PracticeSetupPanel panel (rig.context());
    CHECK (! panel.isShowingNoStats());
    CHECK (panel.getLoopItems().size() == 1 && panel.getSessionItems().size() == 1);

    panel.clearHistoryConfirmed();

    CHECK (rig.stats.isEmpty());
    CHECK (panel.isShowingNoStats());
    CHECK (panel.getProgressSummary() == juce::String (PracticeEmptyStates::noStats));

    PracticeStats readBack;
    CHECK (readBack.load (statsFile));
    CHECK_MSG (readBack.isEmpty(), "stats.json still holds history");
    CHECK ((int) juce::JSON::parse (statsFile)["difficulty"] == 3);

    CHECK_MSG (loop.getChildFile ("loop.json").existsAsFile(), "clearing history deleted a saved loop");
    CHECK_MSG (session.existsAsFile(), "clearing history deleted a saved session");

    panel.refresh();
    CHECK (panel.getLoopItems().size() == 1 && panel.getSessionItems().size() == 1);
}

//==============================================================================
// Routines (11.2, 11.4, 12.1)
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, aRoutineStartsInTheDrawerFromTheTab)
{
    // 12.1 "A routine drives the drawer": START hands the routine to the runner
    // the drawer advances, and opens the drawer on its first entry.
    Rig rig ("Start");
    PracticeSetupPanel panel (rig.context());

    CHECK (panel.getRoutineLibrary().getNumRoutines() == 3);
    CHECK (panel.getUserRoutinesText() == "Your own routines appear here.");

    panel.selectRoutine (0);
    CHECK (panel.getSelectedRoutine().name == "Warm-up, 10 minutes");
    CHECK_MSG (! panel.getDeleteRoutineButton().isEnabled(), "a factory routine can be deleted");
    CHECK_MSG (! panel.getEntryToolBox().isEnabled(), "a factory routine's entries are editable");
    CHECK_MSG (! panel.getAddEntryButton().isEnabled(), "entries can be added to a factory routine");

    CHECK (press (panel.getStartButton()));
    CHECK (rig.runner.isActive());
    CHECK (rig.runner.getEntryIndex() == 0);
    CHECK (rig.runner.getActiveTool() == PracticeTool::metronome);
    CHECK_NEAR (rig.metronome.getTempo(), 60.0, 1.0e-9);
    CHECK_MSG (rig.drawerRequests == 1 && rig.drawerTool == PracticeTool::metronome,
               "the drawer was not opened on the first entry");

    panel.refresh();
    CHECK_MSG (panel.getRunnerStatus().contains ("1 of"), panel.getRunnerStatus());

    // The drawer's timer moves it on; the tab only reports it.
    rig.runner.advance (120.0);
    panel.refresh();
    CHECK (rig.runner.getEntryIndex() == 1);
    CHECK_MSG (panel.getRunnerStatus().contains ("2 of"), panel.getRunnerStatus());

    rig.runner.stop();
}

LUTHIER_TEST (PracticeSetupPanel, routineEditsGoThroughTheModelAndRoundTrip)
{
    Rig rig ("Routines");
    const auto folder = rig.locations().routinesFolder;
    PracticeSetupPanel panel (rig.context());

    // NEW: a user routine, on disk at once.
    CHECK (press (panel.getNewRoutineButton()));
    CHECK (panel.getRoutineLibrary().getNumUserRoutines() == 1);
    CHECK (panel.getUserRoutinesText().isEmpty());
    CHECK (panel.getSelectedRoutine().name == "My routine");
    CHECK (panel.getSelectedRoutine().entries.size() == 1);
    CHECK (folder.getChildFile ("My routine.json").existsAsFile());
    CHECK (panel.getEntryToolBox().isEnabled());

    // The entry editor: tool, settings, repetitions.
    choose (panel.getEntryToolBox(), (int) PracticeTool::scaleTrainer + 1);
    CHECK (panel.getSelectedRoutine().entries[0].tool == PracticeTool::scaleTrainer);
    CHECK (panel.getSelectedRoutine().entries[0].title == "SCALE");
    CHECK (panel.getEntryOptionBox (0).isVisible() && panel.getEntryOptionBox (2).isVisible());

    CHECK (chooseText (panel.getEntryOptionBox (0), "A"));                 // key
    CHECK (chooseText (panel.getEntryOptionBox (1), getScaleTypeName (ScaleType::dorian)));
    CHECK (chooseText (panel.getEntryOptionBox (2), "Quiz"));
    drag (panel.getEntryTempoSlider(), 84.0);

    choose (panel.getEntryModeBox(), 2);
    drag (panel.getEntryRepetitionsSlider(), 4.0);

    choose (panel.getEntryCountInBox(), 2);
    type (panel.getEntryTitleEditor(), "Dorian quiz");

    // ADD copies the selected entry after it; the copy becomes a progression.
    CHECK (press (panel.getAddEntryButton()));
    CHECK (panel.getSelectedEntryIndex() == 1);
    choose (panel.getEntryToolBox(), (int) PracticeTool::progression + 1);
    CHECK (panel.getEntryTextEditor().isVisible());
    type (panel.getEntryTextEditor(), "Dm7 - G7 - Cmaj7 x4");
    choose (panel.getEntryModeBox(), 1);
    drag (panel.getEntryDurationSlider(), 90.0);

    // UP swaps them.
    CHECK (press (panel.getMoveUpButton()));
    CHECK (panel.getSelectedEntryIndex() == 0);
    CHECK (panel.getSelectedRoutine().entries[0].tool == PracticeTool::progression);

    // Reloaded from disk, it is what the editor showed.
    PracticeRoutine loaded;
    juce::String error;
    CHECK_MSG (PracticeRoutine::loadFrom (folder.getChildFile ("My routine.json"), loaded, error), error);
    CHECK (loaded == panel.getSelectedRoutine());
    CHECK (loaded.entries.size() == 2);

    if (loaded.entries.size() == 2)
    {
        const auto& progression = loaded.entries[0];
        CHECK (progression.settings["text"].toString() == "Dm7 - G7 - Cmaj7 x4");
        CHECK_NEAR (progression.durationSeconds, 90.0, 1.0e-9);
        CHECK (progression.isTimed());
        CHECK_MSG (! progression.settings.getDynamicObject()->hasProperty ("key"),
                   "the scale trainer's settings followed the entry to another tool");
        CHECK_NEAR ((double) progression.settings["tempo_bpm"], 84.0, 1.0e-9);   // the practice tempo stays

        const auto& scale = loaded.entries[1];
        CHECK (scale.title == "Dorian quiz");
        CHECK (scale.settings["key"].toString() == "A");
        CHECK (scale.settings["scale"].toString() == "dorian");
        CHECK (scale.settings["mode"].toString() == "quiz");
        CHECK (scale.repetitions == 4 && ! scale.isTimed());
        CHECK (scale.countInBars == 1);

        // And it drives the tools as saved.
        CHECK (applyRoutineSettings (scale, rig.targets()).isEmpty());
        CHECK (rig.scaleTrainer.getKey() == 9 && rig.scaleTrainer.getScale() == ScaleType::dorian);
    }

    // Rename: the file follows the name.
    type (panel.getNameEditor(), "Evening");
    CHECK (panel.getSelectedRoutine().name == "Evening");
    CHECK (folder.getChildFile ("Evening.json").existsAsFile());
    CHECK (! folder.getChildFile ("My routine.json").exists());

    // A factory name is refused.
    type (panel.getNameEditor(), "Warm-up, 10 minutes");
    CHECK (panel.getSelectedRoutine().name == "Evening");
    CHECK (panel.getRoutineStatus().isNotEmpty());

    // DUPLICATE a factory routine to change it.
    panel.selectRoutine (1);
    CHECK (press (panel.getDuplicateButton()));
    CHECK (panel.getSelectedRoutine().name == "Scales and modes, 20 minutes copy");
    CHECK (! panel.getSelectedRoutine().factory);
    CHECK (panel.getSelectedRoutine().entries.size() == PracticeRoutineLibrary::createFactoryRoutines()[1].entries.size());
    CHECK (panel.getDeleteRoutineButton().isEnabled());

    // Editing the copy leaves the factory routine as shipped.
    choose (panel.getEntryOptionBox (0), 1);   // "Tool's own": no key
    const auto& factory = panel.getRoutineLibrary().getRoutine (1);
    CHECK_MSG (factory.entries[0].settings["key"].toString().isNotEmpty(),
               "editing a copy changed the factory routine");

    // DELETE, confirmed.
    panel.deleteRoutineConfirmed();
    CHECK (panel.getRoutineLibrary().indexOf ("Scales and modes, 20 minutes copy") < 0);
    CHECK (panel.getRoutineLibrary().getNumUserRoutines() == 1);

    // A second tab over the same folder finds what was saved.
    PracticeSetupPanel reopened (rig.context());
    CHECK (reopened.getRoutineLibrary().indexOf ("Evening") >= 0);
}

//==============================================================================
// Defaults (11.2, 12.1)
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, defaultsControlsWriteTheModelAndApplyOnReopen)
{
    Rig rig ("Defaults");
    const auto file = rig.locations().defaultsFile;
    PracticeSetupPanel panel (rig.context());

    drag (panel.getDefaultTempoSlider(), 97.0);
    CHECK (chooseText (panel.getTimeSignatureBox(), "7/8"));
    choose (panel.getSubdivisionBox(), (int) ClickSubdivision::sixteenth + 1);
    choose (panel.getClickSoundBox(), (int) ClickSound::cowbell + 1);

    // Seven beats of accent buttons now; beat two goes accent-less, then ghost.
    CHECK (panel.getAccentButton (6) != nullptr && panel.getAccentButton (7) == nullptr);

    if (auto* beatTwo = panel.getAccentButton (1))
    {
        CHECK (press (*beatTwo));   // normal -> ghost
        CHECK (panel.getDefaults().accents[1] == BeatAccent::ghost);
    }

    drag (panel.getLoopLengthSlider(), 16.0);
    choose (panel.getLoopCountInBox(), 3);   // two bars
    choose (panel.getOverdubBox(), (int) LayerMode::playOnce + 1);

    choose (panel.getTrainerKeyBox(), 8);    // G
    CHECK (press (panel.getScaleToggle (ScaleType::ionian)));    // off
    CHECK (press (panel.getScaleToggle (ScaleType::blues)));     // on
    drag (panel.getRangeLowSlider(), 45.0);
    drag (panel.getRangeHighSlider(), 81.0);
    drag (panel.getQuestionsSlider(), 30.0);

    CHECK (press (panel.getShuffleToggle()));
    drag (panel.getBackingLevelSlider(), -9.0);

    PracticeDefaults saved;
    juce::String error;
    CHECK_MSG (PracticeDefaults::load (file, saved, error), error);
    CHECK (saved == panel.getDefaults());

    CHECK_NEAR (saved.metronomeTempo, 97.0, 1.0e-9);
    CHECK (saved.timeSigNumerator == 7 && saved.timeSigDenominator == 8);
    CHECK (saved.accents.size() == 7);
    CHECK (saved.subdivision == ClickSubdivision::sixteenth);
    CHECK (saved.sound == ClickSound::cowbell);
    CHECK_NEAR (saved.loopLengthSeconds, 16.0, 1.0e-9);
    CHECK (saved.loopCountInBars == 2);
    CHECK (saved.overdubMode == LayerMode::playOnce);
    CHECK (saved.trainerKey == 7);
    CHECK ((saved.scaleSet == std::vector<ScaleType> { ScaleType::aeolian, ScaleType::minorPentatonic, ScaleType::blues }));
    CHECK (saved.rangeLowNote == 45 && saved.rangeHighNote == 81);
    CHECK (saved.questionCount == 30);
    CHECK (saved.shuffle);
    CHECK_NEAR (saved.backingLevelDb, -9.0, 1.0e-9);

    // 12.1 "Defaults apply": reopened, a fresh metronome starts at them, and
    // setting them started nothing.
    CHECK (! rig.anyTransportRunning());

    Metronome fresh;
    PracticeTargets targets;
    targets.metronome = &fresh;
    saved.applyTo (targets);
    CHECK_NEAR (fresh.getTempo(), 97.0, 1.0e-9);
    CHECK (! fresh.isEnabled());

    // And a second tab (the next session) shows them.
    PracticeSetupPanel reopened (rig.context());
    CHECK_NEAR (reopened.getDefaultTempoSlider().getValue(), 97.0, 1.0e-9);
    CHECK (reopened.getTimeSignatureBox().getText() == "7/8");
    CHECK (reopened.getScaleToggle (ScaleType::blues).getToggleState());
    CHECK (! reopened.getScaleToggle (ScaleType::ionian).getToggleState());
}

//==============================================================================
// Library (11.2, 11.4)
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, libraryListsLoadsAndDeletesAndStatesItsEmptyLists)
{
    {
        Rig empty ("LibraryEmpty");
        PracticeSetupPanel panel (empty.context());

        // 11.4: "Saved loops appear here", with the folder button still offered.
        CHECK (panel.getLoopsText() == "Saved loops appear here");
        CHECK (panel.getSessionsText() == juce::String (PracticeEmptyStates::noSessions));
        CHECK (panel.getRecentTabsText().isNotEmpty());
        CHECK (! panel.getLoadLoopButton().isEnabled());
    }

    Rig rig ("Library");
    rig.makeLoop ("Riff A");
    rig.makeLoop ("Riff B");
    rig.makeSession ("session-1");

    const auto tab = rig.root.getChildFile ("Tabs").getChildFile ("Blues in A.gp");
    tab.getParentDirectory().createDirectory();
    tab.replaceWithText ("tab");

    PracticeLibrary recent;
    recent.noteTabOpened (tab);
    recent.noteTabOpened (rig.root.getChildFile ("gone.gp"));   // pruned: it is not there
    juce::String error;
    CHECK (recent.save (rig.locations().libraryFile, error));

    PracticeSetupPanel panel (rig.context());
    CHECK (panel.getLoopItems().size() == 2);
    CHECK (panel.getLoopsText().isEmpty());
    CHECK (panel.getSessionItems().size() == 1);
    CHECK (panel.getRecentTabList().getListBoxModel() != nullptr
           && panel.getRecentTabList().getListBoxModel()->getNumRows() == 1);

    // LOAD: into the looper, stopped, and the drawer opens on LOOP.
    panel.getLoopList().selectRow (0);
    CHECK (panel.getLoadLoopButton().isEnabled());
    CHECK (press (panel.getLoadLoopButton()));
    CHECK (rig.drawerTool == PracticeTool::looper);
    CHECK (rig.looper.getLoopLengthSamples() == 100);
    CHECK_MSG (rig.looper.getState() == Looper::State::stopped, "loading a loop started it");

    // DELETE, confirmed: the folder goes, the other loop stays.
    const auto doomed = panel.getLoopItems()[0].file;
    CHECK (panel.deleteLoopConfirmed (0));
    CHECK (! doomed.exists());
    CHECK (panel.getLoopItems().size() == 1);

    // The backing-tracks folder is the defaults' folder.
    panel.setBackingFolder (rig.root);
    PracticeDefaults saved;
    CHECK (PracticeDefaults::load (rig.locations().defaultsFile, saved, error));
    CHECK (saved.backingFolder == rig.root.getFullPathName());
}

//==============================================================================
// Session recorder setup (11.2, performance-budget 3)
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, sessionSetupWritesTheModelAndStatesTheSizeInPlainWords)
{
    Rig rig ("Session");
    PracticeSetupPanel panel (rig.context());

    // The default 60-minute ring, said in plain words beside the control.
    CHECK_NEAR (panel.getRingMinutesSlider().getValue(), 60.0, 1.0e-9);
    CHECK_MSG (panel.getSizeWarning().contains ("1.4 GB"), panel.getSizeWarning());
    CHECK (panel.getSizeWarning().contains ("off by default"));

    // Off, the recorder takes no memory, whatever the length says.
    drag (panel.getRingMinutesSlider(), 2.0);
    CHECK_NEAR (rig.recorder.getCapacityMinutes(), 0.0, 1.0e-9);
    CHECK (panel.getSizeWarning().contains ("2-minute"));

    choose (panel.getRecordWhatBox(), 3);   // MIDI only
    CHECK (press (panel.getAutoSaveToggle()));

    const auto& setup = panel.getSessionSetup();
    CHECK_NEAR (setup.ringMinutes, 2.0, 1.0e-9);
    CHECK (setup.recordMidi && ! setup.recordAudio);
    CHECK (setup.autoSaveOnStop);

    // Kept with the defaults, and read back by the next tab.
    PracticeDefaults saved;
    juce::String error;
    CHECK_MSG (PracticeDefaults::load (rig.locations().defaultsFile, saved, error), error);
    CHECK (SessionRecorderSetup::fromVar (saved.extra[PracticeSetupPanel::kSessionSetupKey]) == setup);

    PracticeSetupPanel reopened (rig.context());
    CHECK (reopened.getSessionSetup() == setup);
    CHECK (reopened.getRecordWhatBox().getSelectedId() == 3);

    // On, a new length resizes the ring.
    rig.recorder.setEnabled (true);
    drag (panel.getRingMinutesSlider(), 1.0);
    CHECK_NEAR (rig.recorder.getCapacityMinutes(), 1.0, 0.01);
    rig.recorder.setEnabled (false);
}

//==============================================================================
// 11.3 and 12.1 "No transport on the tab"
//==============================================================================

LUTHIER_TEST (PracticeSetupPanel, theTabExposesNoTransport)
{
    Rig rig ("NoTransport");
    rig.makeLoop ("Riff");
    PracticeSetupPanel panel (rig.context());

    juce::Array<juce::Button*> buttons;
    findButtons (panel, buttons);
    CHECK (buttons.size() > 20);

    const juce::StringArray transportWords { "PLAY", "STOP", "RECORD", "REC", "PAUSE", "RESUME", "TAP",
                                             "PANIC", "NEXT", "PREVIOUS", "SKIP" };

    for (auto* button : buttons)
        for (const auto& word : transportWords)
            CHECK_MSG (! button->getButtonText().toUpperCase().containsWholeWord (word),
                       "a transport control on the setup tab: " + button->getButtonText());

    /*  Press everything that does not open a dialog or a folder window. START
        is the one deliberate exception, covered above: it hands a routine to
        the drawer, whose runner is the transport. Loading a loop loads it,
        stopped. */
    panel.getLoopList().selectRow (0);
    panel.selectRoutine (0);
    CHECK (press (panel.getDuplicateButton()));   // an editable routine, so the entry buttons are live

    for (auto* button : buttons)
    {
        const auto text = button->getButtonText();

        if (button == &panel.getStartButton() || text.endsWith ("...") || text == "OPEN FOLDER" || text == "REVEAL")
            continue;

        press (*button);
        CHECK_MSG (! rig.anyTransportRunning(), "pressing " + text + " started a tool");
    }

    CHECK (! rig.anyTransportRunning());
}

//==============================================================================
LUTHIER_TEST (PracticeSetupPanel, renders)
{
    Rig rig ("Render");
    practiseAWeek (rig.stats);
    rig.makeLoop ("Blues riff in A");
    rig.makeSession ("session-2026-09-22 21-14-03");

    PracticeSetupPanel panel (rig.context());
    panel.selectRoutine (1);

    // For a person to look at, at the width of column 4.
    LuthierLookAndFeel lookAndFeel;
    panel.setLookAndFeel (&lookAndFeel);
    panel.setSize (360, panel.getPreferredHeight());
    panel.refresh();

    CHECK (panel.getHeight() == panel.getPreferredHeight());

    juce::Image image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        g.fillAll (Palette::panel);
        panel.paintEntireComponent (g, true);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
    dir.createDirectory();
    const auto png = dir.getChildFile ("_practice.png");
    png.deleteFile();
    {
        juce::FileOutputStream out (png);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }

    CHECK (png.existsAsFile());
    panel.setLookAndFeel (nullptr);
}
