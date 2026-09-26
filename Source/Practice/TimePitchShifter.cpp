#include "TimePitchShifter.h"

namespace luthier
{

void TimePitchShifter::prepare()
{
    for (int c = 0; c < 2; ++c)
    {
        ring[(size_t) c].assign ((size_t) kRingSize, 0.0f);
        accumulator[(size_t) c].assign ((size_t) kGrainSize, 0.0f);
        out[(size_t) c].assign ((size_t) kHop, 0.0f);
        pullScratch[(size_t) c].assign (4096, 0.0f);
    }

    // A periodic Hann: at half overlap the windows sum to exactly one.
    window.resize ((size_t) kGrainSize);

    for (int j = 0; j < kGrainSize; ++j)
        window[(size_t) j] = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * j / kGrainSize));

    reset();
}

void TimePitchShifter::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        std::fill (ring[(size_t) c].begin(), ring[(size_t) c].end(), 0.0f);
        std::fill (accumulator[(size_t) c].begin(), accumulator[(size_t) c].end(), 0.0f);
        std::fill (out[(size_t) c].begin(), out[(size_t) c].end(), 0.0f);
    }

    inputCount = 0;
    analysisPos = 0.0;
    previousStart = 0.0;
    hasPrevious = false;
    outRead = kHop;
}

float TimePitchShifter::mono (juce::int64 index) const noexcept
{
    const auto at = (size_t) (index & (kRingSize - 1));
    return ring[0][at] + ring[1][at];
}

float TimePitchShifter::readInterpolated (int channel, double position) const noexcept
{
    // Catmull-Rom between the two samples either side.
    const auto base = (juce::int64) std::floor (position);
    const float t = (float) (position - (double) base);
    const auto& x = ring[(size_t) channel];

    auto at = [&x] (juce::int64 i) { return x[(size_t) (i & (kRingSize - 1))]; };

    const float y0 = at (base - 1), y1 = at (base), y2 = at (base + 1), y3 = at (base + 2);

    const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = -0.5f * y0 + 0.5f * y2;

    return ((a * t + b) * t + c) * t + y1;
}

double TimePitchShifter::bestStart (double target) const noexcept
{
    const auto from = (juce::int64) std::llround (target);
    const auto centre = (juce::int64) std::llround (analysisPos);

    // Never reach before the start of the stream.
    const juce::int64 lowest = juce::jmax ((juce::int64) 2, centre - kSearch);
    const juce::int64 highest = juce::jmax (lowest, centre + kSearch);

    auto score = [this, from] (juce::int64 candidate, int stride)
    {
        double dot = 0.0, energy = 1.0e-9;

        for (int i = 0; i < kCorrelation; i += stride)
        {
            const double a = mono (from + i);
            const double b = mono (candidate + i);
            dot += a * b;
            energy += b * b;
        }

        return dot / std::sqrt (energy);
    };

    juce::int64 best = juce::jlimit (lowest, highest, centre);
    double bestScore = -1.0e300;

    for (auto candidate = lowest; candidate <= highest; candidate += 4)
    {
        const double s = score (candidate, 4);

        if (s > bestScore)
        {
            bestScore = s;
            best = candidate;
        }
    }

    const auto coarse = best;
    bestScore = -1.0e300;

    for (auto candidate = juce::jmax (lowest, coarse - 3); candidate <= juce::jmin (highest, coarse + 3); ++candidate)
    {
        const double s = score (candidate, 2);

        if (s > bestScore)
        {
            bestScore = s;
            best = candidate;
        }
    }

    return (double) best;
}

} // namespace luthier
