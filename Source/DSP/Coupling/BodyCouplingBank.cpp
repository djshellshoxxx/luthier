#include "BodyCouplingBank.h"

#include <algorithm>

namespace luthier
{

namespace
{
    /** A tap's peak force, in the bank's units: about what a firm pluck puts on the bridge. */
    constexpr double kTapForce = 2.0;

    /** 7: body_coupling_amount's 20 ms linear smoothing. */
    constexpr double kAmountRampSeconds = 0.020;

    /** 7: frequency-scale automation glides at 0.2 % per block... */
    constexpr double kFreqSlewPerBlock = 0.002;

    /** ...and the resonators are re-designed only past 0.05 %. */
    constexpr double kRedesignThreshold = 0.0005;

    bool movedBy (double a, double b, double fraction) noexcept
    {
        return std::abs (a - b) > fraction * juce::jmax (1.0e-9, std::abs (b));
    }

    double slewToward (double now, double target) noexcept
    {
        const double limit = std::abs (now) * kFreqSlewPerBlock;
        return now + juce::jlimit (-limit, limit, target - now);
    }
}

//==============================================================================
double BodyCouplingBank::baseMass (Chambering c) noexcept
{
    switch (c)
    {
        case Chambering::acoustic:   return 0.10;
        case Chambering::hollow:     return 0.20;
        case Chambering::semiHollow: return 0.60;
        case Chambering::chambered:  return 2.0;
        case Chambering::solid:
        case Chambering::numChamberings:
        default:                     return 10.0;
    }
}

BridgeCoupling BodyCouplingBank::bridgeFor (int whammyBridgeType, bool acoustic, bool resonator) noexcept
{
    // part-acoustics.md 5's table. An acoustic is a pin bridge (or a
    // resonator's spider); an electric goes by its bridge's type.
    if (resonator)
        return { 0.045, 0.88 };

    if (acoustic)
        return { 0.028, 0.92 };

    switch (whammyBridgeType)
    {
        case 1:  return { 0.165, 0.45 };   // vintage tremolo
        case 2:  return { 0.320, 0.30 };   // Floyd Rose
        case 3:  return { 0.150, 0.48 };   // TransTrem: a two-point-like tremolo
        case 4:  return { 0.480, 0.35 };   // Bigsby
        case 0:
        default: return { 0.110, 0.70 };   // hardtail, strings through
    }
}

//==============================================================================
void BodyCouplingBank::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);

    tapLength = juce::jmax (2, (int) std::lround (kTapSeconds * sr));
    amountStep = 1.0 / juce::jmax (1.0, kAmountRampSeconds * sr);
    needsRedesign = true;
    reset();
}

void BodyCouplingBank::reset() noexcept
{
    beginBlock();

    resetState();

    scalingNow = scalingTarget;
    amountNow = amountTarget;
    tapAmplitude = 0.0;
    tapSample = tapLength;
    samplesRun = samplesCapped = 0;
    needsRedesign = true;
    redesignResonators();
}

//==============================================================================
BodyCouplingDesign BodyCouplingBank::design (const BodyConfig& body, Chambering chambering,
                                             const BridgeCoupling& bridge, const double* z0, int numStrings)
{
    BodyCouplingDesign d;
    d.numStrings = juce::jlimit (1, kMaxStrings, numStrings);
    d.coupling = juce::jlimit (0.0, 1.0, bridge.coupling);
    d.chambering = chambering;

    for (int s = 0; s < d.numStrings; ++s)
        d.z0[(size_t) s] = z0 != nullptr ? juce::jlimit (0.0, 20.0, z0[(size_t) s]) : 0.4;

    // One body, two views (ground rule 3): the radiated body's own modes.
    BodyModels::buildModes (body, scratch);

    scratch.erase (std::remove_if (scratch.begin(), scratch.end(),
                                   [] (const BodyMode& m) { return m.frequencyHz >= kMaxModeHz || m.gain <= 0.0; }),
                   scratch.end());

    std::stable_sort (scratch.begin(), scratch.end(),
                      [] (const BodyMode& a, const BodyMode& b) { return a.gain > b.gain; });

    const double gMax = scratch.empty() ? 1.0 : juce::jmax (1.0e-6, scratch.front().gain);
    const double mBase = baseMass (chambering);
    const double extra = juce::jmax (0.0, bridge.massKg);

    d.count = juce::jmin ((int) scratch.size(), BodyCouplingDesign::kMaxModes);

    for (int k = 0; k < d.count; ++k)
    {
        const auto& mode = scratch[(size_t) k];
        d.f[(size_t) k] = mode.frequencyHz;
        d.q[(size_t) k] = juce::jlimit (0.5, 250.0, mode.q);
        d.m[(size_t) k] = mBase / juce::jmax (0.05, mode.gain / gMax) + extra;
        d.isAir[(size_t) k] = mode.isAir;
    }

    return d;
}

void BodyCouplingBank::stage (const BodyCouplingDesign& d) noexcept
{
    const juce::SpinLock::ScopedLockType lock (stageLock);
    staged = d;
    pending = d;
    stagedReady.store (true);
}

//==============================================================================
BodyCouplingBank::ModeView BodyCouplingBank::viewMode (const BodyCouplingDesign& d, int k,
                                                       const BodyCouplingScaling& s) noexcept
{
    ModeView v {};

    if (! juce::isPositiveAndBelow (k, d.count))
        return v;

    v.isAir = d.isAir[(size_t) k];
    v.hz = d.f[(size_t) k] * (v.isAir ? s.airFreq : s.plateFreq);
    v.q = juce::jlimit (0.05, 1250.0, d.q[(size_t) k] * (v.isAir ? s.airQ : s.q));
    v.massKg = juce::jmax (1.0e-4, d.m[(size_t) k] * s.mass);

    const double w = constants::kTwoPi * juce::jmax (1.0, v.hz);
    const double loading = d.totalImpedance() / (v.massKg * w);
    v.loadedQ = 1.0 / (1.0 / v.q + loading);
    v.peakAdmittance = juce::jlimit (0.0, kMaxKappa, d.coupling) * v.loadedQ / (v.massKg * w);
    return v;
}

double BodyCouplingBank::realAdmittance (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s,
                                         double hz, double amount) noexcept
{
    double re = 0.0;

    for (int i = 0; i < juce::jmin (k, d.count); ++i)
    {
        const auto v = viewMode (d, i, s);

        // A resonance's real part: peak x 1 / (1 + Q'^2 (f/f0 - f0/f)^2).
        const double r = hz / juce::jmax (1.0, v.hz) - v.hz / juce::jmax (1.0, hz);
        re += v.peakAdmittance / (1.0 + v.loadedQ * v.loadedQ * r * r);
    }

    return re * juce::jlimit (0.0, 1.0, amount);
}

double BodyCouplingBank::predictedLoss (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s,
                                        double amount, double f0, double z0, double sigmaString) noexcept
{
    // Weak coupling: each round trip loses 2 z0 Re Y', f0 times a second.
    // The fundamental carries the decay a listener hears, with the first two
    // partials weighted in behind it.
    double sigmaBody = 0.0;

    for (int n = 1; n <= 3; ++n)
        sigmaBody += (1.0 / (double) n) * 2.0 * f0 * z0 * realAdmittance (d, k, s, f0 * (double) n, amount);

    sigmaBody /= (1.0 + 0.5 + 1.0 / 3.0);
    sigmaString = juce::jmax (1.0e-6, sigmaString);
    return juce::jlimit (0.0, 1.0, 1.0 - sigmaString / (sigmaString + sigmaBody));
}

void BodyCouplingBank::wolfMap (const BodyCouplingDesign& d, int k, const BodyCouplingScaling& s, double amount,
                                const double* openHz, const double* sigmaOpen, int numStrings, int numFrets,
                                float* dest) noexcept
{
    for (int str = 0; str < numStrings; ++str)
    {
        for (int fret = 0; fret <= numFrets; ++fret)
        {
            const double ratio = std::pow (2.0, fret / 12.0);
            const double f0 = openHz[str] * ratio;
            const double sigma = sigmaOpen[str] * std::pow (ratio, 0.40);
            const double z0 = d.z0[(size_t) juce::jlimit (0, kMaxStrings - 1, str)];

            dest[str * (numFrets + 1) + fret] = (float) predictedLoss (d, k, s, amount, f0, z0, sigma);
        }
    }
}

//==============================================================================
void BodyCouplingBank::resetState() noexcept
{
    z1.fill (0.0);
    z2.fill (0.0);
}

void BodyCouplingBank::redesignResonators() noexcept
{
    runningCount = juce::jmin (modeCount, active.count);
    const double zTot = active.totalImpedance();

    for (int k = 0; k < BodyCouplingDesign::kMaxModes; ++k)
    {
        if (k >= runningCount)
        {
            cb0[(size_t) k] = ca1[(size_t) k] = ca2[(size_t) k] = 0.0;
            z1[(size_t) k] = z2[(size_t) k] = 0.0;
            modeHz[(size_t) k] = modeQ[(size_t) k] = modeGain[(size_t) k] = 0.0;
            continue;
        }

        const auto v = viewMode (active, k, scalingNow);

        // The band-pass peaks at 1; its gain is Y' at the peak. A backstop
        // keeps the loaded peak under unity where the design has had to
        // clamp Q (advanced extremes only).
        double gain = v.peakAdmittance;

        if (zTot > 0.0)
            gain = juce::jmin (gain, kMaxKappa / zTot);

        const double hz = juce::jlimit (20.0, sr * 0.47, v.hz);
        const double q = juce::jlimit (0.05, 1250.0, v.loadedQ);
        const double w0 = constants::kTwoPi * hz / sr;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        cb0[(size_t) k] = gain * alpha / a0;
        ca1[(size_t) k] = -2.0 * std::cos (w0) / a0;
        ca2[(size_t) k] = (1.0 - alpha) / a0;

        modeHz[(size_t) k] = hz;
        modeQ[(size_t) k] = q;
        modeGain[(size_t) k] = gain;
    }

    /*  The diagonal approximation loads each mode on its own. Where broad
        modes overlap (the advanced Q and mass extremes) their sum can pass the
        passivity bound Re Y >= Z_tot |Y|^2 even though each one alone is
        inside it. Check the summed admittance at each mode's centre and half-
        power points and scale the bank back under it: a no-op for any real
        body, whose modes are separated. */
    if (zTot > 0.0 && runningCount > 0)
    {
        double worst = 0.0;

        for (int k = 0; k < runningCount; ++k)
        {
            for (double offset : { -0.5, 0.0, 0.5 })
            {
                const double f = modeHz[(size_t) k] * (1.0 + offset / juce::jmax (0.05, modeQ[(size_t) k]));

                if (f <= 0.0)
                    continue;

                double re = 0.0, im = 0.0;

                for (int j = 0; j < runningCount; ++j)
                {
                    // Y_j = D_j / (1 + j Q_j (f/f_j - f_j/f))
                    const double x = modeQ[(size_t) j] * (f / modeHz[(size_t) j] - modeHz[(size_t) j] / f);
                    const double d = modeGain[(size_t) j] / (1.0 + x * x);
                    re += d;
                    im -= d * x;
                }

                if (re > 1.0e-12)
                    worst = juce::jmax (worst, zTot * (re * re + im * im) / re);
            }
        }

        if (worst > kMaxKappa)
        {
            const double scale = kMaxKappa / worst;

            for (int k = 0; k < runningCount; ++k)
            {
                cb0[(size_t) k] *= scale;
                modeGain[(size_t) k] *= scale;
            }
        }
    }

    scalingDesigned = scalingNow;
    needsRedesign = false;
}

void BodyCouplingBank::beginBlock() noexcept
{
    if (stagedReady.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (stageLock);

        if (lock.isLocked())
        {
            active = pending;
            stagedReady.store (false);
            needsRedesign = true;
        }
    }

    // 7: frequency scaling glides; Q and mass step (they are block-rate already).
    scalingNow.plateFreq = slewToward (scalingNow.plateFreq, scalingTarget.plateFreq);
    scalingNow.airFreq = slewToward (scalingNow.airFreq, scalingTarget.airFreq);
    scalingNow.q = scalingTarget.q;
    scalingNow.airQ = scalingTarget.airQ;
    scalingNow.mass = scalingTarget.mass;

    if (juce::jmin (modeCount, active.count) != runningCount)
        needsRedesign = true;

    if (movedBy (scalingNow.plateFreq, scalingDesigned.plateFreq, kRedesignThreshold)
        || movedBy (scalingNow.airFreq, scalingDesigned.airFreq, kRedesignThreshold)
        || movedBy (scalingNow.q, scalingDesigned.q, kRedesignThreshold)
        || movedBy (scalingNow.airQ, scalingDesigned.airQ, kRedesignThreshold)
        || movedBy (scalingNow.mass, scalingDesigned.mass, kRedesignThreshold))
        needsRedesign = true;

    if (needsRedesign)
        redesignResonators();
}

void BodyCouplingBank::driveDirect (double impulse, int position) noexcept
{
    const double force = juce::jlimit (0.0, 4.0, std::isfinite (impulse) ? impulse : 0.0);

    if (force <= 0.0)
        return;

    // string-slap-technique 1: which part is struck weights the modes. The top
    // is the soundboard and its air coupling; a side is mostly plate; the
    // back leans on the air cavity and damps the plate.
    const double air = position == 1 ? 0.4 : 1.0;
    const double plate = position == 2 ? 0.5 : (position == 1 ? 0.7 : 1.0);

    for (int k = 0; k < BodyCouplingDesign::kMaxModes; ++k)
        tapWeight[(size_t) k] = active.isAir[(size_t) k] ? air : plate;

    tapAmplitude = kTapForce * force;
    tapSample = 0;
}

template <int Groups>
void BodyCouplingBank::runModes (double force, double* lane) noexcept
{
    for (int k = 0; k < Groups * 4; ++k)
    {
        const double bx = cb0[(size_t) k] * force;
        const double y = bx + z1[(size_t) k];
        z1[(size_t) k] = z2[(size_t) k] - ca1[(size_t) k] * y;
        z2[(size_t) k] = -bx - ca2[(size_t) k] * y;
        lane[k & 3] += y;
    }
}

void BodyCouplingBank::processSample (const double* bridgeWaves, double* couplingInputs, int numStrings) noexcept
{
    // At zero, and settled there, the bank is out of the loop entirely (BC-04).
    if (amountTarget <= 0.0 && amountNow <= 0.0)
        return;

    if (amountNow < amountTarget)
        amountNow = juce::jmin (amountTarget, amountNow + amountStep);
    else if (amountNow > amountTarget)
        amountNow = juce::jmax (amountTarget, amountNow - amountStep);

    const int n = juce::jmin (numStrings, active.numStrings);

    double f4[4] = { 0.0, 0.0, 0.0, 0.0 };
    int j = 0;

    for (; j + 4 <= n; j += 4)
        for (int l = 0; l < 4; ++l)
            f4[l] += active.z0[(size_t) (j + l)] * bridgeWaves[j + l];

    for (; j < n; ++j)
        f4[0] += active.z0[(size_t) j] * bridgeWaves[j];

    const double force = 2.0 * ((f4[0] + f4[1]) + (f4[2] + f4[3]));

    // Four lanes at a time, summed per lane: element-wise work the compiler
    // vectorises, with the one reduction left to the end.
    double lane[4] = { 0.0, 0.0, 0.0, 0.0 };
    const int groups = (runningCount + 3) / 4;

    if (tapSample < tapLength)
    {
        const double tap = tapAmplitude * std::sin (constants::kPi * ((double) tapSample + 0.5) / (double) tapLength);
        ++tapSample;

        for (int g = 0; g < groups; ++g)
            for (int l = 0; l < 4; ++l)
            {
                const int k = g * 4 + l;
                const double x = force + tap * tapWeight[(size_t) k];
                const double y = cb0[(size_t) k] * x + z1[(size_t) k];
                z1[(size_t) k] = z2[(size_t) k] - ca1[(size_t) k] * y;
                z2[(size_t) k] = -cb0[(size_t) k] * x - ca2[(size_t) k] * y;
                lane[l] += y;
            }
    }
    else
    {
        // The common case, unrolled for each mode count (4, 8, 12, 16).
        switch (groups)
        {
            case 1:  runModes<1> (force, lane); break;
            case 2:  runModes<2> (force, lane); break;
            case 3:  runModes<3> (force, lane); break;
            case 4:  runModes<4> (force, lane); break;
            default: break;
        }
    }

    const double v = (lane[0] + lane[1]) + (lane[2] + lane[3]);

    double c = -sanitise (v) * amountNow;

    ++samplesRun;

    if (std::abs (c) > kEnergyCap)
    {
        c = std::copysign (kEnergyCap, c);
        ++samplesCapped;
    }

    for (int i = 0; i < n; ++i)
        couplingInputs[i] += c;

    // Settled at zero: clear the modes so switching back on starts clean.
    if (amountTarget <= 0.0 && amountNow <= 0.0)
        resetState();
}

} // namespace luthier
