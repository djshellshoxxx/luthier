#pragma once

/*  Reference pedals: reusable analogue-circuit primitives.

    spec/reference-pedalboard/ARCHITECTURE.md, "Model primitives". Header-only,
    double precision, no heap use anywhere (fixed-size members only), every
    function noexcept. Each process() sanitises its input (non-finite -> 0,
    clamped to +-kMaxVolts) and its output, so any finite input gives a finite
    output. Voltages are in volts, resistances in ohms, capacitances in farads.

    Nonlinear stages (DiodeClipperPair, BJTCommonEmitter, OpAmpStage's rail)
    expect to be run oversampled: see OversampledStage.
*/

#include "../DSP/Common/DspCommon.h"
#include "../DSP/Common/Oversampler.h"

#include <algorithm>
#include <cmath>

namespace luthier::refpedals::circuit
{

inline constexpr double kMaxVolts = 1.0e6;   ///< input guard, far above any real node

/** Non-finite -> 0, otherwise clamped to +-kMaxVolts. */
inline double guard (double x) noexcept
{
    if (! std::isfinite (x))
        return 0.0;

    return x > kMaxVolts ? kMaxVolts : (x < -kMaxVolts ? -kMaxVolts : x);
}

inline double fastExpClamped (double x, double limit = 40.0) noexcept
{
    return std::exp (x > limit ? limit : x);
}

//==============================================================================
/** ZDF (topology-preserving) bilinear one-pole from R and C. The corner is
    prewarped exactly: g = tan (pi fc / fs) with fc = 1 / (2 pi R C), so the
    response is -3.01 dB at fc for any fc below Nyquist. */
class OnePoleRC
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        setCutoff (fc);
        reset();
    }

    void reset() noexcept { s = 0.0; }

    void setRC (double resistance, double capacitance) noexcept
    {
        const double rc = resistance * capacitance;
        setCutoff (rc > 1.0e-15 ? 1.0 / (constants::kTwoPi * rc) : 0.49 * fs);
    }

    void setCutoff (double hz) noexcept
    {
        fc = std::isfinite (hz) ? std::clamp (hz, 1.0e-3, 0.499 * fs) : 1000.0;
        const double g = std::tan (constants::kPi * fc / fs);
        G = g / (1.0 + g);
    }

    double getCutoff() const noexcept { return fc; }

    /** Low-pass output; the high-pass is available through lastHighpass(). */
    inline double processLP (double x) noexcept
    {
        x = guard (x);
        const double v = (x - s) * G;
        const double lp = v + s;
        s = flushDenormal (lp + v);
        hp = x - lp;
        return lp;
    }

    inline double processHP (double x) noexcept
    {
        processLP (x);
        return hp;
    }

    double lastHighpass() const noexcept { return hp; }

private:
    double fs = 44100.0, fc = 1000.0, G = 0.0, s = 0.0, hp = 0.0;
};

//==============================================================================
/** Biquad designed from analogue prototype coefficients:

        H(s) = (b0 + b1 s + b2 s^2) / (a0 + a1 s + a2 s^2)

    mapped with the bilinear transform s = K (1 - z^-1) / (1 + z^-1). With
    prewarpHz > 0, K = w / tan (w / 2fs) at w = 2 pi prewarpHz, so the analogue
    and digital responses match exactly at that frequency; otherwise K = 2fs. */
class AnalogueBiquad
{
public:
    void design (double b0, double b1, double b2, double a0, double a1, double a2,
                 double sampleRate, double prewarpHz = 0.0) noexcept
    {
        const double fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        double K = 2.0 * fs;

        if (prewarpHz > 0.0 && prewarpHz < 0.499 * fs)
        {
            const double w = constants::kTwoPi * prewarpHz;
            K = w / std::tan (0.5 * w / fs);
        }

        const double K2 = K * K;
        const double B0 = b0 + b1 * K + b2 * K2, B1 = 2.0 * (b0 - b2 * K2), B2 = b0 - b1 * K + b2 * K2;
        const double A0 = a0 + a1 * K + a2 * K2, A1 = 2.0 * (a0 - a2 * K2), A2 = a0 - a1 * K + a2 * K2;

        if (! std::isfinite (A0) || std::abs (A0) < 1.0e-300)
        {
            c0 = 1.0; c1 = c2 = d1 = d2 = 0.0;
            return;
        }

        c0 = B0 / A0; c1 = B1 / A0; c2 = B2 / A0;
        d1 = A1 / A0; d2 = A2 / A0;

        if (! (std::isfinite (c0) && std::isfinite (c1) && std::isfinite (c2)
               && std::isfinite (d1) && std::isfinite (d2)))
        {
            c0 = 1.0; c1 = c2 = d1 = d2 = 0.0;
        }
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    inline double process (double x) noexcept
    {
        x = guard (x);
        double y = c0 * x + z1;

        if (! std::isfinite (y) || std::abs (y) > kMaxVolts)
        {
            reset();
            return 0.0;
        }

        z1 = flushDenormal (c1 * x - d1 * y + z2);
        z2 = flushDenormal (c2 * x - d2 * y);
        return y;
    }

private:
    double c0 = 1.0, c1 = 0.0, c2 = 0.0, d1 = 0.0, d2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

//==============================================================================
struct DiodeParams
{
    double Is = 2.52e-9;    ///< saturation current, A
    double n = 1.752;       ///< ideality factor
    double Vt = 0.02585;    ///< thermal voltage, V
    int stackPositive = 1;  ///< series diodes conducting for v > 0
    int stackNegative = 1;  ///< series diodes conducting for v < 0

    static DiodeParams d1N914 (int pos = 1, int neg = 1) noexcept { return { 2.52e-9, 1.752, 0.02585, pos, neg }; }
    static DiodeParams germanium (int pos = 1, int neg = 1) noexcept { return { 200.0e-9, 1.3, 0.02585, pos, neg }; }
    static DiodeParams led (int pos = 1, int neg = 1) noexcept { return { 1.0e-15, 2.5, 0.02585, pos, neg }; }
};

/** Anti-parallel diode pair (optionally an asymmetric 2+1 stack) in parallel
    with a capacitor, solved per sample.

    Node equation, trapezoidal capacitor:
        iSrc - gPar v - (k (v - v1) - ic1) - Id (v) = 0,   k = 2C/T
    f is strictly monotone, so Newton has one root; the step uses SPICE-style
    pn-junction limiting so exp() never overflows, at most kMaxIterations
    iterations, seeded by the previous sample's voltage. No Lambert W. */
class DiodeClipperPair
{
public:
    static constexpr int kMaxIterations = 8;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        update();
        reset();
    }

    void reset() noexcept { v = 0.0; ic = 0.0; }

    void setDiodes (const DiodeParams& p) noexcept { diode = p; update(); }

    /** Series resistance in shunt mode. */
    void setResistance (double ohms) noexcept { R = ohms > 1.0e-3 ? ohms : 1.0e-3; }

    void setCapacitance (double farads) noexcept { C = farads > 0.0 ? farads : 0.0; update(); }

    /** Shunt network: vin -- R --+-- node, node to ground = C || diode pair.
        Returns the node voltage. */
    inline double process (double vin) noexcept
    {
        const double g = 1.0 / R;
        return solve (guard (vin) * g, g);
    }

    /** Feedback network: a source current into a node that holds Rf || C ||
        diode pair to ground (the op-amp feedback arm). Returns the node voltage. */
    inline double processCurrent (double iIn, double Rf) noexcept
    {
        iIn = guard (iIn);
        return solve (iIn, Rf > 1.0e-3 ? 1.0 / Rf : 1.0e3);
    }

    double lastVoltage() const noexcept { return v; }

    /** Diode pair current at node voltage u. */
    double diodeCurrent (double u) const noexcept
    {
        return diode.Is * (fastExpClamped (u / vsP) - fastExpClamped (-u / vsN));
    }

private:
    void update() noexcept
    {
        vsP = std::max (1.0e-6, (double) std::max (1, diode.stackPositive) * diode.n * diode.Vt);
        vsN = std::max (1.0e-6, (double) std::max (1, diode.stackNegative) * diode.n * diode.Vt);
        k = C > 0.0 ? 2.0 * C * fs : 0.0;
    }

    inline double solve (double iSrc, double gPar) noexcept
    {
        const double vMaxP = 30.0 * vsP, vMaxN = 30.0 * vsN;
        const double v1 = v;
        double u = v1;

        for (int it = 0; it < kMaxIterations; ++it)
        {
            const double eP = fastExpClamped (u / vsP, 30.0), eN = fastExpClamped (-u / vsN, 30.0);
            const double id = diode.Is * (eP - eN);
            const double gd = diode.Is * (eP / vsP + eN / vsN);
            const double f = iSrc - gPar * u - (k * (u - v1) - ic) - id;
            double dv = f / (gPar + k + gd);

            // pn-junction limiting of large upward steps
            const double vs = dv > 0.0 ? vsP : vsN;
            const double a = std::abs (dv) / vs;

            if (a > 2.0)
                dv = (dv > 0.0 ? 1.0 : -1.0) * vs * (2.0 + std::log (a - 1.0));

            u = std::clamp (u + dv, -vMaxN, vMaxP);

            if (std::abs (dv) < 1.0e-10)
                break;
        }

        if (! std::isfinite (u))
            u = 0.0;

        ic = flushDenormal (k * (u - v1) - ic);
        v = flushDenormal (u);
        return v;
    }

    DiodeParams diode;
    double fs = 44100.0, R = 4700.0, C = 0.0, k = 0.0;
    double vsP = 0.0453, vsN = 0.0453;
    double v = 0.0, ic = 0.0;

public:
    DiodeClipperPair() noexcept { update(); }
};

//==============================================================================
/** Soft rail: linear to 80% of the rail, then a tanh knee that approaches
    +-vrail asymptotically (continuous in value and slope). */
inline double softRail (double x, double vrail) noexcept
{
    if (! (vrail > 1.0e-6))
        return 0.0;

    const double knee = 0.8 * vrail, room = vrail - knee, ax = std::abs (x);

    if (ax <= knee)
        return x;

    const double y = knee + room * std::tanh ((ax - knee) / room);
    return x < 0.0 ? -y : y;
}

/** Non-inverting op-amp stage. Gain 1 + Rf/Rg; closed-loop bandwidth
    GBW / gain (one pole); slew rate limit; soft rail clipping at +-Vrail. */
class OpAmpStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        pole.prepare (fs);
        configure();
        reset();
    }

    void reset() noexcept { pole.reset(); y = 0.0; }

    void setGainResistors (double Rf, double Rg) noexcept
    {
        gain = 1.0 + (Rf > 0.0 ? Rf : 0.0) / (Rg > 1.0e-3 ? Rg : 1.0e-3);
        configure();
    }

    void setGBW (double hz) noexcept { gbw = hz > 1.0 ? hz : 1.0; configure(); }
    void setSlewVperUs (double vPerUs) noexcept { slew = vPerUs > 1.0e-6 ? vPerUs : 1.0e-6; configure(); }
    void setRail (double volts) noexcept { rail = volts > 0.1 ? volts : 0.1; }

    double getGain() const noexcept { return gain; }

    inline double process (double x) noexcept
    {
        const double target = softRail (pole.processLP (gain * guard (x)), rail);
        const double d = std::clamp (target - y, -maxStep, maxStep);
        y = flushDenormal (y + d);
        return y;
    }

    double maxStepPerSample() const noexcept { return maxStep; }

private:
    void configure() noexcept
    {
        pole.setCutoff (gbw / std::max (1.0, gain));
        maxStep = slew * 1.0e6 / fs;
    }

    OnePoleRC pole;
    double fs = 44100.0, gain = 1.0, gbw = 1.0e6, slew = 0.5, rail = 4.5;
    double maxStep = 1.0e9, y = 0.0;

public:
    OpAmpStage() noexcept { configure(); }
};

//==============================================================================
struct BJTParams
{
    double Is = 1.0e-14;       ///< transport saturation current, A
    double betaF = 100.0;
    double betaR = 2.0;
    double Vt = 0.02585;
    double Icbo = 0.0;         ///< collector-base leakage, A (germanium: ~1e-6 .. 1e-4)

    static BJTParams silicon() noexcept { return {}; }
    static BJTParams germanium (double leakage = 20.0e-6) noexcept { return { 2.0e-7, 80.0, 2.0, 0.02585, leakage }; }
};

/** NPN Ebers-Moll transport model in a common-emitter stage:

        Vin -- Rb --> base,  emitter -- Re -- gnd,  collector -- Rc -- Vcc

    The unknowns are (Vbe, Vbc); the two node equations are solved with a
    bounded 2D Newton iteration (max kMaxIterations), seeded by the previous
    sample, with pn-junction step limiting. process() returns the collector
    voltage (DC-coupled; the caller blocks DC where the circuit has a cap). */
class BJTCommonEmitter
{
public:
    static constexpr int kMaxIterations = 8;

    void prepare (double) noexcept { reset(); }

    void reset() noexcept { vbe = 0.0; vbc = -vcc; lastIc = 0.0; }

    void setTransistor (const BJTParams& p) noexcept { bjt = p; }
    void setSupply (double volts) noexcept { vcc = volts > 0.1 ? volts : 0.1; }
    void setResistors (double Rb_, double Rc_, double Re_) noexcept
    {
        Rb = Rb_ > 1.0e-3 ? Rb_ : 1.0e-3;
        Rc = Rc_ > 1.0e-3 ? Rc_ : 1.0e-3;
        Re = Re_ > 0.0 ? Re_ : 0.0;
    }
    void setBaseResistance (double ohms) noexcept { Rb = ohms > 1.0e-3 ? ohms : 1.0e-3; }

    inline double process (double vin) noexcept
    {
        vin = guard (vin);
        const double Is = bjt.Is, Vt = bjt.Vt, bf = bjt.betaF, br = bjt.betaR;
        double be = vbe, bc = vbc;

        for (int it = 0; it < kMaxIterations; ++it)
        {
            const double ef = fastExpClamped (be / Vt, 35.0), er = fastExpClamped (bc / Vt, 35.0);
            const double If = Is * (ef - 1.0), Ir = Is * (er - 1.0);
            const double gf = Is * ef / Vt, gr = Is * er / Vt;

            const double Ic = If - Ir * (1.0 + 1.0 / br) + bjt.Icbo;
            const double Ib = If / bf + Ir / br - bjt.Icbo;
            const double Ie = Ic + Ib;

            const double dIc_e = gf, dIc_c = -gr * (1.0 + 1.0 / br);
            const double dIb_e = gf / bf, dIb_c = gr / br;
            const double dIe_e = dIc_e + dIb_e, dIe_c = dIc_c + dIb_c;

            const double F1 = vin - (be + Re * Ie) - Rb * Ib;
            const double F2 = vcc - (be + Re * Ie - bc) - Rc * Ic;

            const double J11 = -(1.0 + Re * dIe_e) - Rb * dIb_e, J12 = -Re * dIe_c - Rb * dIb_c;
            const double J21 = -(1.0 + Re * dIe_e) - Rc * dIc_e, J22 = 1.0 - Re * dIe_c - Rc * dIc_c;

            const double det = J11 * J22 - J12 * J21;

            if (! (std::abs (det) > 1.0e-30))
                break;

            double de = (-F1 * J22 + F2 * J12) / det;
            double dc = (-F2 * J11 + F1 * J21) / det;

            de = limitStep (de, Vt);
            dc = limitStep (dc, Vt);

            if (! (std::isfinite (de) && std::isfinite (dc)))
                break;

            be = std::clamp (be + de, -kVMax, 1.0);
            bc = std::clamp (bc + dc, -kVMax, 1.0);

            if (std::abs (de) < 1.0e-10 && std::abs (dc) < 1.0e-10)
                break;
        }

        if (std::isfinite (be) && std::isfinite (bc))
        {
            vbe = be;
            vbc = bc;
        }

        const double ef = fastExpClamped (vbe / Vt, 35.0), er = fastExpClamped (vbc / Vt, 35.0);
        lastIc = Is * (ef - 1.0) - Is * (er - 1.0) * (1.0 + 1.0 / br) + bjt.Icbo;
        const double out = vcc - Rc * lastIc;
        return std::isfinite (out) ? std::clamp (out, -kMaxVolts, kMaxVolts) : 0.0;
    }

    double lastCollectorCurrent() const noexcept { return lastIc; }
    double getSupply() const noexcept { return vcc; }

private:
    static constexpr double kVMax = 200.0;

    static inline double limitStep (double d, double Vt) noexcept
    {
        const double a = std::abs (d) / Vt;

        if (a > 4.0)
            d = (d > 0.0 ? 1.0 : -1.0) * Vt * (4.0 + std::log (a - 3.0));

        return d;
    }

    BJTParams bjt;
    double vcc = 9.0, Rb = 33000.0, Rc = 10000.0, Re = 0.0;
    double vbe = 0.0, vbc = -9.0, lastIc = 0.0;
};

/** Behavioural two-transistor direct-coupled fuzz core: Q1 collector feeds the
    Q2 base, Q2 collector feeds back to the Q1 base through Rfb. The loading of
    Q2's base on Q1's collector is not modelled; the feedback is one sample late. */
class FuzzFaceCore
{
public:
    void prepare (double sampleRate) noexcept
    {
        q1.prepare (sampleRate);
        q2.prepare (sampleRate);
        q1.setResistors (33000.0, 8200.0, 0.0);
        q2.setResistors (1000.0, 470.0, 0.0);
        reset();
    }

    void reset() noexcept { q1.reset(); q2.reset(); fb = 0.0; }

    void setTransistors (const BJTParams& a, const BJTParams& b) noexcept { q1.setTransistor (a); q2.setTransistor (b); }
    void setSupply (double volts) noexcept { q1.setSupply (volts); q2.setSupply (volts); }

    /** sourceR: input impedance seen by the input cap (fuzz control + source);
        feedbackR: the Q2 collector -> Q1 base resistor. */
    void setNetwork (double sourceR, double feedbackR) noexcept
    {
        rin = sourceR > 1.0 ? sourceR : 1.0;
        rfb = feedbackR > 1.0 ? feedbackR : 1.0;
    }

    inline double process (double vin) noexcept
    {
        const double gin = 1.0 / rin, gfb = 1.0 / rfb;
        q1.setBaseResistance (1.0 / (gin + gfb));
        const double vth = (guard (vin) * gin + fb * gfb) / (gin + gfb);
        const double c1 = q1.process (vth);
        fb = q2.process (c1);
        return fb;
    }

private:
    BJTCommonEmitter q1, q2;
    double rin = 10000.0, rfb = 100000.0, fb = 0.0;
};

//==============================================================================
/** One-pole exponential smoother with a time constant in milliseconds. */
class LinearSmoother
{
public:
    void prepare (double sampleRate, double timeConstantMs, double initial = 0.0) noexcept
    {
        fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        setTimeConstantMs (timeConstantMs);
        setImmediate (initial);
    }

    void setTimeConstantMs (double ms) noexcept
    {
        const double n = (ms > 0.0 ? ms : 0.0) * 0.001 * fs;
        a = n > 1.0e-9 ? std::exp (-1.0 / n) : 0.0;
    }

    void setTarget (double t) noexcept { target = std::isfinite (t) ? t : target; }
    void setImmediate (double t) noexcept { target = value = std::isfinite (t) ? t : 0.0; }
    void reset() noexcept { value = target; }
    double getTarget() const noexcept { return target; }
    double current() const noexcept { return value; }

    inline double next() noexcept
    {
        value = target + a * (value - target);

        if (std::abs (value - target) < 1.0e-12)
            value = target;

        return value;
    }

private:
    double fs = 44100.0, a = 0.0, target = 0.0, value = 0.0;
};

//==============================================================================
/** y[n] = x[n] - x[n-1] + R y[n-1], R = exp (-2 pi fc / fs). */
class DcBlocker
{
public:
    void prepare (double sampleRate, double cutoffHz = 20.0) noexcept
    {
        fs = sampleRate > 1.0 ? sampleRate : 44100.0;
        setCutoff (cutoffHz);
        reset();
    }

    void setCutoff (double hz) noexcept
    {
        R = std::exp (-constants::kTwoPi * std::clamp (std::isfinite (hz) ? hz : 20.0, 0.01, 0.2 * fs) / fs);
    }

    void reset() noexcept { x1 = y1 = 0.0; }

    inline double process (double x) noexcept
    {
        x = guard (x);
        const double y = x - x1 + R * y1;
        x1 = x;
        y1 = std::isfinite (y) ? flushDenormal (y) : 0.0;
        return y1;
    }

private:
    double fs = 44100.0, R = 0.997, x1 = 0.0, y1 = 0.0;
};

//==============================================================================
/** Thin wrapper over luthier::Oversampler (factors 1/2/4/8, fixed storage):
    runs a callable at the oversampled rate with guarded input and output. */
class OversampledStage
{
public:
    void prepare (double baseRate, int factor) noexcept { os.prepare (baseRate, factor); }
    void reset() noexcept { os.reset(); }
    int getFactor() const noexcept { return os.getFactor(); }
    double getOversampledRate() const noexcept { return os.getOversampledRate(); }
    int getLatencySamples() const noexcept { return os.getLatencySamples(); }

    template <typename Fn>
    inline double process (double x, Fn&& shaper) noexcept
    {
        const double y = os.processSample (guard (x), [&] (double s) noexcept { return guard (shaper (s)); });
        return std::isfinite (y) ? y : 0.0;
    }

    template <typename Fn>
    void processBlock (double* data, int numSamples, Fn&& shaper) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = process (data[i], shaper);
    }

private:
    Oversampler os;
};

} // namespace luthier::refpedals::circuit
