#include "ModSources.h"

namespace luthier
{

//==============================================================================
const char* getModCurveName (ModCurve curve) noexcept
{
    switch (curve)
    {
        case ModCurve::linear:       return "Linear";
        case ModCurve::exponential:  return "Exp";
        case ModCurve::logarithmic:  return "Log";
        case ModCurve::sCurve:       return "S";
        case ModCurve::numCurves:
        default:                     return "Linear";
    }
}

//==============================================================================
namespace
{
    struct SyncDivisionEntry
    {
        const char* name;
        double beats;
    };

    // Beats here are quarter notes: a bar is four of them in 4/4, which is the
    // assumption the rest of the plugin already makes for tempo-synced delays.
    const SyncDivisionEntry kDivisions[] =
    {
        { "1/32T", 0.125 * 2.0 / 3.0 },
        { "1/32",  0.125 },
        { "1/16T", 0.25 * 2.0 / 3.0 },
        { "1/16",  0.25 },
        { "1/16.", 0.25 * 1.5 },
        { "1/8T",  0.5 * 2.0 / 3.0 },
        { "1/8",   0.5 },
        { "1/8.",  0.5 * 1.5 },
        { "1/4T",  1.0 * 2.0 / 3.0 },
        { "1/4",   1.0 },
        { "1/4.",  1.0 * 1.5 },
        { "1/2T",  2.0 * 2.0 / 3.0 },
        { "1/2",   2.0 },
        { "1/2.",  2.0 * 1.5 },
        { "1 bar", 4.0 },
        { "2 bar", 8.0 },
        { "4 bar", 16.0 },
        { "8 bar", 32.0 }
    };

    static_assert (sizeof (kDivisions) / sizeof (kDivisions[0])
                     == (size_t) ModSyncDivision::numDivisions,
                   "sync division table and enum disagree");
}

const char* getModSyncDivisionName (ModSyncDivision d) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) ModSyncDivision::numDivisions - 1, (int) d);
    return kDivisions[i].name;
}

double modSyncDivisionBeats (ModSyncDivision d) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) ModSyncDivision::numDivisions - 1, (int) d);
    return kDivisions[i].beats;
}

//==============================================================================
const char* ModLfo::getShapeName (Shape s) noexcept
{
    switch (s)
    {
        case Shape::sine:          return "Sine";
        case Shape::triangle:      return "Triangle";
        case Shape::rampUp:        return "Ramp Up";
        case Shape::rampDown:      return "Ramp Down";
        case Shape::square:        return "Square";
        case Shape::sampleAndHold: return "S+H";
        case Shape::randomSmooth:  return "Random";
        case Shape::custom:        return "Custom";
        case Shape::numShapes:
        default:                   return "Sine";
    }
}

ModLfo::ModLfo() noexcept
{
    // A flat custom shape is a ramp, which is the least surprising thing for a
    // breakpoint editor to start life as.
    for (int i = 0; i < kNumBreakpoints; ++i)
        breakpoints[(size_t) i] = -1.0 + 2.0 * (double) i / (double) (kNumBreakpoints - 1);
}

void ModLfo::prepare (double controlRateHz, uint64_t seed) noexcept
{
    controlRate = juce::jmax (1.0, controlRateHz);
    rngSeed = seed;

    smoother.prepare (controlRate, smoothingMs * 0.001);

    reset();
}

void ModLfo::reset() noexcept
{
    rng.setSeed (rngSeed);

    phase = 0.0;

    heldValue = rng.nextDouble() * 2.0 - 1.0;
    randomTarget = rng.nextDouble() * 2.0 - 1.0;
    randomPrevious = randomTarget;

    current = 0.0;
    smoother.snapTo (0.0);
}

void ModLfo::setSmoothingMs (double ms) noexcept
{
    smoothingMs = juce::jlimit (0.0, 500.0, ms);
    smoother.setTime (smoothingMs * 0.001);
}

void ModLfo::noteOn() noexcept
{
    if (retrigger == Retrigger::onNoteOn)
        phase = 0.0;
}

void ModLfo::transportStarted() noexcept
{
    if (retrigger == Retrigger::onTransportStart || retrigger == Retrigger::onSyncBoundary)
        phase = 0.0;
}

double ModLfo::shapeValue (double p) const noexcept
{
    // p is 0..1 within the cycle. Every branch returns bipolar -1..+1.
    switch (shape)
    {
        case Shape::sine:
            return std::sin (p * 2.0 * constants::kPi);

        case Shape::triangle:
        {
            // Symmetry moves the peak: at 0.5 this is the usual triangle, at
            // 0.1 it is a fast rise and a slow fall.
            if (p < symmetry)
                return -1.0 + 2.0 * (p / symmetry);

            return 1.0 - 2.0 * ((p - symmetry) / juce::jmax (1.0e-6, 1.0 - symmetry));
        }

        case Shape::rampUp:
        {
            // Symmetry compresses the ramp into part of the cycle and holds the
            // rest, which is what makes a ramp useful as a gate.
            const double scaled = juce::jmin (1.0, p / symmetry);
            return -1.0 + 2.0 * scaled;
        }

        case Shape::rampDown:
        {
            const double scaled = juce::jmin (1.0, p / symmetry);
            return 1.0 - 2.0 * scaled;
        }

        case Shape::square:
            return (p < symmetry) ? 1.0 : -1.0;

        case Shape::sampleAndHold:
        case Shape::randomSmooth:
            return heldValue;

        case Shape::custom:
        {
            // Linear interpolation between the eight breakpoints, wrapping so
            // the last point joins back to the first.
            const double scaled = p * (double) kNumBreakpoints;
            const int i0 = juce::jlimit (0, kNumBreakpoints - 1, (int) scaled);
            const int i1 = (i0 + 1) % kNumBreakpoints;
            const double frac = scaled - (double) i0;

            return breakpoints[(size_t) i0] * (1.0 - frac) + breakpoints[(size_t) i1] * frac;
        }

        case Shape::numShapes:
        default:
            return 0.0;
    }
}

double ModLfo::tick (double beatsPerTick, double hostPositionBeats) noexcept
{
    const double phaseBefore = phase;
    bool wrapped = false;
    bool assignedFromHost = false;

    // ---- advance the phase ---------------------------------------------------
    if (synced)
    {
        const double cycleBeats = juce::jmax (1.0e-6, modSyncDivisionBeats (division));

        if (retrigger == Retrigger::onSyncBoundary && hostPositionBeats >= 0.0)
        {
            // Locked to the transport: the phase is a function of where the host
            // is, so scrubbing or looping lands on the right part of the cycle
            // instead of wherever free-running had got to.
            phase = std::fmod (hostPositionBeats / cycleBeats, 1.0);

            if (phase < 0.0)
                phase += 1.0;

            // The phase was assigned rather than advanced, so a wrap shows up as
            // the phase going backwards.
            wrapped = (phase < phaseBefore);
            assignedFromHost = true;
        }
        else
        {
            phase += beatsPerTick / cycleBeats;
        }
    }
    else
    {
        phase += rateHz / controlRate;
    }

    // ---- cycle boundary ------------------------------------------------------
    // The phase is wrapped into [0, 1) at the end of every tick, so asking
    // whether floor(phase) changed would answer yes twice per cycle: once when
    // it reaches 1 and again when the wrap takes it back to 0.
    // Only a phase assigned from the host is already in range. A synced LFO
    // with the transport stopped free-runs, and skipping this for it let the
    // phase grow without bound and froze the stepped shapes.
    if (! assignedFromHost)
    {
        wrapped = (phase >= 1.0);
        phase -= std::floor (phase);
    }

    if (wrapped)
    {
        // The stepped shapes pick a new value once per cycle.
        randomPrevious = randomTarget;
        randomTarget = rng.nextDouble() * 2.0 - 1.0;

        if (shape == Shape::sampleAndHold)
            heldValue = randomTarget;
    }

    double p = phase + phaseOffset;
    p -= std::floor (p);

    // Smooth random interpolates between successive draws rather than stepping.
    if (shape == Shape::randomSmooth)
    {
        const double frac = p;
        heldValue = randomPrevious + (randomTarget - randomPrevious)
                                       * frac * frac * (3.0 - 2.0 * frac);
    }

    double value = shapeValue (p) * depth;

    // Output smoothing, which only has anything to do on the stepped shapes but
    // costs one multiply-add either way.
    if (smoothingMs > 0.0)
    {
        smoother.setTarget (value);
        value = smoother.next();
    }
    else
    {
        smoother.snapTo (value);
    }

    if (! bipolar)
        value = value * 0.5 + 0.5;

    current = sanitise (value);
    return current;
}

//==============================================================================
ModEnvelope::ModEnvelope() noexcept
{
    curves.fill (ModCurve::linear);

    // An exponential decay and release is what an envelope sounds like; a linear
    // one sounds like a fader being pulled.
    curves[(size_t) Stage::decay] = ModCurve::exponential;
    curves[(size_t) Stage::release] = ModCurve::exponential;
}

void ModEnvelope::prepare (double controlRateHz) noexcept
{
    controlRate = juce::jmax (1.0, controlRateHz);
    reset();
}

void ModEnvelope::reset() noexcept
{
    stage = Stage::idle;
    ticksRemaining = 0;
    stageLength = 1;
    stageStart = 0.0;
    current = 0.0;
    held = false;
}

void ModEnvelope::setStageCurve (Stage s, ModCurve curve) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) Stage::numStages - 1, (int) s);
    curves[i] = curve;
}

ModCurve ModEnvelope::getStageCurve (Stage s) const noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) Stage::numStages - 1, (int) s);
    return curves[i];
}

void ModEnvelope::enterStage (Stage s) noexcept
{
    stage = s;
    stageStart = current;

    switch (s)
    {
        case Stage::delay:   stageLength = ticksFor (delayTime);   break;
        case Stage::attack:  stageLength = ticksFor (attackTime);  break;
        case Stage::hold:    stageLength = ticksFor (holdTime);    break;
        case Stage::decay:   stageLength = ticksFor (decayTime);   break;
        case Stage::release: stageLength = ticksFor (releaseTime); break;

        case Stage::sustain:
        case Stage::idle:
        case Stage::numStages:
        default:             stageLength = 1; break;
    }

    ticksRemaining = stageLength;
}

void ModEnvelope::noteOn() noexcept
{
    held = true;

    // Legato only restarts if the envelope had finished; otherwise the note
    // joins the one already sounding, which is the point of legato.
    if (retrigger == Retrigger::legato && stage != Stage::idle)
        return;

    if (retrigger == Retrigger::oneShot && stage != Stage::idle)
        return;

    enterStage (delayTime > 0.0 ? Stage::delay : Stage::attack);
}

void ModEnvelope::noteOff() noexcept
{
    held = false;

    if (stage == Stage::idle || stage == Stage::release)
        return;

    // A one-shot envelope plays out its whole shape regardless of the key.
    if (retrigger == Retrigger::oneShot)
        return;

    enterStage (Stage::release);
}

double ModEnvelope::tick() noexcept
{
    if (stage == Stage::idle)
    {
        current = 0.0;
        return current;
    }

    if (stage == Stage::sustain)
    {
        current = sustainLevel;
        return current;
    }

    const double position = (stageLength <= 1)
                              ? 1.0
                              : 1.0 - ((double) juce::jmax (0, ticksRemaining - 1)
                                         / (double) (stageLength - 1));

    double target = current;

    switch (stage)
    {
        case Stage::delay:   target = stageStart; break;
        case Stage::attack:  target = 1.0; break;
        case Stage::hold:    target = stageStart; break;
        case Stage::decay:   target = sustainLevel; break;
        case Stage::release: target = 0.0; break;

        case Stage::sustain:
        case Stage::idle:
        case Stage::numStages:
        default: break;
    }

    if (stage == Stage::delay || stage == Stage::hold)
    {
        current = stageStart;
    }
    else
    {
        // The curve is applied to the stage's progress, not to its output, so
        // that an exponential decay spends its time where it should.
        const double shaped = 0.5 * (applyModCurve (position * 2.0 - 1.0,
                                                    getStageCurve (stage)) + 1.0);

        current = stageStart + (target - stageStart) * shaped;
    }

    if (--ticksRemaining <= 0)
    {
        switch (stage)
        {
            case Stage::delay:
                enterStage (Stage::attack);
                break;

            case Stage::attack:
                current = 1.0;
                enterStage (holdTime > 0.0 ? Stage::hold : Stage::decay);
                break;

            case Stage::hold:
                enterStage (Stage::decay);
                break;

            case Stage::decay:
                current = sustainLevel;

                if (loopMode == LoopMode::decayToSustain)
                {
                    // Re-run the decay from the top: an evolving pad rather than
                    // a one-shot.
                    current = 1.0;
                    enterStage (Stage::decay);
                }
                else if (loopMode == LoopMode::decayToRelease)
                {
                    enterStage (Stage::release);
                }
                else
                {
                    stage = held ? Stage::sustain : Stage::release;

                    if (stage == Stage::release)
                        enterStage (Stage::release);
                }
                break;

            case Stage::release:
                current = 0.0;

                if (loopMode == LoopMode::decayToRelease && held)
                {
                    current = 1.0;
                    enterStage (Stage::decay);
                }
                else
                {
                    stage = Stage::idle;
                }
                break;

            case Stage::sustain:
            case Stage::idle:
            case Stage::numStages:
            default:
                break;
        }
    }

    current = juce::jlimit (0.0, 1.0, sanitise (current));
    return current;
}

//==============================================================================
ModStepSequencer::ModStepSequencer() noexcept
{
    // A gentle default ramp, so a newly added sequencer does something audible
    // rather than nothing at all. Here, not in prepare() (see ModEnvelope).
    for (int i = 0; i < kMaxSteps; ++i)
    {
        steps[(size_t) i].value = -1.0 + 2.0 * (double) (i % 16) / 15.0;
        steps[(size_t) i].gate = true;
        steps[(size_t) i].slide = false;
        steps[(size_t) i].probability = 1.0;
    }

}

void ModStepSequencer::prepare (double controlRateHz, uint64_t seed) noexcept
{
    controlRate = juce::jmax (1.0, controlRateHz);
    rngSeed = seed;

    reset();
}

void ModStepSequencer::reset() noexcept
{
    rng.setSeed (rngSeed);
    position = 0;
    pingPongDelta = 1;
    stepPhase = 0.0;
    current = 0.0;
    slideFrom = 0.0;
    currentGateOpen = true;
}

void ModStepSequencer::setStep (int index, const Step& step) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxSteps))
        return;

    auto& s = steps[(size_t) index];
    s.value = juce::jlimit (-1.0, 1.0, step.value);
    s.gate = step.gate;
    s.slide = step.slide;
    s.probability = juce::jlimit (0.0, 1.0, step.probability);
}

ModStepSequencer::Step ModStepSequencer::getStep (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kMaxSteps) ? steps[(size_t) index] : Step {};
}

void ModStepSequencer::transportStarted() noexcept
{
    position = (direction == Direction::reverse) ? length - 1 : 0;
    pingPongDelta = 1;
    stepPhase = 0.0;
}

void ModStepSequencer::advance() noexcept
{
    slideFrom = current;

    switch (direction)
    {
        case Direction::forward:
            position = (position + 1) % length;
            break;

        case Direction::reverse:
            position = (position - 1 + length) % length;
            break;

        case Direction::pingPong:
            position += pingPongDelta;

            // Turn around one step inside each end, so the end steps are not
            // played twice in a row.
            if (position >= length - 1) { position = length - 1; pingPongDelta = -1; }
            else if (position <= 0)     { position = 0;          pingPongDelta = 1; }
            break;

        case Direction::random:
            position = (int) (rng.nextDouble() * (double) length);
            position = juce::jlimit (0, length - 1, position);
            break;

        case Direction::brownian:
        {
            // A random walk: mostly neighbours, occasionally standing still.
            const int step = (int) (rng.nextDouble() * 3.0) - 1;
            position = (position + step + length) % length;
            break;
        }

        default:
            break;
    }

    // Probability is rolled per visit, not per step, so a 50% step is a
    // different half of the time on each pass.
    const auto& s = steps[(size_t) position];
    currentGateOpen = s.gate && (rng.nextDouble() <= s.probability);
}

double ModStepSequencer::tick (double beatsPerTick) noexcept
{
    const double stepBeats = juce::jmax (1.0e-6, modSyncDivisionBeats (division));

    // Swing lengthens the odd steps and shortens the even ones by the same
    // amount, so a swung bar is exactly as long as a straight one.
    const bool isOffbeat = (position % 2) != 0;
    const double swingScale = isOffbeat ? (1.0 - swing * 0.5) : (1.0 + swing * 0.5);

    const double advanceAmount = synced
                                   ? (beatsPerTick / (stepBeats * swingScale))
                                   : (internalRateHz / (controlRate * swingScale));

    stepPhase += advanceAmount;

    while (stepPhase >= 1.0)
    {
        stepPhase -= 1.0;
        advance();
    }

    const auto& s = steps[(size_t) juce::jlimit (0, kMaxSteps - 1, position)];

    if (! currentGateOpen)
    {
        // A closed gate holds the last value rather than snapping to zero, which
        // would be an audible click on a pitch destination.
        current = sanitise (current);
        return current;
    }

    if (s.slide)
        current = slideFrom + (s.value - slideFrom) * juce::jlimit (0.0, 1.0, stepPhase);
    else
        current = s.value;

    current = sanitise (current);
    return current;
}

//==============================================================================
void ModEnvelopeFollower::prepare (double controlRateHz) noexcept
{
    controlRate = juce::jmax (1.0, controlRateHz);
    setAttackMs (attackMs);
    setReleaseMs (releaseMs);
    reset();
}

void ModEnvelopeFollower::reset() noexcept
{
    env = 0.0;
    current = 0.0;
}

void ModEnvelopeFollower::setAttackMs (double ms) noexcept
{
    attackMs = juce::jlimit (0.1, 500.0, ms);
    attackCoeff = std::exp (-1.0 / juce::jmax (1.0e-6, attackMs * 0.001 * controlRate));
}

void ModEnvelopeFollower::setReleaseMs (double ms) noexcept
{
    releaseMs = juce::jlimit (1.0, 5000.0, ms);
    releaseCoeff = std::exp (-1.0 / juce::jmax (1.0e-6, releaseMs * 0.001 * controlRate));
}

double ModEnvelopeFollower::tick (double peak, double meanSquare) noexcept
{
    double detected = 0.0;

    switch (detection)
    {
        case Detection::rms:
            detected = std::sqrt (juce::jmax (0.0, meanSquare));
            break;

        case Detection::truePeak:
            // The block peak already over-estimates the sample peak slightly
            // because it is taken before any smoothing; the extra headroom
            // allowance is what distinguishes it from plain peak.
            detected = peak * 1.122;   // +1 dB, the usual true-peak allowance
            break;

        case Detection::peak:
        default:
            detected = peak;
            break;
    }

    detected = std::abs (sanitise (detected));

    // The gate is on the input, not the output: below threshold the follower
    // sees silence and releases, rather than holding a value it must not report.
    if (detected < threshold)
        detected = 0.0;

    const double coeff = (detected > env) ? attackCoeff : releaseCoeff;
    env = flushDenormal (detected + (env - detected) * coeff);

    current = logOutput
                ? juce::jlimit (0.0, 1.0, (juce::Decibels::gainToDecibels (env, -60.0) + 60.0) / 60.0)
                : juce::jlimit (0.0, 1.0, env);

    return current;
}

//==============================================================================
void ModRandomSource::prepare (double controlRateHz, uint64_t seed) noexcept
{
    controlRate = juce::jmax (1.0, controlRateHz);
    rngSeed = seed;
    setSmoothingMs (250.0);
    reset();
}

void ModRandomSource::reset() noexcept
{
    rng.setSeed (rngSeed);
    perNote = rng.nextDouble();
    perBar = rng.nextDouble();
    smoothTarget = rng.nextDouble();
    smoothValue = smoothTarget;
}

void ModRandomSource::setSmoothingMs (double ms) noexcept
{
    const double seconds = juce::jlimit (1.0, 5000.0, ms) * 0.001;
    smoothCoeff = std::exp (-1.0 / juce::jmax (1.0e-6, seconds * controlRate));
}

void ModRandomSource::noteOn() noexcept
{
    perNote = rng.nextDouble();
}

void ModRandomSource::barStarted() noexcept
{
    perBar = rng.nextDouble();
}

void ModRandomSource::tick() noexcept
{
    // A new target every tick, heavily smoothed: a continuous drift rather than
    // the staircase a sample-and-hold would give.
    smoothTarget = rng.nextDouble();
    smoothValue = flushDenormal (smoothTarget + (smoothValue - smoothTarget) * smoothCoeff);
}

} // namespace luthier
