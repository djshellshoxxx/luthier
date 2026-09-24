#pragma once

/*  Modulation sources (modulation-matrix.md section 1).

    Every source here runs at control rate, not audio rate: once per
    control-rate tick the matrix asks each source for its current value, and the
    destinations interpolate between ticks. Control rate is a thirty-second of
    the block size with a floor of 128 samples, so a source costs roughly four
    evaluations per block instead of five hundred.

    Two rules shape the code:

      1. Nothing allocates after prepare(). The step sequencer's steps, the
         LFO's breakpoints and the envelope's stages are fixed-size arrays.

      2. Every source is deterministic. The random sources take a seed and a
         reset() puts them back to it, because an offline render that produced
         different "random" values on every pass would make the renderer useless
         for regression listening.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>
#include <cmath>

namespace luthier
{

//==============================================================================
/** The curve applied to a route's source value before it is scaled by depth
    (modulation-matrix 3). Input and output are both bipolar -1..+1. */
enum class ModCurve
{
    linear = 0,
    exponential,
    logarithmic,
    sCurve,
    numCurves
};

const char* getModCurveName (ModCurve curve) noexcept;

/** Applies a curve to a bipolar value, preserving sign and the -1/0/+1 fixed
    points. Shaping the magnitude and restoring the sign is what keeps a
    bipolar LFO symmetrical through an exponential curve. */
inline double applyModCurve (double bipolar, ModCurve curve) noexcept
{
    const double x = juce::jlimit (-1.0, 1.0, bipolar);
    const double sign = (x < 0.0) ? -1.0 : 1.0;
    const double a = std::abs (x);

    switch (curve)
    {
        case ModCurve::exponential:  return sign * a * a;
        case ModCurve::logarithmic:  return sign * std::sqrt (a);
        case ModCurve::sCurve:       return sign * a * a * (3.0 - 2.0 * a);
        case ModCurve::linear:
        case ModCurve::numCurves:
        default:                     return x;
    }
}

//==============================================================================
/** Musical divisions for tempo-synced sources, from a thirty-second triplet to
    eight bars, with dotted and triplet variants. */
enum class ModSyncDivision
{
    thirtySecondT = 0, thirtySecond, sixteenthT, sixteenth, sixteenthD,
    eighthT, eighth, eighthD,
    quarterT, quarter, quarterD,
    halfT, half, halfD,
    bar1, bar2, bar4, bar8,
    numDivisions
};

const char* getModSyncDivisionName (ModSyncDivision d) noexcept;

/** Length of one cycle of this division, in beats (quarter notes). */
double modSyncDivisionBeats (ModSyncDivision d) noexcept;

//==============================================================================
class ModLfo
{
public:
    /** Sets the default custom shape. Not in prepare(): that runs on every
        prepareToPlay, after the host has restored a session's shape. */
    ModLfo() noexcept;

    enum class Shape
    {
        sine = 0, triangle, rampUp, rampDown, square,
        sampleAndHold, randomSmooth, custom,
        numShapes
    };

    static const char* getShapeName (Shape s) noexcept;

    enum class Retrigger { freeRun = 0, onNoteOn, onTransportStart, onSyncBoundary };

    static constexpr int kNumBreakpoints = 8;

    void prepare (double controlRateHz, uint64_t seed) noexcept;
    void reset() noexcept;

    void setShape (Shape s) noexcept { shape = s; }
    Shape getShape() const noexcept { return shape; }

    /** Free rate in Hz, used when sync is off. */
    void setRateHz (double hz) noexcept { rateHz = juce::jlimit (0.01, 40.0, hz); }
    double getRateHz() const noexcept { return rateHz; }

    void setSynced (bool s) noexcept { synced = s; }
    bool isSynced() const noexcept { return synced; }
    void setSyncDivision (ModSyncDivision d) noexcept { division = d; }
    ModSyncDivision getSyncDivision() const noexcept { return division; }

    void setPhaseOffsetDegrees (double degrees) noexcept
    {
        phaseOffset = juce::jlimit (0.0, 360.0, degrees) / 360.0;
    }

    double getPhaseOffsetDegrees() const noexcept { return phaseOffset * 360.0; }

    void setDepth (double d) noexcept { depth = juce::jlimit (0.0, 1.0, d); }
    double getDepth() const noexcept { return depth; }

    /** Skews triangle and ramp shapes. 0.5 is symmetrical. */
    void setSymmetry (double s) noexcept { symmetry = juce::jlimit (0.01, 0.99, s); }
    double getSymmetry() const noexcept { return symmetry; }

    void setRetrigger (Retrigger r) noexcept { retrigger = r; }
    Retrigger getRetrigger() const noexcept { return retrigger; }

    /** Output smoothing, for the stepped shapes. */
    void setSmoothingMs (double ms) noexcept;
    double getSmoothingMs() const noexcept { return smoothingMs; }

    void setBipolar (bool b) noexcept { bipolar = b; }
    bool isBipolar() const noexcept { return bipolar; }

    void setBreakpoint (int index, double value) noexcept
    {
        if (juce::isPositiveAndBelow (index, kNumBreakpoints))
            breakpoints[(size_t) index] = juce::jlimit (-1.0, 1.0, value);
    }

    double getBreakpoint (int index) const noexcept
    {
        return juce::isPositiveAndBelow (index, kNumBreakpoints)
                 ? breakpoints[(size_t) index] : 0.0;
    }

    void noteOn() noexcept;
    void transportStarted() noexcept;

    /** Advances one control-rate tick and returns the output, bipolar -1..+1 or
        unipolar 0..1 depending on the polarity setting.

        `beatsPerTick` is how far the host transport moved during this tick; it
        drives the synced modes so that a synced LFO stays locked to the grid
        even when the user scrubs. */
    double tick (double beatsPerTick, double hostPositionBeats) noexcept;

    double getCurrent() const noexcept { return current; }

private:
    double shapeValue (double p) const noexcept;

    double controlRate = 344.0;
    Shape shape = Shape::sine;
    Retrigger retrigger = Retrigger::freeRun;

    double rateHz = 1.0;
    bool synced = false;
    ModSyncDivision division = ModSyncDivision::quarter;

    double phase = 0.0;
    double phaseOffset = 0.0;
    double depth = 1.0;
    double symmetry = 0.5;
    bool bipolar = true;

    double smoothingMs = 0.0;
    ExpSmoother smoother;

    // Sample-and-hold and smooth-random state.
    double heldValue = 0.0;
    double randomTarget = 0.0;
    double randomPrevious = 0.0;

    std::array<double, kNumBreakpoints> breakpoints {};

    double current = 0.0;

    RtRandom rng { 0x10DEC0DEull };
    uint64_t rngSeed = 0x10DEC0DEull;
};

//==============================================================================
/** DAHDSR envelope with per-stage curves and optional looping
    (modulation-matrix 1.2). */
class ModEnvelope
{
public:
    enum class Stage { idle = 0, delay, attack, hold, decay, sustain, release, numStages };
    enum class Retrigger { legato = 0, always, oneShot };
    enum class LoopMode { off = 0, decayToSustain, decayToRelease };

    void prepare (double controlRateHz) noexcept;
    void reset() noexcept;

    void setDelaySeconds (double s) noexcept   { delayTime = clampTime (s); }
    void setAttackSeconds (double s) noexcept  { attackTime = clampTime (s); }
    void setHoldSeconds (double s) noexcept    { holdTime = clampTime (s); }
    void setDecaySeconds (double s) noexcept   { decayTime = clampTime (s); }
    void setReleaseSeconds (double s) noexcept { releaseTime = clampTime (s); }

    double getDelaySeconds() const noexcept   { return delayTime; }
    double getAttackSeconds() const noexcept  { return attackTime; }
    double getHoldSeconds() const noexcept    { return holdTime; }
    double getDecaySeconds() const noexcept   { return decayTime; }
    double getReleaseSeconds() const noexcept { return releaseTime; }

    void setSustainLevel (double level) noexcept { sustainLevel = juce::jlimit (0.0, 1.0, level); }
    double getSustainLevel() const noexcept { return sustainLevel; }

    void setStageCurve (Stage stage, ModCurve curve) noexcept;
    ModCurve getStageCurve (Stage stage) const noexcept;

    void setRetrigger (Retrigger r) noexcept { retrigger = r; }
    Retrigger getRetrigger() const noexcept { return retrigger; }

    void setLoopMode (LoopMode m) noexcept { loopMode = m; }
    LoopMode getLoopMode() const noexcept { return loopMode; }

    void noteOn() noexcept;
    void noteOff() noexcept;

    double tick() noexcept;
    double getCurrent() const noexcept { return current; }
    Stage getStage() const noexcept { return stage; }
    bool isActive() const noexcept { return stage != Stage::idle; }

private:
    static double clampTime (double s) noexcept { return juce::jlimit (0.0, 30.0, s); }

    /** Samples remaining for a stage of this length, at least one so a zero
        length stage still passes through rather than dividing by zero. */
    int ticksFor (double seconds) const noexcept
    {
        return juce::jmax (1, (int) std::round (seconds * controlRate));
    }

    void enterStage (Stage s) noexcept;

    double controlRate = 344.0;

    double delayTime = 0.0, attackTime = 0.005, holdTime = 0.0;
    double decayTime = 0.15, releaseTime = 0.2;
    double sustainLevel = 0.7;

    std::array<ModCurve, (size_t) Stage::numStages> curves {};

    Retrigger retrigger = Retrigger::always;
    LoopMode loopMode = LoopMode::off;

    Stage stage = Stage::idle;
    int ticksRemaining = 0;
    int stageLength = 1;
    double stageStart = 0.0;
    double current = 0.0;
    bool held = false;
};

//==============================================================================
/** Step sequencer (modulation-matrix 1.3). */
class ModStepSequencer
{
public:
    static constexpr int kMaxSteps = 64;

    /** Sets the default steps; see ModLfo(). */
    ModStepSequencer() noexcept;

    enum class Direction { forward = 0, reverse, pingPong, random, brownian };

    struct Step
    {
        double value = 0.0;       // -1 .. +1
        bool gate = true;
        bool slide = false;
        double probability = 1.0; // 0 .. 1
    };

    void prepare (double controlRateHz, uint64_t seed) noexcept;
    void reset() noexcept;

    void setLength (int steps) noexcept { length = juce::jlimit (4, kMaxSteps, steps); }
    int getLength() const noexcept { return length; }

    void setDivision (ModSyncDivision d) noexcept { division = d; }
    ModSyncDivision getDivision() const noexcept { return division; }

    void setDirection (Direction d) noexcept { direction = d; }
    Direction getDirection() const noexcept { return direction; }

    void setSwing (double amount) noexcept { swing = juce::jlimit (0.0, 0.75, amount); }
    double getSwing() const noexcept { return swing; }

    void setStep (int index, const Step& step) noexcept;
    Step getStep (int index) const noexcept;

    /** Internal clock rate, used when the sequencer is not synced to the host. */
    void setSynced (bool s) noexcept { synced = s; }
    bool isSynced() const noexcept { return synced; }
    void setInternalRateHz (double hz) noexcept { internalRateHz = juce::jlimit (0.05, 40.0, hz); }

    void transportStarted() noexcept;

    double tick (double beatsPerTick) noexcept;
    double getCurrent() const noexcept { return current; }
    int getCurrentStep() const noexcept { return position; }

private:
    void advance() noexcept;

    double controlRate = 344.0;
    int length = 16;
    ModSyncDivision division = ModSyncDivision::sixteenth;
    Direction direction = Direction::forward;
    double swing = 0.0;
    bool synced = true;
    double internalRateHz = 4.0;

    std::array<Step, kMaxSteps> steps {};

    int position = 0;
    int pingPongDelta = 1;
    double stepPhase = 0.0;
    double current = 0.0;
    double slideFrom = 0.0;
    bool currentGateOpen = true;

    RtRandom rng { 0x5EE9uLL };
    uint64_t rngSeed = 0x5EE9uLL;
};

//==============================================================================
/** Envelope follower with selectable detection and a gate
    (modulation-matrix 1.4). Fed one audio value per control-rate tick from
    whichever signal the user pointed it at. */
class ModEnvelopeFollower
{
public:
    enum class Detection { peak = 0, rms, truePeak };
    enum class Source { mainOutput = 0, sidechain, perString, pickup };

    void prepare (double controlRateHz) noexcept;
    void reset() noexcept;

    void setAttackMs (double ms) noexcept;
    void setReleaseMs (double ms) noexcept;
    double getAttackMs() const noexcept { return attackMs; }
    double getReleaseMs() const noexcept { return releaseMs; }

    void setDetection (Detection d) noexcept { detection = d; }
    Detection getDetection() const noexcept { return detection; }

    void setSource (Source s) noexcept { source = s; }
    Source getSource() const noexcept { return source; }

    void setStringIndex (int index) noexcept { stringIndex = juce::jlimit (0, kMaxStrings - 1, index); }
    int getStringIndex() const noexcept { return stringIndex; }

    void setThreshold (double linear) noexcept { threshold = juce::jlimit (0.0, 1.0, linear); }
    double getThreshold() const noexcept { return threshold; }

    void setLogarithmic (bool l) noexcept { logOutput = l; }
    bool isLogarithmic() const noexcept { return logOutput; }

    /** `peak` and `meanSquare` describe the block the tick covers; which one is
        used depends on the detection mode. */
    double tick (double peak, double meanSquare) noexcept;

    double getCurrent() const noexcept { return current; }

private:
    double controlRate = 344.0;
    double attackMs = 10.0, releaseMs = 200.0;
    double attackCoeff = 0.0, releaseCoeff = 0.0;

    Detection detection = Detection::peak;
    Source source = Source::mainOutput;
    int stringIndex = 0;

    double threshold = 0.0;
    bool logOutput = false;

    double env = 0.0;
    double current = 0.0;
};

//==============================================================================
/** The three random sources of modulation-matrix 1.8, seeded so an offline
    render repeats exactly. */
class ModRandomSource
{
public:
    void prepare (double controlRateHz, uint64_t seed) noexcept;
    void reset() noexcept;

    void setSmoothingMs (double ms) noexcept;

    void noteOn() noexcept;
    void barStarted() noexcept;

    void tick() noexcept;

    double getPerNote() const noexcept { return perNote; }
    double getPerBar() const noexcept { return perBar; }
    double getSmooth() const noexcept { return smoothValue; }

private:
    double controlRate = 344.0;
    double perNote = 0.5;
    double perBar = 0.5;
    double smoothValue = 0.5;
    double smoothTarget = 0.5;
    double smoothCoeff = 0.0;

    RtRandom rng { 0xD1CEuLL };
    uint64_t rngSeed = 0xD1CEuLL;
};

} // namespace luthier
