#include "BassDefaults.h"

#include "../../Parameters.h"
#include "../../DSP/Effects/PedalsDrive.h"
#include "../../DSP/Noise/FretBuzz.h"

namespace luthier
{

BassFamilyDefaults BassFamilyDefaults::forFamily (bool bass) noexcept
{
    BassFamilyDefaults d;

    if (bass)
    {
        d.squeakPressure = 0.35;
        d.setupStyle = 0;            // Factory low: bass is set up lower; buzz is part of the sound
        d.pickThicknessMm = 1.14;
        d.pluckPosition = 0.12;      // 6: "a bass default nearer the bridge"
        d.compressor = true;
    }

    return d;
}

double BassFamilyDefaults::pickThicknessNormalised (double mm) noexcept
{
    // Parameters::pickThicknessMm is 0.38 * (3 / 0.38)^n.
    return juce::jlimit (0.0, 1.0, std::log (juce::jmax (0.38, mm) / 0.38) / std::log (3.0 / 0.38));
}

void BassFamilyDefaults::retarget (juce::AudioProcessorValueTreeState& state, bool fromBass, bool toBass,
                                   const std::function<bool (const juce::String&)>& keep)
{
    if (fromBass == toBass)
        return;

    const auto from = forFamily (fromBass);
    const auto to = forFamily (toBass);

    auto parameter = [&state] (const juce::String& id)
    {
        return dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
    };

    auto plain = [&parameter] (const juce::String& id)
    {
        auto* p = parameter (id);
        return p != nullptr ? (double) p->convertFrom0to1 (p->getValue()) : 0.0;
    };

    auto write = [&parameter, &keep] (const juce::String& id, double v)
    {
        if (keep != nullptr && keep (id))
            return;

        if (auto* p = parameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    };

    // Parameters travel as floats, so "still at the default" allows for that.
    auto follow = [&] (const juce::String& id, double oldDefault, double newDefault)
    {
        if (std::abs (plain (id) - oldDefault) < 1.0e-3)
            write (id, newDefault);
    };

    follow (ParamIDs::squeakPressure, from.squeakPressure, to.squeakPressure);
    follow (ParamIDs::pluckPosition, from.pluckPosition, to.pluckPosition);

    // pick_thickness is stored normalised; the table is in millimetres. The
    // guitar side's default is whatever the parameter's own default is, so a
    // guitar the user never touched follows.
    {
        auto* p = parameter (ParamIDs::pickThickness);
        const double guitarDefault = p != nullptr ? (double) p->convertFrom0to1 (p->getDefaultValue()) : 0.5;
        const double fromValue = fromBass ? pickThicknessNormalised (from.pickThicknessMm) : guitarDefault;
        const double toValue = toBass ? pickThicknessNormalised (to.pickThicknessMm) : guitarDefault;
        follow (ParamIDs::pickThickness, fromValue, toValue);
    }

    // fret-buzz 6.1: the setup style, and the geometry that goes with it when
    // that is still the style's own.
    if (juce::roundToInt (plain (ParamIDs::setupStyle)) == from.setupStyle)
    {
        const auto& oldStyle = getSetupStyle (from.setupStyle);
        const auto& newStyle = getSetupStyle (to.setupStyle);

        write (ParamIDs::setupStyle, to.setupStyle);
        follow (ParamIDs::setupActionTreble, oldStyle.actionTreble, newStyle.actionTreble);
        follow (ParamIDs::setupActionBass, oldStyle.actionBass, newStyle.actionBass);
        follow (ParamIDs::setupRelief, oldStyle.relief, newStyle.relief);
    }

    // "Compressor default: off -> on, 2:1". The first pre-amp slot: an empty
    // one gets a 2:1 compressor; going back, the family's own 2:1 compressor
    // (still at 2:1) comes out. A compressor the user put there or changed stays.
    {
        const auto typeId = ParamIDs::slotType (false, 0);
        const int type = juce::roundToInt (plain (typeId));
        CompressorPedal compressor;
        const int ratioIndex = 1;
        const double ratioNormalised = compressor.getParameterDescriptor (ratioIndex).toNormalised (to.compressorRatio);

        if (to.compressor && type == (int) PedalType::None)
        {
            write (typeId, (double) (int) PedalType::Compressor);
            write (ParamIDs::slotBypass (false, 0), 0.0);
            write (ParamIDs::slotMix (false, 0), 1.0);

            // Written after the type, so the new pedal keeps them (ParameterBridge).
            for (int i = 0; i < compressor.getNumParameters(); ++i)
            {
                const auto& d = compressor.getParameterDescriptor (i);
                write (ParamIDs::slotParam (false, 0, i),
                       i == ratioIndex ? ratioNormalised : d.toNormalised (d.defaultValue));
            }
        }
        else if (! to.compressor && from.compressor && type == (int) PedalType::Compressor)
        {
            const double ratioNow = plain (ParamIDs::slotParam (false, 0, ratioIndex));
            const double familyRatio = compressor.getParameterDescriptor (ratioIndex).toNormalised (from.compressorRatio);

            if (std::abs (ratioNow - familyRatio) < 1.0e-3)
                write (typeId, (double) (int) PedalType::None);
        }
    }
}

} // namespace luthier
