#include "NoiseFloor.h"
#include "NoiseEngine.h"

namespace luthier
{

namespace
{
    // Per-source salts, so each source's random stream is its own (0.3).
    constexpr juce::uint32 kSaltFluor  = 0x0F1u;
    constexpr juce::uint32 kSaltGround = 0x6D0u;
    constexpr juce::uint32 kSaltCable  = 0xCAB1u;
    constexpr juce::uint32 kSaltRadio  = 0x4AD1u;
    constexpr juce::uint32 kSaltHiss   = 0x415Eu;
    constexpr juce::uint32 kSaltAmp    = 0xA3Bu;
    constexpr juce::uint32 kSaltMic    = 0x31Cu;

    inline double dbToLinear (double db) noexcept { return std::pow (10.0, db / 20.0); }

    /*  Targets (noise-floor.md 2), in dB re the reference-pluck peak, at 1.0
        unless stated. The generators are normalised to unit RMS, so the gain is
        the target. The cable's is a peak target; its event peaks at about 1. */
    constexpr double kFluorescentDb = -45.0;   // RMS at the DI
    constexpr double kGroundLoopDb  = -45.0;   // RMS at the amp input
    constexpr double kRadioDb       = -55.0;   // RMS at the amp input
    constexpr double kAmpHissDbAtHalf = -100.0;// input-referred RMS at 0.5
    constexpr double kCablePeakDb   = -42.0;   // mean event peak, 3 m standard

    /*  The magnetic path's own gain into the DI: the electric mix gives the
        pickup (1 - 0.55 x body amount) of the signal, about 0.88 on a solid
        body, and the circuit at 10 passes the hum band at about unity. The
        fluorescent is calibrated at the DI, so this is taken back out. */
    constexpr double kMagneticPathGain = 0.88;
}

//==============================================================================
double NoiseFloor::positionGain (double angleDegrees, double distanceMetres) noexcept
{
    // 2.1. The defaults are exact, so the legacy hum is untouched (NF-06).
    const double theta = juce::degreesToRadians (juce::jlimit (0.0, 90.0, angleDegrees));
    const double gAngle = angleDegrees == 0.0 ? 1.0 : 0.12 + 0.88 * std::abs (std::cos (theta));

    const double d = juce::jlimit (0.05, 50.0, distanceMetres);
    const double gDist = distanceMetres == 1.0 ? 1.0 : 0.4 + 0.6 / (d * d);

    return gAngle * gDist;
}

double NoiseFloor::johnsonVoltsRms (double ohms) noexcept
{
    return std::sqrt (4.0 * kBoltzmann * kRoomKelvin * juce::jmax (0.0, ohms) * kNoiseBandwidthHz);
}

double NoiseFloor::hissResistance (const CircuitComponents& parts) noexcept
{
    // An acoustic's transducer has no coil; its preamp's input resistor
    // stands in, at the same order as a coil's DC resistance.
    const double coil = parts.hasCoil ? parts.coilResistance : 10.0e3;
    const double wiper = GuitarCircuit::taperFraction (parts.volume, parts.taper) * parts.volumePot;

    // The wiper's section to ground is in parallel with the rest of the
    // network as seen from the output; at volume 10 it is the whole pot, which
    // is much larger than the coil and so adds little, as the spec's 6 k says.
    const double parallel = wiper > 1.0 ? (wiper * coil) / (wiper + coil) : coil;
    return juce::jmax (coil, parallel);
}

double NoiseFloor::fluorescentGain() noexcept { return kReferencePluckPeak * dbToLinear (kFluorescentDb) / kMagneticPathGain; }
double NoiseFloor::groundLoopGain() noexcept  { return kReferencePluckPeak * dbToLinear (kGroundLoopDb); }
double NoiseFloor::radioGain() noexcept       { return kReferencePluckPeak * dbToLinear (kRadioDb); }
double NoiseFloor::ampHissGain() noexcept     { return kReferencePluckPeak * dbToLinear (kAmpHissDbAtHalf) / 0.5; }
double NoiseFloor::cableGain() noexcept       { return kReferencePluckPeak * dbToLinear (kCablePeakDb); }

//==============================================================================
void NoiseFloor::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    for (auto* b : { &pickupBuf, &circuitInBuf, &diBuf, &ampInBuf, &prevAmpOut, &ampTotalIn })
        b->assign ((size_t) maxBlock, 0.0);

    // 2.2: each arc re-strike rings a 3.5 kHz band, plus a 200 Hz body.
    fluorBand.setBandpass (sr, 3500.0, 2.5);
    fluorBody.setLowpass (sr, 200.0, 0.707);

    // 2.6: speech band.
    radioHp.setHighpass (sr, 300.0, 0.707);
    radioLp.setLowpass (sr, 3000.0, 0.707);

    // 2.7: Kellet's three pinking poles, designed at 44.1 kHz; the pole
    // frequencies (16.5 Hz, 265 Hz, 3.95 kHz) are kept at every rate.
    pinkA0 = std::exp (-constants::kTwoPi * 16.5 / sr);
    pinkA1 = std::exp (-constants::kTwoPi * 264.6 / sr);
    pinkA2 = std::exp (-constants::kTwoPi * 3945.0 / sr);

    micDc.prepare (sr, 7.0);

    for (auto& v : cable)
    {
        v.thump.setLowpass (sr, 80.0, 0.707);
        v.crackle.setHighpass (sr, 1000.0, 0.707);
    }

    // ---- unit-RMS normalisers, measured on one second of each generator ------
    {
        const int n = (int) sr;

        // Fluorescent: the impulse train through its two filters.
        Biquad band = fluorBand, body = fluorBody;
        band.reset(); body.reset();
        double phase = 0.0, sum = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double before = phase;
            phase += 60.0 / sr;
            const bool strike = (before < 0.5 && phase >= 0.5) || phase >= 1.0;
            if (phase >= 1.0) phase -= 1.0;

            const double x = strike ? sr / 48000.0 : 0.0;
            const double y = band.process (x) + 0.25 * body.process (x);
            sum += y * y;
        }

        fluorNorm = 1.0 / std::sqrt (juce::jmax (1.0e-20, sum / n));

        // Radio: the speech band on white noise (the envelope's own mean
        // square is part of the target, so it is not normalised away).
        Biquad hp = radioHp, lp = radioLp;
        hp.reset(); lp.reset();
        RtRandom r { 0x5EEDull };
        sum = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double y = lp.process (hp.process (r.nextGaussian()));
            sum += y * y;
        }

        radioNorm = 1.0 / std::sqrt (juce::jmax (1.0e-20, sum / n));

        // Amp hiss: white blended with pink at 0.5.
        double p0 = 0.0, p1 = 0.0, p2 = 0.0;
        sum = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double w = r.nextGaussian();
            p0 = pinkA0 * p0 + w * 0.0990460;
            p1 = pinkA1 * p1 + w * 0.2965164;
            p2 = pinkA2 * p2 + w * 1.0526913;
            const double y = 0.5 * w + 0.5 * 0.35 * (p0 + p1 + p2 + w * 0.1848);
            sum += y * y;
        }

        hissNorm = 1.0 / std::sqrt (juce::jmax (1.0e-20, sum / n));
    }

    if (! seeded)
        setSeed (0);

    reset();
}

void NoiseFloor::setSeed (uint64_t newSeed) noexcept
{
    const auto folded = (juce::uint32) (newSeed ^ (newSeed >> 32));

    if (seeded && folded == seed)
        return;

    seed64 = newSeed;
    seed = folded;
    seeded = true;

    buildGroundLoopTable (seed);

    // 2.8: the tube's electrode resonance, per instance.
    micHz = 2500.0 + 4500.0 * noiseUniform (seed ^ kSaltMic, 1);
    micQ  = 20.0 + 30.0 * noiseUniform (seed ^ kSaltMic, 2);
    const double f = juce::jmin (micHz, sr * 0.45);
    micRes.setBandpass (sr, f, micQ);
    micResIn = micRes;
    micResOut = micRes;
}

void NoiseFloor::buildGroundLoopTable (uint32_t s) noexcept
{
    /*  2.5: harmonics 1-20 at k^-0.8, the 2nd +6 dB for the rectifier's
        charging pulses, fixed phases from the seed. One cycle of the mains,
        so the region only changes the read rate and nothing is rebuilt on a
        region change. The highest partial is 1.2 kHz, well inside any rate. */
    std::array<double, 21> phase {};

    for (int k = 1; k <= 20; ++k)
        phase[(size_t) k] = constants::kTwoPi * noiseUniform (s ^ kSaltGround, (juce::uint32) k);

    double sumSq = 0.0;

    for (int i = 0; i < kTableSize; ++i)
    {
        const double t = (double) i / (double) kTableSize;
        double v = 0.0;

        for (int k = 1; k <= 20; ++k)
        {
            const double a = std::pow ((double) k, -0.8) * (k == 2 ? 2.0 : 1.0);
            v += a * std::sin (constants::kTwoPi * k * t + phase[(size_t) k]);
        }

        groundTable[(size_t) i] = v;
        sumSq += v * v;
    }

    const double norm = 1.0 / std::sqrt (juce::jmax (1.0e-20, sumSq / kTableSize));

    for (int i = 0; i < kTableSize; ++i)
        groundTable[(size_t) i] *= norm;

    groundTable[(size_t) kTableSize] = groundTable[0];
}

void NoiseFloor::reset() noexcept
{
    // 0.3: reseeding on reset makes two offline renders sample-identical.
    mainsPhase = 0.0;
    fluorIndex = 0;
    fluorBand.reset();
    fluorBody.reset();

    hissRng.setSeed ((uint64_t) (seed ^ kSaltHiss) + 1);
    ampRng.setSeed ((uint64_t) (seed ^ kSaltAmp) + 1);
    radioRng.setSeed ((uint64_t) (seed ^ kSaltRadio) + 1);

    for (auto& v : cable)
    {
        v.active = false;
        v.env = 0.0;
        v.thump.reset();
        v.crackle.reset();
    }

    rollIndex = voiceAge = 0;
    ringSeconds = 0.0;
    cableEvents = 0;

    radioHp.reset();
    radioLp.reset();
    radioEnv = radioEnvTarget = 0.0;
    radioSyllablePhase = whistlePhase = whistleLfoPhase = 0.0;
    radioSegmentLeft = 0;
    radioInPhrase = false;

    pink0 = pink1 = pink2 = 0.0;

    micRes.reset();
    micResIn.reset();
    micResOut.reset();
    micDc.reset();
    micAmpGain = 1.0;
    micState = 0.0;
    prevAmpOutCount = 0;

    std::fill (prevAmpOut.begin(), prevAmpOut.end(), 0.0);
    std::fill (ampTotalIn.begin(), ampTotalIn.end(), 0.0);

    meterDb.store (-240.0, std::memory_order_relaxed);
}

bool NoiseFloor::isIdle() const noexcept
{
    if (settings.anySourceOn())
        return false;

    for (const auto& v : cable)
        if (v.active)
            return false;

    return true;
}

//==============================================================================
void NoiseFloor::onNoteOn (double velocity) noexcept
{
    // 2.4: each note-on rolls p = amount x 0.02 x v^2.
    if (settings.cableMovement <= 0.0)
        return;

    const double v = juce::jlimit (0.0, 1.0, velocity);
    const double p = settings.cableMovement * 0.02 * v * v;
    const auto index = ++rollIndex;

    if (noiseUniform (seed ^ kSaltCable, index) < p)
        startCableEvent (noiseHash (seed ^ kSaltCable, index + 0x40000000u));
}

void NoiseFloor::startCableEvent (juce::uint32 hash) noexcept
{
    // The oldest voice is stolen (2.4).
    CableVoice* voice = &cable[0];

    for (auto& v : cable)
    {
        if (! v.active) { voice = &v; break; }
        if (v.age < voice->age) voice = &v;
    }

    voice->active = true;
    voice->age = ++voiceAge;
    voice->rng.setSeed ((uint64_t) hash + 1);

    const double seconds = 0.030 + 0.120 * voice->rng.nextDouble();   // 30-150 ms
    const int count = 5 + voice->rng.nextInt (36);                       // 5-40 impulses

    voice->samplesLeft = juce::jmax (1, (int) (seconds * sr * 4.0));
    voice->decay = std::exp (-1.0 / juce::jmax (1.0, seconds * sr));
    voice->crackleProb = (double) count / juce::jmax (1.0, seconds * sr);
    voice->env = 1.0;
    voice->thump.reset();
    voice->crackle.reset();

    ++cableEvents;
}

double NoiseFloor::nextCable() noexcept
{
    double out = 0.0;

    // The thump is noise through an 80 Hz 2-pole; its RMS for unit white noise
    // is small and rate-dependent, so it is scaled to about unit peak.
    const double thumpScale = 3.0 * std::sqrt (sr / (2.0 * 80.0 * 1.11));

    for (auto& v : cable)
    {
        if (! v.active)
            continue;

        const double thump = v.thump.process (v.rng.nextGaussian()) * thumpScale * 0.35;
        const double impulse = v.rng.nextDouble() < v.crackleProb ? v.rng.nextBipolar() : 0.0;
        const double crackle = v.crackle.process (impulse);

        out += (thump + crackle) * v.env;

        v.env *= v.decay;

        if (--v.samplesLeft <= 0 || v.env < 1.0e-5)
            v.active = false;
    }

    return out;
}

double NoiseFloor::nextRadio() noexcept
{
    // 2.6: phrases of syllables (3-6 Hz, 60% depth) and gaps of 0.2-1.5 s.
    if (--radioSegmentLeft <= 0)
    {
        radioInPhrase = ! radioInPhrase;
        const double seconds = radioInPhrase ? 1.0 + 3.0 * radioRng.nextDouble()
                                             : 0.2 + 1.3 * radioRng.nextDouble();
        radioSegmentLeft = juce::jmax (1, (int) (seconds * sr));
        radioSyllableRate = 3.0 + 3.0 * radioRng.nextDouble();
    }

    radioSyllablePhase += radioSyllableRate / sr;
    if (radioSyllablePhase >= 1.0) radioSyllablePhase -= 1.0;

    radioEnvTarget = radioInPhrase ? 1.0 - 0.6 * (0.5 - 0.5 * std::sin (constants::kTwoPi * radioSyllablePhase)) : 0.0;
    radioEnv += (radioEnvTarget - radioEnv) * (1.0 - std::exp (-1.0 / (0.010 * sr)));

    const double speech = radioLp.process (radioHp.process (radioRng.nextGaussian())) * radioNorm * radioEnv;

    // The heterodyne whistle drifts 4-8 kHz at 0.05 Hz, 12 dB under the programme.
    whistleLfoPhase += 0.05 / sr;
    if (whistleLfoPhase >= 1.0) whistleLfoPhase -= 1.0;

    const double whistleHz = juce::jmin (sr * 0.45, 6000.0 + 2000.0 * std::sin (constants::kTwoPi * whistleLfoPhase));
    whistlePhase += whistleHz / sr;
    if (whistlePhase >= 1.0) whistlePhase -= 1.0;

    return speech + 0.25 * std::sqrt (2.0) * std::sin (constants::kTwoPi * whistlePhase);
}

double NoiseFloor::nextAmpHiss() noexcept
{
    const double w = ampRng.nextGaussian();
    pink0 = flushDenormal (pinkA0 * pink0 + w * 0.0990460);
    pink1 = flushDenormal (pinkA1 * pink1 + w * 0.2965164);
    pink2 = flushDenormal (pinkA2 * pink2 + w * 1.0526913);
    return (0.5 * w + 0.5 * 0.35 * (pink0 + pink1 + pink2 + w * 0.1848)) * hissNorm;
}

//==============================================================================
void NoiseFloor::beginBlock (int numSamples, double singleCoilShare, const CircuitComponents& parts,
                             bool anyRinging, bool separateHead) noexcept
{
    const int n = juce::jmin (numSamples, maxBlock);
    const auto& st = settings;

    posGain = positionGain (st.angleDegrees, st.distanceMetres);
    shareNow = juce::jlimit (0.0, 1.0, singleCoilShare);

    const double dt = (double) n / sr;
    const double mainsInc = juce::jlimit (40.0, 70.0, st.mainsHz) / sr;

    // ---- per-block levels ----------------------------------------------------
    const double fluorLevel = st.fluorescent * shareNow * posGain * fluorescentGain() * fluorNorm;
    const double hissSigma = st.passiveHiss > 0.0
                               ? st.passiveHiss * johnsonVoltsRms (hissResistance (parts)) / kEmfVoltsPerUnit
                               : 0.0;

    const double lengthFactor = parts.cableOn ? juce::jmax (0.0, parts.cableLength) / 3.0 : 0.0;
    const double quality = parts.cableQuality == CableQuality::studio  ? 0.5
                         : parts.cableQuality == CableQuality::cheap   ? 2.0
                         : parts.cableQuality == CableQuality::vintage ? 1.5 : 1.0;

    cableLevelNow = st.cableMovement * lengthFactor * quality * cableGain();
    radioLevelNow = st.radio * lengthFactor * radioGain();
    const double groundLevel = st.groundLoop * groundLoopGain();
    const double ampHissLevel = st.ampHiss * ampHissGain();

    // 2.8: the loop gain, with the amp's own gain at the resonance divided out
    // and clamped below 0.95 in every mode, 0.2 of that for a separate head.
    const double gm = juce::jmin (0.95, st.microphonics * 0.5) * (separateHead ? 0.2 : 1.0);
    const double micGain = gm / juce::jmax (1.0, micAmpGain);

    // ---- 2.4: weight-shift rolls while a string rings --------------------------
    if (st.cableMovement > 0.0 && anyRinging && lengthFactor > 0.0)
    {
        ringSeconds += dt;

        while (ringSeconds >= 1.0)
        {
            ringSeconds -= 1.0;
            const auto index = ++rollIndex;

            if (noiseUniform (seed ^ kSaltCable, index) < st.cableMovement * 0.01)
                startCableEvent (noiseHash (seed ^ kSaltCable, index + 0x40000000u));
        }
    }

    const bool cableSounding = [this] { for (auto& v : cable) if (v.active) return true; return false; }();

    double sumSq = 0.0;

    for (int i = 0; i < n; ++i)
    {
        // ---- shared mains phase (2) -------------------------------------------
        const double before = mainsPhase;
        mainsPhase += mainsInc;
        const bool halfCycle = (before < 0.5 && mainsPhase >= 0.5) || mainsPhase >= 1.0;
        if (mainsPhase >= 1.0) mainsPhase -= 1.0;

        // ---- 2.2 fluorescent, magnetic ---------------------------------------
        double pickup = 0.0;

        if (fluorLevel > 0.0)
        {
            double x = 0.0;

            if (halfCycle)
                x = (1.0 + 0.15 * (2.0 * noiseUniform (seed ^ kSaltFluor, ++fluorIndex) - 1.0)) * (sr / 48000.0);

            pickup = sanitise ((fluorBand.process (x) + 0.25 * fluorBody.process (x)) * fluorLevel);
        }

        // ---- 2.3 passive hiss, at the EMF --------------------------------------
        const double circuitIn = hissSigma > 0.0 ? hissRng.nextGaussian() * hissSigma : 0.0;

        // ---- 2.4 cable, after the circuit ---------------------------------------
        const double di = cableSounding ? sanitise (nextCable() * cableLevelNow) : 0.0;

        // ---- 2.5-2.8 at the amp input -------------------------------------------
        double ampIn = 0.0;

        if (groundLevel > 0.0)
        {
            const double pos = mainsPhase * kTableSize;
            const int idx = (int) pos;
            const double frac = pos - idx;
            ampIn += groundLevel * (groundTable[(size_t) idx] * (1.0 - frac) + groundTable[(size_t) idx + 1] * frac);
        }

        if (radioLevelNow > 0.0)
            ampIn += radioLevelNow * nextRadio();

        if (ampHissLevel > 0.0)
            ampIn += ampHissLevel * nextAmpHiss();

        if (gm > 0.0)
        {
            const double tap = i < prevAmpOutCount ? prevAmpOut[(size_t) i] : 0.0;
            micState = sanitise (micDc.process (micRes.process (tap) * micGain));
            ampIn += micState;
        }

        pickupBuf[(size_t) i] = pickup;
        circuitInBuf[(size_t) i] = circuitIn;
        diBuf[(size_t) i] = di;
        ampInBuf[(size_t) i] = sanitise (ampIn);
        ampTotalIn[(size_t) i] = 0.0;

        const double s = pickup * kMagneticPathGain + circuitIn + di + ampIn;
        sumSq += s * s;
    }

    // ---- meter (5): the new sources plus the hum, dB re the reference pluck ----
    const double humRms = humForMeter * 0.0022 * shareNow * posGain * 0.757 * kMagneticPathGain;
    const double total = sumSq / juce::jmax (1, n) + humRms * humRms;
    meterDb.store (gainToDb (std::sqrt (total) / kReferencePluckPeak), std::memory_order_relaxed);
}

void NoiseFloor::pushAmpOutput (const double* data, int numSamples) noexcept
{
    const int n = juce::jmin (numSamples, (int) prevAmpOut.size());

    if (settings.microphonics <= 0.0 || data == nullptr)
    {
        prevAmpOutCount = 0;
        return;
    }

    // The amp's gain at the resonance, measured on the signal it just played,
    // so G_m is the loop gain whatever the amp's own gain is (2.8).
    double eIn = 0.0, eOut = 0.0;

    for (int i = 0; i < n; ++i)
    {
        prevAmpOut[(size_t) i] = data[i];

        const double a = micResIn.process (ampTotalIn[(size_t) i]);
        const double b = micResOut.process (data[i]);
        eIn += a * a;
        eOut += b * b;
    }

    prevAmpOutCount = n;

    if (eIn > 1.0e-18)
    {
        const double measured = std::sqrt (eOut / eIn);
        // Rises at once, falls slowly: the guard must never lag an increase.
        micAmpGain = measured > micAmpGain ? measured : micAmpGain * 0.9 + measured * 0.1;
        micAmpGain = juce::jlimit (1.0, 1.0e5, micAmpGain);
    }
}

} // namespace luthier
