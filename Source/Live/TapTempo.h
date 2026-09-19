#pragma once

/*  Plugin-global tap tempo (live-performance.md section 5).

    Drives the rhythm engine when the host is not playing, plus any delay or LFO
    with its sync toggle on. Host tempo wins whenever the host is rolling, unless
    the user has forced internal tempo in Options.

    Tapping is noisy - a player's hand is not a clock - so the estimate is a
    median of the recent intervals rather than a mean. One late tap in four moves
    a mean by a quarter of its error; it does not move a median at all, which is
    what stops a single fumbled tap from lurching the tempo.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

namespace luthier
{

class TapTempo
{
public:
    /** live-performance 5: taps more than this far apart start a new sequence. */
    static constexpr double kTapWindowSeconds = 3.0;

    /** How many taps are remembered. Eight taps give seven intervals. */
    static constexpr int kMaxTaps = 9;
    static constexpr int kMaxIntervals = kMaxTaps - 1;

    static constexpr double kMinBpm = 20.0;
    static constexpr double kMaxBpm = 300.0;

    /** Intervals further than this fraction from the median are discarded. */
    static constexpr double kOutlierFraction = 0.30;

    /** Within this many bpm of a whole number, snap to it. */
    static constexpr double kSnapBpm = 0.4;

    TapTempo();

    void reset() noexcept;

    /** Registers a tap at a moment in seconds, on whatever clock the caller is
        using - the two only ever get compared with each other.

        Returns true when the tap produced a new tempo estimate, which is what
        the UI uses to flash the header LED on a confirmed change. */
    bool tap (double timeSeconds) noexcept;

    /** The tapped tempo. Valid only once `hasTempo` is true. */
    double getTappedBpm() const noexcept { return tappedBpm.load (std::memory_order_relaxed); }
    bool hasTempo() const noexcept { return tapped.load (std::memory_order_relaxed); }

    /** How many taps are in the current sequence. */
    int getTapCount() const noexcept { return numTaps; }

    //==========================================================================
    /** live-performance 5: host tempo wins while the host is playing, unless the
        user forces internal tempo. */
    void setForceInternal (bool shouldForce) noexcept
    {
        forceInternal.store (shouldForce, std::memory_order_relaxed);
    }

    bool isForcingInternal() const noexcept
    {
        return forceInternal.load (std::memory_order_relaxed);
    }

    /** The tempo everything downstream should actually use. */
    double getEffectiveBpm (double hostBpm, bool hostIsPlaying) const noexcept;

    /** Sets the tempo directly, for a typed-in bpm. */
    void setBpm (double bpm) noexcept;

private:
    /** Rounds to a whole bpm when it is within the snap distance. */
    static double snap (double bpm) noexcept;

    /*  The tap times themselves, not just the gaps between them.

        Tempo is recovered from the span of a run of taps rather than from an
        average of the individual gaps, and that needs the original moments: the
        span of seven intervals divides the jitter of its two endpoints by seven,
        whereas averaging seven noisy gaps only divides by the square root of
        seven. That difference is what gets the estimate inside half a bpm. */
    std::array<double, kMaxTaps> taps {};
    int numTaps = 0;

    double lastTapSeconds = -1.0;

    std::atomic<double> tappedBpm { 120.0 };
    std::atomic<bool> tapped { false };
    std::atomic<bool> forceInternal { false };

    JUCE_LEAK_DETECTOR (TapTempo)
};

} // namespace luthier
