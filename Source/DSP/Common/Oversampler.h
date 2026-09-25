#pragma once

/*  Polyphase half-band oversampler.

    Engine rule 10 and pitfall 3: every nonlinear stage runs oversampled. Aliasing
    on drive and distortion is the single biggest thing that makes an amateur guitar
    plugin sound digital, because the harmonics a waveshaper generates fold back
    down the spectrum as inharmonic hash.

    The structure is the standard two-branch polyphase all-pass half-band filter:
    each 2x stage costs six second-order all-pass sections per sample, which is far
    cheaper than an FIR of equivalent stopband depth, and the phase response is
    irrelevant here because the same filter runs on the way up and on the way down.

    Factors of 1, 2, 4 and 8 are supported. 4 is the default.
*/

#include "DspCommon.h"

namespace luthier
{

//==============================================================================
/** One all-pass section of a half-band branch.

    The half-band filter is H(z) = 0.5 * (A0(z^2) + z^-1 A1(z^2)), each A a chain
    of (a + z^-2) / (1 + a z^-2) sections at the oversampled rate. The branches
    here run at the base rate (one input sample per up() call, one per branch
    per down() call), where z^2 is one sample, so each section is

        y[n] = a * (x[n] - y[n-1]) + x[n-1]

    It used y[n-2] / x[n-2], which at the base rate is A(z^4) at the high rate:
    the branches stopped being complementary, images were rejected 11-14 dB
    less, the passband drooped (-4 dB at 16.8 kHz on a 2x stage at 48 kHz), and
    the round trip delayed about twice what getLatencySamples reports (6.4 /
    9.5 / 11.1 samples against 3 / 5 / 6, which is what this form measures).

    The signs matter too: the mirror-image form `a*(x + y1) - x1` is a stable
    all-pass with the wrong phase for the pair. */
struct PolyphaseSection
{
    double a = 0.0;
    double x1 = 0.0, y1 = 0.0;

    inline double process (double x) noexcept
    {
        const double y = a * (x - y1) + x1;
        x1 = x;
        y1 = flushDenormal (y);
        return y;
    }

    void reset() noexcept { x1 = y1 = 0.0; }
};

//==============================================================================
/** One 2x half-band stage: two all-pass branches, three sections each. */
class HalfbandStage
{
public:
    HalfbandStage() noexcept
    {
        // Elliptic half-band coefficients, roughly -70 dB stopband.
        const double ca[3] = { 0.07569063, 0.26432249, 0.47940086 };
        const double cb[3] = { 0.18762082, 0.37232922, 0.61371816 };

        for (int i = 0; i < 3; ++i)
        {
            branchA[i].a = ca[i];
            branchB[i].a = cb[i];
        }
    }

    void reset() noexcept
    {
        for (int i = 0; i < 3; ++i)
        {
            branchA[i].reset();
            branchB[i].reset();
        }
    }

    /** One input sample becomes two output samples. */
    inline void up (double x, double& out0, double& out1) noexcept
    {
        out0 = runA (x);
        out1 = runB (x);
    }

    /** Two input samples become one output sample. */
    inline double down (double x0, double x1) noexcept
    {
        return 0.5 * (runA (x0) + runB (x1));
    }

private:
    inline double runA (double x) noexcept
    {
        return branchA[2].process (branchA[1].process (branchA[0].process (x)));
    }

    inline double runB (double x) noexcept
    {
        return branchB[2].process (branchB[1].process (branchB[0].process (x)));
    }

    PolyphaseSection branchA[3], branchB[3];
};

//==============================================================================
class Oversampler
{
public:
    static constexpr int kMaxFactor = 8;

    void prepare (double sampleRate, int factorToUse) noexcept
    {
        baseRate = sampleRate;
        setFactor (factorToUse);
        reset();
    }

    void setFactor (int f) noexcept
    {
        const int wanted = (f >= 8) ? 8 : (f >= 4) ? 4 : (f >= 2) ? 2 : 1;

        // Re-sent on every structural change (AmpEngine::setOversamplingFactor):
        // clearing the filters when nothing changed clicked the amp each time.
        if (wanted == factor)
            return;

        factor = wanted;
        reset();
    }

    int getFactor() const noexcept { return factor; }
    double getOversampledRate() const noexcept { return baseRate * (double) factor; }

    void reset() noexcept
    {
        for (int i = 0; i < 3; ++i)
        {
            upStage[i].reset();
            downStage[i].reset();
        }
    }

    /** Expands one sample into `getFactor()` samples in `dest`. */
    inline void up (double x, double* dest) noexcept
    {
        switch (factor)
        {
            case 1:
                dest[0] = x;
                return;

            case 2:
                upStage[0].up (x, dest[0], dest[1]);
                return;

            case 4:
            {
                double a, b;
                upStage[0].up (x, a, b);
                upStage[1].up (a, dest[0], dest[1]);
                upStage[1].up (b, dest[2], dest[3]);
                return;
            }

            case 8:
            default:
            {
                double a, b, c[4];
                upStage[0].up (x, a, b);
                upStage[1].up (a, c[0], c[1]);
                upStage[1].up (b, c[2], c[3]);

                for (int i = 0; i < 4; ++i)
                    upStage[2].up (c[i], dest[i * 2], dest[i * 2 + 1]);

                return;
            }
        }
    }

    /** Collapses `getFactor()` samples from `src` back into one sample. */
    inline double down (const double* src) noexcept
    {
        switch (factor)
        {
            case 1:
                return src[0];

            case 2:
                return downStage[0].down (src[0], src[1]);

            case 4:
            {
                const double a = downStage[1].down (src[0], src[1]);
                const double b = downStage[1].down (src[2], src[3]);
                return downStage[0].down (a, b);
            }

            case 8:
            default:
            {
                double c[4];

                for (int i = 0; i < 4; ++i)
                    c[i] = downStage[2].down (src[i * 2], src[i * 2 + 1]);

                const double a = downStage[1].down (c[0], c[1]);
                const double b = downStage[1].down (c[2], c[3]);
                return downStage[0].down (a, b);
            }
        }
    }

    /** Runs a waveshaper at the oversampled rate.

        @param x       input sample at the base rate
        @param shaper  callable taking and returning a double
        @returns       the shaped sample, back at the base rate
    */
    template <typename ShaperFn>
    inline double processSample (double x, ShaperFn&& shaper) noexcept
    {
        if (factor == 1)
            return shaper (x);

        double work[kMaxFactor];

        up (x, work);

        for (int i = 0; i < factor; ++i)
            work[i] = shaper (work[i]);

        return down (work);
    }

    /** Latency the oversampler adds, in base-rate samples. The all-pass branches
        are not linear phase, so this is the group delay near DC rather than an
        exact figure; it is small enough that reporting it keeps the host's delay
        compensation honest.

        performance-budget.md 10.6: measured, the up + down pair delays DC by
        6.35 / 9.52 / 11.11 samples at 2x / 4x / 8x (Latency.oversamplerReports-
        ItsGroupDelay); the table used to say 3 / 5 / 6. */
    int getLatencySamples() const noexcept
    {
        switch (factor)
        {
            case 1:  return 0;
            case 2:  return 6;
            case 4:  return 10;
            case 8:  default: return 11;
        }
    }

private:
    double baseRate = 44100.0;
    int factor = 4;

    HalfbandStage upStage[3];
    HalfbandStage downStage[3];
};

} // namespace luthier
