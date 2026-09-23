#pragma once

/*  Practice progress (practice-tools.md 11.2, "Progress").

    What the PRACTICE tab reads: time per day for the last 90 days, a per-tool
    breakdown (metronome minutes, looper minutes, trainer sessions), trainer
    accuracy over time per exercise, the best clean tempo for each looped
    phrase, and a streak. Read-only on the tab, with CSV export and a clear
    that the tab confirms.

    It lives in `~/Documents/Luthier/Practice/stats.json` (practice-tools 10),
    which the ear trainer already writes its own statistics into
    (EarTrainer::saveStats). The two share the file by key: the history is the
    `practice_history` object, and saving reads the file first and replaces
    only that key, so the ear trainer's keys survive a save of the history.
    The other direction needs EarTrainer::saveStats to do the same; see the
    change requested alongside this model.

    Dates are local calendar days, "yyyy-mm-dd", because a streak is about the
    player's days, not UTC's.
*/

#include "PracticeRoutine.h"

#include <array>
#include <map>
#include <vector>

namespace luthier
{

//==============================================================================
/** One calendar day of practice. */
struct PracticeDayRecord
{
    juce::String date;   ///< "yyyy-mm-dd"

    /** Seconds each tool was running, by PracticeTool. */
    std::array<double, (size_t) PracticeTool::numTools> seconds {};

    /** Trainer sessions finished, by PracticeTool. */
    std::array<int, (size_t) PracticeTool::numTools> sessions {};

    /** Per exercise ("ear.interval", "scale.dorian.quiz"): questions asked and
        answered right. */
    std::map<juce::String, std::pair<int, int>> accuracy;

    double getTotalSeconds() const noexcept;
    int getTotalSessions() const noexcept;
    bool isEmpty() const noexcept;

    bool operator== (const PracticeDayRecord& other) const;
};

/** Tempo progress for one looped phrase (11.2). */
struct PhraseTempoRecord
{
    juce::String phrase;
    double bestBpm = 0.0;

    /** Every clean tempo recorded, oldest first: (date, bpm). */
    std::vector<std::pair<juce::String, double>> history;

    bool operator== (const PhraseTempoRecord& other) const;
};

//==============================================================================
class PracticeStats
{
public:
    static constexpr int kHistoryDays = 90;
    static constexpr int kSchemaVersion = 1;

    /** The top-level key in stats.json that holds this history. */
    static constexpr const char* kHistoryKey = "practice_history";

    /** "yyyy-mm-dd", local time. */
    static juce::String dateKey (juce::Time when);
    static juce::String today();

    /** The date `days` before `date` ("2026-09-23", 1 -> "2026-09-22"). */
    static juce::String daysBefore (const juce::String& date, int days);

    //==========================================================================
    void addSeconds (const juce::String& date, PracticeTool tool, double seconds);

    /** A trainer session ended: `asked` questions, `correct` right. */
    void addTrainerSession (const juce::String& date, PracticeTool tool, const juce::String& exercise,
                            int asked, int correct);

    /** A phrase was played cleanly at `bpm` (the speed trainer's result). */
    void recordCleanTempo (const juce::String& date, const juce::String& phrase, double bpm);

    //==========================================================================
    double getSeconds (const juce::String& date) const;
    double getSeconds (const juce::String& date, PracticeTool tool) const;
    int getSessions (const juce::String& date, PracticeTool tool) const;

    /** One record per day for the `days` days ending with `lastDate`, oldest
        first, empty days included: the tab's 90-day chart. */
    std::vector<PracticeDayRecord> getRecentDays (const juce::String& lastDate, int days = kHistoryDays) const;

    /** Seconds per tool over the same window, for the breakdown. */
    std::array<double, (size_t) PracticeTool::numTools> getToolTotals (const juce::String& lastDate,
                                                                       int days = kHistoryDays) const;

    /** Accuracy of one exercise per day it was practised, oldest first. */
    std::vector<std::pair<juce::String, double>> getAccuracyHistory (const juce::String& exercise) const;

    /** Every exercise that has a result, sorted. */
    juce::StringArray getExercises() const;

    const std::vector<PhraseTempoRecord>& getTempoProgress() const noexcept { return phrases; }
    double getBestCleanTempo (const juce::String& phrase) const;

    /** Consecutive days with practice, ending today; a day not yet practised
        does not break the streak until it is over, so it counts back from
        yesterday when today is still empty. */
    int getStreak (const juce::String& todayDate) const;

    bool isEmpty() const noexcept { return days.empty() && phrases.empty(); }

    /** 11.2 "Clear history". The tab asks first; this does not. Only the
        history goes: saved loops and sessions live in other folders. */
    void clear();

    //==========================================================================
    /** 11.2 "Export as CSV": one row per day with practice, oldest first.
        date, total and per-tool minutes, trainer sessions. */
    juce::String toCsv() const;

    juce::var toVar() const;
    bool fromVar (const juce::var& history);

    /** Reads the history out of stats.json. A missing file is an empty
        history and returns true; a file that is not JSON returns false and
        leaves the stats as they were. */
    bool load (const juce::File& file = getStatsFile());

    /** Writes the history into stats.json, keeping every other key already in
        the file (the ear trainer's). Atomic. */
    bool save (const juce::File& file, juce::String& error) const;
    bool save() const;

    /** The ear trainer's file: one stats.json (practice-tools 10). */
    static juce::File getStatsFile();

    const std::vector<PracticeDayRecord>& getDays() const noexcept { return days; }

    bool operator== (const PracticeStats& other) const { return days == other.days && phrases == other.phrases; }

private:
    PracticeDayRecord& dayFor (const juce::String& date);
    const PracticeDayRecord* findDay (const juce::String& date) const;

    std::vector<PracticeDayRecord> days;          // sorted by date
    std::vector<PhraseTempoRecord> phrases;
};

//==============================================================================
/** Turns running tools into practice time (12.1, "Run the metronome for 60 s;
    assert stats.json gains 60 s against today"). Owned by whatever runs the
    message-thread timer that also drives the routine runner. */
class PracticeActivityTracker
{
public:
    /** Counts `elapsedSeconds` against each tool whose transport is running:
        the metronome when enabled, the looper when not stopped, the backing
        track when playing. Nothing when the practice panel is closed, because
        the tools are silent then (practice-tools 0.1) and silence is not
        practice. */
    void update (const PracticeTargets& targets, bool practicePanelOpen, double elapsedSeconds,
                 PracticeStats& stats, const juce::String& date) const;

    /** Records the questions a trainer has scored since the last call as one
        session. Call when the player leaves the trainer's tab or a routine
        entry ends. Nothing is recorded when nothing was asked. */
    void recordScaleTrainer (const ScaleTrainer& trainer, PracticeStats& stats, const juce::String& date);
    void recordEarTrainer (const EarTrainer& trainer, PracticeStats& stats, const juce::String& date);

    /** "scale.<scale>.<mode>" and "ear.<exercise>", the keys accuracy is filed under. */
    static juce::String exerciseKey (const ScaleTrainer& trainer);
    static juce::String exerciseKey (const EarTrainer& trainer);

private:
    int scaleAskedSeen = 0, scaleCorrectSeen = 0;
    int earAskedSeen = 0, earCorrectSeen = 0;
};

} // namespace luthier
