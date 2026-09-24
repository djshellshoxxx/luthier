#include "ToneStack.h"

namespace luthier
{

//==============================================================================
ToneStackComponents ToneStackComponents::fender()
{
    // 5F6-A Bassman, also the Twin and Deluxe family.
    ToneStackComponents c;
    c.r1 = 250.0e3; c.r2 = 1.0e6; c.r3 = 25.0e3; c.r4 = 56.0e3;
    c.c1 = 250.0e-12; c.c2 = 20.0e-9; c.c3 = 20.0e-9;
    return c;
}

ToneStackComponents ToneStackComponents::marshall()
{
    // JTM45 / Plexi / JCM800. The bigger treble cap and smaller slope resistor
    // are what give a Marshall its forward upper midrange.
    ToneStackComponents c;
    c.r1 = 220.0e3; c.r2 = 1.0e6; c.r3 = 22.0e3; c.r4 = 33.0e3;
    c.c1 = 470.0e-12; c.c2 = 22.0e-9; c.c3 = 22.0e-9;
    return c;
}

ToneStackComponents ToneStackComponents::vox()
{
    // AC30 Top Boost. The real circuit is a different topology with a cut control;
    // these values put this network in the same place tonally - a small treble cap
    // and a large slope resistor, giving the chimey top and light midrange.
    ToneStackComponents c;
    c.r1 = 1.0e6; c.r2 = 1.0e6; c.r3 = 10.0e3; c.r4 = 100.0e3;
    c.c1 = 50.0e-12; c.c2 = 22.0e-9; c.c3 = 22.0e-9;
    return c;
}

ToneStackComponents ToneStackComponents::modern()
{
    // Rectifier / VH4 family: a deeper mid scoop and tighter bass.
    ToneStackComponents c;
    c.r1 = 250.0e3; c.r2 = 1.0e6; c.r3 = 25.0e3; c.r4 = 100.0e3;
    c.c1 = 500.0e-12; c.c2 = 22.0e-9; c.c3 = 47.0e-9;
    return c;
}

//==============================================================================
void ToneStack::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    recompute();
    reset();
}

void ToneStack::setComponents (const ToneStackComponents& c) noexcept
{
    components = c;
    recompute();
}

void ToneStack::setControls (double bass, double mid, double treble) noexcept
{
    const double b = juce::jlimit (0.0, 1.0, bass);
    const double m = juce::jlimit (0.0, 1.0, mid);
    const double t = juce::jlimit (0.0, 1.0, treble);

    if (b == bassPos && m == midPos && t == treblePos)
        return;

    bassPos = b;
    midPos = m;
    treblePos = t;

    recompute();
}

//==============================================================================
void ToneStack::recompute() noexcept
{
    const double R1 = components.r1;
    const double R2 = components.r2;
    const double R3 = components.r3;
    const double R4 = components.r4;
    const double C1 = components.c1;
    const double C2 = components.c2;
    const double C3 = components.c3;

    // Pot tapers. A real treble pot is linear; bass and mid are audio taper, which
    // is why the bass control does most of its work in the last third of its throw.
    const double t = treblePos;
    const double l = std::pow (bassPos, 1.7);
    const double m = std::pow (midPos, 1.4);

    // ---- continuous-time transfer function ----------------------------------
    // H(s) = (B1 s + B2 s^2 + B3 s^3) / (1 + A1 s + A2 s^2 + A3 s^3)

    const double B1 = t * C1 * R1
                    + m * C3 * R3
                    + l * (C1 * R2 + C2 * R2)
                    + (C1 * R3 + C2 * R3);

    const double B2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
                    - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                    + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                    + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                    + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                    + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);

    const double B3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                    - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                    + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                    + t * C1 * C2 * C3 * R1 * R3 * R4
                    - t * m * C1 * C2 * C3 * R1 * R3 * R4
                    + t * l * C1 * C2 * C3 * R1 * R2 * R4;

    const double A1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4)
                    + m * C3 * R3
                    + l * (C1 * R2 + C2 * R2);

    const double A2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4
                           + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                    - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                    + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                    + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2
                           + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                    + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4
                       + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);

    const double A3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                    - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                    + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3
                           - C1 * C2 * C3 * R1 * R3 * R4)
                    + l * C1 * C2 * C3 * R1 * R2 * R4
                    + C1 * C2 * C3 * R1 * R3 * R4;

    // ---- bilinear transform ---------------------------------------------------
    const double c = 2.0 * sr;
    const double c2 = c * c;
    const double c3 = c2 * c;

    const double bz0 =  B1 * c + B2 * c2 + B3 * c3;
    const double bz1 =  B1 * c - B2 * c2 - 3.0 * B3 * c3;
    const double bz2 = -B1 * c - B2 * c2 + 3.0 * B3 * c3;
    const double bz3 = -B1 * c + B2 * c2 - B3 * c3;

    const double az0 = 1.0 + A1 * c + A2 * c2 + A3 * c3;
    const double az1 = 3.0 + A1 * c - A2 * c2 - 3.0 * A3 * c3;
    const double az2 = 3.0 - A1 * c - A2 * c2 + 3.0 * A3 * c3;
    const double az3 = 1.0 - A1 * c + A2 * c2 - A3 * c3;

    if (std::abs (az0) < 1.0e-24 || ! std::isfinite (az0))
    {
        filter.setCoefficients (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        makeupGain = 1.0;
        return;
    }

    const double inv = 1.0 / az0;

    filter.setCoefficients (bz0 * inv, bz1 * inv, bz2 * inv, bz3 * inv,
                            az1 * inv, az2 * inv, az3 * inv);

    // ---- insertion loss compensation ------------------------------------------
    // A passive stack throws away most of the signal - typically 15 to 25 dB. Real
    // amps make that up in the following gain stage, so the plugin does too, or
    // every amp model would be mysteriously quiet. The compensation is fixed
    // rather than adaptive, so the stack's own level *changes* with the controls
    // exactly as the circuit's do.
    makeupGain = 6.5;
}

} // namespace luthier
