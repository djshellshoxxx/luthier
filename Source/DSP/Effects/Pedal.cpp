#include "Pedal.h"
#include "PedalsDrive.h"
#include "PedalsMod.h"

namespace luthier
{

//==============================================================================
void Pedal::prepareBase (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    bypassFade.prepare (sr, 0.010);

    mixSmooth.prepare (sr, constants::kParamSmoothSeconds);
    mixSmooth.snapTo (mixTarget);

    dryL.assign ((size_t) maxBlock, 0.0);
    dryR.assign ((size_t) maxBlock, 0.0);

    mixSmooth.snapToTarget();

    lastBypassState = bypassed;
}

//==============================================================================
void Pedal::processWithBypass (double* left, double* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    // The dry copy used for the bypass crossfade and the wet/dry mix is sized from
    // maxBlockSize. If a host sends more than it promised, split rather than
    // silently skipping the blend - which would leave a "bypassed" pedal audible.
    if (numSamples > maxBlock)
    {
        for (int offset = 0; offset < numSamples;)
        {
            const int count = juce::jmin (maxBlock, numSamples - offset);
            processWithBypass (left + offset, right + offset, count);
            offset += count;
        }

        return;
    }

    const bool needsDry = (mixTarget < 0.999) || bypassFade.isActive() || (bypassed != lastBypassState);

    if (needsDry && (int) dryL.size() >= numSamples)
    {
        std::copy (left, left + numSamples, dryL.begin());
        std::copy (right, right + numSamples, dryR.begin());
    }

    if (bypassed && ! bypassFade.isActive())
    {
        // Fully bypassed: the pedal still gets reset-free state, but costs nothing.
        lastBypassState = bypassed;
        return;
    }

    process (left, right, numSamples);

    if (! needsDry || (int) dryL.size() < numSamples)
    {
        lastBypassState = bypassed;
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        mixSmooth.setTarget (mixTarget);
        const double mix = mixSmooth.next();

        // Crossfade the bypass switch over 10 ms so a footswitch never clicks.
        double blend = mix;

        if (bypassFade.isActive())
        {
            const double fade = bypassFade.next();
            blend = bypassed ? mix * (1.0 - fade) : mix * fade;
        }
        else if (bypassed)
        {
            blend = 0.0;
        }

        left[i]  = sanitise (dryL[(size_t) i] * (1.0 - blend) + left[i]  * blend);
        right[i] = sanitise (dryR[(size_t) i] * (1.0 - blend) + right[i] * blend);
    }

    lastBypassState = bypassed;
}

//==============================================================================
juce::String Pedal::getParameterText (int index) const
{
    if (! juce::isPositiveAndBelow (index, getNumParameters()))
        return {};

    const auto& d = getParameterDescriptor (index);
    const double v = getParameterValue (index);

    if (d.isDiscrete)
    {
        const int step = juce::jlimit (0, juce::jmax (0, d.numSteps - 1), (int) std::round (v));

        if (d.choices != nullptr)
            return d.choices[step];

        return juce::String (step + 1);
    }

    const juce::String unit (d.unit);

    if (unit == "Hz")
    {
        if (v >= 1000.0)
            return juce::String (v / 1000.0, 2) + " kHz";

        return juce::String (v, v < 100.0 ? 1 : 0) + " Hz";
    }

    if (unit == "ms")
        return juce::String (v, v < 10.0 ? 2 : 1) + " ms";

    if (unit == "dB")
        return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";

    if (unit == "s")
        return juce::String (v, 2) + " s";

    if (unit == ":1")
        return juce::String (v, 1) + ":1";

    if (unit == "st")
        return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " st";

    if (unit == "cent")
        return (v > 0.0 ? "+" : "") + juce::String (v, 0) + " c";

    if (unit == "oct")
        return juce::String (v, 1) + " oct";

    if (unit == "deg")
        return juce::String (v, 0) + juce::String::fromUTF8 ("\xc2\xb0");

    // Unitless controls read as a percentage, which is what a player expects from
    // a knob with no markings.
    if (d.minValue >= 0.0 && d.maxValue <= 2.0)
        return juce::String (juce::roundToInt (v * 100.0)) + "%";

    return juce::String (v, 2);
}

//==============================================================================
const char* Pedal::getTypeName (PedalType t) noexcept
{
    switch (t)
    {
        case PedalType::None:           return "Empty";
        case PedalType::Compressor:     return "Compressor";
        case PedalType::NoiseGate:      return "Noise Gate";
        case PedalType::Wah:            return "Wah";
        case PedalType::EnvelopeFilter: return "Envelope Filter";
        case PedalType::Octaver:        return "Octaver";
        case PedalType::PitchShifter:   return "Pitch Shifter";
        case PedalType::Overdrive:      return "Overdrive";
        case PedalType::Distortion:     return "Distortion";
        case PedalType::Fuzz:           return "Fuzz";
        case PedalType::Boost:          return "Boost";
        case PedalType::VolumePedal:    return "Volume Pedal";
        case PedalType::Chorus:         return "Chorus";
        case PedalType::Phaser:         return "Phaser";
        case PedalType::Flanger:        return "Flanger";
        case PedalType::Tremolo:        return "Tremolo";
        case PedalType::RotarySpeaker:  return "Rotary";
        case PedalType::Delay:          return "Delay";
        case PedalType::Reverb:         return "Reverb";
        case PedalType::SpringReverb:   return "Spring Reverb";
        case PedalType::GraphicEQ:      return "Graphic EQ";
        case PedalType::ParametricEQ:   return "Parametric EQ";
        case PedalType::NumTypes:
        default:                        return "Empty";
    }
}

bool Pedal::isPreAmpPedal (PedalType t) noexcept
{
    switch (t)
    {
        case PedalType::Compressor:
        case PedalType::NoiseGate:
        case PedalType::Wah:
        case PedalType::EnvelopeFilter:
        case PedalType::Octaver:
        case PedalType::PitchShifter:
        case PedalType::Overdrive:
        case PedalType::Distortion:
        case PedalType::Fuzz:
        case PedalType::Boost:
        case PedalType::VolumePedal:
        case PedalType::Chorus:
        case PedalType::GraphicEQ:
        case PedalType::ParametricEQ:
            return true;

        default:
            return false;
    }
}

bool Pedal::isPostAmpPedal (PedalType t) noexcept
{
    switch (t)
    {
        case PedalType::Chorus:
        case PedalType::Phaser:
        case PedalType::Flanger:
        case PedalType::Tremolo:
        case PedalType::RotarySpeaker:
        case PedalType::Delay:
        case PedalType::Reverb:
        case PedalType::SpringReverb:
        case PedalType::GraphicEQ:
        case PedalType::ParametricEQ:
        case PedalType::Compressor:
        case PedalType::NoiseGate:
            return true;

        default:
            return false;
    }
}

//==============================================================================
std::unique_ptr<Pedal> Pedal::create (PedalType t)
{
    switch (t)
    {
        case PedalType::Compressor:     return std::make_unique<CompressorPedal>();
        case PedalType::NoiseGate:      return std::make_unique<NoiseGatePedal>();
        case PedalType::Wah:            return std::make_unique<WahPedal>();
        case PedalType::EnvelopeFilter: return std::make_unique<EnvelopeFilterPedal>();
        case PedalType::Octaver:        return std::make_unique<OctaverPedal>();
        case PedalType::PitchShifter:   return std::make_unique<PitchShifterPedal>();
        case PedalType::Overdrive:      return std::make_unique<OverdrivePedal>();
        case PedalType::Distortion:     return std::make_unique<DistortionPedal>();
        case PedalType::Fuzz:           return std::make_unique<FuzzPedal>();
        case PedalType::Boost:          return std::make_unique<BoostPedal>();
        case PedalType::VolumePedal:    return std::make_unique<VolumePedal>();
        case PedalType::Chorus:         return std::make_unique<ChorusPedal>();
        case PedalType::Phaser:         return std::make_unique<PhaserPedal>();
        case PedalType::Flanger:        return std::make_unique<FlangerPedal>();
        case PedalType::Tremolo:        return std::make_unique<TremoloPedal>();
        case PedalType::RotarySpeaker:  return std::make_unique<RotaryPedal>();
        case PedalType::Delay:          return std::make_unique<DelayPedal>();
        case PedalType::Reverb:         return std::make_unique<ReverbPedal>();
        case PedalType::SpringReverb:   return std::make_unique<SpringReverbPedal>();
        case PedalType::GraphicEQ:      return std::make_unique<GraphicEqPedal>();
        case PedalType::ParametricEQ:   return std::make_unique<ParametricEqPedal>();

        case PedalType::None:
        case PedalType::NumTypes:
        default:
            return nullptr;
    }
}

} // namespace luthier
