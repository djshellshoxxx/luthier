#include "TapTempo.h"

#include <algorithm>

namespace luthier
{

TapTempo::TapTempo()
{
    reset();
}

void TapTempo::reset() noexcept
{
    taps.fill (0.0);
    numTaps = 0;
    lastTapSeconds = -1.0;

    tapped.store (false, std::memory_order_relaxed);
}

double TapTempo::snap (double bpm) noexcept
{
    const double nearest = std::round (bpm);

    return (std::abs (bpm - nearest) <= kSnapBpm) ? nearest : bpm;
}

void TapTempo::setBpm (double bpm) noexcept
{
    tappedBpm.store (juce::jlimit (kMinBpm, kMaxBpm, bpm), std::memory_order_relaxed);
    tapped.store (true, std::memory_order_relaxed);
}

bool TapTempo::tap (double timeSeconds) noexcept
{
    const double previous = lastTapSeconds;
    lastTapSeconds = timeSeconds;

    // The first tap of a sequence, or one so late that the previous sequence has
    // expired, starts counting again rather than producing a wild interval.
    if (previous < 0.0 || (timeSeconds - previous) > kTapWindowSeconds)
    {
        taps[0] = timeSeconds;
        numTaps = 1;
        return false;
    }

    if (numTaps < kMaxTaps)
    {
        taps[(size_t) numTaps++] = timeSeconds;
    }
    else
    {
        // Keep the most recent taps, so a player who speeds up is followed rather
        // than averaged against where they started.
        std::rotate (taps.begin(), taps.begin() + 1, taps.begin() + kMaxTaps);
        taps[(size_t) kMaxTaps - 1] = timeSeconds;
    }

    const int numIntervals = numTaps - 1;

    if (numIntervals < 1)
        return false;

    // ---- the gaps ----------------------------------------------------------------
    std::array<double, kMaxIntervals> intervals {};

    for (int i = 0; i < numIntervals; ++i)
        intervals[(size_t) i] = taps[(size_t) i + 1] - taps[(size_t) i];

    // ---- median, to decide which gaps to believe -----------------------------------
    std::array<double, kMaxIntervals> sorted = intervals;
    std::sort (sorted.begin(), sorted.begin() + numIntervals);

    const double median = (numIntervals % 2 == 1)
                            ? sorted[(size_t) (numIntervals / 2)]
                            : 0.5 * (sorted[(size_t) (numIntervals / 2 - 1)]
                                       + sorted[(size_t) (numIntervals / 2)]);

    if (median <= 0.0)
        return false;

    const double medianBpm = 60.0 / median;

    if (medianBpm < kMinBpm || medianBpm > kMaxBpm)
        return false;

    // ---- the longest run of gaps the median vouches for ------------------------------
    /*  The median decides which gaps to believe; the tempo itself is then fitted
        to the taps of that run.

        Three ways of getting a tempo out of a run of taps, in order of how much
        of the hand's jitter survives into the answer:

          - Average the gaps. Each gap carries its own noise, and averaging N of
            them only divides that noise by the square root of N.
          - Measure the span from the run's first tap to its last. The taps in
            between cancel out entirely, leaving only the two endpoints' jitter,
            divided by N. Better, but it throws away every tap it did not use,
            so one unlucky endpoint moves the whole answer.
          - Fit a line through all of them by least squares. Every tap
            contributes, and the slope's error falls off faster than either of
            the above.

        The last is what this does. It is the difference between an estimate that
        sits inside half a bpm most of the time and one that does so reliably.
    */
    int bestStart = 0, bestLength = 0;
    int runStart = 0, runLength = 0;

    for (int i = 0; i < numIntervals; ++i)
    {
        const bool believable = std::abs (intervals[(size_t) i] - median)
                                  <= median * kOutlierFraction;

        if (believable)
        {
            if (runLength == 0)
                runStart = i;

            ++runLength;

            if (runLength > bestLength)
            {
                bestLength = runLength;
                bestStart = runStart;
            }
        }
        else
        {
            runLength = 0;
        }
    }

    if (bestLength < 1)
        return false;

    // ---- least-squares slope of tap time against tap number ---------------------------
    const int runTaps = bestLength + 1;

    double meanIndex = 0.0, meanTime = 0.0;

    for (int i = 0; i < runTaps; ++i)
    {
        meanIndex += (double) i;
        meanTime += taps[(size_t) (bestStart + i)];
    }

    meanIndex /= (double) runTaps;
    meanTime /= (double) runTaps;

    double covariance = 0.0, variance = 0.0;

    for (int i = 0; i < runTaps; ++i)
    {
        const double dIndex = (double) i - meanIndex;

        covariance += dIndex * (taps[(size_t) (bestStart + i)] - meanTime);
        variance += dIndex * dIndex;
    }

    if (variance <= 0.0)
        return false;

    // The slope is seconds per tap, which is the interval.
    const double averaged = covariance / variance;

    if (averaged <= 0.0)
        return false;

    const double bpm = juce::jlimit (kMinBpm, kMaxBpm, snap (60.0 / averaged));

    tappedBpm.store (bpm, std::memory_order_relaxed);
    tapped.store (true, std::memory_order_relaxed);

    return true;
}

double TapTempo::getEffectiveBpm (double hostBpm, bool hostIsPlaying) const noexcept
{
    const bool internal = forceInternal.load (std::memory_order_relaxed);

    // live-performance 5: while the host is rolling, the host wins - unless the
    // user has deliberately taken the plugin off the host's clock.
    if (hostIsPlaying && ! internal)
        return juce::jlimit (kMinBpm, kMaxBpm, hostBpm);

    if (tapped.load (std::memory_order_relaxed))
        return tappedBpm.load (std::memory_order_relaxed);

    return juce::jlimit (kMinBpm, kMaxBpm, hostBpm);
}

} // namespace luthier
