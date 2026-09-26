#include "FamilyDefaults.h"
#include "../Parameters.h"
#include "../DSP/Amp/AmpEngine.h"

namespace luthier::FamilyDefaults
{

namespace
{
    bool isGuitarAmp (int model) noexcept
    {
        return model != (int) AmpModel::AmpegSVT && model != (int) AmpModel::AcousticDI;
    }

    float plainOf (juce::AudioProcessorValueTreeState& state, const char* id)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return p->convertFrom0to1 (p->getValue());
        return 0.0f;
    }

    void setPlain (juce::AudioProcessorValueTreeState& state, const char* id, float plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
}

int ampModelFor (const juce::String& family, int current) noexcept
{
    const auto f = family.toLowerCase();

    if (f == "bass")
        return current == (int) AmpModel::AmpegSVT ? -1 : (int) AmpModel::AmpegSVT;

    // An acoustic instrument is heard through a clean preamp, not a guitar amp.
    if (f == "acoustic" || f == "classical" || f == "resonator")
        return current == (int) AmpModel::AcousticDI ? -1 : (int) AmpModel::AcousticDI;

    // Electric (and extended-range): any guitar amp stays; a bass amp or the DI
    // goes back to the clean blackface every electric template starts on.
    return isGuitarAmp (current) ? -1 : (int) AmpModel::FenderTwin;
}

juce::String applyAmpDefaults (juce::AudioProcessorValueTreeState& state, const juce::String& family)
{
    juce::StringArray changed;
    const int current = juce::roundToInt (plainOf (state, ParamIDs::ampModel));
    const int model = ampModelFor (family, current);

    if (model >= 0)
    {
        setPlain (state, ParamIDs::ampModel, (float) model);
        changed.add ("amp " + juce::String (AmpEngine::getModelName ((AmpModel) model)));
    }

    // 12.3's "clean DI + reverb": a dry acoustic gets a little room.
    const auto f = family.toLowerCase();

    if (f == "acoustic" || f == "classical")
    {
        if (plainOf (state, ParamIDs::roomOn) < 0.5f)
        {
            setPlain (state, ParamIDs::roomOn, 1.0f);
            changed.add ("room on");
        }

        if (plainOf (state, ParamIDs::roomBlend) < 0.15f)
        {
            setPlain (state, ParamIDs::roomBlend, 0.2f);
            changed.add ("room 20%");
        }
    }

    return changed.isEmpty() ? juce::String() : "Also set: " + changed.joinIntoString (", ") + ".";
}

} // namespace luthier::FamilyDefaults
