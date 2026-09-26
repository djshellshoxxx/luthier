#pragma once

/*  SPEC-SWEEP: advanced-ranges.md 2.1 and 3.4 (PR-44 / AR-15).

    The `modulation` range family. Its members are not parameters - LFO rate,
    envelope times, sequencer rate and follower times are ModMatrix state - so
    the family works by clamping the sources' setters to the stock pair unless
    the family is advanced, rather than by swapping a NormalisableRange.
    ModMatrix::setModulationRangeAdvanced sets every source's flag; a source
    constructed on its own is locked (stock).
*/

#include <juce_core/juce_core.h>

namespace luthier
{

struct ModRangePair
{
    double lo, hi;

    double clamp (double v) const noexcept { return juce::jlimit (lo, hi, std::isfinite (v) ? v : lo); }
};

namespace ModRanges
{
    /** advanced-ranges.md 3.4, stock then advanced. */
    inline ModRangePair lfoRateHz (bool advanced) noexcept
    {
        return advanced ? ModRangePair { 0.001, 200.0 } : ModRangePair { 0.01, 20.0 };
    }

    /** Envelope attack, decay and release, in seconds (the table's 0.1-5000 ms). */
    inline ModRangePair envelopeSeconds (bool advanced) noexcept
    {
        return advanced ? ModRangePair { 0.00001, 60.0 } : ModRangePair { 0.0001, 5.0 };
    }

    inline ModRangePair sequencerRateHz (bool advanced) noexcept
    {
        return advanced ? ModRangePair { 0.01, 400.0 } : ModRangePair { 0.1, 40.0 };
    }

    /** Follower attack and release, in ms. */
    inline ModRangePair followerMs (bool advanced) noexcept
    {
        return advanced ? ModRangePair { 0.01, 10000.0 } : ModRangePair { 0.1, 1000.0 };
    }
}

} // namespace luthier
