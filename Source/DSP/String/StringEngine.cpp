#include "StringEngine.h"
#include "Harmonics.h"
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

    // harmonic-realism.md 2: a contact lands and lifts over 1 ms.
    contactRampStep = 1.0 / juce::jmax (1.0, 0.001 * sr);

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
    bridgeWave = 0.0;
    stealCountdown = 0;
    stealPending = false;
    stealGain = 1.0;
    sounded = false;

    damping = Damping::Open;
    dampingAmount = 1.0;
    harmonicPartial = 0;
    couplingReceptivity = 1.0;
    dampingReceptivity = 1.0;
    couplingSendScale = 1.0;

    // harmonic-realism.md 8: reset clears every contact and its ramp.
    for (auto& c : contacts)
        c = ContactState {};

    numActiveContacts = 0;

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

    // A fresh pluck refreshes the loop coefficients, as the harmonic reset it
    // replaced always did: a render without harmonics stays bit-identical.
    if (p.kind != Excitation::Kind::Harmonic && p.kind != Excitation::Kind::PinchHarmonic)
        needsLoopUpdate = true;
}

void StringEngine::touch (double depth) noexcept
{
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
    dampingReceptivity = (d == Damping::Silenced) ? 0.0
                       : (d == Damping::Chuck) ? 1.0 - dampingAmount
                       : (d == Damping::Choked) ? 0.15
                       : (d == Damping::PalmMute || d == Damping::PalmMuteBass) ? 0.45
                       : 1.0;

    updateReceptivity();
}

void StringEngine::updateReceptivity() noexcept
{
    // harmonic-realism.md 2: while a finger touches the string it accepts
    // what a harmonic always did (0.35), and the damping state's value again
    // when it lifts.
    couplingReceptivity = numActiveContacts > 0 ? juce::jmin (dampingReceptivity, 0.35)
                                                : dampingReceptivity;
}

void StringEngine::setHarmonicRestriction (int partial) noexcept
{
    // The shim (harmonic-realism.md 2): a contact for the partial at its first
    // node, 1/n from the bridge, held until cleared.
    harmonicPartial = juce::jmax (0, partial);

    if (harmonicPartial <= 1)
    {
        clearAllContacts();
        return;
    }

    Contact c;
    c.positionFromBridge = 1.0 / (double) harmonicPartial;
    c.vibratingLengthMm = physical.scaleLengthMm;
    c.seconds = 0.0;
    addContact (c);
}

//==============================================================================
int StringEngine::addContact (const Contact& c) noexcept
{
    for (int slot = 0; slot < kMaxContacts; ++slot)
    {
        auto& st = contacts[(size_t) slot];

        if (st.active)
            continue;

        st.active = true;
        st.contact = c;
        st.contact.strength = juce::jlimit (0.0, 1.0, c.strength);
        st.contact.widthMm = juce::jlimit (0.01, 100.0, c.widthMm);

        const auto node = harmonics::findNode (c.positionFromBridge, c.vibratingLengthMm, st.contact.widthMm);
        st.partial = node.partial;
        st.efficiency = node.partial > 0 ? node.efficiency : 0.0;
        st.gain = 0.0;
        st.target = st.contact.strength;
        st.samplesLeft = c.seconds > 0.0 ? juce::jmax (1, (int) std::round (c.seconds * sr)) : -1;
        st.combSpacing = 0.0;

        ++numActiveContacts;
        updateContactSpacing();
        updateReceptivity();
        return slot;
    }

    return -1;
}

void StringEngine::clearContact (int slot) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kMaxContacts))
        return;

    // Lifts over the 1 ms ramp; the slot frees itself when the gain reaches 0.
    auto& st = contacts[(size_t) slot];

    if (st.active)
    {
        st.target = 0.0;
        st.samplesLeft = 0;
    }
}

void StringEngine::clearAllContacts() noexcept
{
    for (int slot = 0; slot < kMaxContacts; ++slot)
        clearContact (slot);
}

int StringEngine::getContactPartial (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kMaxContacts) && contacts[(size_t) slot].active
             ? contacts[(size_t) slot].partial : 0;
}

double StringEngine::getContactEfficiency (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kMaxContacts) && contacts[(size_t) slot].active
             ? contacts[(size_t) slot].efficiency : 0.0;
}

double StringEngine::getPartialFrequency (int n) const noexcept
{
    /*  The loop rings where its total phase delay is a whole number of
        cycles. Solve f tau(f) = n sr by fixed-point iteration; tau varies
        slowly with f, so six steps are far past convergence. This is the
        model's own stretched partial, so the comb sits exactly on it
        (harmonic-realism.md 1, equation 1). */
    const double delaySamples = smoothedDelay.getCurrent();
    const double pure = juce::jmax (2.0, delaySamples - filterDelayCompensation());
    const double p = loopFilterPole;
    const double a = dispersionCoeff;

    double f = (double) n * juce::jmax (constants::kMinStringHz, sr / juce::jmax (1.0, delaySamples));

    for (int it = 0; it < 6; ++it)
    {
        const double w = juce::jlimit (1.0e-6, juce::MathConstants<double>::pi - 1.0e-6,
                                       constants::kTwoPi * f / sr);
        const std::complex<double> z1 = std::polar (1.0, -w);

        // One-pole lowpass (1 - p) / (1 - p z^-1), and the allpass (a + z^-1) / (1 + a z^-1).
        const double lpPhase = -std::arg (1.0 - p * z1);
        const double apPhase = std::arg (a + z1) - std::arg (1.0 + a * z1);

        const double tau = pure - lpPhase / w - (double) activeDispersionStages * apPhase / w;
        f = (double) n * sr / juce::jmax (1.0, tau);
    }

    return f;
}

bool StringEngine::canRealiseContact (const Contact& c) const noexcept
{
    const auto node = harmonics::findNode (c.positionFromBridge, c.vibratingLengthMm, c.widthMm);

    if (node.partial <= 1)
        return true;

    const double compensated = juce::jmax (2.0, smoothedDelay.getCurrent() - filterDelayCompensation());
    const double spacing = sr / juce::jmax (1.0, getPartialFrequency (node.partial));

    return compensated - (double) (node.partial - 1) * spacing >= 2.0;
}

void StringEngine::updateContactSpacing() noexcept
{
    for (auto& st : contacts)
        if (st.active && st.partial > 1)
            st.combSpacing = sr / juce::jmax (1.0, getPartialFrequency (st.partial));
}

double StringEngine::applyContacts (double delayOut, double compensated) noexcept
{
    double y = delayOut;

    for (auto& st : contacts)
    {
        if (! st.active)
            continue;

        // 1 ms linear ramp in and out: a touch never steps the loop.
        if (st.gain < st.target)
            st.gain = juce::jmin (st.target, st.gain + contactRampStep);
        else if (st.gain > st.target)
            st.gain = juce::jmax (st.target, st.gain - contactRampStep);

        const double g = st.gain;

        // H = (1 - g) + g e C_n: C_n reads the loop m M samples less delayed.
        double comb = 0.0;

        if (st.partial > 1 && st.efficiency > 0.0)
        {
            const double spacing = st.combSpacing;
            double sum = delayOut;
            int taps = 1;

            for (int m = 1; m < st.partial; ++m)
            {
                const double d = compensated - (double) m * spacing;

                if (d < 2.0)
                    break;

                // The kernel only changes when the delay does (a glide), so
                // a held touch costs six multiply-adds per tap.
                auto& k = st.kernels[(size_t) m];

                if (k.delay != d)
                    FractionalDelayLine::makeKernel (k, juce::jlimit (1.0, delayLine.getMaxDelay(), d));

                sum += delayLine.readKernel (k);
                ++taps;
            }

            // A comb that cannot be read in full is only reached when the
            // pitch rose under a held touch; the fallback in triggerNote
            // covers new notes. Its partial sum is still passive.
            comb = sum / (double) juce::jmax (taps, st.partial);
        }

        y = (1.0 - g) * y + g * st.efficiency * comb;

        // Timed release.
        if (st.samplesLeft > 0 && --st.samplesLeft == 0)
            st.target = 0.0;

        if (st.samplesLeft == 0 && st.gain <= 0.0)
        {
            st.active = false;
            --numActiveContacts;
            updateReceptivity();
        }
    }

    return y;
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
    const double b = juce::jlimit (0.0, 0.01, physical.inharmonicityB * agingDispersion);   // string-aging.md 5
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
    lastCoefficientHz = f0;

    const double loopSamples = sr / f0;

    // ---- loop filter cutoff -------------------------------------------------
    // Engine spec 4 gives the physical reading: damping lowers the cutoff.
    // ~5 kHz open down to ~800 Hz under the palm.
    const double open = physical.openBrightnessHz * agingBrightness * terminationBrightness;   // string-aging.md 5
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

    // harmonic-realism.md 2: no harmonic decay factor. A harmonic's decay is
    // the loop filter's at n f0, shorter for the physical reason.

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
    double t60 = physical.sustainSeconds * agingSustain * sustainScale * t60Scale * pitchScale;   // string-aging.md 5

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

    // The contacts' comb spacing follows the pitch, at this rate and no faster.
    if (numActiveContacts > 0)
        updateContactSpacing();
}

//==============================================================================
double StringEngine::processSample (double couplingInput, double directInput) noexcept
{
    beginSample();
    return endSample (couplingInput, directInput);
}

void StringEngine::beginSample() noexcept
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
                needsLoopUpdate = true;
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

    // ---- contacts (harmonic-realism.md 2), between the read and the filter --
    const double touched = numActiveContacts > 0 ? applyContacts (delayOut, compensated) : delayOut;

    // ---- loop: damping, dispersion, loss ------------------------------------
    double fb = loopFilter.process (touched);

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

    /*  body-coupling.md 3: the wave that reflects at the bridge is the loop's
        return - after the loop filter and the dispersion cascade, whose group
        delay (up to a third of the loop on a wound string) the delay line is
        shortened by. The delay output is that wave a filter-delay early, and
        driving the body from it advances the body path by tens of degrees at
        the fundamental, which is enough to make it active. */
    bridgeWave = fb;
    pendingDelayOut = delayOut;
}

double StringEngine::endSample (double couplingInput, double directInput) noexcept
{
    const double delayOut = pendingDelayOut;
    double fb = bridgeWave;   // the bass touch below damps it (MODEL-GAPS)

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

    delayLine.write (fb + exc + couplingInput * couplingReceptivity + noise + directInput);

    // ---- output --------------------------------------------------------------
    double out = dcBlocker.process (delayOut * touch);
    out = sanitise (out);

    levelFollower.process (out);
    bridgeOut = out * physical.couplingSend * couplingSendScale;

    return out;
}

} // namespace luthier
