#include "AcousticMicModel.h"

namespace luthier
{

namespace
{
    // mic-placement.md 3, the radiator table: lower bout, bridge, soundhole,
    // upper bout, 12th fret. Columns: air peak, 220 Hz, 2.5 kHz, 6 kHz shelf
    // (all dB), string-direct send.
    constexpr double kRadiatorAlong[AcousticMicModel::kNumRadiators] = { 1.0, 2.0, 3.0, 3.5, 4.0 };

    constexpr double kRadiatorGains[AcousticMicModel::kNumRadiators][5] =
    {
        { 3.0,  3.0, -1.0, -3.0, 0.05 },   // Lower Bout
        { 1.0,  1.0,  2.0,  0.0, 0.20 },   // Bridge
        { 8.0,  2.0, -2.0, -4.0, 0.05 },   // Soundhole
        { 2.0,  0.0,  1.0,  1.0, 0.15 },   // Upper Bout
        { 0.0, -2.0,  1.0,  3.0, 0.35 }    // 12th Fret
    };

    constexpr double kAirQ = 2.5, kLowHz = 220.0, kLowQ = 1.0, kMidHz = 2500.0, kMidQ = 1.0;
    constexpr double kShelfHz = 6000.0, kShelfQ = 0.7071, kProxQ = 0.9, kPresQ = 1.0;
    constexpr double kSendHpHz = 300.0;

    // The default placement (mic-placement.md 7), the one K calibrates.
    const AcousticMicPlacement kDefaultPlacement {};
}

//==============================================================================
double AcousticMicModel::blurSigmaMm (double distCm) noexcept
{
    /*  mic-placement.md 3 writes the width as 60 mm + 0.9 d(mm). Taken as the
        Gaussian's sigma that blurs the radiators so much that a mic 10 cm over
        the soundhole and one over the 12th fret differ by under 1 dB at the air
        resonance, where MP-21 asks for 6 dB and a real session shows more: a
        close mic over the soundhole booms. So the width is a quarter of that
        (docs/coverage/FEAT-MIC.md): 38 mm at 10 cm, one region; 240 mm at 1 m,
        half the body's length, the whole guitar. */
    const double dMm = juce::jmax (MicPlacementModel::kMinDistCm, distCm) * 10.0;
    return 0.25 * (60.0 + 0.9 * dMm);
}

void AcousticMicModel::radiatorWeights (const AcousticLandmarks& lm, double alongX, double acrossY,
                                        double distCm, double out[kNumRadiators]) noexcept
{
    const double sigma = blurSigmaMm (distCm);
    double sum = 0.0;

    for (int i = 0; i < kNumRadiators; ++i)
    {
        const double dx = alongX - lm.alongToMm (kRadiatorAlong[i]);
        const double r2 = dx * dx + acrossY * acrossY;
        out[i] = std::exp (-r2 / (2.0 * sigma * sigma));
        sum += out[i];
    }

    if (sum < 1.0e-12)
    {
        // Far off the body in every direction: the nearest radiator wins.
        int nearest = 0;
        double best = 1.0e30;

        for (int i = 0; i < kNumRadiators; ++i)
        {
            const double dx = std::abs (alongX - lm.alongToMm (kRadiatorAlong[i]));
            if (dx < best) { best = dx; nearest = i; }
            out[i] = 0.0;
        }

        out[nearest] = 1.0;
        return;
    }

    for (int i = 0; i < kNumRadiators; ++i)
        out[i] /= sum;
}

AcousticMicTerms AcousticMicModel::evaluate (const Input& in) noexcept
{
    AcousticMicTerms t;

    const auto& p = in.placement;
    const double d = juce::jmax (MicPlacementModel::kMinDistCm, p.distCm);
    const double x = in.landmarks.alongToMm (p.along);
    const double y = in.landmarks.acrossToMm (p.across);

    radiatorWeights (in.landmarks, x, y, d, t.weights);

    double g[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

    for (int i = 0; i < kNumRadiators; ++i)
        for (int c = 0; c < 5; ++c)
            g[c] += t.weights[i] * kRadiatorGains[i][c];

    // No soundhole (a resonator, an f-hole body): no Helmholtz port, so no
    // radiated air peak anywhere on it (mic-placement.md 3, MP-22).
    if (! in.landmarks.hasSoundhole)
        g[0] = 0.0;

    t.airHz = in.airHz > 0.0 ? in.airHz : 100.0;
    t.airDb = g[0];
    t.lowDb = g[1];
    t.midDb = g[2];
    t.shelfDb = g[3];
    t.send = g[4];

    // ---- the mic's own voice ---------------------------------------------------
    const auto& mv = micVoice (in.mic);
    const auto& polar = micPolar (in.mic);
    t.proxHz = mv.proximityHz;
    t.proxDb = mv.proximityDb * MicPlacementModel::proximity (d);
    t.presHz = mv.presenceHz;
    t.presDb = mv.presenceDb * 0.5;
    t.topHz = mv.topHz;

    // ---- angle: the same capsule terms as a cabinet mic ------------------------
    const double theta = juce::jmax (0.0, p.angleDeg);
    const double k = (1.0 - std::cos (theta * constants::kPi / 180.0)) / (1.0 - std::cos (constants::kPi * 0.25));
    t.midDb += -2.5 * polar.s * k;
    // The corner factor moves a single-pole-pair roll-off: 40 log10 T.
    t.shelfDb += juce::jlimit (-30.0, 0.0, 40.0 * std::log10 (juce::jmax (0.3, 1.0 - 0.2 * polar.s * k)));

    // ---- level, level match, polarity -----------------------------------------
    const double c = 0.35 * in.landmarks.lowerBoutHalfWidthMm * 0.1;   // cm
    const double levelDist = -20.0 * std::log10 ((d + c) / (kLevelRefCm + c));
    const double levelPolar = MicPlacementModel::polarLevelDb (polar.a, theta);

    const double hFloor = kFloorHeightM;
    t.floorGain = MicPlacementModel::floorGainRaw (d, hFloor, in.floorRho);
    t.floorDelaySec = (MicPlacementModel::floorPathM (d, hFloor) - d * 0.01) / MicPlacementModel::kSpeedOfSound;

    double levelDb = levelDist + levelPolar;

    if (in.levelMatch)
    {
        levelDb += -levelDist + juce::jmin (18.0, -levelPolar);
        levelDb -= 10.0 * std::log10 (1.0 + t.floorGain * t.floorGain);
    }

    t.gain = dbToGain (levelDb) * MicPlacementModel::polarSign (polar.a, theta) * in.calibration;
    t.pathM = d * 0.01;
    return t;
}

std::complex<double> AcousticMicModel::response (const AcousticMicTerms& t, double sr, double hz) noexcept
{
    TptSvf air, low, mid, shelf, prox, pres, top;
    air.setBell (sr, t.airHz, kAirQ, t.airDb);
    low.setBell (sr, kLowHz, kLowQ, t.lowDb);
    mid.setBell (sr, kMidHz, kMidQ, t.midDb);
    shelf.setHighShelf (sr, kShelfHz, kShelfQ, t.shelfDb);
    prox.setBell (sr, t.proxHz, kProxQ, t.proxDb);
    pres.setBell (sr, t.presHz, kPresQ, t.presDb);
    top.setLowpass (sr, t.topHz, 0.7071);

    auto h = air.response (sr, hz) * low.response (sr, hz) * mid.response (sr, hz) * shelf.response (sr, hz)
           * prox.response (sr, hz) * pres.response (sr, hz) * top.response (sr, hz);

    if (t.floorGain > 0.0)
    {
        const double a = std::exp (-constants::kTwoPi * MicPlacementModel::kFloorLowpassHz / sr);
        const double w = constants::kTwoPi * hz / sr;
        const auto lp = (1.0 - a) / (1.0 - a * std::polar (1.0, -w));
        h *= 1.0 + t.floorGain * lp * std::polar (1.0, -w * t.floorDelaySec * sr);
    }

    return h * t.gain;
}

double AcousticMicModel::internalMicGainAt1k (double sr) noexcept
{
    // PickupEngine's internal mic: a -2.5 dB shelf at 4 kHz and +2 dB at 250 Hz.
    TptSvf tilt, bodyPeak;
    tilt.setHighShelf (sr, 4000.0, 0.7, -2.5);
    bodyPeak.setBell (sr, 250.0, 0.9, 2.0);
    return std::abs (tilt.response (sr, 1000.0) * bodyPeak.response (sr, 1000.0));
}

//==============================================================================
void AcousticMicModel::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    const double controlRate = sr / (double) MicPlacementStage::kControlInterval;
    const int floorSize = juce::nextPowerOfTwo (juce::jmax (64, (int) std::ceil (0.012 * juce::jmax (sr, 44100.0)) + 8));

    for (auto& m : mics)
    {
        for (auto* s : { &m.sAlong, &m.sAcross, &m.sDist, &m.sAngle })
            s->prepare (controlRate, 0.030);

        for (auto* s : { &m.gainSmooth, &m.sendSmooth, &m.floorGainSmooth, &m.floorDelaySmooth })
            s->prepare (sr, 0.020);

        m.floorBuffer.assign ((size_t) floorSize, 0.0);
        m.floorMask = floorSize - 1;
        m.floorLp.prepare (sr);
        m.floorLp.setCutoff (MicPlacementModel::kFloorLowpassHz);
        m.tof.prepare (sr, 0.012);
        m.tap.assign ((size_t) maxBlock, 0.0);
        m.scratch.assign ((size_t) maxBlock, 0.0f);
    }

    blendSmooth.prepare (sr, 0.020);
    prepared = true;
    recalibrate();
    reset();
}

void AcousticMicModel::reset() noexcept
{
    for (auto& m : mics)
    {
        for (auto* f : { &m.air, &m.low, &m.mid, &m.shelf, &m.prox, &m.pres, &m.top, &m.sendHp })
            f->reset();

        m.floorLp.reset();
        std::fill (m.floorBuffer.begin(), m.floorBuffer.end(), 0.0);
        m.floorIndex = 0;
        m.tof.reset();

        m.sAlong.snapTo (m.target.along);
        m.sAcross.snapTo (m.target.across);
        m.sDist.snapTo (m.target.distCm);
        m.sAngle.snapTo (m.target.angleDeg);
        m.terms = evaluate (makeInput (m, true));
        applyTerms (m);
        m.gainSmooth.snapTo (m.terms.gain);
        m.sendSmooth.snapTo (m.terms.send);
        m.floorGainSmooth.snapTo (m.terms.floorGain);
        m.floorDelaySmooth.snapTo (m.terms.floorDelaySec * sr);
        m.tof.snapToTarget();
    }

    blendSmooth.snapTo (blendTarget);
    controlCountdown = 0;
}

void AcousticMicModel::setBody (const AcousticLandmarks& lm, double hz) noexcept
{
    landmarks = lm;
    ++bodyVersion;
    airHz = hz > 0.0 ? hz : 100.0;
    recalibrate();
}

void AcousticMicModel::setMicType (int slot, MicType m) noexcept
{
    mics[(size_t) juce::jlimit (0, 1, slot)].type = m;
}

void AcousticMicModel::setPlacement (int slot, const AcousticMicPlacement& p) noexcept
{
    auto& t = mics[(size_t) juce::jlimit (0, 1, slot)].target;
    t.along = juce::jlimit (0.0, 4.0, std::isfinite (p.along) ? p.along : 3.6);
    t.across = juce::jlimit (-1.0, 1.0, std::isfinite (p.across) ? p.across : 0.0);
    t.distCm = juce::jlimit (0.0, 300.0, std::isfinite (p.distCm) ? p.distCm : 20.0);
    t.angleDeg = juce::jlimit (0.0, 180.0, std::isfinite (p.angleDeg) ? p.angleDeg : 0.0);
}

void AcousticMicModel::recalibrate() noexcept
{
    // K: the default placement with the default mic, at 1 kHz, level with the
    // internal mic, so mixing mic and pickup never jumps in loudness.
    Input in;
    in.landmarks = landmarks;
    in.airHz = airHz;
    in.mic = MicType::SM57;
    in.placement = kDefaultPlacement;
    in.levelMatch = true;
    in.floorRho = floorRho;
    in.calibration = 1.0;

    const auto terms = evaluate (in);
    const double atOneK = std::abs (response (terms, sr, 1000.0)) + terms.send;
    calibration = juce::jlimit (0.05, 20.0, internalMicGainAt1k (sr) / juce::jmax (1.0e-6, atOneK));
}

AcousticMicModel::Input AcousticMicModel::makeInput (const Mic& m, bool smoothed) const noexcept
{
    Input in;
    in.landmarks = landmarks;
    in.airHz = airHz;
    in.mic = m.type;
    in.placement = smoothed ? AcousticMicPlacement { m.sAlong.getCurrent(), m.sAcross.getCurrent(),
                                                     m.sDist.getCurrent(), m.sAngle.getCurrent() }
                            : m.target;
    in.levelMatch = levelMatch;
    in.floorRho = floorRho;
    in.calibration = calibration;
    return in;
}

void AcousticMicModel::applyTerms (Mic& m) noexcept
{
    const auto& t = m.terms;
    m.air.setBell (sr, t.airHz, kAirQ, t.airDb);
    m.low.setBell (sr, kLowHz, kLowQ, t.lowDb);
    m.mid.setBell (sr, kMidHz, kMidQ, t.midDb);
    m.shelf.setHighShelf (sr, kShelfHz, kShelfQ, t.shelfDb);
    m.prox.setBell (sr, t.proxHz, kProxQ, t.proxDb);
    m.pres.setBell (sr, t.presHz, kPresQ, t.presDb);
    m.top.setLowpass (sr, juce::jmin (t.topHz, sr * 0.45), 0.7071);
    m.sendHp.setHighpass (sr, kSendHpHz, 0.7071);
}

void AcousticMicModel::updateControl (Mic& m) noexcept
{
    m.sAlong.setTarget (m.target.along);
    m.sAcross.setTarget (m.target.across);
    m.sDist.setTarget (m.target.distCm);
    m.sAngle.setTarget (m.target.angleDeg);

    for (auto* s : { &m.sAlong, &m.sAcross, &m.sDist, &m.sAngle })
        s->next();

    // Only when something moved: a static mic costs its filters alone.
    const auto in = makeInput (m, true);
    const bool same = m.lastValid && m.last.mic == in.mic && m.last.airHz == in.airHz
                      && m.last.placement.along == in.placement.along && m.last.placement.across == in.placement.across
                      && m.last.placement.distCm == in.placement.distCm && m.last.placement.angleDeg == in.placement.angleDeg
                      && m.last.levelMatch == in.levelMatch && m.last.floorRho == in.floorRho
                      && m.last.calibration == in.calibration && m.bodyVersion == bodyVersion;

    if (! same)
    {
        m.terms = evaluate (in);
        applyTerms (m);
        m.last = in;
        m.lastValid = true;
        m.bodyVersion = bodyVersion;
    }

    m.gainSmooth.setTarget (m.terms.gain);
    m.sendSmooth.setTarget (m.terms.send);
    m.floorGainSmooth.setTarget (m.terms.floorGain);
    m.floorDelaySmooth.setTarget (m.terms.floorDelaySec * sr);
}

void AcousticMicModel::processBlock (const float* body, const double* strings, double* out, int numSamples) noexcept
{
    if (! prepared || body == nullptr || strings == nullptr || out == nullptr)
        return;

    numSamples = juce::jmin (numSamples, maxBlock);
    ++processCount;

    const int numMics = secondOn ? 2 : 1;
    constexpr int chunk = MicPlacementStage::kControlInterval;
    double y[chunk], send[chunk];

    // Controls advance in 32-sample steps, shared by both mics.
    for (int start = 0; start < numSamples; )
    {
        if (controlCountdown <= 0)
        {
            for (int k = 0; k < numMics; ++k)
                updateControl (mics[(size_t) k]);

            controlCountdown = chunk;
        }

        const int n = juce::jmin (controlCountdown, numSamples - start);

        for (int k = 0; k < numMics; ++k)
        {
            auto& m = mics[(size_t) k];

            for (int i = 0; i < n; ++i)
            {
                y[i] = (double) body[start + i];
                send[i] = strings[start + i];
            }

            for (auto* f : { &m.top, &m.pres, &m.prox, &m.air, &m.low, &m.mid, &m.shelf })
                for (int i = 0; i < n; ++i)
                    y[i] = f->process (y[i]);

            for (int i = 0; i < n; ++i)
                y[i] += m.sendSmooth.next() * m.sendHp.process (send[i]);

            // The floor bounce of a seated player.
            for (int i = 0; i < n; ++i)
            {
                m.floorBuffer[(size_t) m.floorIndex] = y[i];
                const double fg = m.floorGainSmooth.next();
                const double fd = juce::jlimit (1.0, (double) m.floorMask - 2.0, m.floorDelaySmooth.next());
                const int di = (int) fd;
                const double frac = fd - di;
                const double a0 = m.floorBuffer[(size_t) ((m.floorIndex - di) & m.floorMask)];
                const double a1 = m.floorBuffer[(size_t) ((m.floorIndex - di - 1) & m.floorMask)];
                y[i] += fg * m.floorLp.process (a0 + frac * (a1 - a0));
                m.floorIndex = (m.floorIndex + 1) & m.floorMask;
            }

            for (int i = 0; i < n; ++i)
                m.tap[(size_t) (start + i)] = sanitise (y[i] * m.gainSmooth.next());
        }

        controlCountdown -= n;
        start += n;
    }

    // Time of arrival between the two mics (mic-placement.md 2.3's rules).
    if (secondOn)
    {
        double delay[2] = { 0.0, 0.0 };

        if (tofMode == TofMode::Physical)
        {
            const double diff = (mics[1].terms.pathM - mics[0].terms.pathM) / MicPlacementModel::kSpeedOfSound * sr;
            delay[0] = juce::jmax (0.0, -diff);
            delay[1] = juce::jmax (0.0, diff);
        }

        for (int k = 0; k < 2; ++k)
        {
            auto& m = mics[(size_t) k];
            m.tof.setTargetDelaySamples (delay[k]);

            for (int i = 0; i < numSamples; ++i)
                m.scratch[(size_t) i] = (float) m.tap[(size_t) i];

            m.tof.process (m.scratch.data(), numSamples);

            for (int i = 0; i < numSamples; ++i)
                m.tap[(size_t) i] = (double) m.scratch[(size_t) i];
        }
    }

    // Mic 2 sums with the blend, equal-power, in mono.
    for (int i = 0; i < numSamples; ++i)
    {
        const double b = blendSmooth.next();

        if (secondOn)
        {
            blendSmooth.setTarget (blendTarget);
            out[i] = mics[0].tap[(size_t) i] * std::cos (b * constants::kPi * 0.5)
                   + mics[1].tap[(size_t) i] * std::sin (b * constants::kPi * 0.5);
        }
        else
        {
            out[i] = mics[0].tap[(size_t) i];
        }
    }

    blendSmooth.setTarget (blendTarget);
}

const double* AcousticMicModel::getMicTap (int slot) const noexcept
{
    if (slot > 0 && ! secondOn)
        return nullptr;

    return mics[(size_t) juce::jlimit (0, 1, slot)].tap.data();
}

double AcousticMicModel::getWeightedDistanceM() const noexcept
{
    const double d1 = mics[0].target.distCm * 0.01;

    if (! secondOn)
        return d1;

    return d1 * (1.0 - blendTarget) + mics[1].target.distCm * 0.01 * blendTarget;
}

} // namespace luthier
