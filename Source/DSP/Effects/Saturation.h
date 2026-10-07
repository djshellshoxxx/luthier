#pragma once

/*  fx_saturation: one smoothed soft-clip stage at the end of the pre-amp effects.

    Sits after the pre-amp pedal rack and before the amp input (and the EQ-match
    filter), so it is the last thing the guitar signal passes before the amp: the
    "effects" section's final trim, rather than a pedal that can be reordered or
    bypassed away. It is deliberately not oversampled (a basic control, not a
    drive pedal) and does not reuse PedalsDrive's per-pedal shapers, which are
    voiced to each circuit and carry their own state.

    Amount 0..1 maps to 0..+24 dB of input drive into tanh, with output gain
    compensation (drive^-0.65) so loudness stays roughly constant for a guitar-
    level signal. At 0 it is a true bypass: no per-sample work, bit-identical
    output. Real-time safe: no allocation, parameter smoothed over ~20 ms.
*/

#include <algorithm>
#include <cmath>

namespace luthier
{

class Saturation
{
public:
    void prepare (double sampleRate) noexcept
    {
        smoothCoeff = 1.0 - std::exp (-1.0 / (0.020 * std::max (1000.0, sampleRate)));
        reset();
    }

    void reset() noexcept { current = target; }

    /** 0..1. */
    void setAmount (double amount) noexcept { target = std::min (1.0, std::max (0.0, amount)); }

    bool isActive() const noexcept { return target > 0.0 || current > 0.0; }

    void process (double* left, double* right, int numSamples) noexcept
    {
        if (! isActive())
            return;   // true bypass

        constexpr double kMaxDriveLn = 24.0 * 0.11512925464970229;   // ln(10)/20 per dB

        for (int i = 0; i < numSamples; ++i)
        {
            current += (target - current) * smoothCoeff;

            if (target == 0.0 && current < 1.0e-4)
            {
                current = 0.0;   // settled: from here the stage is bypassed again
                return;          // the rest of the block is left untouched
            }

            const double lnDrive = kMaxDriveLn * current;
            const double drive = std::exp (lnDrive);
            const double comp = std::exp (-0.65 * lnDrive);
            left[i]  = std::tanh (left[i] * drive) * comp;
            right[i] = std::tanh (right[i] * drive) * comp;
        }
    }

private:
    double target = 0.0, current = 0.0, smoothCoeff = 0.001;
};

} // namespace luthier
