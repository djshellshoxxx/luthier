#include "StringEngine.h"

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

    snapToFrequency (targetHz);
    needsLoopUpdate = true;
    updateDispersion();
    updateLoopCoefficients();
    reset();
}

void StringEngine::reset() noexcept
{
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
    couplingReceptivity = (d == Damping::Choked) ? 0.15
                        : (d == Damping::PalmMute) ? 0.45
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
}

double StringEngine::filterDelayCompensation() const noexcept
{
    const double lp = loopFilterPole / juce::jmax (1.0e-6, 1.0 - loopFilterPole);
    const double ap = dispersion[0].delayAtDC() * (double) activeDispersionStages;
    return lp + ap;
}

void StringEngine::updateLoopCoefficients() noexcept
{
    needsLoopUpdate = false;

    const double f0 = juce::jmax (constants::kMinStringHz, getCurrentFrequency());
    const double loopSamples = sr / f0;

    // ---- loop filter cutoff -------------------------------------------------
    // Engine spec 4 gives the physical reading: damping lowers the cutoff.
    // ~5 kHz open down to ~800 Hz under the palm.
    double cutoff = physical.openBrightnessHz;
    double t60Scale = 1.0;

    switch (damping)
    {
        case Damping::Open:
            break;

        case Damping::LightTouch:
            cutoff = juce::jmap (dampingAmount, physical.openBrightnessHz, 2000.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.35);
            break;

        case Damping::PalmMute:
            cutoff = juce::jmap (dampingAmount, physical.openBrightnessHz, 800.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.11);
            break;

        case Damping::Released:
            cutoff = juce::jmap (dampingAmount, physical.openBrightnessHz, 1200.0);
            t60Scale = juce::jmap (dampingAmount, 1.0, 0.13);
            break;

        case Damping::Choked:
            cutoff = 500.0;
            t60Scale = 0.035;
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
    activeDispersionStages = juce::jlimit (0, dispersionStages, affordable);

    // ---- loss gain from the target T60 --------------------------------------
    // Higher notes decay faster on a real string (identity rule 6), so the target
    // sustain is scaled down as the fundamental rises.
    const double pitchScale = std::pow (110.0 / f0, 0.40);
    double t60 = physical.sustainSeconds * sustainScale * t60Scale * pitchScale;
    t60 = juce::jlimit (0.01, 60.0, t60);

    loopGain = std::exp (-kT60Constant * loopSamples / (t60 * sr));
    loopGain = juce::jlimit (0.0, kMaxLoopGain, loopGain);
}

//==============================================================================
double StringEngine::processSample (double couplingInput) noexcept
{
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

    // Recompute loop coefficients when the pitch or articulation has moved. The
    // delay smoother moves every sample during a bend, so rate-limit on a real
    // change in the target rather than on the smoother being busy.
    if (needsLoopUpdate || smoothedDelay.isSmoothing())
        updateLoopCoefficients();

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

    delayLine.write (fb + exc + couplingInput * couplingReceptivity + noise);

    // ---- output --------------------------------------------------------------
    double out = dcBlocker.process (delayOut);
    out = sanitise (out);

    levelFollower.process (out);
    bridgeOut = out * physical.couplingSend;

    return out;
}

} // namespace luthier
