#include "CpuRelief.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

void CpuRelief::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void CpuRelief::reset() noexcept
{
    aboveSeconds = belowSeconds = 0.0;
    average.store (0.0, std::memory_order_relaxed);
    step.store (0, std::memory_order_relaxed);
}

int CpuRelief::update (double load, int numSamples) noexcept
{
    if (numSamples <= 0)
        return getStep();

    if (const double forced = loadOverride.load (std::memory_order_relaxed); forced >= 0.0)
        load = forced;

    if (! std::isfinite (load) || load < 0.0)
        load = 0.0;

    const double seconds = (double) numSamples / sr;

    // A one-pole average whose time constant is the 200 ms window, stepped by
    // the block's duration so the block size does not change what it means.
    const double a = std::exp (-seconds / kWindowSeconds);
    const double avg = average.load (std::memory_order_relaxed) * a + load * (1.0 - a);
    average.store (avg, std::memory_order_relaxed);

    int current = step.load (std::memory_order_relaxed);
    const int ceiling = isStringDropAllowed() ? kMaxStep : kMaxStep - 1;

    if (avg > kHighLoad)
    {
        belowSeconds = 0.0;
        aboveSeconds += seconds;

        if (aboveSeconds >= kStepUpSeconds && current < ceiling)
        {
            ++current;
            aboveSeconds = 0.0;
        }
    }
    else if (avg < kLowLoad)
    {
        aboveSeconds = 0.0;
        belowSeconds += seconds;

        if (belowSeconds >= kStepDownSeconds && current > 0)
        {
            --current;
            belowSeconds = 0.0;
        }
    }
    else
    {
        aboveSeconds = belowSeconds = 0.0;
    }

    // Opting out of step 7 while in it steps back at once.
    current = std::min (current, ceiling);
    step.store (current, std::memory_order_relaxed);
    return current;
}

} // namespace luthier
