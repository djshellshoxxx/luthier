#include "MicPlacement.h"

namespace luthier
{

namespace
{
    // mic-placement.md 2.2, the knot tables.
    constexpr double kUKnots[]      = { 0.0, 0.35, 0.9,  1.0,  1.12, 1.4 };
    constexpr double kAxisKnots[]   = { 2.5, 0.0, -4.5, -5.5, -6.5, -9.0 };
    constexpr double kCornerKnots[] = { 1.15, 1.0, 0.65, 0.60, 0.55, 0.45 };
    constexpr int kNumUKnots = 6;

    constexpr double kDistKnots[] = { 0.5, 2.5, 15.0, 30.0, 100.0, 200.0 };
    constexpr double kProxKnots[] = { 1.35, 1.0, 0.45, 0.10, 0.0, 0.0 };
    constexpr int kNumDistKnots = 6;

    // The legacy Rear placement's axis and corner values (make_irs.py POSITIONS).
    constexpr double kRearAxisDb = -6.0;
    constexpr double kRearCorner = 0.55;

    // A figure-8's null is never perfectly deep; nor is the makeup unlimited.
    constexpr double kPolarFloor = 0.03;
    constexpr double kMaxPolarMakeupDb = 18.0;

    double piecewise (const double* xs, const double* ys, int n, double x) noexcept
    {
        if (x <= xs[0])
            return ys[0];

        for (int i = 1; i < n; ++i)
        {
            if (x <= xs[i])
            {
                // This form lands exactly on a knot's value at t = 1, which is
                // what makes every anchor delta exactly zero.
                const double t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
                return ys[i - 1] * (1.0 - t) + ys[i] * t;
            }
        }

        return ys[n - 1];
    }

    double smoothstep (double e0, double e1, double x) noexcept
    {
        const double t = juce::jlimit (0.0, 1.0, (x - e0) / (e1 - e0));
        return t * t * (3.0 - 2.0 * t);
    }

    MicPlacementModel::Means computeMeans() noexcept
    {
        // Area mean over the cone face: weight 2u du over u in [0, 1].
        constexpr int steps = 2000;
        MicPlacementModel::Means m { 0.0, 0.0, 0.0, 0.0 };

        for (int i = 0; i < steps; ++i)
        {
            const double u = (i + 0.5) / steps;
            const double w = 2.0 * u / steps;
            m.axis     += w * MicPlacementModel::axisDb (u);
            m.corner   += w * MicPlacementModel::cornerFactor (u);
            m.cap      += w * MicPlacementModel::capDb (u);
            m.surround += w * MicPlacementModel::surroundDb (u);
        }

        return m;
    }

    inline double cm (double mm) noexcept { return mm * 0.1; }
}

//==============================================================================
double MicPlacementModel::axisDb (double u) noexcept
{
    return piecewise (kUKnots, kAxisKnots, kNumUKnots, std::abs (u));
}

double MicPlacementModel::cornerFactor (double u) noexcept
{
    return piecewise (kUKnots, kCornerKnots, kNumUKnots, std::abs (u));
}

double MicPlacementModel::proximity (double distCm) noexcept
{
    const double d = juce::jmax (kMinDistCm, distCm);

    if (d >= kDistKnots[kNumDistKnots - 1])
        return kProxKnots[kNumDistKnots - 1];

    for (int i = 1; i < kNumDistKnots; ++i)
    {
        if (d <= kDistKnots[i])
        {
            const double t = std::log (d / kDistKnots[i - 1]) / std::log (kDistKnots[i] / kDistKnots[i - 1]);
            return kProxKnots[i - 1] * (1.0 - t) + kProxKnots[i] * t;
        }
    }

    return kProxKnots[kNumDistKnots - 1];
}

double MicPlacementModel::capDb (double u) noexcept
{
    return 3.0 * (1.0 - smoothstep (0.0, 0.5, std::abs (u)));
}

double MicPlacementModel::surroundDb (double u) noexcept
{
    return -2.5 * smoothstep (0.8, 1.05, std::abs (u));
}

double MicPlacementModel::beamingWeight (double distCm, double coneRadiusCm) noexcept
{
    const double r = (juce::jmax (kMinDistCm, distCm)) / (1.6 * juce::jmax (1.0, coneRadiusCm));
    return 1.0 / (1.0 + r * r);
}

const MicPlacementModel::Means& MicPlacementModel::areaMeans() noexcept
{
    // Computed once, on first use (prepare calls it, so never first on the
    // audio thread); a static of plain doubles needs no allocation.
    static const Means means = computeMeans();
    return means;
}

double MicPlacementModel::distanceLevelDb (double distCm, double coneRadiusCm) noexcept
{
    const double d = juce::jmax (kMinDistCm, distCm);
    const double r = 0.35 * coneRadiusCm;
    return -20.0 * std::log10 ((d + r) / (kAnchorDistCm + r));
}

double MicPlacementModel::polarLevelDb (double a, double angleDeg) noexcept
{
    const double c = std::cos (angleDeg * constants::kPi / 180.0);
    return 20.0 * std::log10 (juce::jmax (kPolarFloor, std::abs (a + (1.0 - a) * c)));
}

double MicPlacementModel::polarSign (double a, double angleDeg) noexcept
{
    const double c = std::cos (angleDeg * constants::kPi / 180.0);
    return (a + (1.0 - a) * c) < 0.0 ? -1.0 : 1.0;
}

double MicPlacementModel::floorPathM (double distCm, double heightM) noexcept
{
    const double d = juce::jmax (kMinDistCm, distCm) * 0.01;
    return std::sqrt (d * d + 4.0 * heightM * heightM);
}

double MicPlacementModel::floorGainRaw (double distCm, double heightM, double rho) noexcept
{
    const double d = juce::jmax (kMinDistCm, distCm) * 0.01;
    return rho * d / floorPathM (distCm, heightM);
}

double MicPlacementModel::pathLengthM (double u, double distCm, double coneRadiusCm, double rearAmount) noexcept
{
    const double d = juce::jmax (kMinDistCm, distCm) * 0.01;
    const double lateral = 0.5 * std::abs (u) * coneRadiusCm * 0.01;
    return std::sqrt (d * d + lateral * lateral) + kRearExtraPathM * rearAmount;
}

void MicPlacementModel::speakerVariation (CabinetType cab, int speaker, double out[3]) noexcept
{
    out[0] = out[1] = out[2] = 0.0;

    if (speaker <= 1)
        return;

    // Deterministic per (cabinet, speaker): real 4x12 speakers differ this much.
    const auto seed = (uint64_t) 0x9E3779B97F4A7C15ull * (uint64_t) ((int) cab + 1)
                    ^ (uint64_t) 0xBF58476D1CE4E5B9ull * (uint64_t) speaker;
    RtRandom rng (seed);

    for (int i = 0; i < 3; ++i)
        out[i] = rng.nextBipolar();
}

double MicPlacementModel::floorRhoFor (int roomMaterial, bool roomOn) noexcept
{
    if (! roomOn)
        return 0.35;

    switch (roomMaterial)
    {
        case 0:  return 0.20;   // Dry
        case 1:  return 0.45;   // Wood
        case 2:  return 0.70;   // Tile
        case 3:  return 0.60;   // Stone
        default: return 0.45;
    }
}

//==============================================================================
PlacementTerms MicPlacementModel::evaluate (const Input& in) noexcept
{
    PlacementTerms t;

    const auto& geom = cabGeometry (in.cabinet);
    const auto& sp = speakerVoice (in.speaker);
    const auto& mv = micVoice (in.mic);
    const auto& polar = micPolar (in.mic);
    const auto& means = areaMeans();

    const double R = cm (coneRadiusMm (geom.sizeInches));          // cm
    const double d = juce::jmax (kMinDistCm, in.distCm);
    const double u = std::hypot (in.x, in.y);
    const double rear = juce::jlimit (0.0, 1.0, in.rearAmount);
    const double front = 1.0 - rear;

    // ---- beaming: close, the mic hears its spot; far, the whole cone ----------
    const double w  = beamingWeight (d, R);
    const double wa = beamingWeight (kAnchorDistCm, R);

    const auto eff = [] (double wt, double local, double mean) noexcept { return wt * local + (1.0 - wt) * mean; };

    const double axisFront   = eff (w,  axisDb (u),            means.axis);
    const double axisAnchor  = eff (wa, axisDb (kAnchorU),     means.axis);
    const double cornFront   = eff (w,  cornerFactor (u),      means.corner);
    const double cornAnchor  = eff (wa, cornerFactor (kAnchorU), means.corner);
    const double capFront    = eff (w,  capDb (u),             means.cap);
    const double capAnchor   = eff (wa, capDb (kAnchorU),      means.cap);
    const double surrFront   = eff (w,  surroundDb (u),        means.surround);
    const double surrAnchor  = eff (wa, surroundDb (kAnchorU), means.surround);

    // Rear replaces the positional axis and corner terms with the legacy
    // Rear values; the cap and surround are the front of the cone's.
    const double axisPos = front * axisFront + rear * kRearAxisDb;
    const double cornPos = front * cornFront + rear * kRearCorner;

    // ---- angle: the capsule's own polar response ------------------------------
    const double theta = juce::jmax (0.0, in.angleDeg);
    const double k = (1.0 - std::cos (theta * constants::kPi / 180.0)) / (1.0 - std::cos (constants::kPi * 0.25));
    const double axisTheta = -2.5 * polar.s * k;
    const double cornTheta = juce::jmax (0.3, 1.0 - 0.2 * polar.s * k);

    // ---- presence / axis --------------------------------------------------------
    t.presHz = sp.presenceHz * (1.0 + 0.03 * in.variation[0]);
    t.presDb = (axisPos + axisTheta + 0.6 * in.variation[1]) - axisAnchor;

    // ---- HF corner --------------------------------------------------------------
    const double T = juce::jmax (1.0e-3, cornPos * cornTheta);
    const double ratio = T / cornAnchor;
    const double G = juce::jlimit (-30.0, 6.0, 80.0 * std::log10 (ratio));
    t.shelfDbEach = 0.5 * G;
    t.shelfHz = juce::jmin (sp.topRollHz, mv.topHz) * std::sqrt (ratio);

    // ---- proximity ----------------------------------------------------------------
    t.proxHz = mv.proximityHz;
    t.proxDb = mv.proximityDb * (proximity (d) - 1.0);

    // ---- dust cap and surround ---------------------------------------------------------
    t.capHz = 0.85 * sp.topRollHz * (1.0 + 0.04 * in.variation[2]);
    t.capDb = front * (capFront - capAnchor);
    t.surrHz = 0.6 * t.presHz;
    t.surrDb = front * (surrFront - surrAnchor);

    // ---- rear ------------------------------------------------------------------------
    t.rearAmount = rear;
    t.rearLowpass = ! geom.openBack;

    // ---- floor bounce: only what the anchor IR does not already hold --------------------
    const double h = juce::jmax (0.05, in.speakerHeightM);
    const double hAnchor = speakerHeightM (in.cabinet, 1);
    const double gFloor = floorGainRaw (d, h, in.floorRho);
    const double gAnchor = floorGainRaw (kAnchorDistCm, hAnchor, in.floorRho);
    t.floorGain = juce::jmax (0.0, gFloor - gAnchor);
    t.floorDelaySec = (floorPathM (d, h) - d * 0.01) / kSpeedOfSound;

    // ---- level, level match, polarity -------------------------------------------------
    const double levelDist = distanceLevelDb (d, R);
    const double levelPolar = polarLevelDb (polar.a, theta);
    double levelDb = levelDist + levelPolar;

    if (in.levelMatch)
    {
        /*  An engineer sets each mic's preamp gain by ear. The distance loss is
            made up in full; the polar loss is capped at +18 dB, so a figure-8
            in its null stays audibly in its null (mic-placement.md 10). The
            floor bounce's added energy is trimmed too, so the matched level
            is the level heard. */
        levelDb += -levelDist + juce::jmin (kMaxPolarMakeupDb, -levelPolar);

        if (t.floorGain > 0.0)
            levelDb -= 10.0 * std::log10 (1.0 + t.floorGain * t.floorGain);

        // ...and by what the tone terms themselves do at 1 kHz, where the
        // engineer sets the gain: a backed-off mic hears the whole cone, and
        // its blend toward the cone's mean moves the mids a little too.
        if (! (t.proxDb == 0.0 && t.presDb == 0.0 && t.capDb == 0.0 && t.surrDb == 0.0 && t.shelfDbEach == 0.0))
            levelDb -= PlacementResponse::toneDb (t, kAnalogRate, 1000.0);
    }

    const double polarity = polarSign (polar.a, theta);
    const double rearSign = in.invertRearPolarity ? (1.0 - 2.0 * rear) : 1.0;

    t.gain = (levelDb == 0.0 ? 1.0 : dbToGain (levelDb)) * polarity * rearSign;

    // ---- time of arrival ---------------------------------------------------------------
    t.pathM = pathLengthM (u, d, R, rear);

    return t;
}

//==============================================================================
namespace
{
    struct ResponseFilters
    {
        TptSvf prox, pres, cap, surr, shelf1, shelf2, rearLp;

        void design (const PlacementTerms& t, double sr) noexcept
        {
            prox.setBell (sr, t.proxHz, MicPlacementModel::kProxQ, t.proxDb);
            pres.setBell (sr, t.presHz, MicPlacementModel::kPresQ, t.presDb);
            cap.setBell (sr, t.capHz, MicPlacementModel::kCapQ, t.capDb);
            surr.setBell (sr, t.surrHz, MicPlacementModel::kSurrQ, t.surrDb);
            shelf1.setHighShelf (sr, t.shelfHz, MicPlacementModel::kShelfQ, t.shelfDbEach);
            shelf2.setHighShelf (sr, t.shelfHz, MicPlacementModel::kShelfQ, t.shelfDbEach);
            rearLp.setLowpass (sr, MicPlacementModel::kRearLowpassHz, 0.7071);
        }
    };
}

std::complex<double> PlacementResponse::response (const PlacementTerms& t, double sr, double hz) noexcept
{
    ResponseFilters f;
    f.design (t, sr);

    auto h = f.prox.response (sr, hz) * f.pres.response (sr, hz) * f.cap.response (sr, hz)
           * f.surr.response (sr, hz) * f.shelf1.response (sr, hz) * f.shelf2.response (sr, hz);

    if (t.rearLowpass && t.rearAmount > 0.0)
        h *= (1.0 - t.rearAmount) + t.rearAmount * f.rearLp.response (sr, hz);

    if (t.floorGain > 0.0)
    {
        // The one-pole low-pass the stage runs the reflection through.
        const double a = std::exp (-constants::kTwoPi * MicPlacementModel::kFloorLowpassHz / sr);
        const double w = constants::kTwoPi * hz / sr;
        const std::complex<double> zInv = std::polar (1.0, -w);
        const auto lp = (1.0 - a) / (1.0 - a * zInv);
        const auto delay = std::polar (1.0, -w * t.floorDelaySec * sr);
        h *= 1.0 + t.floorGain * lp * delay;
    }

    return h * t.gain;
}

double PlacementResponse::magnitudeDb (const PlacementTerms& t, double sr, double hz) noexcept
{
    return gainToDb (std::abs (response (t, sr, hz)));
}

double PlacementResponse::toneDb (const PlacementTerms& t, double sr, double hz) noexcept
{
    PlacementTerms tone = t;
    tone.gain = 1.0;
    tone.floorGain = 0.0;
    tone.rearAmount = 0.0;
    return magnitudeDb (tone, sr, hz);
}

//==============================================================================
void MicPlacementStage::prepare (double sampleRate, int)
{
    sr = sampleRate;
    MicPlacementModel::areaMeans();

    const double controlRate = sr / (double) kControlInterval;

    for (auto* s : { &sx, &sy, &sd, &sa })
        s->prepare (controlRate, 0.030);

    for (auto* s : { &sRear, &sHeight, &sVar[0], &sVar[1], &sVar[2] })
        s->prepare (controlRate, 0.020);

    for (auto* s : { &gainSmooth, &floorGainSmooth, &floorDelaySmooth, &wetSmooth, &rearMixSmooth })
        s->prepare (sr, 0.020);

    // 12 ms covers 200 cm of path plus the floor at 192 kHz (mic-placement.md 5).
    const int needed = (int) std::ceil (0.012 * juce::jmax (sr, 44100.0)) + 8;
    const int size = juce::nextPowerOfTwo (juce::jmax (64, needed));
    floorBuffer.assign ((size_t) size, 0.0);
    floorMask = size - 1;
    floorIndex = 0;

    floorLp.prepare (sr);
    floorLp.setCutoff (MicPlacementModel::kFloorLowpassHz);

    prepared = true;
    snapToTargets();
    reset();
}

void MicPlacementStage::reset() noexcept
{
    for (auto* f : { &prox, &pres, &cap, &surr, &shelf1, &shelf2, &rearLp })
        f->reset();

    floorLp.reset();
    std::fill (floorBuffer.begin(), floorBuffer.end(), 0.0);
    floorIndex = 0;
    snapToTargets();
}

void MicPlacementStage::setVoice (CabinetType cab, SpeakerType spk, MicType m) noexcept
{
    const bool cabinetChanged = (cab != cabinet);
    cabinet = cab;
    speakerType = spk;
    mic = m;

    // A new cabinet is a structural change (a new IR is loading): the speaker
    // layout under the mic jumps with it rather than gliding from the old box.
    if (cabinetChanged && prepared)
    {
        const int s = resolveSpeaker (cabinet, target.speaker);
        double rho[3];
        MicPlacementModel::speakerVariation (cabinet, s, rho);
        sHeight.snapTo (speakerHeightM (cabinet, s));

        for (int i = 0; i < 3; ++i)
            sVar[i].snapTo (rho[i]);
    }
}

void MicPlacementStage::setPlacement (const MicPlacement& p) noexcept
{
    target = p;
}

MicPlacementModel::Input MicPlacementStage::getCurrentInput() const noexcept
{
    MicPlacementModel::Input in;
    in.cabinet = cabinet;
    in.speaker = speakerType;
    in.mic = mic;
    in.x = sx.getCurrent();
    in.y = sy.getCurrent();
    in.distCm = sd.getCurrent();
    in.angleDeg = sa.getCurrent();
    in.rearAmount = sRear.getCurrent();

    for (int i = 0; i < 3; ++i)
        in.variation[i] = sVar[i].getCurrent();

    in.speakerHeightM = sHeight.getCurrent();
    in.levelMatch = levelMatch;
    in.floorRho = floorRho;
    in.invertRearPolarity = invertRear;
    return in;
}

void MicPlacementStage::snapToTargets() noexcept
{
    const int spk = resolveSpeaker (cabinet, target.speaker);
    double rho[3];
    MicPlacementModel::speakerVariation (cabinet, spk, rho);

    sx.snapTo (juce::jlimit (-1.4, 1.4, target.x));
    sy.snapTo (juce::jlimit (-1.4, 1.4, target.y));
    sd.snapTo (juce::jlimit (0.0, 300.0, target.distCm));
    sa.snapTo (juce::jlimit (0.0, 180.0, target.angleDeg));
    sRear.snapTo (target.rear ? 1.0 : 0.0);
    sHeight.snapTo (speakerHeightM (cabinet, spk));

    for (int i = 0; i < 3; ++i)
        sVar[i].snapTo (rho[i]);

    terms = MicPlacementModel::evaluate (getCurrentInput());
    ++evaluateCount;
    applyTerms (false);

    gainSmooth.snapTo (terms.gain);
    rearMixSmooth.snapTo (terms.rearLowpass ? terms.rearAmount : 0.0);
    floorGainSmooth.snapTo (terms.floorGain);
    floorDelaySmooth.snapTo (terms.floorDelaySec * sr);
    wetSmooth.snapTo (bypassTarget);
    controlCountdown = 0;
}

void MicPlacementStage::applyTerms (bool glide) noexcept
{
    // The new coefficients are designed once per control tick; each filter
    // glides to them sample by sample over the next tick.
    TptSvf t[6];
    t[0].setBell (sr, terms.proxHz, MicPlacementModel::kProxQ, terms.proxDb);
    t[1].setBell (sr, terms.presHz, MicPlacementModel::kPresQ, terms.presDb);
    t[2].setBell (sr, terms.capHz, MicPlacementModel::kCapQ, terms.capDb);
    t[3].setBell (sr, terms.surrHz, MicPlacementModel::kSurrQ, terms.surrDb);
    t[4].setHighShelf (sr, terms.shelfHz, MicPlacementModel::kShelfQ, terms.shelfDbEach);
    t[5].setHighShelf (sr, terms.shelfHz, MicPlacementModel::kShelfQ, terms.shelfDbEach);

    TptSvf* live[6] = { &prox, &pres, &cap, &surr, &shelf1, &shelf2 };

    for (int i = 0; i < 6; ++i)
    {
        if (glide)
            live[i]->rampTo (t[i], kControlInterval);
        else
            live[i]->copyCoefficientsFrom (t[i]);
    }

    rearLp.setLowpass (sr, MicPlacementModel::kRearLowpassHz, 0.7071);
}

void MicPlacementStage::updateControl() noexcept
{
    // The switches' derived targets, then one step of every control smoother.
    const int spk = resolveSpeaker (cabinet, target.speaker);
    double rho[3];
    MicPlacementModel::speakerVariation (cabinet, spk, rho);

    sx.setTarget (juce::jlimit (-1.4, 1.4, std::isfinite (target.x) ? target.x : 0.35));
    sy.setTarget (juce::jlimit (-1.4, 1.4, std::isfinite (target.y) ? target.y : 0.0));
    sd.setTarget (juce::jlimit (0.0, 300.0, std::isfinite (target.distCm) ? target.distCm : 2.5));
    sa.setTarget (juce::jlimit (0.0, 180.0, std::isfinite (target.angleDeg) ? target.angleDeg : 0.0));
    sRear.setTarget (target.rear ? 1.0 : 0.0);
    sHeight.setTarget (speakerHeightM (cabinet, spk));

    for (int i = 0; i < 3; ++i)
    {
        sVar[i].setTarget (rho[i]);
        sVar[i].next();
    }

    for (auto* s : { &sx, &sy, &sd, &sa, &sRear, &sHeight })
        s->next();

    terms = MicPlacementModel::evaluate (getCurrentInput());
    ++evaluateCount;
    applyTerms (true);

    gainSmooth.setTarget (terms.gain);
    rearMixSmooth.setTarget (terms.rearLowpass ? terms.rearAmount : 0.0);
    floorGainSmooth.setTarget (terms.floorGain);
    floorDelaySmooth.setTarget (terms.floorDelaySec * sr);
    wetSmooth.setTarget (bypassTarget);
}

void MicPlacementStage::process (float* data, int numSamples) noexcept
{
    if (! prepared || data == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        if (--controlCountdown < 0)
        {
            updateControl();
            controlCountdown = kControlInterval - 1;
        }

        const double wet = wetSmooth.next();
        const double x = (double) data[i];

        double y = prox.process (x);
        y = pres.process (y);
        y = cap.process (y);
        y = surr.process (y);
        y = shelf2.process (shelf1.process (y));

        // Through the back panel: glided per sample, so a rear toggle is a
        // crossfade rather than 32-sample steps.
        {
            const double lp = rearLp.process (y);
            const double r = rearMixSmooth.next();

            if (r != 0.0)
                y += r * (lp - y);
        }

        // Floor bounce: a delayed, darkened copy off the floor image.
        floorBuffer[(size_t) floorIndex] = y;
        const double fg = floorGainSmooth.next();
        const double fd = floorDelaySmooth.next();

        if (fg > 0.0)
        {
            const double delay = juce::jlimit (1.0, (double) floorMask - 4.0, fd);
            const int di = (int) delay;
            const double frac = delay - di;

            // 4-point Lagrange around the read point.
            const double xm1 = floorBuffer[(size_t) ((floorIndex - di + 1) & floorMask)];
            const double x0  = floorBuffer[(size_t) ((floorIndex - di) & floorMask)];
            const double x1  = floorBuffer[(size_t) ((floorIndex - di - 1) & floorMask)];
            const double x2  = floorBuffer[(size_t) ((floorIndex - di - 2) & floorMask)];
            const double c0 = -frac * (frac - 1.0) * (frac - 2.0) / 6.0;
            const double c1 = (frac + 1.0) * (frac - 1.0) * (frac - 2.0) / 2.0;
            const double c2 = -(frac + 1.0) * frac * (frac - 2.0) / 2.0;
            const double c3 = (frac + 1.0) * frac * (frac - 1.0) / 6.0;
            const double reflected = c0 * xm1 + c1 * x0 + c2 * x1 + c3 * x2;
            y += fg * floorLp.process (reflected);
        }
        else
        {
            floorLp.process (0.0);
        }

        floorIndex = (floorIndex + 1) & floorMask;

        y *= gainSmooth.next();
        y = sanitise (y);

        data[i] = (float) (wet == 1.0 ? y : x + wet * (y - x));
    }
}

//==============================================================================
void SlewedDelayLine::prepare (double sampleRate, double maxSeconds)
{
    const int size = juce::nextPowerOfTwo (juce::jmax (64, (int) std::ceil (sampleRate * maxSeconds) + 8));
    buffer.assign ((size_t) size, 0.0);
    mask = size - 1;
    maxDelay = (double) size - 6.0;
    reset();
}

void SlewedDelayLine::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    writeIndex = 0;
    current = targetDelay;
}

void SlewedDelayLine::setTargetDelaySamples (double d) noexcept
{
    targetDelay = juce::jlimit (0.0, maxDelay, std::isfinite (d) ? d : 0.0);
}

void SlewedDelayLine::process (float* data, int numSamples) noexcept
{
    if (buffer.empty() || data == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        buffer[(size_t) writeIndex] = (double) data[i];

        // The delay glides: at most 0.0025 samples per sample, 4.3 cents of
        // Doppler however fast the mic is dragged.
        const double step = juce::jlimit (-kMaxSlew, kMaxSlew, targetDelay - current);
        current += step;

        if (current <= 0.0)
        {
            // Zero delay: exact pass-through (a single mic, or the nearer one).
            writeIndex = (writeIndex + 1) & mask;
            continue;
        }

        // Lagrange over x[n-di+1] .. x[n-di-2] would read the future when
        // di is 0, so below one sample the four points are x[n] .. x[n-3].
        const int di = juce::jmax (1, (int) current);
        const double frac = current - (double) di;   // in [-1, 1)

        const double xm1 = buffer[(size_t) ((writeIndex - di + 1) & mask)];
        const double x0  = buffer[(size_t) ((writeIndex - di) & mask)];
        const double x1  = buffer[(size_t) ((writeIndex - di - 1) & mask)];
        const double x2  = buffer[(size_t) ((writeIndex - di - 2) & mask)];
        const double c0 = -frac * (frac - 1.0) * (frac - 2.0) / 6.0;
        const double c1 = (frac + 1.0) * (frac - 1.0) * (frac - 2.0) / 2.0;
        const double c2 = -(frac + 1.0) * frac * (frac - 2.0) / 2.0;
        const double c3 = (frac + 1.0) * frac * (frac - 1.0) / 6.0;

        data[i] = (float) sanitise (c0 * xm1 + c1 * x0 + c2 * x1 + c3 * x2);
        writeIndex = (writeIndex + 1) & mask;
    }
}

} // namespace luthier
