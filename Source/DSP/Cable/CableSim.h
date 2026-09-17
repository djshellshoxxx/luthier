#pragma once

/*  Cable simulation (engine spec 9).

    A guitar cable is a capacitor hanging off a high-impedance source. The pickup's
    inductance and the cable's capacitance form a lowpass whose corner drops as the
    cable gets longer - which is why a 10 m cable audibly dulls a Strat and a 1 m
    patch lead does not.
*/

#include "../Common/DspCommon.h"

namespace luthier
{

class CableSim
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        lowpass.prepare (sr);
        presence.reset();
        setLengthMetres (lengthM);
        reset();
    }

    void reset() noexcept
    {
        lowpass.reset();
        presence.reset();
    }

    void setEnabled (bool e) noexcept { enabled = e; }
    bool isEnabled() const noexcept { return enabled; }

    /** Cable length in metres, 0.5 to 15. */
    void setLengthMetres (double metres) noexcept
    {
        lengthM = juce::jlimit (0.5, 15.0, metres);

        // Corner frequencies from engine spec 9: 1 m -> 15 kHz, 3 m -> 12 kHz,
        // 6 m -> 9 kHz, 10 m -> 6 kHz. A reciprocal fit reproduces all four,
        // which is right: the corner goes as 1 / (R * C) and C goes with length.
        cutoffHz = juce::jlimit (2000.0, 20000.0, 16800.0 / (1.0 + lengthM * 0.185));
        lowpass.setCutoff (juce::jmin (cutoffHz, sr * 0.47));

        // Real cable/pickup pairs do not roll off cleanly: the resonance between
        // the coil and the cable capacitance leaves a small bump just below the
        // corner before the treble goes.
        presence.setPeaking (sr, juce::jlimit (1000.0, sr * 0.45, cutoffHz * 0.42),
                             1.1, juce::jmin (2.2, lengthM * 0.28));
    }

    double getLengthMetres() const noexcept { return lengthM; }
    double getCutoffHz() const noexcept { return cutoffHz; }

    inline double process (double x) noexcept
    {
        if (! enabled)
            return x;

        return sanitise (lowpass.process (presence.process (x)));
    }

private:
    double sr = 44100.0;
    double lengthM = 3.0;
    double cutoffHz = 12000.0;
    bool enabled = true;

    OnePoleLP lowpass;
    Biquad presence;
};

} // namespace luthier
