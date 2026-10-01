#pragma once

/*  Passive tone stack, modelled from the circuit rather than cascaded EQ bands.

    Pitfall 10 in the engine spec: a naive chain of shelving filters gets the
    *interaction* between bass, mid and treble wrong. In the real circuit the three
    pots sit in one passive RC network, so turning the mids down also changes where
    the bass and treble corners land, and the whole network has insertion loss that
    varies with the settings. A guitarist notices this immediately - it is why a
    Marshall with the mids at zero sounds scooped rather than just quieter in the
    middle.

    The transfer function below is the standard three-pot Fender/Marshall topology
    derived by nodal analysis (the form published in David Yeh's work on the 5F6-A
    Bassman stack), discretised with the bilinear transform.
*/

#include "../Common/DspCommon.h"

namespace luthier
{

//==============================================================================
/** Direct-form-I third-order IIR, in double precision. */
class ThirdOrderFilter
{
public:
    void reset() noexcept
    {
        x1 = x2 = x3 = 0.0;
        y1 = y2 = y3 = 0.0;
    }

    void setCoefficients (double nb0, double nb1, double nb2, double nb3,
                          double na1, double na2, double na3) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; b3 = nb3;
        a1 = na1; a2 = na2; a3 = na3;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 + b3 * x3
                         - a1 * y1 - a2 * y2 - a3 * y3;

        x3 = x2; x2 = x1; x1 = x;
        y3 = y2; y2 = y1; y1 = flushDenormal (y);

        return y;
    }

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, b3 = 0.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double x1 = 0.0, x2 = 0.0, x3 = 0.0;
    double y1 = 0.0, y2 = 0.0, y3 = 0.0;
};

//==============================================================================
/** Component values for one amp's tone stack. */
struct ToneStackComponents
{
    double r1 = 250.0e3;   ///< Treble pot.
    double r2 = 1.0e6;     ///< Bass pot.
    double r3 = 25.0e3;    ///< Mid pot.
    double r4 = 56.0e3;    ///< Slope resistor.
    double c1 = 250.0e-12; ///< Treble cap.
    double c2 = 20.0e-9;   ///< Bass cap.
    double c3 = 20.0e-9;   ///< Mid cap.

    /** The three classic sets. */
    static ToneStackComponents fender();    ///< 5F6-A Bassman / Twin / Deluxe
    static ToneStackComponents marshall();  ///< Plexi / JCM800
    static ToneStackComponents vox();       ///< AC30 Top Boost
    static ToneStackComponents modern();    ///< Rectifier / VH4 style
};

//==============================================================================
class ToneStack
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept { filter.reset(); }

    /** cpu-quality-modes 2.2: a new rate with the filter state kept. */
    void setSampleRateKeepingState (double sampleRate) noexcept;

    void setComponents (const ToneStackComponents& c) noexcept;
    const ToneStackComponents& getComponents() const noexcept { return components; }

    /** All three in 0 to 1. Recomputes the whole network. */
    void setControls (double bass, double mid, double treble) noexcept;

    double getBass() const noexcept { return bassPos; }
    double getMid() const noexcept { return midPos; }
    double getTreble() const noexcept { return treblePos; }

    inline double process (double x) noexcept { return filter.process (x) * makeupGain; }

private:
    void recompute() noexcept;

    double sr = 44100.0;
    ToneStackComponents components;

    double bassPos = 0.5, midPos = 0.5, treblePos = 0.5;
    double makeupGain = 1.0;

    ThirdOrderFilter filter;
};

} // namespace luthier
