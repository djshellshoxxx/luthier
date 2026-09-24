#include "StringEngine.h"
#include "../../Support/QualityProfile.h"
#include <complex>

namespace luthier
{

namespace
{
    /** ln(0.001): the loss over one round trip that produces a -60 dB decay. */
    constexpr double kT60Constant = 6.907755278982137;

    /** Maps the inharmonicity coefficient B onto an allpass coefficient. Calibrated
        so that the guitar-realistic range B = 8e-5 (plain high E) to 8e-4 (wound
        low E) produces a partial stretch that matches f_n = n*f0*sqrt(1 + B*n^2)
        over the first ~20 partials. StringEngineTests measures this directly. */
    constexpr double kDispersionGain = 120.0;

    /** Never let the dispersion cascade eat more than this fraction of the loop,
        or high fretted notes would run out of delay line and go flat. */
    constexpr double kMaxDispersionFraction = 0.30;

    /** Engine rule: loop gain is capped below unity so runaway is impossible. */
    constexpr double kMaxLoopGain = 0.9995;
}

//==============================================================================
void StringEngine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;

    delayLine.prepare (sr, constants::kMinStringHz);
    excitation.prepare (sr);

    loopFilter.prepare (sr);
    dcBlocker.prepare (sr, 7.0);

    levelFollower.prepare (sr);
    levelFollower.setTimes (0.0008, 0.060);

    smoothedDelay.prepare (sr, glideSeconds);

    slideNoiseFilter.prepare (sr);
    slideNoiseFilter.setCutoff (6500.0);

    stealTotal = juce::jmax (1, (int) (sr * 0.005));
    sleepAfterSamples = juce::jmax (1, (int) std::round (QualityProfile::kSleepAfterSeconds * sr));

    snapToFrequency (targetHz);
    needsLoopUpdate = true;
    updateDispersion();
    updateLoopCoefficients();
    reset();
}

void StringEngine::reset() noexcept
{
    touchGain = 1.0;
    touchSamplesLeft = 0;

    // cpu-quality-modes 2.4: awake, uncapped until the next excite latches.
    sleeping = false;
    quietSamples = 0;
    fadeLeft = 0;

    if (latchedStages != dispersionStages)
    {
        latchedStages = dispersionStages;
        cappedCompensation = 0.0;

        for (auto& ap : dispersion)
            ap.setCoefficient (dispersionCoeff);
    }

    delayLine.reset();
    excitation.reset();
    loopFilter.reset();

    for (auto& ap : dispersion)
        ap.reset();

    dcBlocker.reset();
    levelFollower.reset();
    slideNoiseFilter.reset();
    slideNoiseBand.reset();
    fretNoiseBand.reset();

    slideNoiseEnv = 0.0;
    fretNoiseEnv = 0.0;
    buzzPhase = 0.0;

    bridgeOut = 0.0;
    stealCountdown = 0;
    stealPending = false;
    stealGain = 1.0;
    sounded = false;

    damping = Damping::Open;
    dampingAmount = 1.0;
    harmonicPartial = 0;
    couplingReceptivity = 1.0;

    // Reseed, so that resetting really does return to a known state. Without
    // this the humanisation noise carries over and two renders of the same
    // preset differ - which would make offline regression testing impossible.
    rng.setSeed (0x51E3D00Dull + (uint64_t) stringIndex * 7919ull);
    lastCoefficientHz = 0.0;

    // A slide leaves a long glide behind; the next render must not inherit it.
    slideSpeed = 0.0;
    setGlideTime (0.002);

    smoothedDelay.snapTo (sr / juce::jmax (1.0, targetHz));
    needsLoopUpdate = true;
}

//==============================================================================
void StringEngine::setPhysical (const Physical& p) noexcept
{
    physical = p;
    updateDispersion();
    needsLoopUpdate = true;
}

void StringEngine::setTargetFrequency (double hz) noexcept
{
    targetHz = juce::jlimit (constants::kMinStringHz, sr * 0.45, hz);
    smoothedDelay.setTarget (sr / targetHz);
    needsLoopUpdate = true;
}

void StringEngine::snapToFrequency (double hz) noexcept
{
    targetHz = juce::jlimit (constants::kMinStringHz, sr * 0.45, hz);
    smoothedDelay.snapTo (sr / targetHz);
    needsLoopUpdate = true;
}

double StringEngine::getCurrentFrequency() const noexcept
{
    const double d = smoothedDelay.getCurrent();
    return d > 1.0 ? sr / d : targetHz;
}

void StringEngine::setGlideTime (double seconds) noexcept
{
    glideSeconds = juce::jlimit (0.0002, 2.0, seconds);
    smoothedDelay.setTime (glideSeconds);
}

//==============================================================================
void StringEngine::excite (const Excitation::Params& params) noexcept
{
    auto p = params;
    p.delaySamples = juce::jmax (4.0, smoothedDelay.getCurrent());

    // cpu-quality-modes 2.4: a note takes its dispersion stage count here and
    // keeps it until it is re-excited; a sleeping string wakes.
    wake();
    fadeLeft = 0;
    latchDispersion();

    const bool isLegato = (p.kind == Excitation::Kind::HammerOn
                           || p.kind == Excitation::Kind::PullOff
                           || p.kind == Excitation::Kind::Tap);

    // A fresh pluck on a string that is still ringing has to mute the old note
    // first, exactly as the pick would. Legato re-excitations must NOT, because
    // the whole point is that the old vibration carries through.
    if (! isLegato && levelFollower.current() > 0.004)
    {
        pendingParams = p;
        stealPending = true;
        stealCountdown = stealTotal;
        return;
    }

    excitation.trigger (p, rng);
    sounded = true;

    if (p.kind != Excitation::Kind::Harmonic && p.kind != Excitation::Kind::PinchHarmonic)
        setHarmonicRestriction (0);
}

void StringEngine::touch (double depth) noexcept
{
    wake();   // cpu-quality-modes 2.4
    // A 1.5 ms settle: a fingertip landing, fast enough to stop a low string
    // inside 10 ms and slow enough not to click. It holds for one trip round
    // the loop plus the settle, by which time the damping has done its work.
    touchDepthGain = 1.0 - juce::jlimit (0.0, 1.0, depth);
    touchCoeff = 1.0 - std::exp (-1.0 / (0.0015 * sr));
    touchSamplesLeft = (int) (sr / juce::jmax (constants::kMinStringHz, getCurrentFrequency())) + (int) (0.006 * sr);
}

void StringEngine::release (bool letRing) noexcept
{
    if (! letRing)
        setDamping (Damping::Released, 1.0);
}

void StringEngine::setDamping (Damping d, double amount) noexcept
{
    damping = d;
    dampingAmount = juce::jlimit (0.0, 1.0, amount);
    needsLoopUpdate = true;

    // A damped string still receives sympathetic energy (engine spec 5.6) - it
    // just dissipates it quickly - but a choked one accepts very little.
    couplingReceptivity = (d == Damping::Silenced) ? 0.0
                        : (d == Damping::Chuck) ? 1.0 - dampingAmount
                        : (d == Damping::Choked) ? 0.15
                        : (d == Damping::PalmMute || d == Damping::PalmMuteBass) ? 0.45
                        : (harmonicPartial > 0) ? 0.35
                        : 1.0;
}

void StringEngine::setHarmonicRestriction (int partial) noexcept
{
    harmonicPartial = juce::jmax (0, partial);
    couplingReceptivity = (harmonicPartial > 0) ? 0.35 : couplingReceptivity;
    needsLoopUpdate = true;
}

void StringEngine::setFretBuzz (double amount, double actionMm) noexcept
{
    fretBuzzAmount = juce::jlimit (0.0, 1.0, amount);
    fretActionMm = juce::jlimit (0.5, 4.0, actionMm);
}

void StringEngine::setSlideSpeed (double fretsPerSecond) noexcept
{
    slideSpeed = juce::jlimit (0.0, 60.0, std::abs (fretsPerSecond));
}

void StringEngine::setNoiseAmount (double slideNoise, double fretNoise) noexcept
{
    slideNoiseAmount = juce::jlimit (0.0, 1.0, slideNoise);
    fretNoiseAmount  = juce::jlimit (0.0, 1.0, fretNoise);
}

void StringEngine::triggerFretNoise (double strength) noexcept
{
    wake();   // cpu-quality-modes 2.4
    fretNoiseEnv = juce::jmax (fretNoiseEnv, juce::jlimit (0.0, 1.0, strength));
    fretNoiseBand.setBandpass (sr, physical.wound ? 1900.0 : 2600.0, 1.4);
}

//==============================================================================
void StringEngine::updateDispersion() noexcept
{
    // Negative coefficient: low frequencies are delayed more than high ones, so
    // the upper partials come out sharp. See Allpass1's note on the sign.
    const double b = juce::jlimit (0.0, 0.01, physical.inharmonicityB);
    dispersionCoeff = -std::tanh (kDispersionGain * b * (double) dispersionStages);

    for (int i = 0; i < kMaxDispersionStages; ++i)
        dispersion[i].setCoefficient (dispersionCoeff);

    // cpu-quality-modes 2.4: a capped note keeps its own coefficient.
    if (latchedStages < dispersionStages)
        applyCappedDispersion();
}

//==============================================================================
namespace
{
    /** Phase delay, in samples, of Allpass1's (a + z^-1) / (1 + a z^-1) at w. */
    double allpassPhaseDelay (double a, double w) noexcept
    {
        const std::complex<double> z1 = std::polar (1.0, -w);
        const auto h = (a + z1) / (1.0 + a * z1);
        return -std::arg (h) / w;
    }
}

void StringEngine::latchDispersion() noexcept
{
    const int want = QualityProfile::dispersionStagesFor (targetHz, ruleFourHz, ruleTwoHz, dispersionStages);

    if (want == latchedStages)
    {
        if (want < dispersionStages)
            applyCappedDispersion();   // the new note's pitch sets its coefficient

        return;
    }

    // Stages coming back into use start from rest rather than stale state.
    for (int i = latchedStages; i < want && i < kMaxDispersionStages; ++i)
        dispersion[i].reset();

    latchedStages = want;

    if (want >= dispersionStages)
    {
        cappedCompensation = 0.0;

        for (auto& ap : dispersion)
            ap.setCoefficient (dispersionCoeff);
    }
    else
    {
        applyCappedDispersion();
    }

    needsLoopUpdate = true;
}

void StringEngine::applyCappedDispersion() noexcept
{
    /*  Fewer stages, each stronger: the coefficient is chosen so the spread of
        phase delay between the fundamental and the tenth partial matches what
        the full cascade gives this note. The tenth partial therefore lands
        within a few cents of High, and cappedCompensation (updateLoopCoefficients)
        puts the fundamental exactly where High has it. */
    const double f0 = juce::jmax (constants::kMinStringHz, targetHz);
    const double w1 = constants::kTwoPi * f0 / sr;
    const double w10 = juce::jmin (10.0 * w1, 0.9 * juce::MathConstants<double>::pi);
    const double a = dispersionCoeff;
    const double loopSamples = sr / f0;
    const double perStageHigh = (1.0 - a) / (1.0 + a);
    const int highStages = juce::jlimit (0, dispersionStages,
                                         (int) std::floor (loopSamples * kMaxDispersionFraction / juce::jmax (1.0e-6, perStageHigh)));
    const int m = juce::jmax (1, latchedStages);

    const double target = (double) highStages * (allpassPhaseDelay (a, w10) - allpassPhaseDelay (a, w1));

    double lo = -0.97, hi = 0.0;

    auto spread = [&] (double b) { return (double) m * (allpassPhaseDelay (b, w10) - allpassPhaseDelay (b, w1)); };

    if (target <= spread (lo))
    {
        hi = lo;
    }
    else
    {
        for (int i = 0; i < 40; ++i)
        {
            const double mid = 0.5 * (lo + hi);

            if (spread (mid) > target)
                hi = mid;
            else
                lo = mid;
        }
    }

    cappedCoeff = 0.5 * (lo + hi);

    for (int i = 0; i < m && i < kMaxDispersionStages; ++i)
        dispersion[i].setCoefficient (cappedCoeff);

    needsLoopUpdate = true;
}

void StringEngine::setSleepEnabled (bool on) noexcept
{
    sleepEnabled = on;

    if (! on)
        quietSamples = 0;
}

void StringEngine::fadeToSleep (double seconds) noexcept
{
    if (sleepExempt || sleeping || fadeLeft > 0)
        return;

    fadeTotal = juce::jmax (1, (int) std::round (seconds * sr));
    fadeLeft = fadeTotal;
}

void StringEngine::goToSleep() noexcept
{
    // Below -100 dBFS: clearing the loop once is inaudible, and from here the
    // string costs nothing until something reaches it.
    delayLine.reset();
    loopFilter.reset();

    for (auto& ap : dispersion)
        ap.reset();

    dcBlocker.reset();
    levelFollower.reset();
    slideNoiseEnv = 0.0;
    fretNoiseEnv = 0.0;
    touchGain = 1.0;
    bridgeOut = 0.0;
    fadeLeft = 0;
    quietSamples = 0;
    sleeping = true;
}

double StringEngine::filterDelayCompensation() const noexcept
{
    const double lp = loopFilterPole / juce::jmax (1.0e-6, 1.0 - loopFilterPole);
    const double ap = dispersion[0].delayAtDC() * (double) activeDispersionStages;
    return lp + ap + cappedCompensation;   // 0 unless capped (cpu-quality-modes 2.4)
}

void StringEngine::updateLoopCoefficients() noexcept
{
    needsLoopUpdate = false;

    const double f0 = juce::jmax (constants::kMinStringHz, getCurrentFrequency());
    lastCoefficientHz = f0;

    const double loopSamples = sr / f0;

    // ---- loop filter cutoff -------------------------------------------------
    // Engine spec 4 gives the physical reading: damping lowers the cutoff.
    // ~5 kHz open down to ~800 Hz under the palm.
    const double open = physical.openBrightnessHz * terminationBrightness;
    double cutoff = open;
    double t60Scale = 1.0;

    switch (damping)
    {
        case Damping::Open:
            break;

        case Damping::LightTouch:
            cutoff = juce::jmap (dampingAmount, open, 2000.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.35);
            break;

        case Damping::PalmMute:
            cutoff = juce::jmap (dampingAmount, open, 800.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.11);
            break;

        case Damping::PalmMuteBass:
            // bass-techniques 7 (MODEL-GAPS): the palm sits on a heavier string.
            // It dies sooner than a guitar's palm mute, and the loss is darker,
            // so what is left is mostly the fundamental.
            cutoff = juce::jmap (dampingAmount, open, 450.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.07);
            break;

        case Damping::Released:
            cutoff = juce::jmap (dampingAmount, open, 1200.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.13);
            break;

        case Damping::Choked:
            cutoff = 500.0;
            t60Scale = 0.035;
            break;

        case Damping::Silenced:
            cutoff = 400.0;
            break;

        case Damping::Chuck:
            cutoff = juce::jmap (dampingAmount, open, 400.0);
            break;
    }

    // Harmonics ring clean but die noticeably sooner than a stopped note.
    if (harmonicPartial > 0)
        t60Scale *= 0.55;

    loopCutoffHz = juce::jlimit (120.0, sr * 0.48, cutoff);
    loopFilter.setCutoff (loopCutoffHz);
    loopFilterPole = std::exp (-constants::kTwoPi * loopCutoffHz / sr);

    // ---- how many dispersion stages the loop can afford ---------------------
    const double perStage = dispersion[0].delayAtDC();
    const int affordable = (perStage > 1.0e-6)
                             ? (int) std::floor (loopSamples * kMaxDispersionFraction / perStage)
                             : kMaxDispersionStages;
    activeDispersionStages = juce::jlimit (0, latchedStages, affordable);

    // cpu-quality-modes 2.4: with the stage count capped, hold the fundamental
    // where the full cascade puts it (phase delay at f0, not the DC figure).
    if (latchedStages < dispersionStages)
    {
        const double w1 = constants::kTwoPi * f0 / sr;
        const double a = dispersionCoeff;
        const double perStageHigh = (1.0 - a) / (1.0 + a);
        const int highStages = juce::jlimit (0, dispersionStages,
                                             (int) std::floor (loopSamples * kMaxDispersionFraction / juce::jmax (1.0e-6, perStageHigh)));
        const double b = cappedCoeff;
        const double highExcess = (double) highStages * (allpassPhaseDelay (a, w1) - perStageHigh);
        const double cappedExcess = (double) activeDispersionStages * (allpassPhaseDelay (b, w1) - (1.0 - b) / (1.0 + b));
        cappedCompensation = cappedExcess - highExcess;
    }

    // ---- loss gain from the target T60 --------------------------------------
    // Higher notes decay faster on a real string (identity rule 6), so the target
    // sustain is scaled down as the fundamental rises.
    const double pitchScale = std::pow (110.0 / f0, 0.40);
    double t60 = physical.sustainSeconds * sustainScale * t60Scale * pitchScale;

    // Silenced is an absolute time: the E-Bow letting go (ambiguity-resolutions
    // 2.4) has to be inaudible in 200 ms on a string of any sustain.
    if (damping == Damping::Silenced)
        t60 = 0.08;

    /*  strum-dynamics 6.1: a chuck's decay is geometric between the note's own
        and 10 ms, so a light chuck shortens the note and a full one stops the
        loop within about a period of a low E - which is what takes the pitch. */
    if (damping == Damping::Chuck)
        t60 = std::exp (juce::jmap (dampingAmount, std::log (juce::jmax (0.01, t60)), std::log (0.01)));

    t60 = juce::jlimit (0.01, 60.0, t60);

    loopGain = std::exp (-kT60Constant * loopSamples / (t60 * sr));
    loopGain = juce::jlimit (0.0, kMaxLoopGain, loopGain);
}

//==============================================================================
double StringEngine::processSample (double couplingInput) noexcept
{
    // cpu-quality-modes 2.4: a sleeping string costs nothing until coupling,
    // excitation or a touch reaches it. It still receives (the matrix keeps
    // feeding it) and sends 0, so sympathetic ring survives.
    if (sleeping)
    {
        if (stealPending || excitation.isActive() || touchSamplesLeft > 0
            || std::abs (couplingInput * couplingReceptivity) > QualityProfile::kSleepCouplingLevel)
        {
            wake();
        }
        else
        {
            bridgeOut = 0.0;
            return 0.0;
        }
    }

    // ---- voice stealing ------------------------------------------------------
    if (stealPending)
    {
        if (stealCountdown > 0)
        {
            stealGain = (double) stealCountdown / (double) stealTotal;
            --stealCountdown;
        }
        else
        {
            excitation.trigger (pendingParams, rng);
            sounded = true;
            stealPending = false;
            stealGain = 1.0;

            if (pendingParams.kind != Excitation::Kind::Harmonic
                && pendingParams.kind != Excitation::Kind::PinchHarmonic)
                setHarmonicRestriction (0);
        }
    }
    else
    {
        stealGain = 1.0;
    }

    // Recompute the loop coefficients only when they would actually change.
    //
    // The delay length is updated every sample, because that is the pitch and a
    // step there would be audible. The loss gain and the filter cutoff are not:
    // they move smoothly with pitch and each recomputation costs a pow() and two
    // exp(). Bending a note used to redo that every sample on every string,
    // which was most of the engine's CPU for no audible benefit. The 0.2%
    // threshold is about three and a half cents.
    if (needsLoopUpdate
        || std::abs (targetHz - lastCoefficientHz) > lastCoefficientHz * 0.002)
    {
        updateLoopCoefficients();
    }

    // ---- read the waveguide --------------------------------------------------
    const double delaySamples = smoothedDelay.next();
    const double compensated = juce::jmax (2.0, delaySamples - filterDelayCompensation());

    const double delayOut = delayLine.read (compensated);

    // ---- loop: damping, dispersion, loss ------------------------------------
    double fb = loopFilter.process (delayOut);

    for (int i = 0; i < activeDispersionStages; ++i)
        fb = dispersion[i].process (fb);

    fb *= loopGain * stealGain;

    // ---- fret buzz -----------------------------------------------------------
    // Low action plus light fretting lets the string slap the frets: the peaks
    // are clipped against the fret wire and the contact adds a bright rattle.
    if (fretBuzzAmount > 1.0e-4)
    {
        const double threshold = 0.10 + fretActionMm * 0.22;
        const double mag = std::abs (fb);

        if (mag > threshold)
        {
            const double excess = mag - threshold;
            const double clipped = threshold + excess / (1.0 + excess * 9.0 * fretBuzzAmount);
            fb = std::copysign (clipped, fb);
            fb += rng.nextBipolar() * excess * fretBuzzAmount * 0.30;
        }
    }

    fb = sanitise (fb);

    // ---- injections ----------------------------------------------------------
    const double exc = excitation.next();

    double noise = 0.0;

    // Finger sliding along a wound string. Volume tracks slide speed; plain
    // strings squeak far less than wound ones (identity rule 8).
    if (slideSpeed > 0.01 && slideNoiseAmount > 1.0e-4)
    {
        const double woundScale = physical.wound ? 1.0 : 0.22;
        const double target = juce::jmin (1.0, slideSpeed / 18.0) * slideNoiseAmount * woundScale;
        slideNoiseEnv += (target - slideNoiseEnv) * 0.002;

        if (slideNoiseEnv > 1.0e-5)
        {
            const double raw = rng.nextBipolar();
            noise += slideNoiseFilter.process (raw) * slideNoiseEnv * 0.05;
        }
    }
    else if (slideNoiseEnv > 1.0e-6)
    {
        slideNoiseEnv *= 0.9992;
    }

    // Finger landing on a fret: a short, bright click.
    if (fretNoiseEnv > 1.0e-5)
    {
        noise += fretNoiseBand.process (rng.nextBipolar()) * fretNoiseEnv * fretNoiseAmount * 0.12;
        fretNoiseEnv *= 0.9965;
    }

    // bass-techniques 6 (MODEL-GAPS): a finger laid on the string stops it
    // where it lies, not a period later. What is already travelling in the
    // loop is damped with it; once the loop has gone round once the damping
    // has taken over and the touch lets go.
    double touch = 1.0;

    if (touchGain < 1.0 || touchSamplesLeft > 0)
    {
        const double target = touchSamplesLeft > 0 ? touchDepthGain : 1.0;
        touchGain += (target - touchGain) * touchCoeff;

        if (touchSamplesLeft > 0)
            --touchSamplesLeft;
        else if (touchGain > 0.9999)
            touchGain = 1.0;

        touch = touchGain;
        fb *= touch;
    }

    // cpu-quality-modes 2.4 / 7: ring-out truncation and E3 fade what is left.
    double fade = 1.0;

    if (fadeLeft > 0)
    {
        fade = (double) (fadeLeft - 1) / (double) fadeTotal;
        --fadeLeft;
        fb *= fade;
    }

    delayLine.write (fb + exc + couplingInput * couplingReceptivity + noise);

    // ---- output --------------------------------------------------------------
    double out = dcBlocker.process (delayOut * touch);
    out = sanitise (out);

    if (fade < 1.0)
    {
        out *= fade;

        if (fadeLeft == 0)
        {
            goToSleep();
            return 0.0;
        }
    }

    levelFollower.process (out);
    bridgeOut = out * physical.couplingSend;

    if (sleepEnabled && ! sleepExempt)
    {
        if (levelFollower.current() < QualityProfile::kSleepLevel && ! excitation.isActive()
            && ! stealPending && touchSamplesLeft == 0
            && std::abs (couplingInput * couplingReceptivity) < QualityProfile::kSleepCouplingLevel)
        {
            if (++quietSamples >= sleepAfterSamples)
                goToSleep();
        }
        else
        {
            quietSamples = 0;
        }
    }

    return out;
}

} // namespace luthier
