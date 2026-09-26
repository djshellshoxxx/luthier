#pragma once

/*  The band's clock (jam-mode.md 2): where the band is in musical time, and
    how that maps to samples.

    Three clocks drive it (2.3): the host's (its ppq, bar start, tempo and
    meter, re-read every block, so tempo automation and cycle jumps are
    followed), a playing tune's own clock, and the band's own - an anchor
    (sample, ppq, bpm) that is moved only at a bar line when the tempo
    changes, so every step's sample is computed from one formula whatever the
    host's block sizes are (JM-05).

    A step's sample is rounded to the nearest sample, computed from absolute
    numbers, so a grid point lands on the same sample however the blocks fall.
*/

#include "JamStyle.h"
#include <cmath>
#include <cstdint>

namespace luthier
{

struct JamClockMapping
{
    int64_t refSample = 0;
    double refPpq = 0.0;
    double samplesPerQuarter = 24000.0;

    int64_t sampleOf (double ppq) const noexcept
    {
        return refSample + (int64_t) std::floor ((ppq - refPpq) * samplesPerQuarter + 0.5);
    }

    double ppqOf (int64_t sample) const noexcept
    {
        return refPpq + (double) (sample - refSample) / samplesPerQuarter;
    }

    double bpm (double sampleRate) const noexcept { return 60.0 * sampleRate / samplesPerQuarter; }
};

/** The scheduler's position: the next step it has not scheduled yet. */
struct JamCursor
{
    bool valid = false;
    int64_t bar = 0;              ///< the band's bar number (count-in bars are negative)
    double barStart = 0.0;        ///< ppq
    double barLength = 4.0;       ///< quarters
    int step = 0;                 ///< within the bar
    int stepsInBar = 16;
    int stepsPerQuarter = 4;
    int numerator = 4, denominator = 4;

    double stepLength() const noexcept { return 1.0 / (double) stepsPerQuarter; }
    double gridPpq (int s) const noexcept { return barStart + (double) s * stepLength(); }
    double barEnd() const noexcept { return barStart + barLength; }
    bool isBeat (int s) const noexcept { return s % stepsPerQuarter == 0; }
};

namespace jamclock
{
    /** jam-mode 3.2: the follow quantum in quarters. */
    inline double quantum (int follow, double barLength) noexcept
    {
        switch ((jam::Follow) follow)
        {
            case jam::Follow::tight:   return 0.5;
            case jam::Follow::natural: return 1.0;
            case jam::Follow::relaxed: return barLength * 0.5;
            case jam::Follow::bar:     return barLength;
        }

        return 1.0;
    }

    /** The first quantum boundary at or after `ppq` (boundaries run from the
        bar start). */
    inline double nextBoundary (double ppq, double barStart, double q) noexcept
    {
        const double k = std::ceil ((ppq - barStart) / q - 1.0e-9);
        return barStart + k * q;
    }

    inline double lastBoundary (double ppq, double barStart, double q) noexcept
    {
        const double k = std::floor ((ppq - barStart) / q + 1.0e-9);
        return barStart + k * q;
    }

    inline double nearestBoundary (double ppq, double barStart, double q) noexcept
    {
        const double k = std::floor ((ppq - barStart) / q + 0.5);
        return barStart + k * q;
    }

    /** Swing: the off-16ths of a 16 grid move late by `amount` of a step. */
    inline double swingOffset (int step, int stepsPerQuarter, double amount) noexcept
    {
        if (stepsPerQuarter != 4 || (step % 2) == 0)
            return 0.0;

        return amount / (double) stepsPerQuarter;
    }

    /** A bar's length in quarters. */
    inline double barLength (int numerator, int denominator) noexcept
    {
        return (double) numerator * 4.0 / (double) juce::jmax (1, denominator);
    }
}

} // namespace luthier
