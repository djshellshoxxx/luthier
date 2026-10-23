#pragma once
#include <algorithm>
#include <cmath>
namespace luthier::reference::midi
{
// One-pole state is owned by the consumer that advances it per sample.
class ExprEngine
{
public:
    void prepare (double sr) noexcept { sampleRate = sr > 0 ? sr : 48000; }
    void setTarget (float v, float ms) noexcept
    {
        target = std::isfinite (v) ? std::clamp (v, 0.f, 1.f) : 0.f;
        coefficient = ms > 0 ? (float) std::exp (-1. / (.001 * ms * sampleRate)) : 0.f;
        if (! initialised) { current = target; initialised = true; }
    }
    float next() noexcept { current = target + coefficient * (current - target); if (std::abs (current) < 1e-15f) current = 0; return current; }
    void reset() noexcept { initialised = false; current = target = 0; }
    static float pitchBend (int value, bool bipolar = false) noexcept
    { value = std::clamp (value, 0, 16383); return bipolar ? (value < 8192 ? (value - 8192) / 8192.f : (value - 8192) / 8191.f) : value / 16383.f; }
private:
    double sampleRate = 48000;
    float current = 0, target = 0, coefficient = 0;
    bool initialised = false;
};
}
