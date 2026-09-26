#pragma once

/*  Following the chords (jam-mode.md 3).

    - JamChordFollower: the Live source. It owns its own ChordDetector (the
      same class, templates, 30 ms burst window and confidence floor as the
      rhythm engine) and turns note-ons into chord changes, each stamped with
      the sample of its first note and the sample its burst closed. Only a
      detection with two or more pitch classes above the floor is a chord;
      single notes are melody and an Unknown detection changes nothing.
    - JamPredictor: the last 32 bar-aligned changes. A cycle of 1, 2, 4, 8 or
      16 bars heard twice in full predicts the next change (3.3); a
      contradicting chord clears the history, so two more clean cycles are
      needed.
    - JamChordMap: a tune's chords as a sorted array of at most 1024 changes
      {ppq, root, bass, template, section}, built on the message thread from
      the tune's timeline and handed to the engine by pointer swap.

    Follower and predictor are audio-thread objects: fixed arrays, no
    allocation, no locks.
*/

#include "../Rhythm/ChordDetector.h"
#include <array>
#include <memory>
#include <vector>

namespace luthier
{

class TuneTimeline;
struct Tune;

//==============================================================================
/** A section's optional jam hint (jam-mode 11): `"jam": {"intensity": 1-5,
    "fill_into": true}`. Intensity 0 means none. */
struct JamSectionHint
{
    int intensity = 0;
    bool fillInto = true;
};

struct JamChordMap
{
    static constexpr int kMaxEntries = 1024;
    static constexpr int kMaxSections = 64;

    struct Entry
    {
        double ppq = 0.0;
        int root = -1, bass = -1, templateIndex = -1, sectionIndex = 0;
        bool sectionStart = false;

        ChordSymbol toSymbol() const noexcept
        {
            ChordSymbol s;
            s.root = root;
            s.bass = bass;
            s.templateIndex = templateIndex;
            s.confidence = 1.0;
            return s;
        }
    };

    std::array<Entry, kMaxEntries> entries {};
    int count = 0;
    double lengthPpq = 0.0;
    bool loop = false;
    bool truncated = false;
    std::array<JamSectionHint, kMaxSections> hints {};

    /** The entry in force at `ppq` (wrapped when looping), or -1. */
    int findIndexAt (double ppq) const noexcept;
    ChordSymbol chordAt (double ppq) const noexcept;

    /** The first change strictly after `afterPpq` and before `beforePpq`
        (tune positions, unwrapped). */
    bool nextChange (double afterPpq, double beforePpq, Entry& out, double& atPpq) const noexcept;

    /** Builds the map from a tune's timeline: the chord channel's note-ons,
        detected with the rhythm engine's detector at each position. Over 1024
        changes it is truncated (13) and `truncated` is set. Message thread. */
    static std::unique_ptr<JamChordMap> fromTimeline (const TuneTimeline& timeline, bool loop,
                                                      const std::vector<JamSectionHint>& hints = {});
};

//==============================================================================
class JamChordFollower
{
public:
    struct Change
    {
        ChordSymbol chord;
        int64_t firstNoteSample = 0;
        int64_t detectionSample = 0;
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn (int note, int64_t sample) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;

    /** Closes a burst whose window ends at or before `upTo`. Returns true with
        the change when it detected a chord (2+ pitch classes above the floor);
        a closed burst that was melody or Unknown returns false. */
    bool poll (int64_t upTo, Change& out) noexcept;

    bool isBurstOpen() const noexcept { return burstOpen; }
    int64_t getBurstCloseSample() const noexcept { return burstStart + windowSamples; }

    /** The detector's own rule, as JM-09 checks it. */
    static bool isChord (const ChordSymbol& s) noexcept;

private:
    ChordDetector detector;
    std::array<int, 24> held {};
    int numHeld = 0;
    bool burstOpen = false;
    int64_t burstStart = 0, windowSamples = 1440;
};

//==============================================================================
class JamPredictor
{
public:
    static constexpr int kHistory = 32;

    void reset() noexcept;

    /** Records a live change at its quantised position. A repeat of the last
        recorded chord is not a change. */
    void record (double ppq, const ChordSymbol& chord) noexcept;

    /** 3.3: a contradicting chord. Clears the history from `ppq` on, so two
        clean cycles are needed again. */
    void contradict (double ppq, const ChordSymbol& chord) noexcept;

    /** At a bar line: looks for a cycle of 1, 2, 4, 8 or 16 bars heard twice. */
    void onBarLine (double barStartPpq, double barLengthPpq) noexcept;

    bool isPredicting() const noexcept { return cycleBars > 0; }
    int getCycleBars() const noexcept { return cycleBars; }

    /** The predicted change strictly after `afterPpq` and before `beforePpq`. */
    bool predictNext (double afterPpq, double beforePpq, double& atPpq, ChordSymbol& chord) const noexcept;

private:
    struct Entry { double ppq = 0.0; ChordSymbol chord; };

    const Entry& at (int i) const noexcept { return ring[(size_t) ((start + i) % kHistory)]; }
    bool chordAt (double ppq, ChordSymbol& out) const noexcept;

    std::array<Entry, kHistory> ring {};
    int start = 0, count = 0;
    double historyStart = 0.0;
    int cycleBars = 0;
    double cycleLength = 0.0;
};

} // namespace luthier
