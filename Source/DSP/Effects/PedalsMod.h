#pragma once

/*  Modulation, delay, reverb and EQ - the pedals that normally live in the amp's
    effects loop, after the preamp.

    All of these are stereo, and all of them are checked for mono compatibility:
    a wide chorus or reverb that cancels when a listener folds the mix to mono is
    a real failure mode (pitfall 19), so the wide effects here decorrelate by
    delay and filtering rather than by polarity inversion.
*/

#include "Pedal.h"
#include <vector>

namespace luthier
{

//==============================================================================
/** Modulated delay line shared by chorus, flanger and the rotary's Doppler. */
class ModDelayLine
{
public:
    void prepare (double sampleRate, double maxDelayMs);
    void reset() noexcept;

    /** Writes one sample and reads back `delayMs` milliseconds ago. */
    double process (double input, double delayMs) noexcept;

    /** Read without writing, for multi-tap use. */
    double tap (double delayMs) const noexcept;
    void write (double input) noexcept;

private:
    std::vector<double> buffer;
    int size = 0, mask = 0, writeIndex = 0;
    double sr = 44100.0;
};

//==============================================================================
class ChorusPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Chorus; }
    const char* getName() const noexcept override { return "Chorus"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    static constexpr int kMaxVoices = 4;

    double rateHz = 0.6, depthMs = 3.5, centreMs = 18.0, mix = 0.45;
    int numVoices = 2;

    ModDelayLine lineL[kMaxVoices], lineR[kMaxVoices];
    Lfo lfo[kMaxVoices];
};

//==============================================================================
/** ambiguity-resolutions.md 3: a classic ADT / hardware doubler. A second
    performance, slightly late and slightly out of tune, panned away from the
    first; stereo adds a mirror-image third. Post-amp and pre-cab, so the
    cabinet colours both takes the same. At 0 cents a voice is an exact delay
    (the crossfaded read heads are centred on it), which 3.2's cross-
    correlation test relies on. */
class DoublerPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Doubler; }
    const char* getName() const noexcept override { return "Doubler"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 7; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    /** One take: a delay whose read point drifts by the pitch offset, with two
        heads half a window apart crossfaded so the drift wraps without a click. */
    struct Voice
    {
        std::vector<double> buffer;
        int size = 0, mask = 0, writeIndex = 0;
        double phase = 0.0;
        int window = 480;

        void prepare (int maxDelaySamples, int windowSamples);
        void reset() noexcept;
        double process (double input, double delaySamples, double ratio) noexcept;
    };

    void updateFilters() noexcept;

    double delayMs = 22.0, cents = -8.0, pan = -0.7, mix = 0.4, hpHz = 100.0, lpHz = 8000.0;
    bool stereo = true;

    Voice first, second;
    Biquad hp1, lp1, hp2, lp2;
};

//==============================================================================
class PhaserPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Phaser; }
    const char* getName() const noexcept override { return "Phaser"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    static constexpr int kMaxStages = 12;

    double rateHz = 0.4, depth = 0.7, feedback = 0.35, mix = 0.5;
    int stages = 4;

    Allpass1 apL[kMaxStages], apR[kMaxStages];
    Lfo lfo;
    double fbL = 0.0, fbR = 0.0;
};

//==============================================================================
class FlangerPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Flanger; }
    const char* getName() const noexcept override { return "Flanger"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double rateHz = 0.25, depthMs = 3.0, centreMs = 4.0, feedback = 0.6, mix = 0.5;

    ModDelayLine lineL, lineR;
    Lfo lfo;
    double fbL = 0.0, fbR = 0.0;
};

//==============================================================================
class TremoloPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Tremolo; }
    const char* getName() const noexcept override { return "Tremolo"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double rateHz = 5.0, depth = 0.6, stereoPhase = 0.0;
    int shape = 0, sync = 0;

    Lfo lfoL, lfoR;
};

//==============================================================================
/** Rotary speaker: horn and drum rotating at different speeds, each producing
    Doppler pitch shift and amplitude modulation, picked up by two mics. */
class RotaryPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::RotarySpeaker; }
    const char* getName() const noexcept override { return "Rotary"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double hornRateHz = 6.0, drumRateHz = 0.8, mix = 0.7, distance = 0.5;
    int speedMode = 1;   // 0 = slow (chorale), 1 = fast (tremolo)

    double hornPhase = 0.0, drumPhase = 0.0;
    double hornTarget = 6.0, drumTarget = 0.8;
    double hornCurrent = 6.0, drumCurrent = 0.8;

    ModDelayLine hornL, hornR, drumL, drumR;
    Biquad crossoverLowL, crossoverLowR, crossoverHighL, crossoverHighR;
};

//==============================================================================
class DelayPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Delay; }
    const char* getName() const noexcept override { return "Delay"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 7; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double timeMs = 375.0, feedback = 0.35, mix = 0.3;
    int character = 0;   // 0 = digital, 1 = analogue, 2 = tape
    int syncMode = 0;    // 0 = free, 1..n = note divisions
    int pingPong = 0;
    double toneHz = 6000.0;

    std::vector<double> bufL, bufR;
    int size = 0, mask = 0, writeIndex = 0;

    ExpSmoother timeSmooth;
    OnePoleLP dampL, dampR;
    OnePoleHP hpL, hpR;
    Lfo wow, flutter;
    DCBlocker dcL, dcR;

    double currentDelaySamples() const noexcept;
    double readTap (const std::vector<double>& buf, double delaySamples) const noexcept;
};

//==============================================================================
/** Feedback-delay-network reverb: plate, hall, room and chamber characters. */
class ReverbPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::Reverb; }
    const char* getName() const noexcept override { return "Reverb"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 6; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    static constexpr int kFdnSize = 8;

    double size = 0.5, decaySeconds = 2.0, damping = 0.5, preDelayMs = 20.0, mix = 0.25;
    int character = 0;   // 0 = plate, 1 = hall, 2 = room, 3 = chamber

    static constexpr double kMaxLineSeconds = 0.35;
    std::vector<double> lines[kFdnSize];
    int lineLengths[kFdnSize] = {};
    int lineIndex[kFdnSize] = {};
    OnePoleLP lineDamp[kFdnSize];
    double feedbackGain = 0.7;

    std::vector<double> preDelayBuf;
    int preSize = 0, preMask = 0, preIndex = 0;

    Allpass1 diffusionL[4], diffusionR[4];
    DCBlocker dcL, dcR;

    void rebuildLines();
};

//==============================================================================
/** Spring reverb: three parallel dispersive delay lines. The chirpy "boing" comes
    from the all-pass cascade in each line, which delays low frequencies more than
    high ones exactly as a real spring does. */
class SpringReverbPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::SpringReverb; }
    const char* getName() const noexcept override { return "Spring Reverb"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 5; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    static constexpr int kSprings = 3;
    static constexpr int kDispersion = 24;

    double decay = 0.5, tension = 0.5, mix = 0.3, toneHz = 4500.0, drip = 0.5;

    std::vector<double> springBuf[kSprings];
    int springLen[kSprings] = {};
    int springIndex[kSprings] = {};
    Allpass1 dispersion[kSprings][kDispersion];
    OnePoleLP springTone[kSprings];
    Biquad bodyResonance;
    DCBlocker dcOut;
};

//==============================================================================
class GraphicEqPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::GraphicEQ; }
    const char* getName() const noexcept override { return "Graphic EQ"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 7; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    static constexpr int kBands = 6;
    static const double kBandFrequencies[kBands];

    double gains[kBands] = {};
    double outputLevel = 1.0;
    Biquad bandL[kBands], bandR[kBands];
};

//==============================================================================
class ParametricEqPedal : public Pedal
{
public:
    PedalType getType() const noexcept override { return PedalType::ParametricEQ; }
    const char* getName() const noexcept override { return "Parametric EQ"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void process (double* left, double* right, int numSamples) noexcept override;

    int getNumParameters() const noexcept override { return 9; }
    const PedalParam& getParameterDescriptor (int index) const noexcept override;

protected:
    void parameterChanged (int index, double value) override;

private:
    double lowFreq = 120.0, lowGain = 0.0;
    double midFreq = 800.0, midGain = 0.0, midQ = 1.0;
    double highFreq = 4000.0, highGain = 0.0;
    double hpFreq = 20.0, lpFreq = 20000.0;

    Biquad lowL, lowR, midBL, midBR, highL, highR, hpL, hpR, lpL, lpR;
};

} // namespace luthier
