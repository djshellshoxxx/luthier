#include "PresetFeatures.h"
#include "../../Parameters.h"
#include "../../Model/Guitar/GuitarLibrary.h"
#include "../../DSP/Effects/Pedal.h"

namespace luthier
{

namespace
{
    /*  How much gain a model makes from the same knob position. A cranked Twin
        is still clean-ish; half a Rectifier is metal. 6.1 weights amp_gain by
        this so a drive index means the same thing across amps. */
    double gainClass (AmpModel m) noexcept
    {
        switch (m)
        {
            case AmpModel::FenderTwin:     return 0.40;
            case AmpModel::FenderTweed:    return 0.70;
            case AmpModel::FenderDeluxe:   return 0.65;
            case AmpModel::FenderChamp:    return 0.70;
            case AmpModel::MarshallPlexi:  return 0.80;
            case AmpModel::MarshallJCM800: return 0.95;
            case AmpModel::VoxAC30:        return 0.70;
            case AmpModel::MesaRectifier:  return 1.15;
            case AmpModel::BognerEcstasy:  return 1.05;
            case AmpModel::DiezelVH4:      return 1.20;
            case AmpModel::OrangeOR120:    return 0.95;
            case AmpModel::AmpegSVT:       return 0.55;
            case AmpModel::AcousticDI:     return 0.05;
            case AmpModel::Custom:
            case AmpModel::NumModels:
            default:                       return 0.70;
        }
    }

    int midiForHz (double hz) noexcept
    {
        return hz > 0.0 ? (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0)) : 40;
    }
}

//==============================================================================
const char* PresetFeatures::getFamilyName (int f) noexcept
{
    switch (f)
    {
        case electric:  return "Electric";
        case acoustic:  return "Acoustic";
        case classical: return "Classical";
        case bass:      return "Bass";
        default:        return "Electric";
    }
}

const char* PresetFeatures::getTechniqueName (int t) noexcept
{
    static const char* const names[numTechniques] = { "Scrape", "Slide", "Slap", "Mute", "Tap", "Bend" };
    return juce::isPositiveAndBelow (t, (int) numTechniques) ? names[t] : "";
}

const char* PresetFeatures::getTechniqueParamId (int t) noexcept
{
    /*  Mute, Tap and Bend have no arm parameter in this build (their specs have
        not landed); the ids are the ones gui-techniques-updates 2 implies, so
        their chips light up by themselves once the parameters exist. */
    static const char* const ids[numTechniques] = {
        ParamIDs::scrapeArmed, ParamIDs::slideGuitar, ParamIDs::slapArmed,
        "mute_armed", "tap_armed", "bend_armed"
    };

    return juce::isPositiveAndBelow (t, (int) numTechniques) ? ids[t] : nullptr;
}

//==============================================================================
PresetFeatureReader::PresetFeatureReader (const juce::AudioProcessor& rangeSource)
{
    for (auto* p : rangeSource.getParameters())
        if (auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (p))
            ranges[ranged->paramID.toStdString()] = { ranged->getNormalisableRange(), ranged->getDefaultValue() };
}

double PresetFeatureReader::plain (const juce::var& json, const juce::String& paramId) const
{
    const auto it = ranges.find (paramId.toStdString());

    if (it == ranges.end())
        return 0.0;

    float normalised = it->second.defaultNormalised;

    if (auto* params = json.getProperty ("parameters", {}).getDynamicObject())
    {
        const auto v = params->getProperty (paramId);

        if (v.isDouble() || v.isInt() || v.isInt64() || v.isBool())
            normalised = juce::jlimit (0.0f, 1.0f, (float) (double) v);
    }

    return (double) it->second.range.convertFrom0to1 (normalised);
}

PresetFeatures PresetFeatureReader::read (const juce::var& json) const
{
    PresetFeatures f;

    auto value = [&] (const juce::String& id) { return plain (json, id); };
    auto choice = [&] (const juce::String& id) { return (int) std::round (value (id)); };
    auto flag = [&] (const juce::String& id) { return hasParameter (id) && value (id) >= 0.5; };

    // ---- the instrument -------------------------------------------------------
    f.guitarType = juce::jlimit (0, (int) GuitarType::NumTypes - 1, choice (ParamIDs::guitarType));
    const auto& spec = GuitarLibrary::get ((GuitarType) f.guitarType);

    f.guitarName = GuitarLibrary::getName ((GuitarType) f.guitarType);

    // A Workshop guitar carries its own name in the guitar block's reference.
    if (auto* block = json.getProperty ("guitar", {}).getDynamicObject())
    {
        const auto reference = block->getProperty ("reference").toString();

        if (reference.isNotEmpty())
            f.guitarName = juce::File::createFileWithoutCheckingPath (reference.replaceCharacter ('\\', '/'))
                               .getFileNameWithoutExtension();

        if (f.guitarName.isEmpty())
            f.guitarName = GuitarLibrary::getName ((GuitarType) f.guitarType);
    }

    f.bodyShape = (int) spec.bodyShape;

    const auto type = (GuitarType) f.guitarType;

    if (spec.category == GuitarCategory::Bass)
        f.family = PresetFeatures::bass;
    else if (type == GuitarType::Classical || type == GuitarType::Flamenco)
        f.family = PresetFeatures::classical;
    else if (spec.category == GuitarCategory::Acoustic)
        f.family = PresetFeatures::acoustic;
    else
        f.family = PresetFeatures::electric;

    f.acousticBody = f.family == PresetFeatures::acoustic || f.family == PresetFeatures::classical;
    f.hollowOrArchtop = spec.bodyShape == BodyShape::Hollow || spec.bodyShape == BodyShape::SemiHollow;

    // ---- pickups ------------------------------------------------------------------
    int humbuckers = 0, singles = 0;

    for (int i = 0; i < spec.numPickups; ++i)
    {
        const auto t = spec.pickupTypes[i];

        if (t == PickupType::Humbucker)                                   ++humbuckers;
        else if (t == PickupType::SingleCoil || t == PickupType::P90)     ++singles;
    }

    if (f.acousticBody && humbuckers == 0 && singles == 0)
        f.pickup = PresetFeatures::piezo;
    else if (spec.hasPiezo && f.acousticBody)
        f.pickup = PresetFeatures::piezo;
    else
        f.pickup = humbuckers > singles ? PresetFeatures::humbucker : PresetFeatures::singleCoil;

    // Selector index 4 is the neck pickup alone (Parameters::pickupSelectorNames).
    f.neckHumbucker = humbuckers > 0 && choice (ParamIDs::pickupSelector) == 4;

    // ---- strings --------------------------------------------------------------------
    const auto material = (StringMaterial) juce::jlimit (0, (int) StringMaterial::NumMaterials - 1,
                                                         choice (ParamIDs::stringMaterial));
    f.nylon = material == StringMaterial::Nylon || material == StringMaterial::Fluorocarbon
              || f.family == PresetFeatures::classical;
    f.flat = material == StringMaterial::Flatwound;
    f.steel = ! f.nylon;

    // ---- tuning: the lowest open string after tuning and capo -------------------------
    {
        double freqs[16] {};
        const auto tuning = (TuningPreset) juce::jlimit (0, (int) TuningPreset::NumPresets - 1,
                                                         choice (ParamIDs::tuningPreset));
        int count = TuningEngine::getPresetStringCount (tuning);
        TuningEngine::getPresetFrequencies (tuning, freqs, 16);

        if (auto* strings = json.getProperty ("strings", {}).getDynamicObject())
        {
            if ((bool) strings->getProperty ("useCustomTuning"))
                if (auto* hz = strings->getProperty ("openFrequencyHz").getArray())
                {
                    int n = 0;

                    for (int i = 0; i < juce::jmin (16, hz->size()); ++i)
                        if ((double) (*hz)[i] > 1.0)
                            freqs[n++] = (double) (*hz)[i];

                    if (n > 0)
                        count = n;
                }
        }

        double lowest = 1.0e9, highest = 0.0;

        for (int i = 0; i < count; ++i)
        {
            lowest = juce::jmin (lowest, freqs[i]);
            highest = juce::jmax (highest, freqs[i]);
        }

        const int capo = juce::jlimit (0, 12, choice (ParamIDs::capoFret));

        f.numStrings = spec.twelveString ? 12 : count;
        f.lowestOpenMidi = midiForHz (lowest) + capo;
        f.highestMidi = midiForHz (highest) + juce::jmax (12, spec.maxFrets);
    }

    // ---- the rig ------------------------------------------------------------------------
    f.ampModel = juce::jlimit (0, (int) AmpModel::NumModels - 1, choice (ParamIDs::ampModel));
    f.ampName = AmpEngine::getModelName ((AmpModel) f.ampModel);

    // Parameters.cpp's bridge: the drive macro adds up to 0.55 of amp gain.
    const double effectiveGain = juce::jlimit (0.0, 1.0, value (ParamIDs::ampGain) + 0.55 * value (ParamIDs::macroDrive));
    double drive = effectiveGain * gainClass ((AmpModel) f.ampModel);

    double reverbPedal = 0.0, delay = 0.0, compression = 0.0;

    for (int chain = 0; chain < 2; ++chain)
    {
        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const bool post = chain == 1;
            const auto pedal = (PedalType) choice (ParamIDs::slotType (post, slot));

            if (pedal == PedalType::None || flag (ParamIDs::slotBypass (post, slot)))
                continue;

            const double mix = juce::jlimit (0.0, 1.0, value (ParamIDs::slotMix (post, slot)));
            const double p0 = juce::jlimit (0.0, 1.0, value (ParamIDs::slotParam (post, slot, 0)));

            switch (pedal)
            {
                case PedalType::Overdrive:   drive += mix * (0.15 + 0.25 * p0); break;
                case PedalType::Distortion:  drive += mix * (0.30 + 0.35 * p0); break;
                case PedalType::Fuzz:        drive += mix * (0.35 + 0.40 * p0); f.fuzzPedal = true; break;
                case PedalType::Boost:       drive += mix * 0.10; break;
                case PedalType::Reverb:
                case PedalType::SpringReverb: reverbPedal = juce::jmax (reverbPedal, mix); break;
                case PedalType::Delay:       delay = juce::jmax (delay, mix * (0.4 + 0.6 * p0)); break;
                case PedalType::Compressor:  compression = juce::jmax (compression, mix * (0.3 + 0.7 * p0)); break;
                default: break;
            }
        }
    }

    f.drive = juce::jlimit (0.0, 1.0, drive);
    f.delay = delay;
    f.compression = compression;

    const bool roomOn = ! hasParameter (ParamIDs::roomOn) || value (ParamIDs::roomOn) >= 0.5;
    f.roomBlend = roomOn ? value (ParamIDs::roomBlend) : 0.0;

    // room_decay is a multiplier around 1; normalised through its own range.
    double decay01 = 0.5;

    if (hasParameter (ParamIDs::roomDecay))
    {
        const auto it = json.getProperty ("parameters", {}).getProperty (ParamIDs::roomDecay, juce::var());
        decay01 = it.isVoid() ? 0.5 : juce::jlimit (0.0, 1.0, (double) it);
    }

    f.reverb = juce::jlimit (0.0, 1.5, f.roomBlend * (0.5 + decay01) + reverbPedal);

    // ---- techniques ------------------------------------------------------------------------
    for (int t = 0; t < PresetFeatures::numTechniques; ++t)
        if (auto* id = PresetFeatures::getTechniqueParamId (t))
            f.techniques[(size_t) t] = flag (id);

    f.slideOn = f.techniques[PresetFeatures::slide];

    // The rhythm engine's own state rides in the preset's `rhythmEngine` block.
    if (auto* rhythm = json.getProperty ("rhythmEngine", {}).getDynamicObject())
        f.rhythmEngineOn = (bool) rhythm->getProperty ("enabled") || (bool) rhythm->getProperty ("on");

    return f;
}

} // namespace luthier
