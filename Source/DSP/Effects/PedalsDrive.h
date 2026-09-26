#pragma once

/*  Dynamics, filter and drive pedals - everything that normally sits in front of
    the amp. Every clipping stage runs oversampled (engine rule 10).
*/

#include "Pedal.h"
#include <vector>

namespace luthier
{

//==============================================================================
/** Peak compressor with selectable optical or FET character.

    Optical: slow, program-dependent release, very forgiving - a Dyna Comp.
    FET: fast attack, hard knee, audible pump - an 1176 in a stompbox.
*/
class CompressorPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Compressor; }
    const char* getName() const noexcept override { return "Compressor"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 6; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

    /** Current gain reduction in dB, for the UI meter. */
    double getGainReductionDb() const noexcept { return lastGrDb; }

protected:
    void parameterChanged (int index, double value) override;

private:
    double thresholdDb = -18.0, ratio = 4.0, attackMs = 8.0, releaseMs = 120.0;
    double makeupDb = 0.0;
    int character = 0;   // 0 = optical, 1 = FET

    EnvelopeFollower detector;
    ExpSmoother grSmooth;
    double lastGrDb = 0.0;
};

//==============================================================================
/** Noise gate. Not in the spec's pedal list, but a high-gain preset without one
    is unusable, and the string model's noise floor is real rather than dithered. */
class NoiseGatePedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::NoiseGate; }
    const char* getName() const noexcept override { return "Noise Gate"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 4; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double thresholdDb = -60.0, attackMs = 1.0, holdMs = 40.0, releaseMs = 120.0;
    EnvelopeFollower detector;
    double envelope = 0.0;
    int holdCounter = 0, holdSamples = 0;
    double attackCoeff = 0.0, releaseCoeff = 0.0;
};

//==============================================================================
/** Wah: a resonant bandpass swept by the expression pedal. */
class WahPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Wah; }
    const char* getName() const noexcept override { return "Wah"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double minHz = 400.0, maxHz = 2200.0, q = 4.0, level = 1.0;
    int autoMode = 0;      // 0 = pedal, 1 = auto LFO
    double autoRateHz = 1.2;

    Biquad filterL, filterR;
    Lfo lfo;
    ExpSmoother sweep;
    double lastFreq = 0.0;
};

//==============================================================================
/** Envelope filter: the same bandpass, swept by the playing dynamics instead. */
class EnvelopeFilterPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::EnvelopeFilter; }
    const char* getName() const noexcept override { return "Envelope Filter"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 6; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double sensitivity = 0.5, baseHz = 250.0, rangeOctaves = 3.0, q = 3.5;
    double attackMs = 10.0, releaseMs = 160.0;
    int direction = 0;   // 0 = up, 1 = down

    EnvelopeFollower follower;
    Biquad filterL, filterR;
    double lastFreq = 0.0;
};

//==============================================================================
/** Octaver: a monophonic sub-octave built by tracking zero crossings and
    generating a square at half the rate, then filtering it back to something
    musical. That is exactly how the analogue originals worked, including their
    tracking glitches on chords. */
class OctaverPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Octaver; }
    const char* getName() const noexcept override { return "Octaver"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 4; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double subLevel = 0.6, sub2Level = 0.0, dryLevel = 1.0, toneHz = 2500.0;

    OnePoleHP inputHp;
    OnePoleLP trackLp, outputLp;
    bool flipFlop1 = false, flipFlop2 = false;
    double lastSign = 0.0;
    EnvelopeFollower amplitude;
};

//==============================================================================
/** Pitch shifter: overlap-add (WSOLA-style) with pitch-synchronous window
    selection, +/- 12 semitones with a dry mix. */
class PitchShifterPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::PitchShifter; }
    const char* getName() const noexcept override { return "Pitch Shifter"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 4; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;
    int getLatencySamples() const noexcept override { return 0; }

protected:
    void parameterChanged (int index, double value) override;

private:
    struct Shifter
    {
        std::vector<double> buffer;
        int writeIndex = 0;
        double readPhase = 0.0;
        int size = 0;
        int windowSamples = 0;

        void prepare (int bufferSize, int window);
        void reset() noexcept;
        double process (double input, double ratio) noexcept;
    };

    double semitones = -12.0, fineCents = 0.0, mix = 0.5, ratio = 0.5;
    Shifter shifterL, shifterR;
};

//==============================================================================
/** Shared implementation for the three drive pedals. They differ in their
    pre-EQ, their transfer curve and their post-EQ, which is exactly what makes a
    Tube Screamer sound different from a DS-1 and both different from a Fuzz Face. */
class DrivePedalBase : public Pedal
{
public:
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    int getLatencySamples() const noexcept override { return oversampler.getLatencySamples(); }

    void setOversamplingFactor (int factor) noexcept override;

protected:
    void parameterChanged (int index, double value) override;
    virtual double shape (double x) const noexcept = 0;
    virtual void updateFilters() = 0;

    double drive = 0.4, tone = 0.5, level = 0.5, presence = 0.5;
    int voicing = 0;

    Biquad preL, preR, postL, postR, midL, midR;
    OnePoleHP dcL, dcR;
    Oversampler oversampler, oversamplerR;
    ExpSmoother driveSmooth, levelSmooth;
};

//==============================================================================
/** Overdrive: symmetric soft clipping with a strong mid hump. Tube Screamer. */
class OverdrivePedal : public DrivePedalBase
{
public:
    PedalType getType() const noexcept override { return PedalType::Overdrive; }
    const char* getName() const noexcept override { return "Overdrive"; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    double shape (double x) const noexcept override;
    void updateFilters() override;
};

//==============================================================================
/** Distortion: harder, more symmetric clipping with a scooped, brighter voice. */
class DistortionPedal : public DrivePedalBase
{
public:
    PedalType getType() const noexcept override { return PedalType::Distortion; }
    const char* getName() const noexcept override { return "Distortion"; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    double shape (double x) const noexcept override;
    void updateFilters() override;
};

//==============================================================================
/** Fuzz: asymmetric, gated, and deliberately unstable, with the low input
    impedance that makes a real fuzz clean up when you roll the guitar volume off. */
class FuzzPedal : public DrivePedalBase
{
public:
    PedalType getType() const noexcept override { return PedalType::Fuzz; }
    const char* getName() const noexcept override { return "Fuzz"; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    double shape (double x) const noexcept override;
    void updateFilters() override;
};

//==============================================================================
/** Boost: clean gain with a gentle tilt. Klon-ish. */
class BoostPedal : public DrivePedalBase
{
public:
    PedalType getType() const noexcept override { return PedalType::Boost; }
    const char* getName() const noexcept override { return "Boost"; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    double shape (double x) const noexcept override;
    void updateFilters() override;
};

//==============================================================================
/** Volume pedal: gain driven by the expression input, with a selectable taper. */
class VolumePedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::VolumePedal; }
    const char* getName() const noexcept override { return "Volume Pedal"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 3; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double minLevel = 0.0, maxLevel = 1.0;
    int taper = 0;   // 0 = log, 1 = linear
    ExpSmoother gain;
};

} // namespace luthier
