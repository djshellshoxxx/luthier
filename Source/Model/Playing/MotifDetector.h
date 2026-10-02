#pragma once

/*  Motif detector (easter-egg: Dueling Banjos).

    Listens to the stream of played note-on pitches and fires once when the
    player performs the recognisable opening phrase of "Dueling Banjos" - the
    pick-up run that resolves up the scale. The match is on relative INTERVALS,
    not absolute pitches, so it triggers in any key and any tuning.

    Real-time contract: this lives on the audio thread (it is fed from
    MidiInterpreter::handleNoteOn). Everything here is allocation-free and
    lock-free - a fixed ring buffer of recent integer intervals and an
    integer edit-distance over a small stack table. No heap, no std::function,
    no locks.

    Why the phrase survives imperfect playing:
      - Transposition: we store successive semitone deltas, so the key is
        irrelevant (SPEC: "relative intervals, not absolute pitches").
      - Timing: a pause longer than `gapSamples` resets the buffer, so a motif
        has to be played as one phrase, but there is no tempo requirement.
      - Wrong / extra / missing note: the reference is matched with a bounded
        edit distance (kMaxCost = 1), so one inserted, dropped or substituted
        interval still triggers.

    Why it does NOT fire mid-song (false-positive resistance):
      - The reference is the major-pentatonic climb to the octave
        (degrees 1 2 3 5 6 8 -> intervals +2 +2 +3 +2 +3). The two minor-third
        skips make it distinct from an ordinary diatonic scale run
        (+2 +2 +1 +2 +2), which differs by two edits and so cannot match.
      - The final resolution must keep the characteristic minor-third leap.
        A one-semitone sharp landing (+4) is accepted as the documented human
        slip, but the common +2 leading-tone continuation is not.
      - The search window is capped at kRefLen + kMaxCost intervals, so the
        phrase can only be recognised as the most recent few notes, never
        assembled out of scattered notes across a passage.
*/

#include <cstdint>
#include <cstdlib>

namespace luthier
{

class MotifDetector
{
public:
    MotifDetector() noexcept { reset(); }

    /** Sets the sample rate so the phrase-reset gap can be expressed in time.
        Call from prepare(). */
    void prepare (double sampleRate) noexcept
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        gapSamples = (int64_t) (kGapSeconds * sr);
        reset();
    }

    /** Clears the rolling buffer. Call on transport reset / allNotesOff. */
    void reset() noexcept
    {
        count = 0;
        head = 0;
        hasLast = false;
        lastPitch = 0;
        lastTimestamp = 0;
        triggered = false;
    }

    /** Feeds one played note-on. `midiNote` is the raw MIDI pitch (0..127),
        `timestamp` an absolute sample position (monotonic within a session).
        Audio thread; allocation-free. */
    void noteOn (int midiNote, int64_t timestamp) noexcept
    {
        // A long pause (or a backwards jump from a loop / seek) ends the phrase:
        // the buffer clears AND this note becomes a fresh anchor, so the large
        // interval spanning the gap is never counted.
        if (hasLast)
        {
            const int64_t dt = timestamp - lastTimestamp;
            if (dt < 0 || dt > gapSamples)
            {
                clearIntervals();
                hasLast = false;
            }
        }

        if (hasLast)
        {
            const int interval = midiNote - lastPitch;
            pushInterval (interval);

            if (matchesReference())
                triggered = true;
        }

        lastPitch = midiNote;
        lastTimestamp = timestamp;
        hasLast = true;
    }

    /** True once the motif has been recognised. Consumes the latch so the
        caller acts exactly once per performance of the phrase. Audio thread. */
    bool consumeTrigger() noexcept
    {
        const bool t = triggered;
        triggered = false;
        if (t)
            clearIntervals();   // require a fresh phrase before it can fire again
        return t;
    }

    /** Non-consuming peek, for tests. */
    bool isTriggered() const noexcept { return triggered; }

    //==========================================================================
    // The reference phrase. Successive semitone intervals of the recognisable
    // "Dueling Banjos" opening call: major-pentatonic degrees 1 2 3 5 6 8.
    static constexpr int  kRefLen  = 5;
    static constexpr int  kRefIntervals[kRefLen] = { 2, 2, 3, 2, 3 };

    // One edit (insertion / deletion / substitution) of tolerance.
    static constexpr int  kMaxCost = 1;

    // Phrase-reset gap, in seconds of silence between note-ons.
    static constexpr double kGapSeconds = 1.6;

    // Only the most recent (kRefLen + kMaxCost) intervals can take part in a
    // match, so the phrase is always a tight, recent run.
    static constexpr int  kWindow  = kRefLen + kMaxCost;      // 6
    static constexpr int  kRingLen = kWindow;

private:
    void clearIntervals() noexcept { count = 0; head = 0; }

    void pushInterval (int interval) noexcept
    {
        ring[head] = interval;
        head = (head + 1) % kRingLen;
        if (count < kRingLen)
            ++count;
    }

    /** The j-th most-recent interval, j = 0 is newest. */
    int recent (int j) const noexcept
    {
        // head points one past the newest sample.
        int idx = head - 1 - j;
        idx %= kRingLen;
        if (idx < 0)
            idx += kRingLen;
        return ring[idx];
    }

    /** Anchored bounded edit distance of the reference against the tail of the
        interval stream. Leading history intervals may be skipped for free (the
        phrase can begin after anything), which makes this a fuzzy-suffix match
        ending at the newest note. Triggers when the cost is <= kMaxCost.

        Integer DP over a fixed (kRefLen+1) x (kWindow+1) table, stack only. */
    bool matchesReference() const noexcept
    {
        const int w = count < kWindow ? count : kWindow;   // history intervals in play
        if (w < kRefLen - kMaxCost)
            return false;                                   // not enough notes yet

        // The hidden phrase is defined as resolving with its characteristic
        // final minor-third leap. Keep the documented one-semitone-sharp slip,
        // but reject the common whole-step continuation that otherwise lands at
        // edit distance 1 and produces false positives in normal playing.
        const int finalInterval = recent (0);
        if (finalInterval < kRefIntervals[kRefLen - 1]
              || finalInterval > kRefIntervals[kRefLen - 1] + 1)
            return false;

        // h[0..w-1] oldest..newest of the window.
        int h[kWindow];
        for (int k = 0; k < w; ++k)
            h[k] = recent (w - 1 - k);

        int dp[kRefLen + 1][kWindow + 1];

        // Skipping any leading history interval is free: the phrase may start
        // anywhere in the window.
        for (int j = 0; j <= w; ++j)
            dp[0][j] = 0;
        // Dropping reference intervals (a note the player never played) costs 1 each.
        for (int i = 1; i <= kRefLen; ++i)
            dp[i][0] = i;

        for (int i = 1; i <= kRefLen; ++i)
        {
            for (int j = 1; j <= w; ++j)
            {
                const int sub = (std::abs (kRefIntervals[i - 1] - h[j - 1]) <= kTolSemitones) ? 0 : 1;
                int best = dp[i - 1][j - 1] + sub;        // match / substitute
                const int del = dp[i - 1][j] + 1;         // reference note missing from play
                const int ins = dp[i][j - 1] + 1;         // extra note in play
                if (del < best) best = del;
                if (ins < best) best = ins;
                dp[i][j] = best;
            }
        }

        return dp[kRefLen][w] <= kMaxCost;
    }

    // Per-interval pitch slack. 0 = intervals must be exact (MIDI input lands on
    // integer semitones; human error shows up as whole wrong/extra notes, which
    // the edit distance already absorbs).
    static constexpr int kTolSemitones = 0;

    int     ring[kRingLen] = { 0 };
    int     count = 0;
    int     head = 0;

    bool    hasLast = false;
    int     lastPitch = 0;
    int64_t lastTimestamp = 0;
    int64_t gapSamples = (int64_t) (kGapSeconds * 44100.0);

    bool    triggered = false;
};

} // namespace luthier
