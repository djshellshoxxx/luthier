#include "DrumPieces.h"

namespace luthier
{

namespace
{
    /** A raised-cosine pulse of `seconds` whose area is `velocity`: the modes
        below the pulse's corner then ring at `gain x velocity` whatever the
        sample rate (engine.md 0.5). */
    double pulseAmplitude (double velocity, double seconds, double sr) noexcept
    {
        const double length = juce::jmax (2.0, std::round (seconds * sr));
        return velocity * 2.0 / length;
    }
}

//==============================================================================
void MembranePiece::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    pulse.prepare (sr);
    bank.prepare (sr);
    head.prepare (sr);
    output.prepare (sr);
    setDesign (design);
    reset();
}

void MembranePiece::reset() noexcept
{
    pulse.reset();
    bank.reset();
    head.reset();
    output.reset();
    active = false;
    choking = false;
    lastBatter = 0.0;
    envelope = 0.0;
}

void MembranePiece::setDesign (const Design& d) noexcept
{
    design = d;
    design.numModes = juce::jlimit (1, kMaxModes, d.numModes);
    bank.setNumModes (design.numModes);
    head.setNumModes (design.resonantHead ? 2 : 0);
    applyModes();
}

void MembranePiece::setTuning (double ratio, double scale) noexcept
{
    tuningRatio = juce::jlimit (0.25, 4.0, ratio);
    t60Scale = juce::jlimit (0.05, 8.0, scale);

    if (! active)
        applyModes();
}

void MembranePiece::applyModes() noexcept
{
    hitF0 = design.f0 * tuningRatio;

    for (int m = 0; m < design.numModes; ++m)
        bank.setMode (m, hitF0 * design.ratios[(size_t) m],
                      design.t60 * design.decays[(size_t) m] * t60Scale,
                      design.gains[(size_t) m]);

    if (design.resonantHead)
        for (int m = 0; m < 2; ++m)
            head.setMode (m, hitF0 * design.headRatios[(size_t) m], design.t60 * 1.2 * t60Scale, 0.8);
}

void MembranePiece::strike (double velocity) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);

    choking = false;
    applyModes();

    // jam-mode 5: tension modulation, k up to its maximum at velocity 127.
    k = design.pitchDropK * velocity;
    envelope = 1.0;
    envelopeStep = std::exp (-(double) kControlInterval / (juce::jmax (0.001, design.sweepSeconds) * sr));
    controlCountdown = 0;

    pulse.trigger (pulseAmplitude (velocity, design.pulseSeconds, sr), design.pulseSeconds);
    active = true;
}

void MembranePiece::choke (double t60, double seconds) noexcept
{
    if (! active)
        return;

    // A damping ramp: the extra loss per sample grows from nothing to a
    // `t60` decay over `seconds`, applied to the ringing state itself.
    chokeTo = std::pow (10.0, -3.0 / (juce::jmax (1.0e-4, t60) * sr));
    chokeFrom = 1.0;
    chokePosition = 0.0;
    chokeStep = 1.0 / juce::jmax (1.0, seconds * sr);
    choking = true;
}

void MembranePiece::updateControl() noexcept
{
    controlCountdown = kControlInterval;

    if (k > 0.0 && envelope > 1.0e-4)
    {
        const double f = hitF0 * (1.0 + k * envelope * envelope);

        for (int m = 0; m < design.numModes; ++m)
            bank.setModeFrequency (m, f * design.ratios[(size_t) m]);

        envelope *= envelopeStep;

        if (envelope <= 1.0e-4)
            for (int m = 0; m < design.numModes; ++m)
                bank.setModeFrequency (m, hitF0 * design.ratios[(size_t) m]);
    }

    if (choking)
    {
        chokePosition = juce::jmin (1.0, chokePosition + chokeStep * (double) kControlInterval);
        const double perSample = chokeFrom + (chokeTo - chokeFrom) * chokePosition;
        const double g = std::pow (perSample, (double) kControlInterval);
        bank.scaleState (g);
        head.scaleState (g);
    }
}

void MembranePiece::housekeep() noexcept
{
    if (! active)
        return;

    bank.housekeep();
    head.housekeep();

    if (! bank.isRunning() && ! head.isRunning() && ! pulse.isActive())
    {
        active = false;
        choking = false;
        output.reset();
    }
}

//==============================================================================
void SnarePiece::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    batter.prepare (sr);
    rimBank.prepare (sr);
    rimPulse.prepare (sr);
    output.prepare (sr);

    rimBank.setNumModes (3);
    rimBank.setMode (0, 1450.0, 0.045, 1.0);
    rimBank.setMode (1, 2650.0, 0.030, 0.6);
    rimBank.setMode (2, 4250.0, 0.022, 0.4);

    wireBand.setBandpass (sr, 3500.0, 0.7);
    brushBand.setBandpass (sr, 2000.0, 0.9);
    reset();
}

void SnarePiece::reset() noexcept
{
    batter.reset();
    rimBank.reset();
    rimPulse.reset();
    wireBand.reset();
    brushBand.reset();
    output.reset();
    brushRemaining = 0;
    rimActive = false;
    active = false;
}

void SnarePiece::strike (double velocity, Stroke stroke) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);

    // The wires' gate reads the head relative to a full-velocity hit.
    headNorm = 0.8;

    if (stroke == Stroke::brush)
    {
        // jam-mode 5: a 120-400 ms filtered-noise sweep, the head barely moved.
        const double seconds = 0.12 + 0.28 * rng.nextDouble();
        brushLength = juce::jmax (1, (int) (seconds * sr));
        brushRemaining = brushLength;
        brushAmp = 0.35 * velocity;
        sweepCountdown = 0;
        batter.strike (velocity * 0.2);
    }
    else
    {
        batter.strike (velocity);

        // Accents above 115 add rim contact.
        if (stroke == Stroke::rimshot || velocity > 115.0 / 127.0)
        {
            rimActive = true;
            rimPulse.trigger (pulseAmplitude (velocity, 0.0003, sr), 0.0003);
        }
    }

    active = true;
}

void SnarePiece::choke (double t60, double seconds) noexcept
{
    batter.choke (t60, seconds);
    brushRemaining = juce::jmin (brushRemaining, (int) (seconds * sr));
}

void SnarePiece::housekeep() noexcept
{
    if (! active)
        return;

    batter.housekeep();
    rimBank.housekeep();

    if (! rimBank.isRunning() && ! rimPulse.isActive())
        rimActive = false;

    if (! batter.isActive() && ! rimActive && brushRemaining <= 0)
    {
        active = false;
        output.reset();
    }
}

//==============================================================================
void CymbalPiece::prepare (double sampleRate, Kind k) noexcept
{
    sr = sampleRate;
    kind = k;
    totalModes = (kind == Kind::hat) ? 32 : 48;

    pulse.prepare (sr);
    low.prepare (sr);
    high.prepare (sr);
    output.prepare (sr);

    setDesign (design, seed);
    reset();
}

void CymbalPiece::reset() noexcept
{
    pulse.reset();
    low.reset();
    high.reset();
    output.reset();
    bloomRemaining = chickRemaining = rampRemaining = 0;
    currentScale = 1.0;
    active = false;
}

void CymbalPiece::setDesign (const Design& d, uint64_t newSeed) noexcept
{
    design = d;
    seed = newSeed;
    layoutModes();
}

void CymbalPiece::setReduced (bool shouldReduce) noexcept
{
    if (reduced == shouldReduce)
        return;

    reduced = shouldReduce;
    layoutModes();
}

double CymbalPiece::modeT60 (int index, int count) const noexcept
{
    const double frac = count > 1 ? (double) index / (double) (count - 1) : 0.0;

    switch (kind)
    {
        case Kind::hat:   return (1.4 + (0.9 - 1.4) * frac) * design.decayScale;    // open; closed and pedal scale it
        case Kind::ride:  return (4.5 + (1.5 - 4.5) * frac) * design.decayScale;    // 3-6 s low, 1-2 s high
        case Kind::crash: return (3.0 + (2.0 - 3.0) * frac) * design.decayScale;    // 2-3 s
    }

    return 1.0;
}

void CymbalPiece::layoutModes() noexcept
{
    const int count = reduced ? totalModes / 2 : totalModes;
    RtRandom jitter (seed * 0x9E3779B97F4A7C15ull + (uint64_t) kind + 1);

    int numLow = 0, numHigh = 0;

    for (int i = 0; i < count; ++i)
    {
        double hz = 0.0;

        if (kind == Kind::hat)
        {
            // f_k = f_h k^1.35 with seeded +-3 % jitter (5). Reduced keeps
            // every other mode, so the spread stays the same.
            const int kIndex = reduced ? 2 * i + 1 : i + 1;
            hz = design.baseHz * std::pow ((double) kIndex, 1.35);
        }
        else
        {
            const double frac = count > 1 ? (double) i / (double) (count - 1) : 0.0;
            hz = design.baseHz * std::pow (design.topHz / design.baseHz, frac);
        }

        hz *= 1.0 + 0.03 * jitter.nextBipolar();

        const double frac = count > 1 ? (double) i / (double) (count - 1) : 0.0;
        const double tilt = std::pow (juce::jmax (1.0, hz / design.baseHz), -0.3);
        double g = tilt * (1.0 + (design.brightness - 1.0) * frac);

        // Reduced banks carry the energy of the modes they dropped.
        if (reduced)
            g *= 1.41;

        modeHz[(size_t) i] = hz;
        modeGain[(size_t) i] = g;
        modeIsHigh[(size_t) i] = hz >= 4000.0;
        modeSlot[(size_t) i] = modeIsHigh[(size_t) i] ? numHigh++ : numLow++;
    }

    low.setNumModes (numLow);
    high.setNumModes (numHigh);

    for (int i = 0; i < count; ++i)
    {
        auto& bank = modeIsHigh[(size_t) i] ? high : low;
        bank.setMode (modeSlot[(size_t) i], modeHz[(size_t) i], modeT60 (i, count) * currentScale, modeGain[(size_t) i]);
    }

    openT60 = modeT60 (0, count);
}

void CymbalPiece::setDecays (double scale) noexcept
{
    currentScale = scale;
    const int count = reduced ? totalModes / 2 : totalModes;

    for (int i = 0; i < count; ++i)
    {
        auto& bank = modeIsHigh[(size_t) i] ? high : low;
        bank.setModeDecay (modeSlot[(size_t) i], modeT60 (i, count) * scale);
    }
}

void CymbalPiece::updateRamp() noexcept
{
    controlCountdown = 16;
    rampRemaining = juce::jmax (0, rampRemaining - 16);

    const double frac = 1.0 - (double) rampRemaining / (double) rampTotal;
    setDecays (rampFrom * std::pow (rampTo / rampFrom, frac));
}

void CymbalPiece::choke (double seconds, double t60) noexcept
{
    if (! active)
        return;

    rampFrom = currentScale;
    rampTo = juce::jlimit (1.0e-4, 10.0, t60 / juce::jmax (1.0e-4, openT60));
    rampTotal = juce::jmax (16, (int) (seconds * sr));
    rampRemaining = rampTotal;
    controlCountdown = 0;
}

void CymbalPiece::strike (double velocity, Hit hit) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);
    const double stick = 0.0004;
    const double amp = pulseAmplitude (velocity, stick, sr);
    const int count = reduced ? totalModes / 2 : totalModes;

    highDirect = 1.0;

    // Hat decay scales against the open T60s: closed 50-90 ms, pedal ~30 ms.
    constexpr double kClosedScale = 0.062, kPedalScale = 0.025;

    switch (hit)
    {
        case Hit::closed:
        case Hit::pedal:
        {
            const double target = hit == Hit::closed ? kClosedScale : kPedalScale;

            if (active && currentScale > target * 1.5)
            {
                // Closing an open hat: its damping ramps in over 10 ms.
                rampFrom = currentScale;
                rampTo = target;
                rampTotal = juce::jmax (16, (int) (0.010 * sr));
                rampRemaining = rampTotal;
                controlCountdown = 0;
            }
            else
            {
                rampRemaining = 0;
                setDecays (target);
            }

            if (hit == Hit::pedal)
            {
                chickLength = juce::jmax (1, (int) (0.005 * sr));
                chickRemaining = chickLength;
                chickAmp = 0.02 * velocity;
            }
            else
            {
                pulse.trigger (amp, stick);
            }
            break;
        }

        case Hit::close:
            choke (0.010, 0.05 * kClosedScale * 20.0);
            return;

        case Hit::open:
            rampRemaining = 0;
            setDecays (1.0);
            pulse.trigger (amp, stick);
            break;

        case Hit::bow:
        case Hit::bell:
        {
            // A bell hit weights the lowest 8 modes; a re-strike adds to the
            // ringing state (the banks are not reset).
            rampRemaining = 0;

            if (currentScale != 1.0)
                setDecays (1.0);

            for (int i = 0; i < count; ++i)
            {
                double g = modeGain[(size_t) i];

                if (hit == Hit::bell)
                    g *= (i < 8) ? 3.0 : 0.3;

                auto& bank = modeIsHigh[(size_t) i] ? high : low;
                bank.setModeGain (modeSlot[(size_t) i], g);
            }

            pulse.trigger (amp, stick);
            break;
        }

        case Hit::crash:
        {
            rampRemaining = 0;

            if (currentScale != 1.0)
                setDecays (1.0);

            // The bloom: the stick excites the low band; the high band rises
            // over 30 ms, standing in for the nonlinear energy cascade.
            highDirect = 0.3;
            bloomLength = juce::jmax (1, (int) (0.060 * sr));
            bloomRemaining = bloomLength;
            bloomAmp = 0.06 * velocity;
            pulse.trigger (amp, stick);
            break;
        }
    }

    lastHit = hit;
    active = true;
}

void CymbalPiece::housekeep() noexcept
{
    if (! active)
        return;

    low.housekeep();
    high.housekeep();

    if (! low.isRunning() && ! high.isRunning() && ! pulse.isActive()
          && bloomRemaining <= 0 && chickRemaining <= 0)
    {
        active = false;
        rampRemaining = 0;
        output.reset();
    }
}

//==============================================================================
void RimPiece::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    pulse.prepare (sr);
    bank.prepare (sr);
    output.prepare (sr);
    configure (Hit::rim);
    reset();
}

void RimPiece::reset() noexcept
{
    pulse.reset();
    bank.reset();
    output.reset();
    active = false;
}

void RimPiece::setBatterHz (double snareF0) noexcept
{
    batterHz = snareF0;
}

void RimPiece::configure (Hit hit) noexcept
{
    if (hit == Hit::sticks)
    {
        // Stick on stick: wood only, higher.
        bank.setNumModes (3);
        bank.setMode (0, 2200.0, 0.030, 1.0);
        bank.setMode (1, 3400.0, 0.022, 0.7);
        bank.setMode (2, 5300.0, 0.016, 0.45);
        return;
    }

    // Three wood modes and faint batter modes.
    bank.setNumModes (7);
    bank.setMode (0, 1600.0, 0.035, 1.0);
    bank.setMode (1, 2950.0, 0.025, 0.6);
    bank.setMode (2, 4300.0, 0.020, 0.4);

    const double ratios[] = { 1.0, 1.594, 2.136, 2.653 };

    for (int i = 0; i < 4; ++i)
        bank.setMode (3 + i, batterHz * ratios[i], 0.15, 0.08);
}

void RimPiece::strike (double velocity, Hit hit) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);
    configure (hit);
    pulse.trigger (pulseAmplitude (velocity, 0.0003, sr), 0.0003);
    active = true;
}

void RimPiece::choke (double) noexcept
{
    reset();
}

void RimPiece::housekeep() noexcept
{
    if (! active)
        return;

    bank.housekeep();

    if (! bank.isRunning() && ! pulse.isActive())
    {
        active = false;
        output.reset();
    }
}

//==============================================================================
void ShakerPiece::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    output.prepare (sr);

    // Cook's PhISEM figures are for 22.05 kHz; scaled so the shake sounds the
    // same at any rate (engine.md 0.5).
    const double scale = 22050.0 / sr;
    energyDecay = std::pow (0.9985, scale);
    soundDecay = std::pow (0.95, scale);
    collisionProbability = (1.0 / 1024.0) * scale;

    resonanceA.setBandpass (sr, 3200.0, 3.0);
    resonanceB.setBandpass (sr, 6500.0, 3.0);
    reset();
}

void ShakerPiece::reset() noexcept
{
    shakeEnergy = soundLevel = 0.0;
    resonanceA.reset();
    resonanceB.reset();
    output.reset();
    active = false;
}

void ShakerPiece::strike (double velocity) noexcept
{
    shakeEnergy += juce::jlimit (0.0, 1.0, velocity);
    active = true;
}

void ShakerPiece::choke (double) noexcept
{
    shakeEnergy = 0.0;
}

void ShakerPiece::housekeep() noexcept
{
    if (! active)
        return;

    if (! std::isfinite (soundLevel) || ! std::isfinite (shakeEnergy))
    {
        reset();
        ModalResonatorBank::getNanResetCounter().fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (shakeEnergy < 1.0e-5 && soundLevel < 1.0e-6)
        reset();
}

} // namespace luthier
