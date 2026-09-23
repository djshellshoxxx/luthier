#pragma once

/*  Practice routines (practice-tools.md 11.2, "Routines").

    "A named, ordered list of exercises ... Starting a routine drives the
    drawer: it sets the tool, loads the settings, runs the timer and advances."

    Three pieces:

      - PracticeRoutine: the data. Entries name a drawer tool, its settings, and
        a duration or a repetition count. Saved as JSON under
        `~/Documents/Luthier/Practice/Routines/`.
      - PracticeRoutineLibrary: the three factory routines, which are always
        present (11.4: "this state cannot occur"), plus the user's own.
      - PracticeRoutineRunner: plays a routine through the tools that already
        exist - Metronome, Looper, BackingTrackPlayer, ScaleTrainer, EarTrainer,
        ProgressionLooper - through their own setters, and tells the drawer
        which tab to show. Time is handed to it by the caller's timer rather
        than read from a clock, so it is deterministic and testable, and it
        never runs on the audio thread.

    Settings are stored per tool as a JSON object with stable snake_case keys
    ("dotted_eighth", "harmonic_minor"), never enum indices or display names,
    so reordering an enum or renaming a menu item cannot repoint a saved
    routine. Keys a tool does not know are kept and written back
    (file-formats 0.3).
*/

#include "Metronome.h"
#include "Looper.h"
#include "BackingTrack.h"
#include "Trainers.h"

#include <functional>
#include <vector>

namespace luthier
{

//==============================================================================
/** The drawer's tools, in its tab order (practice-tools 9):
    METRO | LOOP | TRACK | SCALE | EAR | TAB | PROG | SESSION. The value is the
    drawer's tab index. */
enum class PracticeTool
{
    metronome = 0, looper, backingTrack, scaleTrainer, earTrainer, tabReader, progression, sessionRecorder,
    numTools
};

/** "metronome", "backing_track", ... as written to files. */
const char* getPracticeToolKey (PracticeTool tool) noexcept;

/** "METRO", "LOOP", ... the drawer's tab label. */
const char* getPracticeToolTabLabel (PracticeTool tool) noexcept;

bool parsePracticeTool (const juce::String& key, PracticeTool& result);

//==============================================================================
/** The stable file keys of the existing tools' enums. */
namespace practicekeys
{
    const char* subdivision (ClickSubdivision) noexcept;     ///< "quarter", "dotted_eighth"
    const char* sound (ClickSound) noexcept;                 ///< "wood_block", "side_stick"
    const char* accent (BeatAccent) noexcept;                ///< "silent", "ghost", "normal", "accent"
    const char* scale (ScaleType) noexcept;                  ///< "ionian", "harmonic_minor", "blues"
    const char* scaleMode (ScaleTrainer::Mode) noexcept;     ///< "explore", "quiz", "interval", "chord_tone"
    const char* earExercise (EarTrainer::Exercise) noexcept; ///< "interval", "chord_quality", "progression"
    const char* layerMode (LayerMode) noexcept;              ///< "overdub", "replace", "play_once"

    bool parse (const juce::String&, ClickSubdivision&);
    bool parse (const juce::String&, ClickSound&);
    bool parse (const juce::String&, BeatAccent&);
    bool parse (const juce::String&, ScaleType&);
    bool parse (const juce::String&, ScaleTrainer::Mode&);
    bool parse (const juce::String&, EarTrainer::Exercise&);
    bool parse (const juce::String&, LayerMode&);
}

//==============================================================================
/** The tools a routine drives. The processor already owns one of each; build
    this from its getters (getMetronome(), getLooper(), getBackingTrack(),
    getScaleTrainer(), getEarTrainer(), getProgressionLooper(),
    getSessionRecorder()). A null entry is a tool the caller does not have, and
    applying settings to it is reported rather than crashing. */
struct PracticeTargets
{
    Metronome* metronome = nullptr;
    Looper* looper = nullptr;
    BackingTrackPlayer* backingTrack = nullptr;
    ScaleTrainer* scaleTrainer = nullptr;
    EarTrainer* earTrainer = nullptr;
    ProgressionLooper* progression = nullptr;
    SessionRecorder* sessionRecorder = nullptr;
};

//==============================================================================
/** One step of a routine. */
struct RoutineEntry
{
    PracticeTool tool = PracticeTool::metronome;
    juce::String title;
    juce::String instructions;

    /** Seconds on the timer. Used when `repetitions` is 0. */
    double durationSeconds = 60.0;

    /** When above 0, the entry advances after this many repetitions reported
        by the drawer (a pass through a loop, a quiz answered) instead of on
        the timer. */
    int repetitions = 0;

    /** Bars of metronome count-in before the timer starts. */
    int countInBars = 0;

    /*  The tool's settings, as a JSON object. Keys per tool:

          metronome        tempo_bpm, time_sig ("7/8"), subdivision, sound,
                           accents [per beat], silent_bar_period, level_db,
                           progressive { from_bpm, to_bpm, bars }
          looper           layer_mode
          backing_track    file, level_db, tempo_ratio, pitch_semitones,
                           loop_start_s, loop_end_s, loop, play
          scale_trainer    key ("A"), scale, mode
          ear_trainer      exercise, difficulty, adaptive
          tab_reader       file, tempo_percent, loop_start, loop_end
          progression      text ("Am - F - C - G x4")

        Any entry may also carry tempo_bpm (the practice tempo, set on the
        metronome) and click (true keeps the metronome running under a tool
        that is not the metronome). */
    juce::var settings;

    juce::NamedValueSet extra;

    bool isTimed() const noexcept { return repetitions <= 0; }

    bool operator== (const RoutineEntry& other) const;
    bool operator!= (const RoutineEntry& other) const { return ! (*this == other); }
};

//==============================================================================
struct PracticeRoutine
{
    static constexpr int kSchemaVersion = 1;
    static constexpr const char* kMagic = "luthier.routine";
    static constexpr const char* kFileExtension = ".json";   // 11.2: "Routines/*.json"

    juce::String name;
    juce::String author;
    juce::String notes;

    /** True for the three that ship. Not stored: a factory routine is one
        that came from the built-in table. */
    bool factory = false;

    std::vector<RoutineEntry> entries;

    juce::NamedValueSet extra;

    /** Timer seconds plus count-ins; repetition entries count as nothing, as
        their length is the player's. */
    double getTotalSeconds() const;

    juce::var toVar() const;

    /** False, with a reason naming the field, when the object is not a routine.
        `result` is only written on success. */
    static bool fromVar (const juce::var& state, PracticeRoutine& result, juce::String& error);

    /** Atomic: written beside the target as `.tmp`, then renamed over it
        (file-formats 13). */
    bool saveTo (const juce::File& file, juce::String& error) const;
    static bool loadFrom (const juce::File& file, PracticeRoutine& result, juce::String& error);

    bool operator== (const PracticeRoutine& other) const;
    bool operator!= (const PracticeRoutine& other) const { return ! (*this == other); }
};

//==============================================================================
class PracticeRoutineLibrary
{
public:
    /** 11.2's three, in this order. factory-content.md does not list them yet
        (the spec says it "gains them"); these are that content. */
    static std::vector<PracticeRoutine> createFactoryRoutines();

    /** `~/Documents/Luthier/Practice/Routines`. */
    static juce::File getUserDirectory();

    explicit PracticeRoutineLibrary (const juce::File& userDirectory = getUserDirectory());

    /** Rebuilds the factory routines and re-reads the user folder. Files that
        do not load are skipped and named in getLoadErrors(). */
    void refresh();

    int getNumRoutines() const noexcept { return (int) routines.size(); }
    const PracticeRoutine& getRoutine (int index) const;
    int indexOf (const juce::String& name) const;

    int getNumUserRoutines() const noexcept;

    /** Writes a user routine and adds or replaces it by name. A factory
        routine's name is refused: save a copy under another name instead. */
    bool save (const PracticeRoutine& routine, juce::String& error);

    /** Deletes a user routine's file. Factory routines cannot be removed. */
    bool remove (const juce::String& name);

    const juce::StringArray& getLoadErrors() const noexcept { return loadErrors; }

    juce::File getFileFor (const juce::String& routineName) const;

private:
    juce::File userDirectory;
    std::vector<PracticeRoutine> routines;
    juce::StringArray loadErrors;
};

//==============================================================================
/** Loads an entry's settings into the tools. Message thread: loading a backing
    track touches the disk. Returns what could not be applied, one line each;
    empty means everything took. Tab-reader settings have no model to go into
    (the tab reader lives in the drawer) and are left for the drawer to read
    from the entry. */
juce::StringArray applyRoutineSettings (const RoutineEntry& entry, const PracticeTargets& targets);

//==============================================================================
/** Runs a routine (11.2, 12.1 "A routine drives the drawer"). */
class PracticeRoutineRunner
{
public:
    enum class Phase { idle = 0, countIn, running, paused, finished };

    explicit PracticeRoutineRunner (PracticeTargets targets = {});

    void setTargets (const PracticeTargets& newTargets) { targets = newTargets; }

    /** Starts at the first entry: applies its settings, starts its count-in or
        timer, and reports it through onEntryStarted. False for an empty routine. */
    bool start (const PracticeRoutine& routine);

    /** Stops the routine and the transport it started. */
    void stop();

    void pause();
    void resume();

    /** Time passes. Call from the message thread's timer with the seconds since
        the last call; one long call crosses as many entries as it covers. */
    void advance (double elapsedSeconds);

    /** The drawer reports a repetition (a clean pass, an answered question).
        Advances a repetition-counted entry when its count is reached. */
    void completeRepetition();

    /** Jump to the next or previous entry, applying it. */
    bool next();
    bool previous();

    //==========================================================================
    Phase getPhase() const noexcept { return phase; }
    bool isActive() const noexcept { return phase == Phase::countIn || phase == Phase::running || phase == Phase::paused; }

    const PracticeRoutine& getRoutine() const noexcept { return routine; }
    int getEntryIndex() const noexcept { return entryIndex; }
    const RoutineEntry* getCurrentEntry() const noexcept;

    /** The tab the drawer should show. */
    PracticeTool getActiveTool() const noexcept;

    double getEntryElapsedSeconds() const noexcept { return entryElapsed; }

    /** Seconds left on a timed entry's timer, 0 for a repetition entry. */
    double getEntryRemainingSeconds() const noexcept;

    int getRepetitionsDone() const noexcept { return repetitionsDone; }
    double getCountInRemainingSeconds() const noexcept { return countInRemaining; }

    /** Lines from the last settings load that did not take. */
    const juce::StringArray& getLastApplyProblems() const noexcept { return lastProblems; }

    //==========================================================================
    /** An entry has started (after its settings loaded): the drawer switches
        to getActiveTool(). */
    std::function<void (int entryIndex)> onEntryStarted;

    /** A count-in has finished and the timer is running. */
    std::function<void (int entryIndex)> onTimerStarted;

    std::function<void()> onFinished;

private:
    void beginEntry (int index);
    void leaveEntry();
    void finish();
    void startTimer();
    double countInSecondsFor (const RoutineEntry& entry) const;

    PracticeTargets targets;
    PracticeRoutine routine;

    Phase phase = Phase::idle;
    Phase phaseBeforePause = Phase::idle;

    int entryIndex = -1;
    double entryElapsed = 0.0;
    double countInRemaining = 0.0;
    int repetitionsDone = 0;

    juce::StringArray lastProblems;
};

//==============================================================================
namespace practicefiles
{
    /** `~/Documents/Luthier/Practice` (practice-tools 10). */
    juce::File getPracticeDirectory();

    /** Writes JSON atomically: `<name>.tmp` beside the target, flushed, then
        one rename over it (file-formats 13). LF line endings. */
    bool writeJson (const juce::File& file, const juce::var& data, juce::String& error);

    /** Reads a JSON file into `result`. False with a reason otherwise. */
    bool readJson (const juce::File& file, juce::var& result, juce::String& error);

    /** Equality for settings objects and unknown-field sets: by JSON text,
        because juce::var compares objects by identity. */
    bool sameJson (const juce::var& a, const juce::var& b);
    bool sameExtras (const juce::NamedValueSet& a, const juce::NamedValueSet& b);
}

} // namespace luthier
