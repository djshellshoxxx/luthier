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

#if defined (__SSE2__) || defined (_M_X64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <emmintrin.h>
 #define TRUEPEAK_SSE 1
#else
 #define TRUEPEAK_SSE 0
#endif
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
        for (auto& h : history)
            h = 0.0f;

        writeIndex = 0;
    }

    /** Feeds one sample and returns the absolute true peak of the four
        interpolated points it completes.

        The four phases run side by side (the coefficients are stored
        transposed, tap-major), so each tap is one 4-wide multiply-add: the
        whole detector is 12 vector operations per sample. The history is
        stored twice over so the taps are always contiguous. Single precision
        is plenty for a peak detector. */
    float process (float x) noexcept
    {
        writeIndex = (writeIndex == 0) ? kTapsPerPhase - 1 : writeIndex - 1;
        const float v = std::isfinite (x) ? x : 0.0f;
        history[(size_t) writeIndex] = v;
        history[(size_t) (writeIndex + kTapsPerPhase)] = v;

        const float* h = history.data() + writeIndex;   // h[t] = x[n - t]
        const auto& c = transposed();

       #if TRUEPEAK_SSE
        __m128 acc = _mm_setzero_ps();

        for (int t = 0; t < kTapsPerPhase; ++t)
            acc = _mm_add_ps (acc, _mm_mul_ps (_mm_load_ps (c.data() + 4 * t), _mm_set1_ps (h[t])));

        const __m128 absAcc = _mm_andnot_ps (_mm_set1_ps (-0.0f), acc);
        __m128 m = _mm_max_ps (absAcc, _mm_shuffle_ps (absAcc, absAcc, _MM_SHUFFLE (2, 3, 0, 1)));
        m = _mm_max_ps (m, _mm_shuffle_ps (m, m, _MM_SHUFFLE (1, 0, 3, 2)));
        return _mm_cvtss_f32 (m);
       #else
        float acc[kPhases] = { 0.0f, 0.0f, 0.0f, 0.0f };

        for (int t = 0; t < kTapsPerPhase; ++t)
            for (int p = 0; p < kPhases; ++p)
                acc[p] += c[(size_t) (4 * t + p)] * h[t];

        return std::max (std::max (std::abs (acc[0]), std::abs (acc[1])),
                         std::max (std::abs (acc[2]), std::abs (acc[3])));
       #endif
    }

    /** Pushes a sample into the history without running the filter. For a
        caller that knows the output cannot matter (see l1Norm). */
    void push (float x) noexcept
    {
        writeIndex = (writeIndex == 0) ? kTapsPerPhase - 1 : writeIndex - 1;
        const float v = std::isfinite (x) ? x : 0.0f;
        history[(size_t) writeIndex] = v;
        history[(size_t) (writeIndex + kTapsPerPhase)] = v;
    }

    /** The largest L1 norm of any phase: no interpolated point can exceed the
        largest of the last kTapsPerPhase samples by more than this factor. */
    static float l1Norm() noexcept
    {
        static const float n = []
        {
            double best = 0.0;

            for (const auto& phase : coefficients())
            {
                double sum = 0.0;
                for (double c : phase) sum += std::abs (c);
                best = std::max (best, sum);
            }

            return (float) best;
        }();

        return n;
    }

    /** The true peak of a whole buffer (tests and offline measurement). */
    static double measure (const float* samples, int numSamples) noexcept
    {
        TruePeakDetector d;
        d.reset();
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, d.process (samples[i]));

        for (int i = 0; i < kTapsPerPhase; ++i)
            peak = std::max (peak, d.process (0.0f));

        return (double) peak;
    }

    /** Tap-major: [t][phase], 16-byte aligned. */
    static const std::array<float, kPhases * kTapsPerPhase>& transposed() noexcept
    {
        alignas (16) static const std::array<float, kPhases * kTapsPerPhase> t = []
        {
            std::array<float, kPhases * kTapsPerPhase> out {};

            for (int tap = 0; tap < kTapsPerPhase; ++tap)
                for (int p = 0; p < kPhases; ++p)
                    out[(size_t) (4 * tap + p)] = (float) coefficients()[(size_t) p][(size_t) tap];

            return out;
        }();

        return t;
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
    alignas (16) std::array<float, 2 * kTapsPerPhase> history {};
    int writeIndex = 0;
};

} // namespace luthier
