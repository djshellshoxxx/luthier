#pragma once

/*  Pedal base class and the catalogue of pedal types.

    Every effect in both chains derives from Pedal and exposes its controls through
    one generic indexed interface. That is what lets the preset format, the MIDI
    Learn system, the randomiser and the drag-and-drop rack all work on any pedal
    without knowing what it is.

    Each pedal declares its parameters as PedalParam descriptors: a name, a unit, a
    range, a default and a skew. The UI draws knobs straight from those.
*/

#include "../Common/DspCommon.h"
#include "../Common/Oversampler.h"

namespace luthier
{

//==============================================================================
enum class PedalType
{
    None,

    // Pre-amp pedals
    Compressor, NoiseGate, Wah, EnvelopeFilter, Octaver, PitchShifter,
    Overdrive, Distortion, Fuzz, Boost, VolumePedal,

    // Post-amp / loop pedals
    Chorus, Phaser, Flanger, Tremolo, RotarySpeaker,
    Delay, Reverb, SpringReverb, GraphicEQ, ParametricEQ,

    NumTypes
};

//==============================================================================
struct PedalParam
{
    const char* name = "";
    const char* unit = "";
    double minValue = 0.0;
    double maxValue = 1.0;
    double defaultValue = 0.5;
    double skew = 1.0;          ///< 1 = linear; <1 weights the low end.
    bool   isDiscrete = false;
    int    numSteps = 0;        ///< For discrete parameters.
    const char* const* choices = nullptr;

    double fromNormalised (double n) const noexcept
    {
        n = juce::jlimit (0.0, 1.0, n);

        if (isDiscrete && numSteps > 1)
            return (double) juce::jlimit (0, numSteps - 1, (int) std::round (n * (numSteps - 1)));

        const double shaped = (skew == 1.0) ? n : std::pow (n, 1.0 / juce::jmax (0.01, skew));
        return minValue + (maxValue - minValue) * shaped;
    }

    double toNormalised (double v) const noexcept
    {
        if (isDiscrete && numSteps > 1)
            return juce::jlimit (0.0, 1.0, v / (double) (numSteps - 1));

        const double range = maxValue - minValue;

        if (std::abs (range) < 1.0e-12)
            return 0.0;

        const double t = juce::jlimit (0.0, 1.0, (v - minValue) / range);
        return (skew == 1.0) ? t : std::pow (t, juce::jmax (0.01, skew));
    }
};

//==============================================================================
class Pedal
{
public:
    static constexpr int kMaxParams = 10;

    virtual ~Pedal() = default;

    //==========================================================================
    virtual PedalType getType() const noexcept = 0;
    virtual const char* getName() const noexcept = 0;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;

    /** Processes one stereo block in place. Both pointers are always valid; a
        mono pedal writes the same signal to both. */
    virtual void process (double* left, double* right, int numSamples) noexcept = 0;

    //==========================================================================
    virtual int getNumParameters() const noexcept = 0;
    virtual const PedalParam& getParameterDescriptor (int index) const noexcept = 0;

    /** Sets a parameter from a normalised 0-1 value. */
    void setParameterNormalised (int index, double normalised) noexcept
    {
        if (! juce::isPositiveAndBelow (index, getNumParameters()))
            return;

        normalisedValues[index] = juce::jlimit (0.0, 1.0, normalised);
        parameterChanged (index, getParameterDescriptor (index).fromNormalised (normalisedValues[index]));
    }

    double getParameterNormalised (int index) const noexcept
    {
        return juce::isPositiveAndBelow (index, kMaxParams) ? normalisedValues[index] : 0.0;
    }

    /** The parameter in its real units. */
    double getParameterValue (int index) const noexcept
    {
        if (! juce::isPositiveAndBelow (index, getNumParameters()))
            return 0.0;

        return getParameterDescriptor (index).fromNormalised (normalisedValues[index]);
    }

    void setParameterValue (int index, double value) noexcept
    {
        if (! juce::isPositiveAndBelow (index, getNumParameters()))
            return;

        setParameterNormalised (index, getParameterDescriptor (index).toNormalised (value));
    }

    /** Formats a parameter for display, e.g. "4.5 kHz" or "Triangle". */
    virtual juce::String getParameterText (int index) const;

    void resetParametersToDefault() noexcept
    {
        for (int i = 0; i < getNumParameters(); ++i)
        {
            const auto& d = getParameterDescriptor (i);
            setParameterNormalised (i, d.toNormalised (d.defaultValue));
        }
    }

    //==========================================================================
    void setBypassed (bool shouldBypass) noexcept
    {
        if (shouldBypass != bypassed)
        {
            bypassed = shouldBypass;
            bypassFade.trigger();
        }
    }

    bool isBypassed() const noexcept { return bypassed; }

    /** Settles the mix and bypass ramps where they are heading. The chain calls
        this beside reset(): a pedal swapped in by a preset load gets its mix set
        after prepare(), so without it the first render glides from full wet and
        does not match the next one (MidiExport round-trip null). */
    void resetBase() noexcept
    {
        mixSmooth.snapTo (mixTarget);
        bypassFade.prepare (sr, 0.010);
        lastBypassState = bypassed;
    }

    /** Mix between the pedal's output and its input, 0 to 1. */
    void setMix (double m) noexcept { mixTarget = juce::jlimit (0.0, 1.0, m); }
    double getMix() const noexcept { return mixTarget; }

    /** Runs the pedal with bypass crossfade and wet/dry mix applied.
        Call this rather than process() from the chain. */
    void processWithBypass (double* left, double* right, int numSamples) noexcept;

    /** Latency this pedal reports, in samples at the host rate. */
    virtual int getLatencySamples() const noexcept { return 0; }

    //==========================================================================
    /** Tempo for synced parameters, pushed in from the host each block. */
    void setTempoBpm (double bpm) noexcept { tempoBpm = juce::jlimit (20.0, 300.0, bpm); }
    double getTempoBpm() const noexcept { return tempoBpm; }

    /** Expression pedal position, used by the wah and volume pedals. */
    void setExpression (double value) noexcept { expression = juce::jlimit (0.0, 1.0, value); }
    double getExpression() const noexcept { return expression; }

    //==========================================================================
    static const char* getTypeName (PedalType t) noexcept;
    static bool isPreAmpPedal (PedalType t) noexcept;
    static bool isPostAmpPedal (PedalType t) noexcept;

    /** Factory. Returns nullptr for PedalType::None. */
    static std::unique_ptr<Pedal> create (PedalType t);

protected:
    /** Called whenever a parameter changes, with the value in real units. */
    virtual void parameterChanged (int index, double value) = 0;

    void prepareBase (double sampleRate, int maxBlockSize);

    double sr = 44100.0;
    int maxBlock = 512;
    double tempoBpm = 120.0;
    double expression = 0.5;

private:
    double normalisedValues[kMaxParams] = {};
    bool bypassed = false;
    double mixTarget = 1.0;

    SwitchCrossfade bypassFade;
    ExpSmoother mixSmooth;
    bool lastBypassState = false;

    std::vector<double> dryL, dryR;
};

} // namespace luthier
