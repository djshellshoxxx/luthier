#include "PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

void PitchTracker::prepare (double rate, int size, double minFrequency, double maxFrequency)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    frameSize = std::max (64, size);
    maxLag = std::min (frameSize / 2, (int) std::ceil (sampleRate / std::max (20.0, minFrequency)));
    minLag = std::max (2, (int) std::floor (sampleRate / std::max (minFrequency + 1.0, maxFrequency)));
    difference.assign ((size_t) maxLag + 2, 0.0);
    cumulative.assign ((size_t) maxLag + 2, 1.0);
}

PitchTracker::Estimate PitchTracker::analyse (const float* x) noexcept
{
    Estimate e;

    if (x == nullptr)
        return e;

    double energy = 0.0;

    for (int i = 0; i < frameSize; ++i)
        energy += (double) x[i] * (double) x[i];

    e.rms = std::sqrt (energy / frameSize);

    if (! std::isfinite (e.rms))
        return {};

    if (e.rms < 1.0e-6)
        return e;

    const int window = frameSize - maxLag;

    // Step 2: the difference function.
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double sum = 0.0;

        for (int i = 0; i < window; ++i)
        {
            const double d = (double) x[i] - (double) x[i + tau];
            sum += d * d;
        }

        difference[(size_t) tau] = sum;
    }

    // Step 3: cumulative mean normalisation.
    cumulative[0] = 1.0;
    double running = 0.0;

    for (int tau = 1; tau <= maxLag; ++tau)
    {
        running += difference[(size_t) tau];
        cumulative[(size_t) tau] = running > 0.0 ? difference[(size_t) tau] * tau / running : 1.0;
    }

    // Step 4: the first dip under the threshold, else the global minimum.
    int best = -1;

    for (int tau = minLag; tau < maxLag; ++tau)
    {
        if (cumulative[(size_t) tau] < kThreshold)
        {
            while (tau + 1 < maxLag && cumulative[(size_t) tau + 1] < cumulative[(size_t) tau])
                ++tau;

            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        best = minLag;

        for (int tau = minLag; tau < maxLag; ++tau)
            if (cumulative[(size_t) tau] < cumulative[(size_t) best])
                best = tau;
    }

    // Step 5: parabolic interpolation around the dip.
    double lag = best;

    if (best > 1 && best < maxLag)
    {
        const double a = cumulative[(size_t) best - 1], b = cumulative[(size_t) best], c = cumulative[(size_t) best + 1];
        const double denominator = a - 2.0 * b + c;

        if (std::abs (denominator) > 1.0e-12)
            lag = best + 0.5 * (a - c) / denominator;
    }

    e.confidence = std::clamp (1.0 - cumulative[(size_t) best], 0.0, 1.0);
    e.frequency = lag > 0.0 ? sampleRate / lag : 0.0;

    if (! std::isfinite (e.frequency))
        e = {};

    return e;
}

double frequencyToMidi (double hz) noexcept
{
    return hz > 0.0 ? 69.0 + 12.0 * std::log2 (hz / 440.0) : 0.0;
}

} // namespace luthier
