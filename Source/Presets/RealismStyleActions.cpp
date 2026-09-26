#include "RealismStyleActions.h"
#include "RealismStyles.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    void write (juce::AudioProcessorValueTreeState& state, const char* id, double plain)
    {
        if (plain < 0.0)
            return;

        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) plain));
            parameter->endChangeGesture();
        }
    }

    double plain (juce::AudioProcessorValueTreeState& state, const char* id)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return (double) parameter->convertFrom0to1 (parameter->getValue());

        return 0.0;
    }

    bool same (double a, double b) { return b < 0.0 || std::abs (a - b) < 0.005 * juce::jmax (1.0, std::abs (b)); }
}

//==============================================================================
void applyNoiseFloorStyle (LuthierAudioProcessor& processor, int index)
{
    const auto& style = RealismStyles::getNoiseFloorStyle (index);
    auto& state = processor.getState();

    // action-and-undo.md 3.2: a discrete switch, one entry for every write.
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, juce::String ("Noise floor style ") + style.name);

    write (state, ParamIDs::noiseFloorStyle, (double) index);
    write (state, ParamIDs::ampBuzz, style.hum);
    write (state, ParamIDs::noiseFluorescent, style.fluorescent);
    write (state, ParamIDs::noisePassiveHiss, style.passiveHiss);
    write (state, ParamIDs::noiseCableMovement, style.cable);
    write (state, ParamIDs::noiseRadio, style.radio);
    write (state, ParamIDs::noiseGroundLoop, style.groundLoop);
    write (state, ParamIDs::noiseAmpHiss, style.ampHiss);
    write (state, ParamIDs::noiseMicrophonics, style.microphonics);
}

juce::String describeNoiseFloorStyle (LuthierAudioProcessor& processor)
{
    auto& state = processor.getState();
    const auto& style = RealismStyles::getNoiseFloorStyle (juce::roundToInt (plain (state, ParamIDs::noiseFloorStyle)));

    const bool matches = same (plain (state, ParamIDs::ampBuzz), style.hum)
                      && same (plain (state, ParamIDs::noiseFluorescent), style.fluorescent)
                      && same (plain (state, ParamIDs::noisePassiveHiss), style.passiveHiss)
                      && same (plain (state, ParamIDs::noiseCableMovement), style.cable)
                      && same (plain (state, ParamIDs::noiseRadio), style.radio)
                      && same (plain (state, ParamIDs::noiseGroundLoop), style.groundLoop)
                      && same (plain (state, ParamIDs::noiseAmpHiss), style.ampHiss)
                      && same (plain (state, ParamIDs::noiseMicrophonics), style.microphonics);

    return juce::String (style.name) + (matches ? "" : " (modified)");
}

//==============================================================================
void applySustainStyle (LuthierAudioProcessor& processor, int index)
{
    const auto& style = RealismStyles::getSustainStyle (index);
    auto& state = processor.getState();

    // sustain-and-decay.md 8: the eight writes are one undo entry. The style
    // never touches sustain_scale (6.1).
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, juce::String ("Sustain style ") + style.name);

    write (state, ParamIDs::sustainStyle, (double) index);
    write (state, ParamIDs::sustainAttackTransient, style.transient);
    write (state, ParamIDs::sustainAttackTime, style.attackMs);
    write (state, ParamIDs::sustainFastShare, style.fastShare);
    write (state, ParamIDs::sustainFastRatio, style.fastRatio);
    write (state, ParamIDs::sustainTensionMod, style.tension);
    write (state, ParamIDs::sustainReleaseTime, style.releaseMs);
    write (state, ParamIDs::sustainReleaseSag, style.sagMm);
    write (state, ParamIDs::sustainReleaseRing, style.ring);
}

juce::String describeSustainStyle (LuthierAudioProcessor& processor)
{
    auto& state = processor.getState();
    const auto& style = RealismStyles::getSustainStyle (juce::roundToInt (plain (state, ParamIDs::sustainStyle)));

    const bool matches = same (plain (state, ParamIDs::sustainAttackTransient), style.transient)
                      && same (plain (state, ParamIDs::sustainAttackTime), style.attackMs)
                      && same (plain (state, ParamIDs::sustainFastShare), style.fastShare)
                      && same (plain (state, ParamIDs::sustainFastRatio), style.fastRatio)
                      && same (plain (state, ParamIDs::sustainTensionMod), style.tension)
                      && same (plain (state, ParamIDs::sustainReleaseTime), style.releaseMs)
                      && same (plain (state, ParamIDs::sustainReleaseSag), style.sagMm)
                      && same (plain (state, ParamIDs::sustainReleaseRing), style.ring);

    return juce::String (style.name) + (matches ? "" : " (modified)");
}

} // namespace luthier
