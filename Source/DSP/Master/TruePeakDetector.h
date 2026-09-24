#pragma once

/*  4x-oversampled true-peak detector (output-normalization.md 4.1), in the
    style of ITU-R BS.1770-4 Annex 2: a 48-tap polyphase interpolation FIR,
    12 taps per phase, whose largest absolute output across the four phases
    is the sample's true peak. The coefficients are Annex 2's.

    Header-only and allocation-free: prepare() is the only call that is not
    real-time safe, and it does not allocate either. The detector's output
    lags its input by kLatency samples, which the master limiter's 1.5 ms
    lookahead covers many times over.
*/

#include <array>
#include <cmath>
#include <algorithm>

namespace luthier
{

class TruePeakDetector
{
public:
    static constexpr int kPhases = 4;
    static constexpr int kTapsPerPhase = 12;
    static constexpr int kLatency = kTapsPerPhase / 2;

    void reset() noexcept
    {
        history.fill (0.0);
        writeIndex = 0;
    }

    /** Feeds one sample and returns the absolute true peak of the four
        interpolated points it completes. */
    double process (double x) noexcept
    {
        writeIndex = (writeIndex + kTapsPerPhase - 1) % kTapsPerPhase;
        history[(size_t) writeIndex] = std::isfinite (x) ? x : 0.0;

        double peak = 0.0;

        for (int phase = 0; phase < kPhases; ++phase)
        {
            double acc = 0.0;
            const auto& c = coefficients()[(size_t) phase];

            for (int t = 0; t < kTapsPerPhase; ++t)
                acc += c[(size_t) t] * history[(size_t) ((writeIndex + t) % kTapsPerPhase)];

            peak = std::max (peak, std::abs (acc));
        }

        return peak;
    }

    /** The true peak of a whole buffer (tests and offline measurement). */
    static double measure (const float* samples, int numSamples) noexcept
    {
        TruePeakDetector d;
        d.reset();
        double peak = 0.0;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, d.process ((double) samples[i]));

        for (int i = 0; i < kTapsPerPhase; ++i)
            peak = std::max (peak, d.process (0.0));

        return peak;
    }

    static const std::array<std::array<double, kTapsPerPhase>, kPhases>& coefficients() noexcept
    {
        // BS.1770-4 Annex 2, the 48-tap 4x interpolator, split into its phases.
        static const std::array<std::array<double, kTapsPerPhase>, kPhases> c {{
            {{  0.0017089843750,  0.0109863281250, -0.0196533203125,  0.0332031250000,
               -0.0594482421875,  0.1373291015625,  0.9721679687500, -0.1022949218750,
                0.0476074218750, -0.0266113281250,  0.0148925781250, -0.0083007812500 }},
            {{ -0.0291748046875,  0.0292968750000, -0.0517578125000,  0.0891113281250,
               -0.1665039062500,  0.4650878906250,  0.7797851562500, -0.2003173828125,
                0.1015625000000, -0.0582275390625,  0.0330810546875, -0.0189208984375 }},
            {{ -0.0189208984375,  0.0330810546875, -0.0582275390625,  0.1015625000000,
               -0.2003173828125,  0.7797851562500,  0.4650878906250, -0.1665039062500,
                0.0891113281250, -0.0517578125000,  0.0292968750000, -0.0291748046875 }},
            {{ -0.0083007812500,  0.0148925781250, -0.0266113281250,  0.0476074218750,
               -0.1022949218750,  0.9721679687500,  0.1373291015625, -0.0594482421875,
                0.0332031250000, -0.0196533203125,  0.0109863281250,  0.0017089843750 }}
        }};

        return c;
    }

private:
    std::array<double, kTapsPerPhase> history {};
    int writeIndex = 0;
};

} // namespace luthier
