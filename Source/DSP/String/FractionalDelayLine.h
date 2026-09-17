#pragma once

/*  Fractional-delay circular buffer for the string waveguide.

    Interpolation modes (engine spec 5.2 lists all three; all three are built):

      Allpass1   - 1st-order allpass. Flattest magnitude response at a *static*
                   delay, but it is an IIR: its state has to settle whenever the
                   delay changes, which is audible as a chirp under the fast
                   modulation that dive-bombs and fast vibrato demand.
      Lagrange3  - 3rd-order FIR. Stateless, clean under modulation, very slight
                   HF loss near Nyquist.
      Lagrange5  - 5th-order FIR. Stateless, negligible HF loss below ~0.4 fs.
                   This is the default: it is the mode that satisfies both of the
                   spec's goals (no HF loss, no modulation artefacts) at once.

    See docs/GUITAR_PHYSICS.md for the measured comparison behind that default.
*/

#include "../Common/DspCommon.h"
#include <vector>

namespace luthier
{

class FractionalDelayLine
{
public:
    enum class Interpolation { Allpass1, Lagrange3, Lagrange5 };

    /** Allocates the buffer. Call from prepareToPlay only - never from processBlock. */
    void prepare (double sampleRate, double minFrequencyHz)
    {
        // Longest delay we must be able to hold, plus interpolation taps and margin.
        const double maxDelay = sampleRate / juce::jmax (1.0, minFrequencyHz);
        int required = (int) std::ceil (maxDelay) + 16;

        int size = 64;
        while (size < required)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0);
        mask = size - 1;
        writeIndex = 0;
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        apState = 0.0;
        apInput = 0.0;
        lastCoeff = 0.0;
        haveLastCoeff = false;
    }

    void setInterpolation (Interpolation i) noexcept
    {
        if (i != mode)
        {
            mode = i;
            apState = 0.0;
            apInput = 0.0;
            haveLastCoeff = false;
        }
    }

    Interpolation getInterpolation() const noexcept { return mode; }

    int getBufferSize() const noexcept { return (int) buffer.size(); }

    /** Largest delay this buffer can represent, leaving room for interpolation taps. */
    double getMaxDelay() const noexcept { return (double) (buffer.size() - 8); }

    inline void write (double value) noexcept
    {
        buffer[(size_t) writeIndex] = flushDenormal (value);
        writeIndex = (writeIndex + 1) & mask;
    }

    /** Reads `delaySamples` back from the most recent write position. */
    inline double read (double delaySamples) noexcept
    {
        delaySamples = juce::jlimit (1.0, getMaxDelay(), delaySamples);

        switch (mode)
        {
            case Interpolation::Allpass1:  return readAllpass  (delaySamples);
            case Interpolation::Lagrange3: return readLagrange3 (delaySamples);
            case Interpolation::Lagrange5: default: return readLagrange5 (delaySamples);
        }
    }

    /** Phase delay contributed by the interpolator itself, in samples, at DC.
        Lagrange interpolators are exact at DC; the allpass contributes none either,
        because its coefficient is derived from the fraction we asked for. */
    static constexpr double interpolatorDelayAtDC() noexcept { return 0.0; }

private:
    inline double at (int backwards) const noexcept
    {
        return buffer[(size_t) ((writeIndex - backwards) & mask)];
    }

    inline double readAllpass (double delaySamples) noexcept
    {
        // The allpass interpolates the fractional part in [1, 2) for stability,
        // which is the standard formulation (a fraction near 0 makes the pole
        // approach the unit circle and the filter rings).
        int intPart = (int) std::floor (delaySamples);
        double frac = delaySamples - (double) intPart;

        if (frac < 0.1)
        {
            frac += 1.0;
            intPart -= 1;
        }

        const double coeff = (1.0 - frac) / (1.0 + frac);

        // A large coefficient jump means the delay moved fast. Re-seed the state
        // from the equivalent Lagrange read so the transient never reaches the
        // output; this is what keeps dive-bombs clean in allpass mode.
        if (haveLastCoeff && std::abs (coeff - lastCoeff) > 0.25)
        {
            apState = readLagrange5 (delaySamples);
            apInput = at (intPart + 1);
        }

        lastCoeff = coeff;
        haveLastCoeff = true;

        const double x = at (juce::jmax (1, intPart));
        const double y = coeff * x + apInput - coeff * apState;

        apInput = x;
        apState = flushDenormal (y);
        return apState;
    }

    inline double readLagrange3 (double delaySamples) noexcept
    {
        const int i = (int) std::floor (delaySamples);
        const double d = delaySamples - (double) i;

        const double xm1 = at (juce::jmax (1, i - 1));
        const double x0  = at (juce::jmax (1, i));
        const double x1  = at (juce::jmax (1, i + 1));
        const double x2  = at (juce::jmax (1, i + 2));

        // Cubic Lagrange through the four points, evaluated at d.
        const double c0 = x0;
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);

        return ((c3 * d + c2) * d + c1) * d + c0;
    }

    inline double readLagrange5 (double delaySamples) noexcept
    {
        // Centre the 6-tap kernel so the fraction sits in the middle of the span,
        // which is where Lagrange interpolation is most accurate.
        const int i = (int) std::floor (delaySamples);
        const double d = delaySamples - (double) i;

        double x[6];
        for (int k = 0; k < 6; ++k)
            x[k] = at (juce::jmax (1, i - 2 + k));

        // Lagrange basis for nodes at -2..3 evaluated at d.
        const double t = d;
        const double tm2 = t + 2.0, tm1 = t + 1.0, t0 = t, t1 = t - 1.0, t2 = t - 2.0, t3 = t - 3.0;

        const double l0 =  (tm1 * t0 * t1 * t2 * t3) / -120.0;
        const double l1 =  (tm2 * t0 * t1 * t2 * t3) /   24.0;
        const double l2 =  (tm2 * tm1 * t1 * t2 * t3) / -12.0;
        const double l3 =  (tm2 * tm1 * t0 * t2 * t3) /   12.0;
        const double l4 =  (tm2 * tm1 * t0 * t1 * t3) /  -24.0;
        const double l5 =  (tm2 * tm1 * t0 * t1 * t2) /  120.0;

        return x[0] * l0 + x[1] * l1 + x[2] * l2 + x[3] * l3 + x[4] * l4 + x[5] * l5;
    }

    std::vector<double> buffer;
    int mask = 0;
    int writeIndex = 0;

    Interpolation mode = Interpolation::Lagrange5;

    double apState = 0.0, apInput = 0.0;
    double lastCoeff = 0.0;
    bool haveLastCoeff = false;
};

} // namespace luthier
