// OWNER: WP9
#pragma once
#include "../Common/Lfo.h"
#include "../Common/TempoSync.h"
#include "../IPedal.h"
#include <cmath>
#include <algorithm>

namespace luthier::reference::generic::detail
{
constexpr double pi = 3.14159265358979323846;
// These are pedal-level tempo policies, not a second oscillator implementation.
class ModulationClock
{
public:
    void prepare(double sampleRate) noexcept { sr = sampleRate; oscillator.prepare(sr); reset(); }
    void reset() noexcept { oscillator.setPhase(0); phaseCorrection = 0; rate = 0; }
    void time(const TimeInfo& time, double quarters) noexcept
    {
        if (quarters > 0 && time.valid && time.playing && std::isfinite(time.ppq))
        {
            const double desired = common::TempoSync::phase(time.ppq, quarters);
            phaseCorrection = desired - oscillator.phase();
            phaseCorrection -= std::round(phaseCorrection);
        }
    }
    double phase() const noexcept { return oscillator.phase(); }
    void advance(double targetRate) noexcept
    {
        if (rate == 0) rate = targetRate;
        rate += (targetRate-rate) * (1-std::exp(-1/(0.1*sr)));
        const double correction = phaseCorrection * (1-std::exp(-1/(0.02*sr)));
        phaseCorrection -= correction;
        oscillator.setPhase(oscillator.phase()+correction);
        oscillator.setRate(rate); oscillator.advance();
    }
private:
    common::Lfo oscillator;
    double sr = 48000, rate = 0, phaseCorrection = 0;
};
inline double wave(double phase, bool triangle) noexcept
{
    phase -= std::floor(phase);
    return triangle ? 1-4*std::abs(phase-0.5) : std::sin(2*pi*phase);
}
inline double modulationQuarters(int index) noexcept
{
    constexpr double values[] {0,1,0.5,0.75,0.25,2,4};
    return values[std::clamp(index,0,6)];
}
inline double tremoloQuarters(int index) noexcept
{
    constexpr double values[] {0,1,2.0/3,0.5,0.75,1.0/3,0.25,2,4,8};
    return values[std::clamp(index,0,9)];
}
inline double tempoRate(const TimeInfo& time, double quarters, double fallback) noexcept
{
    return quarters > 0 && time.valid && std::isfinite(time.bpm) && time.bpm>0
        ? common::TempoSync::rate(time.bpm, quarters) : fallback;
}
}
