#pragma once

/*  The metronome (practice-tools.md section 1).

    A click generator that is accurate to the sample, because the one thing a
    metronome cannot be is approximately on time. practice-tools 11 asks for an
    inter-click interval within half a millisecond at 48 kHz over a minute, which
    at 120 bpm is a drift of under one part in fifty thousand.

    Two decisions get it there.

    The first is that the beat position is carried as a double in beats and
    advanced once per block, rather than as a sample counter that is reset on
    every click. A counter reset on each click accumulates the rounding error of
    every click before it; a running position in beats carries only the error of
    the current block.

    The second is that clicks are scheduled to a sample offset inside the block
    rather than to the block boundary. A 512-sample block at 48 kHz is ten and a
    half milliseconds, so snapping to the boundary would be twenty times worse
    than the tolerance on its own.

    The click itself is synthesised rather than loaded. practice-tools 1 asks for
    six click sounds; a wood block and a cowbell are a few lines of filtered
    noise and a decaying sine apiece, and synthesising them means the metronome
    has no files to find, no sample-rate conversion, and no way to be silent
    because an installer dropped a folder.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** The click sounds (practice-tools 1). */
enum class ClickSound
{
    woodBlock = 0, cowbell, digitalBlip, sideStick, shaker, tap,
    numSounds
};

const char* getClickSoundName (ClickSound sound) noexcept;

//==============================================================================
/** Which subdivision of the beat clicks (practice-tools 1). */
enum class ClickSubdivision
{
    quarter = 0, eighth, triplet, sixteenth, dottedEighth,
    numSubdivisions
};

const char* getClickSubdivisionName (ClickSubdivision s) noexcept;

/** How many of these fit in one beat. */
double clicksPerBeat (ClickSubdivision s) noexcept;

//==============================================================================
/** How loud a given beat is (practice-tools 1). */
enum class BeatAccent { silent = 0, ghost, normal, accent, numLevels };

//==============================================================================
/** A time signature. */
struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;

    /** How many quarter-note beats one bar lasts. A 6/8 bar is three quarters. */
    double beatsPerBar() const noexcept
    {
        return (double) numerator * 4.0 / (double) juce::jmax (1, denominator);
    }

    bool operator== (const TimeSignature& other) const noexcept
    {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

//==============================================================================
class Metronome
{
public:
    static constexpr int kMaxBeatsPerBar = 32;

    /** practice-tools 1: mute every Nth bar, up to sixteen. */
    static constexpr int kMaxSilentBarPeriod = 16;

    Metronome();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    void setEnabled (bool shouldBeEnabled) noexcept;
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setTempo (double bpm) noexcept;
    double getTempo() const noexcept { return bpm.load (std::memory_order_relaxed); }

    /*  SPEC-SWEEP PT-6 (practice-tools 1, live-performance 5): whether the
        click follows the plugin's own tempo - the host's, or a tapped one
        while the host is stopped. On for a new session; typing a tempo on the
        METRO tab turns it off. A running progressive ramp always wins. */
    void setFollowsTempo (bool shouldFollow) noexcept { followsTempo.store (shouldFollow, std::memory_order_relaxed); }
    bool getFollowsTempo() const noexcept { return followsTempo.load (std::memory_order_relaxed); }

    /** Audio thread, once a block: the plugin's effective tempo, taken only
        while following and no ramp is running. */
    void followTempo (double effectiveBpm) noexcept
    {
        if (getFollowsTempo() && ! isProgressiveTempoRunning() && effectiveBpm > 0.0)
            setTempo (effectiveBpm);
    }

    void setTimeSignature (int numerator, int denominator) noexcept;
    TimeSignature getTimeSignature() const noexcept;

    void setSubdivision (ClickSubdivision s) noexcept;
    ClickSubdivision getSubdivision() const noexcept
    {
        return (ClickSubdivision) subdivision.load (std::memory_order_relaxed);
    }

    void setSound (ClickSound s) noexcept { sound.store ((int) s, std::memory_order_relaxed); }
    ClickSound getSound() const noexcept { return (ClickSound) sound.load (std::memory_order_relaxed); }

    void setLevelDb (double db) noexcept;
    double getLevelDb() const noexcept { return levelDb.load (std::memory_order_relaxed); }

    /** The gain of the off-beat subdivision clicks, relative to the beats. */
    void setSubdivisionLevelDb (double db) noexcept;

    //==========================================================================
    /** The accent pattern, one entry per beat of the bar. */
    void setBeatAccent (int beat, BeatAccent accent) noexcept;
    BeatAccent getBeatAccent (int beat) const noexcept;

    /** Restores the default: an accent on beat one, normal everywhere else. */
    void resetAccents() noexcept;

    //==========================================================================
    /** practice-tools 1: mute every Nth bar, to force internal timekeeping. 0 or
        1 means never. */
    void setSilentBarPeriod (int everyNBars) noexcept;
    int getSilentBarPeriod() const noexcept { return silentBarPeriod.load (std::memory_order_relaxed); }

    //==========================================================================
    // Progressive tempo (practice-tools 1): ramp from A to B over N bars.

    void startProgressiveTempo (double fromBpm, double toBpm, int overBars) noexcept;
    void stopProgressiveTempo() noexcept;
    bool isProgressiveTempoRunning() const noexcept { return progressive.load (std::memory_order_relaxed); }

    //==========================================================================
    /** Renders this block's clicks into a mono buffer, which the caller mixes
        wherever it wants. The buffer is written, not added to.

        Returns the number of clicks that fired, which is what the visual
        indicator counts. */
    int processBlock (float* destination, int numSamples) noexcept;

    /** Clicks on another clock's grid - the tune player's, whose count-in and
        metronome are its own (tune-builder 3.6) - at the given sample offsets,
        in order, a downbeat accented. Written, not added, into the mono
        buffer, in this metronome's sound and level; its own grid, tempo and
        enabled flag play no part. Returns the clicks fired. */
    int renderClicksAt (float* destination, int numSamples,
                        const int* offsets, const bool* downbeats, int count) noexcept;

    //==========================================================================
    // Live position, for the four-dot indicator.

    int getCurrentBeat() const noexcept { return currentBeat.load (std::memory_order_relaxed); }
    int getCurrentBar() const noexcept { return currentBar.load (std::memory_order_relaxed); }
    bool isCurrentBarSilent() const noexcept { return barIsSilent.load (std::memory_order_relaxed); }

    /** How far through the current beat, 0 to 1. */
    double getBeatPhase() const noexcept { return beatPhase.load (std::memory_order_relaxed); }

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    /** One click, being rendered. The metronome can have several in flight at
        once when the subdivision is fast and the click is long. */
    struct Voice
    {
        bool active = false;
        int samplesRemaining = 0;
        double phase = 0.0;
        double phaseIncrement = 0.0;
        double envelope = 0.0;
        double envelopeDecay = 0.0;
        double gain = 0.0;
        ClickSound sound = ClickSound::woodBlock;

        /** Two-pole state for the noise-based sounds. */
        double z1 = 0.0, z2 = 0.0;
    };

    static constexpr int kMaxVoices = 8;

    void triggerClick (double frequencyHz, double decaySeconds, double gain,
                       ClickSound clickSound) noexcept;

    double renderVoice (Voice& voice) noexcept;

    /** The accent level of a subdivision click that is not on a beat. */
    void fireClickFor (int beatInBar, bool onBeat) noexcept;
    void fireClick (BeatAccent accent) noexcept;

    double sr = 44100.0;

    std::atomic<bool> enabled { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> followsTempo { true };   // SPEC-SWEEP PT-6
    std::atomic<int> numerator { 4 }, denominator { 4 };
    std::atomic<int> subdivision { (int) ClickSubdivision::quarter };
    std::atomic<int> sound { (int) ClickSound::woodBlock };
    std::atomic<double> levelDb { -6.0 };
    std::atomic<double> subdivisionLevelDb { -9.0 };
    std::atomic<int> silentBarPeriod { 0 };

    std::array<std::atomic<int>, kMaxBeatsPerBar> accents {};

    // --- progressive tempo ------------------------------------------------------------
    std::atomic<bool> progressive { false };
    std::atomic<double> progressiveFrom { 120.0 }, progressiveTo { 120.0 };
    std::atomic<int> progressiveBars { 8 };
    double progressiveStartBar = 0.0;

    /*  Position, in clicks rather than in beats.

        The click grid is what actually has to be accurate, and expressing the
        position in its own units means a click lands exactly when the fractional
        part of this crosses an integer - no division, no rounding, and no
        accumulating remainder.

        It starts just below zero, so the first step crosses 0 and click 0 -
        beat one - sounds; starting at exactly 0 skipped it. */
    static constexpr double kStartPosition = -1.0e-9;
    double clickPosition = kStartPosition;

    /** Set by setEnabled on the message thread; the audio thread restarts the
        grid, so clickPosition has a single writer. */
    std::atomic<bool> restartPending { false };

    std::array<Voice, kMaxVoices> voices {};

    std::atomic<int> currentBeat { 0 }, currentBar { 0 };
    std::atomic<bool> barIsSilent { false };
    std::atomic<double> beatPhase { 0.0 };

    RtRandom rng { 0xc10c17ull };

    JUCE_LEAK_DETECTOR (Metronome)
};

} // namespace luthier
