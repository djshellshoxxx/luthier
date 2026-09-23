#pragma once

/*  Tempo tools for practice: the count-in, loop regions, and the speed
    trainer (practice-tools.md 2, 3, 6 and 11.2).

    None of these makes sound or touches the audio thread. The count-in says
    how long a count lasts and which count is sounding; the metronome that
    clicks it is the existing one. A loop region is a start and an end with
    wrap arithmetic, applied to the backing track through its own loop
    setters. The speed trainer is 6's "Speed trainer mode: play a section,
    incrementally increase tempo N% each pass until the user misses notes",
    and its result is what 11.2's tempo progress records per phrase.
*/

#include "PracticeRoutine.h"

namespace luthier
{

class PracticeStats;

//==============================================================================
/** A metronome count-in (practice-tools 6, "count-in"; the looper default in
    11.2). Counts are the time signature's beats: six eighths in 6/8. */
struct PracticeCountIn
{
    int bars = 1;
    int numerator = 4;
    int denominator = 4;
    double bpm = 120.0;           ///< Quarter notes per minute, as Metronome::getTempo.

    /** The count-in the metronome would click now: its tempo and signature. */
    static PracticeCountIn fromMetronome (const Metronome& metronome, int bars);

    int getCounts() const noexcept { return juce::jmax (0, bars) * juce::jmax (1, numerator); }

    /** Quarter-note beats the count-in lasts. */
    double getBeats() const noexcept;
    double getSeconds() const noexcept;
    double getSecondsPerCount() const noexcept;

    /** 1-based count sounding `elapsedSeconds` in ("one, two, three, four"),
        or 0 once the count-in is over. */
    int getCountAt (double elapsedSeconds) const noexcept;

    bool isFinishedAt (double elapsedSeconds) const noexcept { return elapsedSeconds >= getSeconds() - 1.0e-9; }
};

//==============================================================================
/** A section to loop, in any unit the caller keeps (seconds for the backing
    track, beats for the tab reader). */
struct PracticeLoopRegion
{
    double start = 0.0;
    double end = 0.0;
    bool enabled = false;

    bool isValid() const noexcept { return end > start; }
    double getLength() const noexcept { return juce::jmax (0.0, end - start); }

    bool contains (double position) const noexcept { return position >= start && position < end; }

    /** Where `position` plays when the region loops: positions at or past the
        end come back round from the start. Positions before the start are
        left alone (playback has not reached the region yet). */
    double wrap (double position) const noexcept;

    /** How many times playback from `from` to `to` passes the loop's end. */
    int countWraps (double from, double to) const noexcept;

    /** 3 "Section marker support": the region from one marker to the next,
        or to the end of the track when `toMarker` is -1. Invalid when a
        marker index is out of range. */
    static PracticeLoopRegion betweenMarkers (const BackingTrackPlayer& player, int fromMarker, int toMarker = -1);

    /** Sets the backing track's loop points (which it snaps to zero
        crossings, 3) and loop switch. Message thread. */
    void applyTo (BackingTrackPlayer& player) const;

    juce::var toVar() const;
    static PracticeLoopRegion fromVar (const juce::var& state);

    bool operator== (const PracticeLoopRegion& o) const noexcept
    {
        return start == o.start && end == o.end && enabled == o.enabled;
    }
};

//==============================================================================
/** The tempo ladder (practice-tools 6, "Speed trainer"). */
class SpeedTrainer
{
public:
    struct Settings
    {
        juce::String phrase;          ///< What 11.2's tempo progress files the result under.
        double startBpm = 80.0;
        double stepPercent = 5.0;     ///< "increase tempo N% each pass"
        double maxBpm = 240.0;
        int allowedMisses = 0;        ///< Misses a pass may have and still count as clean.
        int cleanPassesPerStep = 1;   ///< Clean passes needed before the tempo rises.

        juce::var toVar() const;
        static Settings fromVar (const juce::var& state);

        bool operator== (const Settings& o) const noexcept
        {
            return phrase == o.phrase && startBpm == o.startBpm && stepPercent == o.stepPercent
                && maxBpm == o.maxBpm && allowedMisses == o.allowedMisses
                && cleanPassesPerStep == o.cleanPassesPerStep;
        }
    };

    void start (const Settings& settings);
    void stop() noexcept { running = false; }

    bool isRunning() const noexcept { return running; }

    /** True once a pass has missed notes or the ceiling has been reached. */
    bool isFinished() const noexcept { return finished; }

    const Settings& getSettings() const noexcept { return settings; }

    double getTempo() const noexcept { return tempo; }
    int getPass() const noexcept { return pass; }
    int getCleanPassesAtThisTempo() const noexcept { return cleanAtTempo; }

    /** The fastest tempo played cleanly so far; 0 before any clean pass. */
    double getBestCleanTempo() const noexcept { return bestClean; }

    /** Scores a pass through the section. A clean pass (no more misses than
        allowed) counts toward the next step; once enough clean passes are in,
        the tempo rises by stepPercent, up to maxBpm. A pass with misses ends
        the run (6: "until the user misses notes"), and so does a clean pass at
        the ceiling. Returns the tempo for the next pass. */
    double passCompleted (int misses);

    /** Sets the metronome to the trainer's tempo. */
    void applyTo (Metronome& metronome) const;

    /** For a backing track recorded at `trackBpm`: the tempo ratio that plays
        it at the trainer's tempo, within the player's 25-200% (3). */
    double getTempoRatioFor (double trackBpm) const noexcept;

    /** Files the best clean tempo under the phrase, if there was one. */
    void recordResult (PracticeStats& stats, const juce::String& date) const;

private:
    Settings settings;
    double tempo = 80.0;
    double bestClean = 0.0;
    int pass = 0;
    int cleanAtTempo = 0;
    bool running = false;
    bool finished = false;
};

} // namespace luthier
