/*  Reference pedals: circuit primitives (ARCHITECTURE.md, "Model primitives"). */

#include "TestFramework.h"

#include "../ReferencePedals/CircuitPrimitives.h"

using namespace luthier;
using namespace luthier::tests;
namespace ckt = luthier::refpedals::circuit;

namespace
{
    constexpr double kPiD = 3.141592653589793238462643;

    /** Drives `fn` with a sine and returns the steady-state RMS of the second half. */
    template <typename Fn>
    double sineRms (Fn&& fn, double freq, double amp, double fs, int n)
    {
        double sum = 0.0;
        int count = 0;

        for (int i = 0; i < n; ++i)
        {
            const double y = fn (amp * std::sin (2.0 * kPiD * freq * i / fs));

            if (i >= n / 2)
            {
                sum += y * y;
                ++count;
            }
        }

        return std::sqrt (sum / count);
    }

    /** Pushes silence, +-100 square, alternating extremes and a non-finite input
        through `fn` and checks every output is finite. */
    template <typename Fn>
    bool finiteOnStress (Fn&& fn)
    {
        bool ok = true;

        for (int i = 0; i < 2000; ++i) ok = ok && std::isfinite (fn (0.0));
        for (int i = 0; i < 4000; ++i) ok = ok && std::isfinite (fn ((i / 50) % 2 ? 100.0 : -100.0));
        for (int i = 0; i < 2000; ++i) ok = ok && std::isfinite (fn (i % 2 ? 100.0 : -100.0));
        for (int i = 0; i < 500; ++i)  ok = ok && std::isfinite (fn (50.0 * std::sin (0.3 * i)));
        ok = ok && std::isfinite (fn (std::numeric_limits<double>::infinity()));
        ok = ok && std::isfinite (fn (std::numeric_limits<double>::quiet_NaN()));
        for (int i = 0; i < 2000; ++i) ok = ok && std::isfinite (fn (0.0));
        return ok;
    }
}

//==============================================================================
LUTHIER_TEST (ReferencePedalPrimitives, onePoleCornerIsMinus3dB)
{
    for (const double fs : { 44100.0, 48000.0, 96000.0 })
    {
        // R = 10k, C = 15.9nF -> fc = 1 kHz ; and a 8 kHz corner
        for (const double fc : { 1000.0, 8000.0 })
        {
            const double R = 10000.0, C = 1.0 / (2.0 * kPiD * R * fc);
            ckt::OnePoleRC lp, hp;
            lp.prepare (fs); lp.setRC (R, C);
            hp.prepare (fs); hp.setRC (R, C);
            CHECK_NEAR (lp.getCutoff(), fc, fc * 1.0e-9);

            const int n = (int) fs;
            const double dbLp = gainToDb (sineRms ([&] (double x) { return lp.processLP (x); }, fc, 1.0, fs, n) * std::sqrt (2.0));
            const double dbHp = gainToDb (sineRms ([&] (double x) { return hp.processHP (x); }, fc, 1.0, fs, n) * std::sqrt (2.0));
            CHECK_NEAR (dbLp, -3.0103, 0.05);
            CHECK_NEAR (dbHp, -3.0103, 0.05);
        }
    }

    ckt::OnePoleRC f;
    f.prepare (48000.0);
    f.setCutoff (1000.0);
    CHECK (finiteOnStress ([&] (double x) { return f.processLP (x); }));
    CHECK (finiteOnStress ([&] (double x) { return f.processHP (x); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, biquadMatchesPrototypeAtPrewarpFrequency)
{
    const double fs = 48000.0, f0 = 2000.0, w0 = 2.0 * kPiD * f0, Q = 1.0 / std::sqrt (2.0);

    // Butterworth-ish 2nd-order LP: 1 / (s^2/w0^2 + s/(Q w0) + 1)
    ckt::AnalogueBiquad lp;
    lp.design (1.0, 0.0, 0.0, 1.0, 1.0 / (Q * w0), 1.0 / (w0 * w0), fs, f0);

    const double db = gainToDb (sineRms ([&] (double x) { return lp.process (x); }, f0, 1.0, fs, 48000) * std::sqrt (2.0));
    CHECK_NEAR (db, -3.0103, 0.05);

    ckt::AnalogueBiquad dc;
    dc.design (1.0, 0.0, 0.0, 1.0, 1.0 / (Q * w0), 1.0 / (w0 * w0), fs, f0);
    double y = 0.0;
    for (int i = 0; i < 4800; ++i) y = dc.process (1.0);
    CHECK_NEAR (y, 1.0, 1.0e-6);

    // Degenerate design falls back to a pass-through
    ckt::AnalogueBiquad bad;
    bad.design (1, 0, 0, 0, 0, 0, fs);
    CHECK_NEAR (bad.process (0.5), 0.5, 1.0e-12);

    CHECK (finiteOnStress ([&] (double x) { return lp.process (x); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, diodeClipperSymmetricAndAsymmetric)
{
    const double fs = 48000.0;

    auto run = [&] (const ckt::DiodeParams& p, double amp, double& pk, double& tr)
    {
        ckt::DiodeClipperPair d;
        d.prepare (fs);
        d.setDiodes (p);
        d.setResistance (4700.0);
        d.setCapacitance (1.0e-6);
        pk = -1.0e9; tr = 1.0e9;

        for (int i = 0; i < 9600; ++i)
        {
            const double y = d.process (amp * std::sin (2.0 * kPiD * 1000.0 * i / fs));

            if (i >= 4800)
            {
                pk = std::max (pk, y);
                tr = std::min (tr, y);
            }
        }
    };

    double pk = 0, tr = 0;
    run (ckt::DiodeParams::d1N914(), 10.0, pk, tr);
    CHECK_NEAR (pk + tr, 0.0, 2.0e-3);                 // symmetric
    CHECK (pk > 0.55 && pk < 0.70);                    // 1N914 clipping level
    CHECK (tr < -0.55 && tr > -0.70);

    run (ckt::DiodeParams::d1N914 (2, 1), 10.0, pk, tr);
    CHECK (pk > 1.5 * std::abs (tr));                  // 2-diode side clips higher
    CHECK (std::abs (tr) > 0.5 && pk < 1.5);

    // Feedback-arm form is bounded too
    ckt::DiodeClipperPair fb;
    fb.prepare (fs);
    fb.setCapacitance (1.0e-6);
    double worst = 0.0;
    for (int i = 0; i < 4800; ++i)
        worst = std::max (worst, std::abs (fb.processCurrent (1.0e-3 * std::sin (0.05 * i), 4700.0)));
    CHECK (worst < 1.0 && worst > 0.3);

    ckt::DiodeClipperPair s;
    s.prepare (fs);
    s.setCapacitance (1.0e-6);
    CHECK (finiteOnStress ([&] (double x) { return s.process (x); }));

    ckt::DiodeClipperPair noCap;
    noCap.prepare (fs);
    noCap.setDiodes (ckt::DiodeParams::germanium (2, 1));
    noCap.setResistance (1000.0);
    bool bounded = true;
    for (int i = 0; i < 4000; ++i)
    {
        const double y = noCap.process (i % 2 ? 50.0 : -50.0);
        bounded = bounded && std::isfinite (y) && std::abs (y) < 3.0;
    }
    CHECK (bounded);
    CHECK (finiteOnStress ([&] (double x) { return noCap.process (x); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, opAmpGainSlewAndRail)
{
    const double fs = 48000.0;

    ckt::OpAmpStage op;
    op.prepare (fs);
    op.setGainResistors (10000.0, 1000.0);
    op.setGBW (1.0e6);
    op.setSlewVperUs (10.0);
    op.setRail (4.5);
    CHECK_NEAR (op.getGain(), 11.0, 1.0e-12);

    double y = 0.0;
    for (int i = 0; i < 2000; ++i) y = op.process (0.1);
    CHECK_NEAR (y, 1.1, 0.01);

    for (int i = 0; i < 2000; ++i) y = op.process (5.0);
    CHECK (y <= 4.5 && y > 4.3);                       // soft rail, never beyond
    for (int i = 0; i < 2000; ++i) y = op.process (-100.0);
    CHECK (y >= -4.5 && y < -4.3);

    // Slew: unity gain, slow 0.001 V/us = 1000 V/s -> 0.0208333 V per sample
    ckt::OpAmpStage sl;
    sl.prepare (fs);
    sl.setGainResistors (0.0, 1000.0);
    sl.setGBW (1.0e9);
    sl.setSlewVperUs (0.001);
    sl.setRail (9.0);
    double prev = 0.0, maxDelta = 0.0;
    for (int i = 0; i < 1000; ++i)
    {
        const double o = sl.process (3.0);
        maxDelta = std::max (maxDelta, std::abs (o - prev));
        prev = o;
    }
    CHECK_NEAR (maxDelta, 1000.0 / fs, 1.0e-3 * 1000.0 / fs + 1.0e-6);
    CHECK_NEAR (prev, 3.0, 1.0e-3);                    // reached the target eventually

    // Soft rail is linear below the knee and monotonic
    CHECK_NEAR (ckt::softRail (3.0, 9.0), 3.0, 1.0e-12);
    CHECK (ckt::softRail (100.0, 9.0) <= 9.0);
    CHECK (ckt::softRail (8.0, 9.0) > ckt::softRail (7.5, 9.0));

    CHECK (finiteOnStress ([&] (double x) { return op.process (x); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, bjtCommonEmitter)
{
    ckt::BJTCommonEmitter q;
    q.prepare (48000.0);
    q.setSupply (9.0);
    q.setResistors (33000.0, 10000.0, 0.0);

    CHECK (finiteOnStress ([&] (double x) { return q.process (x); }));

    // Cutoff at zero drive, saturated at high drive, monotone in between
    q.reset();
    double quiescent = 0.0;
    for (int i = 0; i < 50; ++i) quiescent = q.process (0.0);
    CHECK_NEAR (quiescent, 9.0, 0.05);

    double last = 1.0e9;
    bool monotone = true;
    for (double v = 0.3; v <= 1.0; v += 0.01)
    {
        double c = 0.0;
        for (int i = 0; i < 20; ++i) c = q.process (v);
        monotone = monotone && c <= last + 1.0e-6;
        last = c;
    }
    CHECK (monotone);
    CHECK (last < 1.0);                                // driven into saturation

    // Germanium leakage pulls the collector down at zero drive
    ckt::BJTCommonEmitter si, ge;
    si.prepare (48000.0); ge.prepare (48000.0);
    si.setResistors (33000.0, 10000.0, 0.0);
    ge.setResistors (33000.0, 10000.0, 0.0);
    ge.setTransistor (ckt::BJTParams::germanium (50.0e-6));
    double vsi = 0.0, vge = 0.0;
    for (int i = 0; i < 50; ++i) { vsi = si.process (0.0); vge = ge.process (0.0); }
    CHECK (vge < vsi - 0.1);

    ckt::FuzzFaceCore ff;
    ff.prepare (48000.0);
    CHECK (finiteOnStress ([&] (double x) { return ff.process (x); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, smootherTimeConstant)
{
    const double fs = 48000.0;

    for (const double ms : { 5.0, 20.0, 100.0 })
    {
        ckt::LinearSmoother s;
        s.prepare (fs, ms, 0.0);
        s.setTarget (1.0);
        int n = 0;
        while (s.next() < 1.0 - std::exp (-1.0) && n < 1000000) ++n;
        const double measuredMs = 1000.0 * (n + 1) / fs;
        CHECK_NEAR (measuredMs, ms, ms * 0.05);
    }

    ckt::LinearSmoother s;
    s.prepare (fs, 10.0, 0.5);
    s.setTarget (-2.0);
    double v = 0.0;
    for (int i = 0; i < 48000; ++i) v = s.next();
    CHECK_NEAR (v, -2.0, 1.0e-9);
    CHECK (finiteOnStress ([&] (double x) { s.setTarget (x); return s.next(); }));
}

LUTHIER_TEST (ReferencePedalPrimitives, dcBlockerAndOversampler)
{
    const double fs = 48000.0;

    ckt::DcBlocker dc;
    dc.prepare (fs, 20.0);
    double y = 0.0;
    for (int i = 0; i < 48000; ++i) y = dc.process (1.0);
    CHECK_NEAR (y, 0.0, 1.0e-6);

    dc.reset();
    const double r = sineRms ([&] (double x) { return dc.process (x); }, 1000.0, 1.0, fs, 48000);
    CHECK_NEAR (r * std::sqrt (2.0), 1.0, 0.01);
    CHECK (finiteOnStress ([&] (double x) { return dc.process (x); }));

    for (const int factor : { 1, 2, 4 })
    {
        ckt::OversampledStage os;
        os.prepare (fs, factor);
        CHECK (os.getFactor() == factor);
        const double g = sineRms ([&] (double x) { return os.process (x, [] (double s) noexcept { return s; }); },
                                  1000.0, 1.0, fs, 48000);
        CHECK_NEAR (g * std::sqrt (2.0), 1.0, 0.02);

        ckt::DiodeClipperPair d;
        d.prepare (os.getOversampledRate());
        d.setCapacitance (1.0e-6);
        os.reset();
        CHECK (finiteOnStress ([&] (double x) { return os.process (x, [&] (double s) noexcept { return d.process (s); }); }));
    }
}
