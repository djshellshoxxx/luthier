/*  The PRACTICE tab's model: practice-tools.md 11 (routines, progress,
    defaults, library, session setup, empty states) and its 12.1 tests, plus
    the speed trainer (6), count-in and loop regions.

    Everything here runs against the real practice tools - Metronome, Looper,
    ScaleTrainer, EarTrainer, ProgressionLooper - built standalone, the same
    objects the processor owns. Files go to a temp folder, never to the user's
    Documents.
*/

#include "TestFramework.h"

#include "../Practice/PracticeRoutine.h"
#include "../Practice/PracticeRoutineProgress.h"
#include "../Practice/PracticeRoutineTempo.h"
#include "../Practice/PracticeRoutineSetup.h"

#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** One of each tool, standalone, and the targets that point at them. */
    struct Tools
    {
        Metronome metronome;
        Looper looper;
        ScaleTrainer scaleTrainer;
        EarTrainer earTrainer;
        ProgressionLooper progression;

        Tools()
        {
            metronome.prepare (48000.0, 512);
        }

        PracticeTargets targets()
        {
            PracticeTargets t;
            t.metronome = &metronome;
            t.looper = &looper;
            t.scaleTrainer = &scaleTrainer;
            t.earTrainer = &earTrainer;
            t.progression = &progression;
            return t;
        }
    };

    juce::File tempFolder (const char* name)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
        folder.deleteRecursively();
        folder.createDirectory();
        return folder;
    }

    juce::var settings (std::initializer_list<std::pair<const char*, juce::var>> pairs)
    {
        auto* o = new juce::DynamicObject();

        for (const auto& p : pairs)
            o->setProperty (p.first, p.second);

        return juce::var (o);
    }

    RoutineEntry entry (PracticeTool tool, double seconds, juce::var s)
    {
        RoutineEntry e;
        e.tool = tool;
        e.title = getPracticeToolTabLabel (tool);
        e.durationSeconds = seconds;
        e.settings = std::move (s);
        return e;
    }

    /** Five entries across the tools, with a count-in, a repetition entry,
        an unknown settings key and an unknown entry field. */
    PracticeRoutine makeFiveEntryRoutine()
    {
        PracticeRoutine r;
        r.name = "My routine";
        r.author = "Test";
        r.notes = "Five entries.";

        r.entries.push_back (entry (PracticeTool::metronome, 30.0,
                                    settings ({ { "tempo_bpm", 100.0 }, { "subdivision", "triplet" },
                                                { "future_key", "kept" } })));

        auto scale = entry (PracticeTool::scaleTrainer, 45.0,
                            settings ({ { "key", "E" }, { "scale", "blues" }, { "mode", "quiz" } }));
        scale.countInBars = 1;
        r.entries.push_back (scale);

        auto ear = entry (PracticeTool::earTrainer, 0.0, settings ({ { "exercise", "chord_quality" } }));
        ear.repetitions = 3;
        ear.extra.set ("future_entry_field", 12);
        r.entries.push_back (ear);

        r.entries.push_back (entry (PracticeTool::progression, 20.0, settings ({ { "text", "Am - F - C - G x2" } })));
        r.entries.push_back (entry (PracticeTool::looper, 25.0, settings ({ { "layer_mode", "replace" } })));

        return r;
    }
}

//==============================================================================
// Tools and keys
//==============================================================================

LUTHIER_TEST (PracticeRoutine, toolsAreInTheDrawersTabOrder)
{
    const char* const labels[] = { "METRO", "LOOP", "TRACK", "SCALE", "EAR", "TAB", "PROG", "SESSION" };

    CHECK ((int) PracticeTool::numTools == 8);

    for (int i = 0; i < (int) PracticeTool::numTools; ++i)
    {
        CHECK (juce::String (getPracticeToolTabLabel ((PracticeTool) i)) == labels[i]);

        PracticeTool parsed;
        CHECK (parsePracticeTool (getPracticeToolKey ((PracticeTool) i), parsed) && parsed == (PracticeTool) i);
    }

    // Every enum the settings use has a key for every value, and parses back.
    for (int i = 0; i < (int) ClickSubdivision::numSubdivisions; ++i)
    {
        ClickSubdivision v;
        CHECK (practicekeys::parse (practicekeys::subdivision ((ClickSubdivision) i), v) && v == (ClickSubdivision) i);
    }

    for (int i = 0; i < (int) ClickSound::numSounds; ++i)
    {
        ClickSound v;
        CHECK (practicekeys::parse (practicekeys::sound ((ClickSound) i), v) && v == (ClickSound) i);
    }

    for (int i = 0; i < (int) ScaleType::numScales; ++i)
    {
        ScaleType v;
        CHECK (practicekeys::parse (practicekeys::scale ((ScaleType) i), v) && v == (ScaleType) i);
    }

    for (int i = 0; i < (int) LayerMode::numModes; ++i)
    {
        LayerMode v;
        CHECK (practicekeys::parse (practicekeys::layerMode ((LayerMode) i), v) && v == (LayerMode) i);
    }
}

//==============================================================================
// Routines (11.2)
//==============================================================================

LUTHIER_TEST (PracticeRoutine, theThreeFactoryRoutinesLastTheirStatedLengthAndAllApply)
{
    const auto routines = PracticeRoutineLibrary::createFactoryRoutines();

    CHECK (routines.size() == 3);

    if (routines.size() != 3)
        return;

    CHECK (routines[0].name == "Warm-up, 10 minutes");
    CHECK (routines[1].name == "Scales and modes, 20 minutes");
    CHECK (routines[2].name == "Timing and feel, 15 minutes");

    CHECK_NEAR (routines[0].getTotalSeconds(), 10.0 * 60.0, 1.0e-9);
    CHECK_NEAR (routines[1].getTotalSeconds(), 20.0 * 60.0, 1.0e-9);
    CHECK_NEAR (routines[2].getTotalSeconds(), 15.0 * 60.0, 1.0e-9);

    Tools tools;

    for (const auto& r : routines)
    {
        CHECK (r.factory);

        for (const auto& e : r.entries)
        {
            const auto problems = applyRoutineSettings (e, tools.targets());
            CHECK_MSG (problems.isEmpty(), r.name + " / " + e.title + ": " + problems.joinIntoString ("; "));
        }
    }
}

LUTHIER_TEST (PracticeRoutine, aFiveEntryRoutineRoundTripsIdentically)
{
    // 12.1: "Save a routine with five entries, reload, assert identical."
    const auto folder = tempFolder ("LuthierPracticeRoutineRoundTrip");
    const auto file = folder.getChildFile ("My routine.json");
    const auto routine = makeFiveEntryRoutine();

    juce::String error;
    CHECK_MSG (routine.saveTo (file, error), error);
    CHECK (! file.getSiblingFile (file.getFileName() + ".tmp").exists());

    PracticeRoutine loaded;
    CHECK_MSG (PracticeRoutine::loadFrom (file, loaded, error), error);
    CHECK (loaded == routine);
    CHECK (loaded.entries.size() == 5);

    // Unknown keys survive, in the settings and on the entry.
    const auto text = file.loadFileAsString();
    CHECK (text.contains ("\"future_key\": \"kept\""));
    CHECK (text.contains ("\"future_entry_field\": 12"));
    CHECK (text.contains ("\"magic\": \"luthier.routine\""));

    // Not a routine: refused with a reason, the destination left alone.
    PracticeRoutine untouched = routine;
    CHECK (! PracticeRoutine::fromVar (juce::JSON::parse ("{ \"magic\": \"luthier.tune\" }"), untouched, error));
    CHECK (error.isNotEmpty() && untouched == routine);

    CHECK (! PracticeRoutine::fromVar (juce::JSON::parse (
               "{ \"magic\": \"luthier.routine\", \"meta\": { \"name\": \"x\" }, \"entries\": [ { \"tool\": \"kazoo\" } ] }"),
           untouched, error));
    CHECK (error.contains ("kazoo"));

    folder.deleteRecursively();
}

LUTHIER_TEST (PracticeRoutine, theLibraryAlwaysHasTheFactoryThreeAndKeepsUserRoutines)
{
    const auto folder = tempFolder ("LuthierPracticeRoutineLibrary");

    PracticeRoutineLibrary library (folder);
    CHECK (library.getNumRoutines() == 3);
    CHECK (library.getNumUserRoutines() == 0);

    juce::String error;
    CHECK (library.save (makeFiveEntryRoutine(), error));
    CHECK (library.getNumRoutines() == 4 && library.getNumUserRoutines() == 1);

    // A factory routine cannot be overwritten or removed.
    auto factoryCopy = library.getRoutine (0);
    CHECK (! library.save (factoryCopy, error));
    CHECK (! library.remove (factoryCopy.name));

    // A second library over the same folder finds the saved one.
    PracticeRoutineLibrary reopened (folder);
    CHECK (reopened.getNumRoutines() == 4);
    CHECK (reopened.indexOf ("My routine") == 3);
    CHECK (! reopened.getRoutine (3).factory);

    // A broken file is skipped and named.
    folder.getChildFile ("Broken.json").replaceWithText ("{ not json");
    reopened.refresh();
    CHECK (reopened.getNumRoutines() == 4);
    CHECK (reopened.getLoadErrors().size() == 1);

    CHECK (reopened.remove ("My routine"));
    CHECK (reopened.getNumUserRoutines() == 0);
    CHECK (! reopened.getFileFor ("My routine").exists());

    folder.deleteRecursively();
}

LUTHIER_TEST (PracticeRoutine, aRoutineDrivesTheDrawerEntryByEntryOnTime)
{
    // 12.1: "Start a factory routine; assert the drawer's active tool,
    // settings and timer match each entry in turn and that it advances on time."
    Tools tools;
    PracticeRoutineRunner runner (tools.targets());

    std::vector<int> started;
    bool finished = false;
    runner.onEntryStarted = [&started] (int index) { started.push_back (index); };
    runner.onFinished = [&finished] { finished = true; };

    const auto warmUp = PracticeRoutineLibrary::createFactoryRoutines()[0];
    CHECK (runner.start (warmUp));

    // Entry 1: metronome, 60 bpm quarters, clicking.
    CHECK (runner.getActiveTool() == PracticeTool::metronome);
    CHECK_NEAR (tools.metronome.getTempo(), 60.0, 1.0e-9);
    CHECK (tools.metronome.getSubdivision() == ClickSubdivision::quarter);
    CHECK (tools.metronome.isEnabled());
    CHECK_NEAR (runner.getEntryRemainingSeconds(), 120.0, 1.0e-9);

    for (int second = 0; second < 119; ++second)
        runner.advance (1.0);

    CHECK (runner.getEntryIndex() == 0);
    CHECK_NEAR (runner.getEntryRemainingSeconds(), 1.0, 1.0e-6);

    // On time: the 120th second moves it on.
    runner.advance (1.0);
    CHECK (runner.getEntryIndex() == 1);
    CHECK_NEAR (tools.metronome.getTempo(), 72.0, 1.0e-9);
    CHECK (tools.metronome.getSubdivision() == ClickSubdivision::eighth);

    // One long step crosses into the third entry and runs its clock.
    runner.advance (120.0 + 30.0);
    CHECK (runner.getEntryIndex() == 2);
    CHECK_NEAR (runner.getEntryElapsedSeconds(), 30.0, 1.0e-6);
    CHECK (tools.metronome.isProgressiveTempoRunning());

    // Entry 4: the scale trainer, with the click stopped.
    runner.advance (150.0);
    CHECK (runner.getEntryIndex() == 3);
    CHECK (runner.getActiveTool() == PracticeTool::scaleTrainer);
    CHECK (tools.scaleTrainer.getKey() == 0);
    CHECK (tools.scaleTrainer.getScale() == ScaleType::ionian);
    CHECK (tools.scaleTrainer.getMode() == ScaleTrainer::Mode::explore);
    CHECK (! tools.metronome.isEnabled());
    CHECK (! tools.metronome.isProgressiveTempoRunning());

    // Entry 5: the progression looper, loaded with the progression.
    runner.advance (120.0);
    CHECK (runner.getActiveTool() == PracticeTool::progression);
    CHECK (tools.progression.getNumChords() == 4);
    CHECK_NEAR (tools.metronome.getTempo(), 90.0, 1.0e-9);

    runner.advance (60.0);
    CHECK (finished);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::finished);
    CHECK ((started == std::vector<int> { 0, 1, 2, 3, 4 }));
    CHECK (! tools.metronome.isEnabled());
}

LUTHIER_TEST (PracticeRoutine, countInsRepetitionsPauseAndSkipBehave)
{
    Tools tools;
    PracticeRoutineRunner runner (tools.targets());

    int timersStarted = 0;
    runner.onTimerStarted = [&timersStarted] (int) { ++timersStarted; };

    auto routine = makeFiveEntryRoutine();
    CHECK (runner.start (routine));
    CHECK (timersStarted == 1);   // no count-in on the first entry

    // Entry 2 has a one-bar count-in at the first entry's 100 bpm, in 4/4:
    // 4 beats * 0.6 s = 2.4 s, with the click on even though it is a scale entry.
    runner.advance (30.0);
    CHECK (runner.getEntryIndex() == 1);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::countIn);
    CHECK_NEAR (runner.getCountInRemainingSeconds(), 2.4, 1.0e-6);
    CHECK (tools.metronome.isEnabled());
    CHECK (tools.scaleTrainer.getScale() == ScaleType::blues);
    CHECK (tools.scaleTrainer.getKey() == 4);

    runner.advance (2.4);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::running);
    CHECK (! tools.metronome.isEnabled());
    CHECK (timersStarted == 2);

    // Paused, time does not count.
    runner.pause();
    runner.advance (1000.0);
    CHECK (runner.getEntryIndex() == 1);
    CHECK_NEAR (runner.getEntryElapsedSeconds(), 0.0, 1.0e-9);
    runner.resume();

    runner.advance (45.0);
    CHECK (runner.getEntryIndex() == 2);

    // Entry 3 counts repetitions, not time.
    runner.advance (600.0);
    CHECK (runner.getEntryIndex() == 2);
    CHECK (tools.earTrainer.getExercise() == EarTrainer::Exercise::chordQuality);

    runner.completeRepetition();
    runner.completeRepetition();
    CHECK (runner.getEntryIndex() == 2 && runner.getRepetitionsDone() == 2);
    runner.completeRepetition();
    CHECK (runner.getEntryIndex() == 3);

    // Skipping around.
    CHECK (runner.previous());
    CHECK (runner.getEntryIndex() == 2 && runner.getRepetitionsDone() == 0);
    CHECK (runner.next() && runner.next());
    CHECK (runner.getActiveTool() == PracticeTool::looper);
    CHECK (tools.looper.getLayer (tools.looper.getActiveLayer()).getMode() == LayerMode::replace);

    runner.stop();
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::idle);
    CHECK (! runner.isActive());

    // An empty routine does not start.
    CHECK (! runner.start (PracticeRoutine()));
}

LUTHIER_TEST (PracticeRoutine, badSettingsAreReportedNotApplied)
{
    Tools tools;

    const auto bad = entry (PracticeTool::metronome, 10.0,
                            settings ({ { "subdivision", "quintuplet" }, { "time_sig", "4/5" }, { "tempo_bpm", 97.0 } }));

    const auto problems = applyRoutineSettings (bad, tools.targets());
    CHECK (problems.size() == 2);
    CHECK (tools.metronome.getSubdivision() == ClickSubdivision::quarter);
    CHECK_NEAR (tools.metronome.getTempo(), 97.0, 1.0e-9);   // the good value still took

    // A tool the caller does not have is reported, not dereferenced.
    const auto track = entry (PracticeTool::backingTrack, 10.0, settings ({ { "level_db", -3.0 } }));
    CHECK (applyRoutineSettings (track, tools.targets()).size() == 1);
}

//==============================================================================
// Progress (11.2)
//==============================================================================

LUTHIER_TEST (PracticeRoutine, sixtySecondsOfMetronomeAddSixtySecondsAgainstToday)
{
    // 12.1: "Run the metronome for 60 s; assert stats.json gains 60 s against
    // today and that the tab reads it back."
    const auto folder = tempFolder ("LuthierPracticeStats");
    const auto file = folder.getChildFile ("stats.json");

    // The ear trainer's own statistics are already in the file.
    file.replaceWithText ("{ \"difficulty\": 3, \"adaptive\": true, \"lifetimeAsked\": [1, 2, 3] }");

    Tools tools;
    tools.metronome.setEnabled (true);

    PracticeStats stats;
    CHECK (stats.load (file));
    CHECK (stats.isEmpty());

    PracticeActivityTracker tracker;
    const auto today = PracticeStats::today();

    for (int second = 0; second < 60; ++second)
        tracker.update (tools.targets(), true, 1.0, stats, today);

    // With the drawer closed the tools are silent, and that is not practice.
    tracker.update (tools.targets(), false, 30.0, stats, today);

    juce::String error;
    CHECK_MSG (stats.save (file, error), error);

    PracticeStats readBack;
    CHECK (readBack.load (file));
    CHECK_NEAR (readBack.getSeconds (today), 60.0, 1.0e-9);
    CHECK_NEAR (readBack.getSeconds (today, PracticeTool::metronome), 60.0, 1.0e-9);
    CHECK_NEAR (readBack.getSeconds (today, PracticeTool::looper), 0.0, 1.0e-9);

    // The ear trainer's keys are still there, untouched.
    const auto root = juce::JSON::parse (file);
    CHECK ((int) root["difficulty"] == 3);
    CHECK (root["lifetimeAsked"].getArray() != nullptr);

    folder.deleteRecursively();
}

LUTHIER_TEST (PracticeRoutine, progressReportsDaysStreaksAccuracyAndTempo)
{
    PracticeStats stats;
    const juce::String today ("2026-09-23");

    CHECK (PracticeStats::daysBefore (today, 1) == "2026-09-22");
    CHECK (PracticeStats::daysBefore ("2026-03-01", 1) == "2026-02-28");
    CHECK (PracticeStats::daysBefore ("2027-01-01", 1) == "2026-12-31");

    stats.addSeconds ("2026-09-20", PracticeTool::metronome, 300.0);
    stats.addSeconds ("2026-09-21", PracticeTool::looper, 120.0);
    stats.addSeconds ("2026-09-22", PracticeTool::metronome, 60.0);
    stats.addTrainerSession ("2026-09-22", PracticeTool::earTrainer, "ear.interval", 10, 7);
    stats.addTrainerSession ("2026-09-23", PracticeTool::earTrainer, "ear.interval", 10, 9);

    // Four days running, ending today.
    CHECK (stats.getStreak (today) == 4);

    // Today not practised yet: the streak stands until the day is over.
    PracticeStats yesterdayOnly;
    yesterdayOnly.addSeconds ("2026-09-22", PracticeTool::metronome, 10.0);
    CHECK (yesterdayOnly.getStreak (today) == 1);

    // A gap breaks it.
    stats.addSeconds ("2026-09-18", PracticeTool::metronome, 60.0);
    CHECK (stats.getStreak (today) == 4);

    const auto recent = stats.getRecentDays (today);
    CHECK (recent.size() == (size_t) PracticeStats::kHistoryDays);
    CHECK (recent.back().date == today && recent.front().date == PracticeStats::daysBefore (today, 89));
    CHECK_NEAR (recent[recent.size() - 4].getTotalSeconds(), 300.0, 1.0e-9);

    const auto totals = stats.getToolTotals (today);
    CHECK_NEAR (totals[(size_t) PracticeTool::metronome], 420.0, 1.0e-9);
    CHECK_NEAR (totals[(size_t) PracticeTool::looper], 120.0, 1.0e-9);
    CHECK (stats.getSessions (today, PracticeTool::earTrainer) == 1);

    const auto accuracy = stats.getAccuracyHistory ("ear.interval");
    CHECK (accuracy.size() == 2);
    CHECK_NEAR (accuracy[0].second, 0.7, 1.0e-9);
    CHECK_NEAR (accuracy[1].second, 0.9, 1.0e-9);
    CHECK (stats.getExercises().contains ("ear.interval"));

    stats.recordCleanTempo (today, "Solo intro", 96.0);
    stats.recordCleanTempo (today, "Solo intro", 92.0);
    CHECK_NEAR (stats.getBestCleanTempo ("Solo intro"), 96.0, 1.0e-9);
    CHECK (stats.getTempoProgress()[0].history.size() == 2);

    // CSV: a header and one row per day with practice, oldest first.
    const auto csv = juce::StringArray::fromLines (stats.toCsv().trimEnd());
    CHECK (csv[0].startsWith ("date,total_minutes,metronome_minutes,looper_minutes"));
    CHECK (csv[0].endsWith ("trainer_sessions"));
    CHECK (csv.size() == 1 + 5);
    CHECK (csv[1].startsWith ("2026-09-18,1.0,1.0"));

    // The var round trip is exact.
    PracticeStats copy;
    CHECK (copy.fromVar (stats.toVar()));
    CHECK (copy == stats);
}

LUTHIER_TEST (PracticeRoutine, trainerSessionsAreRecordedFromTheTrainersOwnScores)
{
    PracticeStats stats;
    PracticeActivityTracker tracker;
    const juce::String today ("2026-09-23");

    EarTrainer ear;
    ear.setExercise (EarTrainer::Exercise::interval);
    ear.setAdaptive (false);

    juce::Random random (42);
    std::array<int, EarTrainer::kMaxNotesInQuestion> notes {};
    std::array<double, EarTrainer::kMaxNotesInQuestion> offsets {};

    for (int q = 0; q < 5; ++q)
    {
        ear.nextQuestion (random, notes.data(), offsets.data(), (int) notes.size());
        ear.answer (q < 3 ? ear.getCorrectChoice() : (ear.getCorrectChoice() + 1) % juce::jmax (2, ear.getChoices().size()));
    }

    tracker.recordEarTrainer (ear, stats, today);
    CHECK (stats.getSessions (today, PracticeTool::earTrainer) == 1);

    const auto accuracy = stats.getAccuracyHistory ("ear.interval");
    CHECK (accuracy.size() == 1);
    CHECK_NEAR (accuracy[0].second, (double) ear.getCorrect() / 5.0, 1.0e-9);

    // Nothing new asked: nothing new recorded.
    tracker.recordEarTrainer (ear, stats, today);
    CHECK (stats.getSessions (today, PracticeTool::earTrainer) == 1);

    ScaleTrainer scale;
    scale.setScale (ScaleType::dorian);
    scale.setMode (ScaleTrainer::Mode::quiz);
    CHECK (PracticeActivityTracker::exerciseKey (scale) == "scale.dorian.quiz");
}

LUTHIER_TEST (PracticeRoutine, clearingHistoryEmptiesStatsButKeepsLoopsAndSessions)
{
    // 12.1: "Clear history ... on confirm, empties stats.json without deleting
    // saved loops or sessions." The confirmation is the tab's.
    const auto folder = tempFolder ("LuthierPracticeClear");
    const auto statsFile = folder.getChildFile ("Practice").getChildFile ("stats.json");
    const auto loop = folder.getChildFile ("Loops").getChildFile ("Riff");
    const auto session = folder.getChildFile ("Sessions").getChildFile ("session-2026-09-23.wav");

    loop.createDirectory();
    loop.getChildFile ("loop.json").replaceWithText ("{}");
    session.getParentDirectory().createDirectory();
    session.replaceWithText ("RIFF");

    PracticeStats stats;
    stats.addSeconds ("2026-09-23", PracticeTool::metronome, 100.0);
    stats.recordCleanTempo ("2026-09-23", "Riff", 120.0);

    juce::String error;
    CHECK (stats.save (statsFile, error));

    stats.clear();
    CHECK (stats.isEmpty());
    CHECK (stats.save (statsFile, error));

    PracticeStats readBack;
    CHECK (readBack.load (statsFile));
    CHECK (readBack.isEmpty());

    CHECK (loop.getChildFile ("loop.json").existsAsFile());
    CHECK (session.existsAsFile());

    // A stats.json that is not JSON is not overwritten.
    statsFile.replaceWithText ("{ broken");
    CHECK (! stats.save (statsFile, error));
    CHECK (statsFile.loadFileAsString() == "{ broken");

    folder.deleteRecursively();
}

//==============================================================================
// Defaults, session setup, library, empty states (11.2, 11.4)
//==============================================================================

LUTHIER_TEST (PracticeRoutine, defaultsSurviveAReopenAndStartTheMetronomeAtThem)
{
    // 12.1: "Set a default tempo, close and reopen the plugin, assert the
    // metronome starts at it." Reopening is a fresh load into a fresh metronome.
    const auto folder = tempFolder ("LuthierPracticeDefaults");
    const auto file = folder.getChildFile ("defaults.json");

    PracticeDefaults defaults;
    defaults.metronomeTempo = 97.0;
    defaults.timeSigNumerator = 7;
    defaults.timeSigDenominator = 8;
    defaults.subdivision = ClickSubdivision::sixteenth;
    defaults.sound = ClickSound::cowbell;
    defaults.accents = { BeatAccent::accent, BeatAccent::ghost, BeatAccent::normal };
    defaults.overdubMode = LayerMode::playOnce;
    defaults.trainerKey = 7;
    defaults.scaleSet = { ScaleType::mixolydian, ScaleType::blues };
    defaults.backingFolder = folder.getFullPathName();
    defaults.shuffle = true;
    defaults.extra.set ("future_default", "kept");

    juce::String error;
    CHECK_MSG (defaults.save (file, error), error);

    PracticeDefaults reopened;
    CHECK_MSG (PracticeDefaults::load (file, reopened, error), error);
    CHECK (reopened == defaults);

    Tools fresh;
    CHECK (reopened.applyTo (fresh.targets()).isEmpty());
    CHECK_NEAR (fresh.metronome.getTempo(), 97.0, 1.0e-9);
    CHECK (fresh.metronome.getTimeSignature().numerator == 7);
    CHECK (fresh.metronome.getSubdivision() == ClickSubdivision::sixteenth);
    CHECK (fresh.metronome.getSound() == ClickSound::cowbell);
    CHECK (fresh.metronome.getBeatAccent (1) == BeatAccent::ghost);
    CHECK (fresh.looper.getLayer (fresh.looper.getActiveLayer()).getMode() == LayerMode::playOnce);
    CHECK (fresh.scaleTrainer.getKey() == 7 && fresh.scaleTrainer.getScale() == ScaleType::mixolydian);

    // Defaults start nothing: the transport stays in the drawer (11.3).
    CHECK (! fresh.metronome.isEnabled());

    folder.deleteRecursively();
}

LUTHIER_TEST (PracticeRoutine, sessionSetupStatesTheRingSizeInPlainWords)
{
    // practice-tools 8: 60 minutes at 48 kHz stereo float32 is ~1.4 GB.
    CHECK (SessionRecorderSetup::estimateBytes (60.0) == (juce::int64) 1382400000);
    CHECK (SessionRecorderSetup::describeBytes (1382400000) == "1.4 GB");
    CHECK (SessionRecorderSetup::describeBytes (345600000) == "346 MB");

    SessionRecorderSetup setup;
    CHECK (setup.getSizeWarning().contains ("1.4 GB"));
    CHECK (setup.getSizeWarning().contains ("off by default"));

    setup.ringMinutes = 5.0;
    setup.recordMidi = false;
    setup.autoSaveOnStop = true;

    const auto copy = SessionRecorderSetup::fromVar (setup.toVar());
    CHECK (copy == setup);

    // Neither audio nor MIDI is the recorder being off, not a setting.
    auto neither = setup;
    neither.recordAudio = false;
    CHECK (SessionRecorderSetup::fromVar (neither.toVar()).recordAudio);

    // A small ring really is allocated by the recorder.
    SessionRecorder recorder;
    setup.ringMinutes = 1.0;
    CHECK (setup.applyTo (recorder, 48000.0));
    CHECK_NEAR (recorder.getCapacityMinutes(), 1.0, 0.01);
}

LUTHIER_TEST (PracticeRoutine, theLibraryListsLoopsSessionsAndRecentTabs)
{
    const auto folder = tempFolder ("LuthierPracticeLibrary");
    const auto loops = folder.getChildFile ("Loops");
    const auto sessions = folder.getChildFile ("Sessions");

    for (auto name : { "Riff A", "Riff B" })
    {
        loops.getChildFile (name).createDirectory();
        loops.getChildFile (name).getChildFile ("loop.json").replaceWithText ("{}");
    }

    loops.getChildFile ("Not a loop").createDirectory();
    sessions.getChildFile ("tmp").createDirectory();
    sessions.getChildFile ("tmp").getChildFile ("ring.wav").replaceWithText ("RIFF");
    sessions.getChildFile ("session-1.wav").replaceWithText ("RIFF");

    const auto loopItems = PracticeLibrary::listLoops (loops);
    CHECK (loopItems.size() == 2);

    const auto sessionItems = PracticeLibrary::listSessions (sessions);
    CHECK (sessionItems.size() == 1 && sessionItems[0].name == "session-1");

    // Deleting refuses a folder that is not a loop.
    PracticeLibrary::Item stray;
    stray.file = loops.getChildFile ("Not a loop");
    CHECK (! PracticeLibrary::deleteLoop (stray));
    CHECK (stray.file.isDirectory());

    CHECK (PracticeLibrary::deleteLoop (loopItems[0]));
    CHECK (PracticeLibrary::listLoops (loops).size() == 1);

    // Recent tabs: newest first, no duplicates, at most ten.
    PracticeLibrary library;

    for (int i = 0; i < 12; ++i)
        library.noteTabOpened (folder.getChildFile ("tab" + juce::String (i) + ".gp"));

    library.noteTabOpened (folder.getChildFile ("tab5.gp"));
    CHECK (library.getRecentTabs().size() == PracticeLibrary::kMaxRecentTabs);
    CHECK (library.getRecentTabs()[0].getFileName() == "tab5.gp");

    juce::String error;
    const auto libraryFile = folder.getChildFile ("library.json");
    CHECK (library.save (libraryFile, error));

    PracticeLibrary reopened;
    CHECK (reopened.load (libraryFile));
    CHECK (reopened.getRecentTabs() == library.getRecentTabs());

    // None of the tab files exist, so pruning empties the list.
    reopened.pruneMissing();
    CHECK (reopened.getRecentTabs().isEmpty());

    folder.deleteRecursively();
}

LUTHIER_TEST (PracticeRoutine, emptyStatesReadAsTheSpecWritesThem)
{
    CHECK (juce::String (PracticeEmptyStates::noStats)
             == "No practice recorded yet. The metronome, looper and trainers all count time once you start them.");
    CHECK (juce::String (PracticeEmptyStates::noUserRoutines) == "Your own routines appear here.");
    CHECK (juce::String (PracticeEmptyStates::noLoops) == "Saved loops appear here");
}

//==============================================================================
// Count-in, loop regions, speed trainer (2, 3, 6)
//==============================================================================

LUTHIER_TEST (PracticeRoutine, countInsCountTheBarInTheMetronomesTempo)
{
    Metronome metronome;
    metronome.setTempo (120.0);
    metronome.setTimeSignature (4, 4);

    const auto countIn = PracticeCountIn::fromMetronome (metronome, 1);
    CHECK (countIn.getCounts() == 4);
    CHECK_NEAR (countIn.getSeconds(), 2.0, 1.0e-9);
    CHECK (countIn.getCountAt (0.0) == 1);
    CHECK (countIn.getCountAt (0.5) == 2);
    CHECK (countIn.getCountAt (1.99) == 4);
    CHECK (countIn.getCountAt (2.0) == 0);

    // 6/8: six eighth-note counts, three quarter notes long.
    metronome.setTimeSignature (6, 8);
    const auto compound = PracticeCountIn::fromMetronome (metronome, 2);
    CHECK (compound.getCounts() == 12);
    CHECK_NEAR (compound.getBeats(), 6.0, 1.0e-9);
    CHECK (compound.getCountAt (0.25 * 7.0) == 2);   // the second bar's second eighth
}

LUTHIER_TEST (PracticeRoutine, loopRegionsWrapAndCountTheirPasses)
{
    PracticeLoopRegion region;
    region.start = 10.0;
    region.end = 14.0;
    region.enabled = true;

    CHECK_NEAR (region.wrap (5.0), 5.0, 1.0e-9);     // not reached yet
    CHECK_NEAR (region.wrap (12.0), 12.0, 1.0e-9);
    CHECK_NEAR (region.wrap (14.0), 10.0, 1.0e-9);
    CHECK_NEAR (region.wrap (19.0), 11.0, 1.0e-9);
    CHECK (region.countWraps (0.0, 13.0) == 0);
    CHECK (region.countWraps (0.0, 18.5) == 2);

    region.enabled = false;
    CHECK_NEAR (region.wrap (19.0), 19.0, 1.0e-9);

    CHECK (PracticeLoopRegion::fromVar (region.toVar()) == region);
}

LUTHIER_TEST (PracticeRoutine, theSpeedTrainerClimbsUntilAPassMissesNotes)
{
    SpeedTrainer::Settings settings;
    settings.phrase = "Solo intro";
    settings.startBpm = 100.0;
    settings.stepPercent = 10.0;
    settings.maxBpm = 200.0;
    settings.cleanPassesPerStep = 1;

    SpeedTrainer trainer;
    trainer.start (settings);
    CHECK_NEAR (trainer.getTempo(), 100.0, 1.0e-9);

    CHECK_NEAR (trainer.passCompleted (0), 110.0, 1.0e-9);
    CHECK_NEAR (trainer.passCompleted (0), 121.0, 1.0e-9);

    // 6: "until the user misses notes": the pass at 121 misses, so the run
    // ends and 110, the fastest pass played cleanly, is the result.
    CHECK_NEAR (trainer.passCompleted (2), 110.0, 1.0e-9);
    CHECK (trainer.isFinished());
    CHECK_NEAR (trainer.getBestCleanTempo(), 110.0, 1.0e-9);
    CHECK (trainer.getPass() == 3);

    PracticeStats stats;
    trainer.recordResult (stats, "2026-09-23");
    CHECK_NEAR (stats.getBestCleanTempo ("Solo intro"), 110.0, 1.0e-9);

    // Two clean passes per step, a miss allowance, and a ceiling.
    settings.cleanPassesPerStep = 2;
    settings.allowedMisses = 1;
    settings.maxBpm = 105.0;
    trainer.start (settings);
    CHECK_NEAR (trainer.passCompleted (1), 100.0, 1.0e-9);
    CHECK_NEAR (trainer.passCompleted (0), 105.0, 1.0e-9);   // 110, capped
    trainer.passCompleted (0);
    trainer.passCompleted (0);
    CHECK (trainer.isFinished());
    CHECK_NEAR (trainer.getBestCleanTempo(), 105.0, 1.0e-9);

    // It drives the metronome and the backing track's tempo ratio.
    Metronome metronome;
    trainer.applyTo (metronome);
    CHECK_NEAR (metronome.getTempo(), 105.0, 1.0e-9);
    CHECK_NEAR (trainer.getTempoRatioFor (140.0), 0.75, 1.0e-9);
    CHECK_NEAR (trainer.getTempoRatioFor (20.0), 2.0, 1.0e-9);

    CHECK (SpeedTrainer::Settings::fromVar (settings.toVar()) == settings);
}
