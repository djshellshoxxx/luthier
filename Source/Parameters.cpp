#include "Parameters.h"
#include "PhysicalRange.h"
#include "Rhythm/StrumGesture.h"

namespace luthier
{

using APVTS = juce::AudioProcessorValueTreeState;

namespace ParamIDs
{
const char* macroByIndex (int index) noexcept
{
    switch (index)
    {
        case 0:  return macroAttack;
        case 1:  return macroBody;
        case 2:  return macroDrive;
        case 3:  return macroTone;
        case 4:  return macroSpace;
        case 5:  return macroHumanize;
        case 6:  return macroAssignA;
        case 7:  return macroAssignB;
        default: return macroAttack;
    }
}

    juce::String pickupType (int slot)     { return "pickup" + juce::String (slot) + "_type"; }
    juce::String pickupPosition (int slot) { return "pickup" + juce::String (slot) + "_position"; }
    juce::String pickupHeight (int slot)   { return "pickup" + juce::String (slot) + "_height"; }
    juce::String pickupMagnet (int slot)   { return "pickup" + juce::String (slot) + "_magnet"; }
    juce::String pickupVolume (int slot)   { return "pickup" + juce::String (slot) + "_volume"; }
    juce::String setupNutDepth (int n)     { return "setup_nut_depth_" + juce::String (n); }

    static juce::String chainPrefix (bool post, int slot)
    {
        return (post ? "post" : "pre") + juce::String (slot);
    }

    juce::String slotType (bool post, int slot)   { return chainPrefix (post, slot) + "_type"; }
    juce::String slotBypass (bool post, int slot) { return chainPrefix (post, slot) + "_bypass"; }
    juce::String slotMix (bool post, int slot)    { return chainPrefix (post, slot) + "_mix"; }

    juce::String slotParam (bool post, int slot, int param)
    {
        return chainPrefix (post, slot) + "_p" + juce::String (param);
    }
}

//==============================================================================
namespace
{
    template <typename EnumType, typename NameFn>
    juce::StringArray namesFor (int count, NameFn fn)
    {
        juce::StringArray names;

        for (int i = 0; i < count; ++i)
            names.add (fn ((EnumType) i));

        return names;
    }

    juce::ParameterID pid (const juce::String& s) { return { s, 1 }; }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id,
                                                           const juce::String& name,
                                                           float min, float max, float def,
                                                           float skew = 1.0f,
                                                           const juce::String& unit = {})
    {
        juce::NormalisableRange<float> range (min, max);

        if (skew != 1.0f)
            range.setSkewForCentre (min + (max - min) * skew);

        /*  advanced-ranges.md 0.1 and 1.0: a physical parameter is constructed
            on its stock range, and its stock range is the one declared here.

            The declaration stays the source of truth and is recorded rather
            than overwritten. Taking the range from the registry instead would
            make `Ranges::stockMatchesTheDeclaredRange` compare the registry
            with itself - which it did, and which let a deliberately wrong
            stock pair pass. Presets store normalised values, so that
            disagreement would silently re-map every preset ever saved. */
        RangeRegistry::noteDeclaration (id, min, max);

        return std::make_unique<juce::AudioParameterFloat> (
            pid (id), name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (unit));
    }

    /*  A resistance, shown and typed the way it is printed on a part. Declared
        through floatParam's range rules, so the stock-range record is kept. */
    std::unique_ptr<juce::AudioParameterFloat> ohmParam (const juce::String& id,
                                                         const juce::String& name,
                                                         float min, float max, float def,
                                                         float skew)
    {
        juce::NormalisableRange<float> range (min, max);
        range.setSkewForCentre (min + (max - min) * skew);

        RangeRegistry::noteDeclaration (id, min, max);

        return std::make_unique<juce::AudioParameterFloat> (
            pid (id), name, range, def,
            juce::AudioParameterFloatAttributes()
                .withLabel ("ohm")
                .withStringFromValueFunction ([] (float v, int) { return Parameters::formatOhms (v); })
                .withValueFromStringFunction ([] (const juce::String& t) { return (float) Parameters::parseOhms (t); }));
    }

    std::unique_ptr<juce::AudioParameterChoice> choiceParam (const juce::String& id,
                                                             const juce::String& name,
                                                             const juce::StringArray& choices,
                                                             int def = 0)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            pid (id), name, choices, juce::jlimit (0, juce::jmax (0, choices.size() - 1), def));
    }

    std::unique_ptr<juce::AudioParameterBool> boolParam (const juce::String& id,
                                                         const juce::String& name,
                                                         bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (pid (id), name, def);
    }
}

//==============================================================================
juce::StringArray Parameters::guitarTypeNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) GuitarType::NumTypes; ++i)
        names.add (GuitarLibrary::getName ((GuitarType) i));

    return names;
}

juce::StringArray Parameters::tuningNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) TuningPreset::NumPresets; ++i)
        names.add (TuningEngine::getTuningPresetName ((TuningPreset) i));

    return names;
}

juce::StringArray Parameters::capoNames()
{
    /*  Twelve frets, which is the range RhythmEngine's own capo already used and
        the point past which a capo stops being a capo. "Off" rather than "Fret 0"
        because a capo at the nut is not a capo, and a host showing "Fret 0" would
        read as a setting rather than as its absence. */
    juce::StringArray names { "Off" };

    for (int fret = 1; fret <= 12; ++fret)
        names.add ("Fret " + juce::String (fret));

    return names;
}

juce::StringArray Parameters::temperamentNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) Temperament::NumTemperaments; ++i)
        names.add (TuningEngine::getTemperamentName ((Temperament) i));

    return names;
}

juce::StringArray Parameters::stringMaterialNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) StringMaterial::NumMaterials; ++i)
        names.add (StringMaterials::getMaterialName ((StringMaterial) i));

    return names;
}

juce::StringArray Parameters::stringGaugeNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) StringGauge::NumGauges; ++i)
        names.add (StringMaterials::getGaugeName ((StringGauge) i));

    return names;
}

juce::StringArray Parameters::stringAgeNames()
{
    return { "Fresh", "Broken In", "Old" };
}

juce::StringArray Parameters::pickMaterialNames()
{
    return { "Nylon Pick", "Celluloid Pick", "Delrin Pick", "Metal Pick", "Wood Pick",
             "Felt Pick", "Fingernail", "Fingertip", "Thumb", "Thumbpick", "Brush", "Slide" };
}

juce::StringArray Parameters::bodyModeNames()
{
    return { "Convolution", "Modal", "Hybrid", "No Body (Experimental)" };
}

juce::StringArray Parameters::bracingNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) Bracing::NumBracings; ++i)
        names.add (BodyModels::getBracingName ((Bracing) i));

    return names;
}

juce::StringArray Parameters::woodNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) Wood::NumWoods; ++i)
        names.add (BodyModels::getWoodName ((Wood) i));

    return names;
}

juce::StringArray Parameters::pickupTypeNames()
{
    return { "Single Coil", "Humbucker", "P90", "Piezo", "Magnetic Soundhole", "Internal Mic" };
}

juce::StringArray Parameters::magnetNames()
{
    return { "Alnico 2", "Alnico 3", "Alnico 5", "Ceramic" };
}

juce::StringArray Parameters::pickupSelectorNames()
{
    return { "Bridge", "Bridge + Middle", "Middle", "Middle + Neck", "Neck", "All", "Bridge + Neck" };
}

juce::StringArray Parameters::playingModeNames()
{
    return { "Mono / Lead", "Poly / Chord", "Guitar Controller" };
}

juce::StringArray Parameters::strumDirectionNames()
{
    return { "Down", "Up", "Alternate" };
}

juce::StringArray Parameters::vibratoShapeNames()
{
    return { "Sine", "Triangle", "Square", "Saw", "Random", "Finger" };
}

juce::StringArray Parameters::bridgeTypeNames()
{
    return { "Fixed / Hardtail", "Vintage Tremolo", "Locking Tremolo", "Transposing Tremolo", "Vintage Vibrato" };
}

juce::StringArray Parameters::ampModelNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
        names.add (AmpEngine::getModelName ((AmpModel) i));

    return names;
}

juce::StringArray Parameters::cabinetNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) CabinetType::NumCabinets; ++i)
        names.add (CabinetEngine::getCabinetName ((CabinetType) i));

    return names;
}

juce::StringArray Parameters::speakerNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) SpeakerType::NumSpeakers; ++i)
        names.add (CabinetEngine::getSpeakerName ((SpeakerType) i));

    return names;
}

juce::StringArray Parameters::micNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) MicType::NumMics; ++i)
        names.add (CabinetEngine::getMicName ((MicType) i));

    return names;
}

juce::StringArray Parameters::micPositionNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) MicPosition::NumPositions; ++i)
        names.add (CabinetEngine::getPositionName ((MicPosition) i));

    return names;
}

juce::StringArray Parameters::micDistanceNames()
{
    return { "Close", "Medium", "Far" };
}

juce::StringArray Parameters::roomSizeNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) RoomSize::NumRoomSizes; ++i)
        names.add (RoomEngine::getRoomSizeName ((RoomSize) i));

    return names;
}

juce::StringArray Parameters::roomMaterialNames()
{
    return { "Dry", "Wood", "Tile", "Stone" };
}

juce::StringArray Parameters::pedalTypeNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) PedalType::NumTypes; ++i)
        names.add (Pedal::getTypeName ((PedalType) i));

    return names;
}

juce::StringArray Parameters::oversamplingNames()
{
    return { "1x (Off)", "2x", "4x", "8x" };
}

// Orders match PotTaper, TrebleBleed and CableQuality in GuitarCircuit.h.
juce::StringArray Parameters::potTaperNames()     { return { "Audio", "Linear", "50s Wiring" }; }
juce::StringArray Parameters::trebleBleedNames()  { return { "None", "Modern RC", "Vintage Cap", "Custom" }; }
juce::StringArray Parameters::bleedModeNames()    { return { "Parallel", "Series" }; }
juce::StringArray Parameters::cableQualityNames() { return { "Studio", "Standard", "Cheap", "Vintage" }; }

// string-squeak.md 8, in its order.
juce::StringArray Parameters::squeakStyleNames()
{
    return { "Silent", "Studio (polished)", "Natural", "Folk / close-mic", "Exaggerated" };
}

juce::StringArray Parameters::setupStyleNames()
{
    juce::StringArray names;

    for (int i = 0; i < kNumSetupStyles; ++i)
        names.add (getSetupStyle (i).name);

    return names;
}

juce::StringArray Parameters::slideModeNames()
{
    // Matches SlideMode.
    return { "Bottleneck", "Lap steel", "Resonator", "Hybrid" };
}

double Parameters::defaultDampingBehind (int slideModeIndex) noexcept
{
    return (slideModeIndex == (int) SlideMode::lapSteel || slideModeIndex == (int) SlideMode::dobro) ? 1.0 : 0.55;
}

double Parameters::pickThicknessMm (double normalised) noexcept
{
    // Logarithmic across pick-noise.md 2's 0.38-3.0 mm: picks are sold in
    // steps that are roughly even ratios, not even millimetres.
    return 0.38 * std::pow (3.0 / 0.38, normalised);
}

double Parameters::pickAngleDegrees (double normalised) noexcept
{
    return 60.0 * normalised;
}

juce::String Parameters::formatOhms (double ohms)
{
    auto trimmed = [] (double v)
    {
        // Three significant figures, and no trailing zeros: "470k", "1.5M".
        auto text = juce::String (v, v < 10.0 ? 2 : (v < 100.0 ? 1 : 0));

        if (text.containsChar ('.'))
            text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return text;
    };

    if (ohms >= 1.0e6) return trimmed (ohms / 1.0e6) + "M";
    if (ohms >= 1.0e3) return trimmed (ohms / 1.0e3) + "k";
    return trimmed (ohms);
}

double Parameters::parseOhms (const juce::String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");

    for (const char* suffix : { "ohms", "ohm", "\xce\xa9" })
        if (t.endsWith (juce::CharPointer_UTF8 (suffix)))
            t = t.dropLastCharacters ((int) juce::String (juce::CharPointer_UTF8 (suffix)).length());

    // Scale letters as a guitarist writes them: 500k, 1M, 1meg.
    double scale = 1.0;

    if (t.contains ("meg") || t.endsWithChar ('m'))
        scale = 1.0e6;
    else if (t.endsWithChar ('k'))
        scale = 1.0e3;

    return t.getDoubleValue() * scale;
}

//==============================================================================
APVTS::ParameterLayout Parameters::createLayout()
{
    APVTS::ParameterLayout layout;

    auto add = [&layout] (auto p) { layout.add (std::move (p)); };

    // --- macros ---------------------------------------------------------------
    add (floatParam (ParamIDs::macroAttack,   "Attack",   0.0f, 1.0f, 0.5f));
    add (floatParam (ParamIDs::macroBody,     "Body",     0.0f, 1.0f, 0.35f));
    add (floatParam (ParamIDs::macroDrive,    "Drive",    0.0f, 1.0f, 0.3f));
    add (floatParam (ParamIDs::macroTone,     "Tone",     0.0f, 1.0f, 0.5f));
    add (floatParam (ParamIDs::macroSpace,    "Space",    0.0f, 1.0f, 0.25f));
    add (floatParam (ParamIDs::macroHumanize, "Humanize", 0.0f, 1.0f, 0.4f));

    // The two spare macros drive nothing on their own. They exist so the
    // modulation matrix has eight macro sources, as modulation-matrix 1.7 asks,
    // and so a user can assemble a macro of their own out of routes.
    add (floatParam (ParamIDs::macroAssignA,  "Macro 7",  0.0f, 1.0f, 0.0f));
    add (floatParam (ParamIDs::macroAssignB,  "Macro 8",  0.0f, 1.0f, 0.0f));

    // --- instrument -----------------------------------------------------------
    add (choiceParam (ParamIDs::guitarType,     "Guitar",          guitarTypeNames(), 0));
    add (choiceParam (ParamIDs::tuningPreset,   "Tuning",          tuningNames(), 0));
    add (choiceParam (ParamIDs::temperament,    "Temperament",     temperamentNames(), 0));
    add (floatParam  (ParamIDs::concertA,       "Concert A",       415.0f, 466.0f, 440.0f, 0.5f, "Hz"));
    add (choiceParam (ParamIDs::capoFret,       "Capo",            capoNames(), 0));
    add (choiceParam (ParamIDs::stringMaterial, "String Material", stringMaterialNames(), 0));
    add (choiceParam (ParamIDs::stringGauge,    "String Gauge",    stringGaugeNames(), 2));
    add (choiceParam (ParamIDs::stringAge,      "String Age",      stringAgeNames(), 1));
    add (floatParam  (ParamIDs::realismDetune,  "Realism Detune",  0.0f, 20.0f, 3.0f, 0.4f, "cents"));
    add (floatParam  (ParamIDs::intonationErr,  "Intonation",      0.0f, 1.5f, 0.3f, 0.5f, "c/fret"));
    add (boolParam   (ParamIDs::tuningDrift,    "Tuning Drift",    false));
    add (boolParam   (ParamIDs::fretless,       "Fretless",        false));
    add (floatParam  (ParamIDs::fretAction,     "Fret Action",     0.5f, 4.0f, 1.6f, 0.5f, "mm"));
    add (floatParam  (ParamIDs::fretBuzz,       "Fret Buzz",       0.0f, 1.0f, 0.06f));
    add (floatParam  (ParamIDs::sustainScale,   "Sustain",         0.25f, 3.0f, 1.0f, 0.4f));
    add (floatParam  (ParamIDs::couplingAmount, "Sympathetic",     0.05f, 1.0f, 0.85f));

    // --- right hand -----------------------------------------------------------
    add (boolParam   (ParamIDs::useFingers,    "Fingers",        false));
    add (choiceParam (ParamIDs::pickMaterial,  "Pick Material",  pickMaterialNames(), 1));
    add (floatParam  (ParamIDs::pickThickness, "Pick Thickness", 0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::pickAngle,     "Pick Angle",     0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::pluckPosition, "Pick Position",  0.02f, 0.5f, 0.16f));
    add (floatParam  (ParamIDs::nailVsFlesh,   "Nail / Flesh",   0.0f, 1.0f, 0.5f));

    // --- string noise ---------------------------------------------------------
    add (floatParam (ParamIDs::slideNoise,   "Slide Noise",   0.0f, 1.0f, 0.35f));
    add (floatParam (ParamIDs::fretNoise,    "Fret Noise",    0.0f, 1.0f, 0.30f));
    add (floatParam (ParamIDs::releaseNoise, "Release Noise", 0.0f, 1.0f, 0.25f));
    add (floatParam (ParamIDs::bodyKnock,    "Body Knock",    0.0f, 1.0f, 0.0f));
    add (floatParam (ParamIDs::pickNoise,    "Pick Attack",   0.0f, 1.0f, 0.30f));

    // --- pick noise (pick-noise.md 7) -------------------------------------------------
    // pick_material, pick_thickness and pick_angle already exist and are
    // re-pointed rather than duplicated.
    add (floatParam (ParamIDs::pickTipRadius,    "Pick Tip Radius",    0.2f, 4.0f, 1.0f, 0.3f, "mm"));
    add (floatParam (ParamIDs::pickBevel,        "Pick Bevel",         0.0f, 1.0f, 0.2f));
    add (floatParam (ParamIDs::pickWear,         "Pick Wear",          0.0f, 1.0f, 0.1f));
    add (floatParam (ParamIDs::pickClickAmount,  "Pick Click",         0.0f, 1.0f, 0.5f));
    add (floatParam (ParamIDs::pickChirpAmount,  "Pick Chirp",         0.0f, 1.0f, 0.4f));
    add (floatParam (ParamIDs::pickScrapeAmount, "Pick Scrape",        0.0f, 1.0f, 0.25f));

    // --- finger squeak (string-squeak.md 9) -------------------------------------------
    // onboarding.md 1: the ship default amount is 0.25, not Natural's 0.35.
    add (floatParam  (ParamIDs::squeakAmount,      "Squeak",              0.0f, 1.0f, 0.25f));
    add (floatParam  (ParamIDs::squeakProbability, "Squeak Probability",  0.0f, 1.0f, 0.65f));
    add (floatParam  (ParamIDs::squeakMoisture,    "Finger Moisture",     0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::squeakPressure,    "Finger Pressure",     0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::squeakMinTravel,   "Squeak Min Travel",   1.0f, 4.0f, 1.5f, 1.0f, "frets"));
    add (choiceParam (ParamIDs::squeakStyle,       "Squeak Style",        squeakStyleNames(), 2));

    // --- setup and fret buzz (fret-buzz.md 7) -----------------------------------------
    // Defaults are the Player-friendly style, onboarding.md 1's ship default.
    add (floatParam  (ParamIDs::setupActionTreble,  "Action Treble",   1.0f,  3.0f, 1.6f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::setupActionBass,    "Action Bass",     1.2f,  3.5f, 2.0f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::setupRelief,        "Neck Relief",    -0.05f, 0.5f, 0.2f, 1.0f, "mm"));

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
        add (floatParam (ParamIDs::setupNutDepth (n), "Nut Depth " + juce::String (n), 0.0f, 1.2f, 0.45f, 1.0f, "mm"));

    add (floatParam  (ParamIDs::setupFretHeight,    "Fret Height",     0.6f,  1.6f, 1.0f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::setupBuzzThreshold, "Buzz Threshold",  0.0f,  1.0f, 0.35f));
    add (boolParam   (ParamIDs::setupSitarMode,     "Sitar Mode",      false));
    add (choiceParam (ParamIDs::setupStyle,         "Setup Style",     setupStyleNames(), kDefaultSetupStyle));

    // --- slide (slide-guitar.md 7) -----------------------------------------------------------
    add (choiceParam (ParamIDs::slideMode,             "Slide Mode",          slideModeNames(), (int) SlideMode::hybrid));
    add (floatParam  (ParamIDs::slidePressure,         "Slide Pressure",      0.0f,   1.0f,  0.55f));
    add (floatParam  (ParamIDs::slideSlant,            "Slide Slant",        -30.0f, 30.0f,  0.0f, 0.5f, "deg"));
    add (floatParam  (ParamIDs::slideDampingBehind,    "Slide Damping Behind", 0.0f,  1.0f,  0.55f));
    add (floatParam  (ParamIDs::slideNoiseAmount,      "Slide Noise",         0.0f,   1.0f,  0.4f));
    add (floatParam  (ParamIDs::slideClankAmount,      "Slide Clank",         0.0f,   1.0f,  0.45f));
    add (floatParam  (ParamIDs::slideIntonationAssist, "Intonation Assist",   0.0f,   1.0f,  0.15f));
    add (floatParam (ParamIDs::ampBuzz,      "Amp Buzz",      0.0f, 1.0f, 0.12f));

    // --- body -----------------------------------------------------------------
    add (choiceParam (ParamIDs::bodyMode,      "Body Mode",     bodyModeNames(), 0));
    add (floatParam  (ParamIDs::bodyAmount,    "Body Amount",   0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::bodyWidth,     "Body Size",     0.6f, 1.4f, 1.0f));
    add (floatParam  (ParamIDs::bodyDepth,     "Body Depth",    0.6f, 1.6f, 1.0f));
    add (floatParam  (ParamIDs::bodyTopThick,  "Top Thickness", 1.4f, 4.0f, 2.8f, 0.5f, "mm"));
    add (floatParam  (ParamIDs::bodySoundhole, "Sound Hole",    0.5f, 1.5f, 1.0f));
    add (choiceParam (ParamIDs::bodyBracing,   "Bracing",       bracingNames(), 0));
    add (choiceParam (ParamIDs::bodyTopWood,   "Top Wood",      woodNames(), 0));
    add (choiceParam (ParamIDs::bodyBackWood,  "Back Wood",     woodNames(), 6));
    add (floatParam  (ParamIDs::bodyAge,       "Body Age",      0.0f, 1.0f, 0.3f));
    add (floatParam  (ParamIDs::bodyAirGain,   "Air Resonance", -12.0f, 12.0f, 0.0f, 0.5f, "dB"));

    // --- pickups --------------------------------------------------------------
    add (choiceParam (ParamIDs::pickupSelector, "Pickup Selector", pickupSelectorNames(), 0));
    add (floatParam  (ParamIDs::pickupBlend,    "Pickup Blend",    0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::guitarTone,     "Guitar Tone",     0.0f, 1.0f, 1.0f));
    add (floatParam  (ParamIDs::guitarVolume,   "Guitar Volume",   0.0f, 1.0f, 1.0f));
    add (boolParam   (ParamIDs::coilTap,        "Coil Tap",        false));
    add (floatParam  (ParamIDs::piezoMicBlend,  "Piezo / Mic",     0.0f, 1.0f, 0.35f));

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        const juce::String n (slot + 1);
        add (choiceParam (ParamIDs::pickupType (slot),     "Pickup " + n + " Type",     pickupTypeNames(), 0));
        // Position and height are the guitar's placement now (guitar-workshop.md 9).
        add (choiceParam (ParamIDs::pickupMagnet (slot),   "Pickup " + n + " Magnet",   magnetNames(), 2));
        add (floatParam  (ParamIDs::pickupVolume (slot),   "Pickup " + n + " Volume",   0.0f, 1.5f, 1.0f));
    }

    // --- performance ----------------------------------------------------------
    add (choiceParam (ParamIDs::playingMode,  "Playing Mode",  playingModeNames(), 1));
    add (boolParam   (ParamIDs::mpeEnabled,   "MPE",           false));
    add (floatParam  (ParamIDs::bendRange,    "Bend Range",    1.0f, 48.0f, 2.0f, 0.3f, "st"));
    add (floatParam  (ParamIDs::strumSpeed,   "Strum Speed",   0.0f, 30.0f, 9.0f, 0.5f, "ms"));
    add (choiceParam (ParamIDs::strumDir,     "Strum",         strumDirectionNames(), 0));
    add (floatParam  (ParamIDs::chordWindow,  "Chord Window",  0.0f, 20.0f, 2.0f, 0.4f, "ms"));
    add (floatParam  (ParamIDs::vibratoRate,  "Vibrato Rate",  2.0f, 10.0f, 5.2f, 0.5f, "Hz"));
    add (floatParam  (ParamIDs::vibratoDepth, "Vibrato Depth", 0.0f, 80.0f, 22.0f, 0.5f, "cents"));
    add (choiceParam (ParamIDs::vibratoShape, "Vibrato Shape", vibratoShapeNames(), 5));
    add (floatParam  (ParamIDs::legatoWindow, "Legato Window", 5.0f, 200.0f, 40.0f, 0.4f, "ms"));
    add (boolParam   (ParamIDs::slideGuitar,  "Slide Guitar",  false));
    // --- sustain: freeze and e-bow (ambiguity-resolutions 2) ------------------
    add (boolParam   (ParamIDs::ebowEnable,      "E-Bow",             false));

    add (boolParam   (ParamIDs::freezeEnable,    "Freeze",            false));
    add (floatParam  (ParamIDs::freezeCaptureMs, "Freeze Capture",   200.0f, 1000.0f, 400.0f, 1.0f, "ms"));
    add (floatParam  (ParamIDs::freezeLevel,     "Freeze Level",     -60.0f,    0.0f,  -6.0f, 1.0f, "dB"));
    add (floatParam  (ParamIDs::freezeAttackMs,  "Freeze Attack",      5.0f,  500.0f,  40.0f, 0.4f, "ms"));
    add (floatParam  (ParamIDs::freezeReleaseMs, "Freeze Release",    20.0f, 2000.0f, 300.0f, 0.4f, "ms"));
    add (floatParam  (ParamIDs::freezeLpCutoff,  "Freeze Low Pass",  200.0f, 20000.0f, 18000.0f, 0.3f, "Hz"));
    add (floatParam  (ParamIDs::freezeHpCutoff,  "Freeze High Pass",  20.0f, 2000.0f,  20.0f, 0.3f, "Hz"));

    // --- whammy ---------------------------------------------------------------
    add (choiceParam (ParamIDs::bridgeType,    "Bridge",         bridgeTypeNames(), 1));
    add (floatParam  (ParamIDs::whammyPos,     "Whammy",        -1.0f, 1.0f, 0.0f));
    add (floatParam  (ParamIDs::whammyDown,    "Whammy Down",    0.0f, 36.0f, 2.0f, 0.4f, "st"));
    add (floatParam  (ParamIDs::whammyUp,      "Whammy Up",      0.0f, 24.0f, 1.0f, 0.4f, "st"));
    add (floatParam  (ParamIDs::whammySprings, "Spring Noise",   0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::transposeLock, "Transpose Lock", -12.0f, 12.0f, 0.0f, 0.5f, "st"));

    // --- cable ----------------------------------------------------------------
    add (boolParam  (ParamIDs::cableOn,     "Cable",        true));
    add (floatParam (ParamIDs::cableLength, "Cable Length", 0.5f, 15.0f, 3.0f, 0.4f, "m"));

    // --- guitar circuit (volume-knob-interaction.md 3) ---------------------------
    // Stock ranges are advanced-ranges.md 3.2's. Capacitors are declared in nF
    // rather than farads: a float carrying 2.2e-8 prints as "0.00" in every
    // host's automation lane, and nF is the unit on the part's own label.
    add (choiceParam (ParamIDs::cableQuality,       "Cable Quality",       cableQualityNames(), 1));
    add (ohmParam    (ParamIDs::circuitVolumePot,   "Volume Pot",          100.0e3f, 1.0e6f, 500.0e3f, 0.24f));
    add (ohmParam    (ParamIDs::circuitTonePot,     "Tone Pot",            100.0e3f, 1.0e6f, 500.0e3f, 0.24f));
    add (floatParam  (ParamIDs::circuitToneCap,     "Tone Cap",            10.0f, 100.0f, 22.0f, 0.24f, "nF"));
    add (choiceParam (ParamIDs::circuitPotTaper,    "Pot Taper",           potTaperNames(), 0));
    add (choiceParam (ParamIDs::circuitTrebleBleed, "Treble Bleed",        trebleBleedNames(), 0));
    add (ohmParam    (ParamIDs::circuitBleedR,      "Bleed Resistor",      10.0e3f, 1.0e6f, 130.0e3f, 0.2f));
    add (floatParam  (ParamIDs::circuitBleedC,      "Bleed Cap",           0.1f, 10.0f, 1.1f, 0.2f, "nF"));
    add (choiceParam (ParamIDs::circuitBleedMode,   "Bleed Wiring",        bleedModeNames(), 0));
    add (boolParam   (ParamIDs::circuitActive,      "Active Electronics",  false));
    add (ohmParam    (ParamIDs::ampInputImpedance,  "Amp Input Impedance", 220.0e3f, 1.0e6f, 1.0e6f, 0.32f));

    // --- amp ------------------------------------------------------------------
    add (choiceParam (ParamIDs::ampModel,    "Amp",       ampModelNames(), 0));
    add (floatParam  (ParamIDs::ampGain,     "Gain",      0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::ampBass,     "Bass",      0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::ampMid,      "Mid",       0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::ampTreble,   "Treble",    0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::ampPresence, "Presence",  0.0f, 1.0f, 0.4f));
    add (floatParam  (ParamIDs::ampMaster,   "Master",    0.0f, 1.0f, 0.7f));
    add (boolParam   (ParamIDs::ampBright,   "Bright",    false));
    add (boolParam   (ParamIDs::ampMidBoost, "Mid Boost", false));
    add (boolParam   (ParamIDs::ampStandby,  "Standby",   false));

    // --- cabinet --------------------------------------------------------------
    add (boolParam   (ParamIDs::cabOn,         "Cabinet",        true));
    add (choiceParam (ParamIDs::cabType,       "Cab Type",       cabinetNames(), 4));
    add (choiceParam (ParamIDs::cabSpeaker,    "Speaker",        speakerNames(), 1));
    add (floatParam  (ParamIDs::cabSpeakerAge, "Speaker Age",    0.0f, 1.0f, 0.4f));
    add (choiceParam (ParamIDs::micType,       "Mic 1",          micNames(), 0));
    add (choiceParam (ParamIDs::micPosition,   "Mic 1 Position", micPositionNames(), 1));
    add (choiceParam (ParamIDs::micDistance,   "Mic 1 Distance", micDistanceNames(), 0));
    add (boolParam   (ParamIDs::dualMic,       "Dual Mic",       false));
    add (choiceParam (ParamIDs::micType2,      "Mic 2",          micNames(), 4));
    add (choiceParam (ParamIDs::micPosition2,  "Mic 2 Position", micPositionNames(), 2));
    add (choiceParam (ParamIDs::micDistance2,  "Mic 2 Distance", micDistanceNames(), 1));
    add (floatParam  (ParamIDs::micBlend,      "Mic Blend",      0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::micWidth,      "Mic Width",      0.0f, 1.0f, 0.5f));
    add (floatParam  (ParamIDs::micPhaseAlign, "Phase Align",    0.0f, 300.0f, 0.0f, 0.4f, "mm"));

    // --- room -----------------------------------------------------------------
    add (boolParam   (ParamIDs::roomOn,       "Room",          true));
    add (choiceParam (ParamIDs::roomSize,     "Room Size",     roomSizeNames(), 2));
    add (choiceParam (ParamIDs::roomMaterial, "Room Material", roomMaterialNames(), 1));
    add (floatParam  (ParamIDs::roomBlend,    "Room Blend",    0.0f, 1.0f, 0.18f));
    add (floatParam  (ParamIDs::roomDecay,    "Room Decay",    0.25f, 4.0f, 1.0f, 0.4f));
    add (floatParam  (ParamIDs::roomWidth,    "Room Width",    0.0f, 1.0f, 0.6f));

    // --- master ---------------------------------------------------------------
    add (floatParam  (ParamIDs::masterGain, "Master Gain", -60.0f, 12.0f, 0.0f, 0.75f, "dB"));
    add (boolParam   (ParamIDs::limiterOn,  "Limiter",     true));
    add (choiceParam (ParamIDs::oversample, "Oversampling", oversamplingNames(), 2));

    // --- humanisation ---------------------------------------------------------
    add (floatParam (ParamIDs::humTiming,   "Timing Jitter",     0.0f, 25.0f, 3.0f, 0.4f, "ms"));
    add (floatParam (ParamIDs::humVelocity, "Velocity Variation",0.0f, 0.5f, 0.08f));
    add (floatParam (ParamIDs::humDetune,   "Micro Detune",      0.0f, 15.0f, 2.5f, 0.4f, "cents"));
    add (floatParam (ParamIDs::humAttack,   "Attack Variation",  0.0f, 0.6f, 0.10f));
    add (floatParam (ParamIDs::humNoise,    "Noise Probability", 0.0f, 1.0f, 0.25f));
    add (floatParam (ParamIDs::humStrum,    "Strum Variation",   0.0f, 1.0f, 0.20f));

    // --- feedback and doubler --------------------------------------------------
    add (boolParam  (ParamIDs::feedbackOn,    "Feedback",           false));
    add (floatParam (ParamIDs::feedbackThres, "Feedback Threshold", 0.1f, 1.0f, 0.65f));
    add (floatParam (ParamIDs::feedbackSpeed, "Feedback Speed",     0.0f, 1.0f, 0.4f));
    add (boolParam  (ParamIDs::doublerOn,     "Doubler",            false));
    add (floatParam (ParamIDs::doublerAmount, "Doubler Amount",     0.0f, 1.0f, 0.5f));

    // --- the hidden effect ---------------------------------------------------
    add (boolParam  (ParamIDs::secretOn,       "Wolf",             false));
    add (floatParam (ParamIDs::secretRate,     "Wolf Rate",        0.02f, 8.0f, 0.35f, 0.4f, "Hz"));
    add (floatParam (ParamIDs::secretDepth,    "Wolf Depth",       0.0f, 1.0f, 0.55f));
    add (floatParam (ParamIDs::secretFeedback, "Wolf Regeneration",0.0f, 0.92f, 0.62f));
    add (floatParam (ParamIDs::secretMix,      "Wolf Mix",         0.0f, 1.0f, 0.35f));

    // --- effect slots ----------------------------------------------------------
    const auto pedalNames = pedalTypeNames();

    for (int chain = 0; chain < 2; ++chain)
    {
        const bool post = (chain == 1);
        const juce::String chainName = post ? "Post " : "Pre ";

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const juce::String label = chainName + juce::String (slot + 1);

            add (choiceParam (ParamIDs::slotType (post, slot),   label + " Type",   pedalNames, 0));
            add (boolParam   (ParamIDs::slotBypass (post, slot), label + " Bypass", false));
            add (floatParam  (ParamIDs::slotMix (post, slot),    label + " Mix",    0.0f, 1.0f, 1.0f));

            for (int p = 0; p < Pedal::kMaxParams; ++p)
                add (floatParam (ParamIDs::slotParam (post, slot, p),
                                 label + " P" + juce::String (p + 1), 0.0f, 1.0f, 0.5f));
        }
    }

    // gui-integration.md 3.3 / 3.4 (2026-09-23). Appended, never inserted: a
    // host may address automation by parameter index.
    add (floatParam  (ParamIDs::inputGain,  "Input Gain",  -24.0f, 24.0f, 0.0f, 1.0f, "dB"));
    add (floatParam  (ParamIDs::outputMix,  "Wet/Dry",     0.0f, 1.0f, 1.0f));
    add (floatParam  (ParamIDs::stereoWidth, "Stereo Width", 0.0f, 2.0f, 1.0f));
    add (floatParam  (ParamIDs::macroCharacter, "Character", 0.0f, 1.0f, 0.25f));

    // ambiguity-resolutions.md 1.2 (2026-09-23), appended likewise.
    add (floatParam  (ParamIDs::feedbackAmount,     "Feedback Amount",   0.0f, 100.0f, 0.0f, 1.0f, "%"));
    add (floatParam  (ParamIDs::feedbackDistance,   "Feedback Distance", 0.0f, 3.0f, 0.5f, 1.0f, "m"));
    add (floatParam  (ParamIDs::feedbackAngle,      "Feedback Angle", -180.0f, 180.0f, 0.0f, 1.0f, "deg"));
    add (floatParam  (ParamIDs::feedbackFocus,      "Feedback Focus",    0.0f, 100.0f, 60.0f, 1.0f, "%"));
    add (floatParam  (ParamIDs::feedbackOctaveBias, "Feedback Octave Bias", -2.0f, 2.0f, 0.0f));

    // ambiguity-resolutions.md 2.2, appended likewise.
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::ebowStringMask), "E-Bow Strings",
                                                    0, (1 << kMaxStrings) - 1, 0));
    add (floatParam  (ParamIDs::ebowIntensity, "E-Bow Intensity", 0.0f, 100.0f, 50.0f, 1.0f, "%"));
    add (choiceParam (ParamIDs::ebowHarmonic, "E-Bow Harmonic",
                      { "Fundamental", "2nd", "3rd", "4th", "5th" }, 0));

    // ambiguity-resolutions.md 5.2, appended likewise.
    add (floatParam  (ParamIDs::presetMorphPosition, "Preset Morph", 0.0f, 1.0f, 0.0f));

    // string-scraping.md 2 (2026-09-23), appended likewise.
    add (boolParam   (ParamIDs::scrapeArmed,       "Scrape Armed", false));
    add (choiceParam (ParamIDs::scrapeTrigger,     "Scrape Trigger", { "Keyswitch", "CC", "MPE Zone", "Button Only" }, 0));
    add (choiceParam (ParamIDs::scrapeDirection,   "Scrape Direction", { "Bridge to Nut", "Nut to Bridge", "Hold + Sweep" }, 0));
    add (choiceParam (ParamIDs::scrapeSweepSource, "Scrape Sweep Source",
                      { "Auto", "Mod Wheel", "Expression", "Aftertouch", "Custom CC" }, 0));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::scrapeTriggerCc), "Scrape Trigger CC", 0, 127, 85));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::scrapeSweepCc), "Scrape Sweep CC", 0, 127, 16));
    add (floatParam  (ParamIDs::scrapeStartMm,     "Scrape Start",    0.0f, 1200.0f, 200.0f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::scrapeEndMm,       "Scrape End",      0.0f, 1200.0f, 900.0f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::scrapeDuration,    "Scrape Duration", 100.0f, 3000.0f, 600.0f, 0.25f, "ms"));
    add (floatParam  (ParamIDs::scrapePressure,    "Scrape Pressure", 0.0f, 1.0f, 0.5f));
    add (choiceParam (ParamIDs::scrapeTool,        "Scrape Tool", { "Pick", "Nail", "Thumb" }, 0));
    add (floatParam  (ParamIDs::scrapeAngle,       "Scrape Angle", -60.0f, 60.0f, 20.0f, 1.0f, "deg"));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::scrapeStringMask), "Scrape Strings",
                                                    0, (1 << kMaxStrings) - 1, 0));
    add (floatParam  (ParamIDs::scrapeRetrigger,   "Scrape Retrigger", 0.0f, 1000.0f, 200.0f, 1.0f, "ms"));

    // strum-dynamics.md 7, appended likewise. Crossing centres on its 200 sps
    // default; misses skew toward the small values that matter.
    add (floatParam  (ParamIDs::strumCrossingSps,     "Strum Crossing",      20.0f, 800.0f, 200.0f, 0.2308f, "sps"));
    add (floatParam  (ParamIDs::strumAcceleration,    "Strum Acceleration",   0.0f, 1.0f, 0.35f));
    add (floatParam  (ParamIDs::strumUpVelocityRatio, "Up-Stroke Speed",      0.5f, 2.0f, 1.25f));
    add (floatParam  (ParamIDs::strumTilt,            "Strum Tilt",          -1.0f, 1.0f, 0.15f));
    add (floatParam  (ParamIDs::strumMissProbability, "Strum Misses",         0.0f, 1.0f, 0.04f, 0.1f));
    add (choiceParam (ParamIDs::strumStrikerDown,     "Down Striker",         getStrikerNames(), 0));
    add (choiceParam (ParamIDs::strumStrikerUp,       "Up Striker",           getStrikerNames(), 0));
    add (floatParam  (ParamIDs::chuckAmount,          "Chuck Amount",         0.0f, 1.0f, 0.0f));
    add (floatParam  (ParamIDs::chuckDamping,         "Chuck Damping",        0.0f, 1.0f, 0.92f));

    // bass-techniques.md 2-5, string-slap-technique.md 1: the slap (params 426-450).
    add (floatParam  (ParamIDs::slapStrength,       "Slap Strength",       0.0f, 1.0f, 0.70f));
    add (floatParam  (ParamIDs::slapPositionMm,     "Slap Position",       5.0f, 400.0f, 60.0f, 1.0f, "mm"));
    add (floatParam  (ParamIDs::slapThumbHardness,  "Thumb Hardness",      0.0f, 1.0f, 0.55f));
    add (floatParam  (ParamIDs::slapFretContact,    "Slap Fret Contact",   0.0f, 1.0f, 0.80f));
    add (floatParam  (ParamIDs::popStrength,        "Pop Strength",        0.0f, 1.0f, 0.75f));
    add (floatParam  (ParamIDs::popPositionMm,      "Pop Position",        5.0f, 400.0f, 40.0f, 1.0f, "mm"));
    add (boolParam   (ParamIDs::doubleThumpEnabled, "Double Thump", false));
    add (floatParam  (ParamIDs::doubleThumpUpRatio, "Double Thump Up Ratio", 0.0f, 1.0f, 0.65f));
    add (floatParam  (ParamIDs::ghostLevel,         "Ghost Level",         0.0f, 1.0f, 0.45f));
    add (floatParam  (ParamIDs::ghostDamping,       "Ghost Damping",       0.0f, 1.0f, 0.94f));
    add (boolParam   (ParamIDs::ghostAuto,          "Auto Ghost", true));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::ghostVelocityThreshold), "Ghost Velocity Threshold", 1, 127, 32));
    add (boolParam   (ParamIDs::slapArmed,          "Slap Armed", false));
    add (choiceParam (ParamIDs::slapType,           "Slap Type", { "Thumb Slap", "Finger Pop", "Palm Slap", "Body Tap" }, 0));
    add (choiceParam (ParamIDs::slapTrigger,        "Slap Trigger", { "Keyswitch", "CC", "MPE Zone", "Button Only", "Velocity Zone" }, 0));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::slapVelocityZone), "Slap Velocity Zone", 1, 127, 100));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::slapTriggerCc), "Slap Trigger CC", 0, 127, 86));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::slapGhostCc), "Slap Ghost CC", 0, 127, 87));
    add (floatParam  (ParamIDs::slapForce,          "Slap Force",          0.0f, 1.0f, 0.6f));
    add (floatParam  (ParamIDs::slapPalmPositionMm, "Palm Slap Position",  5.0f, 400.0f, 100.0f, 1.0f, "mm"));
    add (std::make_unique<juce::AudioParameterInt> (pid (ParamIDs::slapStringMask), "Slap Strings", 0, (1 << kMaxStrings) - 1, 0));
    add (boolParam   (ParamIDs::slapGhostMode,      "Slap Ghost Mode", false));
    add (floatParam  (ParamIDs::slapReboundGap,     "Rebound Gap",         1.0f, 500.0f, 60.0f, 0.5f, "ms"));
    add (floatParam  (ParamIDs::slapSnapBack,       "Snap-Back",           0.0f, 1.0f, 0.5f));
    add (choiceParam (ParamIDs::slapBodyPart,       "Body Tap Resonance", { "Top", "Side", "Back" }, 0));

    return layout;
}

//==============================================================================
ParameterBridge::ParameterBridge (APVTS& state, LuthierEngine& e)
    : apvts (state), engine (e)
{
}

ParameterBridge::~ParameterBridge()
{
    cancelPendingUpdate();

    for (auto* p : watched)
        p->removeListener (this);
}

void ParameterBridge::parameterValueChanged (int parameterIndex, float)
{
    if (! juce::isPositiveAndBelow (parameterIndex, (int) slotOfParameter.size()))
        return;

    const int code = slotOfParameter[(size_t) parameterIndex];

    if (code < 0)
        return;

    const int which = code % 16;
    const int chain = (code / 16) / EffectsChain::kNumSlots, slot = (code / 16) % EffectsChain::kNumSlots;
    const auto serial = ++writeSerial;

    if (which == 15)
        typeWritten[(size_t) chain][(size_t) slot].store (serial, std::memory_order_relaxed);
    else
        paramsWritten[(size_t) chain][(size_t) slot].store (serial, std::memory_order_relaxed);
}

void ParameterBridge::cachePointers()
{
    pointers.clear();
    indices.clear();

    const auto& parameters = apvts.processor.getParameters();

    for (int i = 0; i < parameters.size(); ++i)
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
        {
            if (auto* ptr = apvts.getRawParameterValue (withId->paramID))
            {
                pointers.set (withId->paramID, ptr);

                // The matrix addresses destinations by parameter index, so the
                // audio thread never has to hash a string to apply modulation.
                indices.set (withId->paramID, i);
            }
        }
    }

    // The pedal slots' write order (see parameterValueChanged).
    for (auto* p : watched)
        p->removeListener (this);

    watched.clear();
    slotOfParameter.assign ((size_t) parameters.size(), -1);

    static_assert (Pedal::kMaxParams < 15, "the slot code keeps 15 for the type");

    auto watch = [this, &parameters] (const juce::String& id, int code)
    {
        const int index = parameterIndex (id);

        if (juce::isPositiveAndBelow (index, parameters.size()))
        {
            slotOfParameter[(size_t) index] = code;
            parameters[index]->addListener (this);
            watched.add (parameters[index]);
        }
    };

    for (int chain = 0; chain < 2; ++chain)
    {
        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const int base = (chain * EffectsChain::kNumSlots + slot) * 16;
            watch (ParamIDs::slotType (chain == 1, slot), base + 15);

            for (int p = 0; p < Pedal::kMaxParams; ++p)
                watch (ParamIDs::slotParam (chain == 1, slot, p), base + p);
        }
    }
}

int ParameterBridge::parameterIndex (const juce::String& id) const noexcept
{
    return indices.contains (id) ? indices[id] : -1;
}

float ParameterBridge::baseValue (const juce::String& id) const noexcept
{
    if (auto* ptr = raw (id))
        return ptr->load (std::memory_order_relaxed);

    return 0.0f;
}

std::atomic<float>* ParameterBridge::raw (const juce::String& id) const noexcept
{
    return pointers.contains (id) ? pointers[id] : nullptr;
}

float ParameterBridge::value (const juce::String& id) const noexcept
{
    const float base = baseValue (id);

    // The common case is a preset with no modulation at all, and it costs one
    // relaxed atomic read to find that out.
    if (modMatrix == nullptr || ! modMatrix->isActive())
        return base;

    return modMatrix->apply (parameterIndex (id), base);
}

//==============================================================================
void ParameterBridge::applyToEngine() noexcept
{
    // ---- macros ---------------------------------------------------------------
    // Each macro drives several underlying parameters at once. They multiply the
    // detailed controls rather than replacing them, so Advanced mode edits survive
    // a macro move.
    const double macroAttack = value (ParamIDs::macroAttack);
    const double macroBody = value (ParamIDs::macroBody);
    const double macroDrive = value (ParamIDs::macroDrive);
    const double macroTone = value (ParamIDs::macroTone);
    const double macroSpace = value (ParamIDs::macroSpace);
    const double macroHumanize = value (ParamIDs::macroHumanize);

    // ---- instrument -----------------------------------------------------------
    engine.setAttackBrightness (macroAttack);
    engine.setPluckPosition (value (ParamIDs::pluckPosition));
    engine.setPickThickness (value (ParamIDs::pickThickness));

    // pick-noise.md 2: these two were declared and shown and never read.
    engine.setPickMaterialAndFingers ((Excitation::Material) juce::jlimit (
                                          0, (int) Excitation::Material::NumMaterials - 1,
                                          (int) value (ParamIDs::pickMaterial)),
                                      value (ParamIDs::useFingers) > 0.5f);

    {
        PickSettings pick;
        pick.thicknessMm  = Parameters::pickThicknessMm (value (ParamIDs::pickThickness));
        pick.angleDegrees = Parameters::pickAngleDegrees (value (ParamIDs::pickAngle));
        pick.tipRadiusMm  = value (ParamIDs::pickTipRadius);
        pick.bevel        = value (ParamIDs::pickBevel);
        pick.wear         = value (ParamIDs::pickWear);
        pick.clickAmount  = value (ParamIDs::pickClickAmount);
        pick.chirpAmount  = value (ParamIDs::pickChirpAmount);
        pick.scrapeAmount = value (ParamIDs::pickScrapeAmount);
        engine.setPickNoise (pick);

        SqueakSettings squeak;
        squeak.amount         = value (ParamIDs::squeakAmount);
        squeak.probability    = value (ParamIDs::squeakProbability);
        squeak.moisture       = value (ParamIDs::squeakMoisture);
        squeak.pressure       = value (ParamIDs::squeakPressure);
        squeak.minTravelFrets = value (ParamIDs::squeakMinTravel);
        engine.setSqueak (squeak);
    }

    // ---- string scraping (string-scraping.md 2) ------------------------------------
    {
        ScrapeSettings scrape;
        scrape.armed           = value (ParamIDs::scrapeArmed) > 0.5f;
        scrape.trigger         = (ScrapeTriggerSource) juce::jlimit (0, (int) ScrapeTriggerSource::numSources - 1,
                                                                      (int) value (ParamIDs::scrapeTrigger));
        scrape.direction       = (ScrapeDirection) juce::jlimit (0, (int) ScrapeDirection::numDirections - 1,
                                                                  (int) value (ParamIDs::scrapeDirection));
        scrape.sweepSource     = (ScrapeSweepSource) juce::jlimit (0, (int) ScrapeSweepSource::numSources - 1,
                                                                    (int) value (ParamIDs::scrapeSweepSource));
        scrape.triggerCc       = juce::roundToInt (value (ParamIDs::scrapeTriggerCc));
        scrape.sweepCc         = juce::roundToInt (value (ParamIDs::scrapeSweepCc));
        scrape.startPositionMm = value (ParamIDs::scrapeStartMm);
        scrape.endPositionMm   = value (ParamIDs::scrapeEndMm);
        scrape.durationMs      = value (ParamIDs::scrapeDuration);
        scrape.pressure        = value (ParamIDs::scrapePressure);
        scrape.tool            = (ScrapeTool) juce::jlimit (0, (int) ScrapeTool::numTools - 1,
                                                             (int) value (ParamIDs::scrapeTool));
        scrape.angleDegrees    = value (ParamIDs::scrapeAngle);
        scrape.stringMask      = juce::roundToInt (value (ParamIDs::scrapeStringMask));
        scrape.retriggerMs     = value (ParamIDs::scrapeRetrigger);

        // Coverage C-29: pick_scrape_amount is the scrape's level; 1 at its 0.25 default.
        scrape.level           = value (ParamIDs::pickScrapeAmount) / 0.25f;
        engine.setScrapeSettings (scrape);
    }
    engine.setPickAngle (value (ParamIDs::pickAngle));
    engine.setNailVsFlesh (value (ParamIDs::nailVsFlesh));
    // fret_action is superseded by the setup geometry below (fret-buzz.md);
    // it stays declared because host automation is indexed against the list.

    {
        SetupGeometry setup;
        setup.actionTreble  = value (ParamIDs::setupActionTreble);
        setup.actionBass    = value (ParamIDs::setupActionBass);
        setup.relief        = value (ParamIDs::setupRelief);
        setup.fretHeight    = value (ParamIDs::setupFretHeight);
        setup.buzzThreshold = value (ParamIDs::setupBuzzThreshold);
        setup.sitarMode     = value (ParamIDs::setupSitarMode) > 0.5f;

        for (int s = 0; s < SetupGeometry::kMaxStrings; ++s)
            setup.nutDepth[(size_t) s] = value (ParamIDs::setupNutDepth (s % ParamIDs::kNumNutDepths + 1));

        engine.setSetupGeometry (setup);
    }

    {
        SlideSettings slide;
        slide.enabled          = value (ParamIDs::slideGuitar) > 0.5f;
        slide.mode             = (SlideMode) juce::jlimit (0, (int) SlideMode::numModes - 1,
                                                           (int) value (ParamIDs::slideMode));
        slide.pressure         = value (ParamIDs::slidePressure);
        slide.slantDegrees     = value (ParamIDs::slideSlant);
        slide.dampingBehind    = value (ParamIDs::slideDampingBehind);
        slide.noiseAmount      = value (ParamIDs::slideNoiseAmount);
        slide.clankAmount      = value (ParamIDs::slideClankAmount);
        slide.intonationAssist = value (ParamIDs::slideIntonationAssist);
        engine.setSlideSettings (slide);
    }
    engine.setFretBuzzAmount (value (ParamIDs::fretBuzz));

    engine.setNoiseAmounts (value (ParamIDs::slideNoise),
                            value (ParamIDs::fretNoise),
                            value (ParamIDs::releaseNoise),
                            value (ParamIDs::bodyKnock),
                            value (ParamIDs::pickNoise));

    engine.setAmpBuzzAmount (value (ParamIDs::ampBuzz));
    engine.getCouplingMatrix().setAmount (value (ParamIDs::couplingAmount));

    // ---- body ------------------------------------------------------------------
    engine.setBodyAmount (juce::jlimit (0.0, 1.0, value (ParamIDs::bodyAmount) * (0.4 + macroBody * 1.2)));
    engine.getBodyEngine().setAirResonanceGainDb (value (ParamIDs::bodyAirGain));

    // ---- pickups ----------------------------------------------------------------
    auto& pickups = engine.getPickupEngine();
    pickups.setSelector ((PickupSelector) (int) value (ParamIDs::pickupSelector));
    pickups.setBlend (value (ParamIDs::pickupBlend));

    pickups.setPiezoMicBlend (value (ParamIDs::piezoMicBlend));

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        pickups.setPickupVolume (slot, value (ParamIDs::pickupVolume (slot)));
    }

    // ---- performance -------------------------------------------------------------
    auto& interp = engine.getMidiInterpreter();
    interp.setMpeEnabled (value (ParamIDs::mpeEnabled) > 0.5f);
    interp.setPitchBendRange (value (ParamIDs::bendRange));
    // strum-dynamics 1.1 source 4: live chords cross at the plugin-global
    // velocity, which strum_speed (ms per string) used to set.
    interp.setStrumSpeedMs (1000.0 / juce::jmax (1.0, (double) value (ParamIDs::strumCrossingSps)));
    interp.setStrumDirection ((StrumDirection) (int) value (ParamIDs::strumDir));
    interp.setChordWindowMs (value (ParamIDs::chordWindow));

    MidiInterpreter::Humanisation hum;
    hum.timingJitterMs = value (ParamIDs::humTiming);
    hum.velocityVariation = value (ParamIDs::humVelocity);
    hum.microDetuneCents = value (ParamIDs::humDetune);
    hum.attackVariation = value (ParamIDs::humAttack);
    hum.stringNoiseProbability = value (ParamIDs::humNoise);
    hum.strumSpeedVariation = value (ParamIDs::humStrum);
    hum.amount = macroHumanize;
    interp.setHumanisation (hum);

    // ---- strum (strum-dynamics.md 7) --------------------------------------------
    {
        auto& rhythm = engine.getRhythmEngine();

        StrumSettings strum;
        strum.crossingSps     = value (ParamIDs::strumCrossingSps);
        strum.acceleration    = value (ParamIDs::strumAcceleration);
        strum.upVelocityRatio = value (ParamIDs::strumUpVelocityRatio);
        strum.tilt            = value (ParamIDs::strumTilt);
        strum.missProbability = value (ParamIDs::strumMissProbability);
        strum.strikerDown     = (Striker) juce::jlimit (0, (int) Striker::numStrikers - 1, (int) value (ParamIDs::strumStrikerDown));
        strum.strikerUp       = (Striker) juce::jlimit (0, (int) Striker::numStrikers - 1, (int) value (ParamIDs::strumStrikerUp));
        strum.chuckAmount     = value (ParamIDs::chuckAmount);
        strum.chuckDamping    = value (ParamIDs::chuckDamping);

        // strum_evenness is the rhythm engine's own; live play follows it too.
        strum.evenness = rhythm.getStrumEvenness();

        rhythm.setStrumSettings (strum);
        interp.setStrumSettings (strum);
    }

    // ---- slap (bass-techniques.md 2-5, string-slap-technique.md 1) ---------------
    {
        SlapSettings slap;
        slap.armed                  = value (ParamIDs::slapArmed) > 0.5f;
        slap.type                   = (SlapType) juce::jlimit (0, (int) SlapType::numTypes - 1, (int) value (ParamIDs::slapType));
        slap.trigger                = (TriggerSource) juce::jlimit (0, (int) TriggerSource::numSources - 1,
                                                                    (int) value (ParamIDs::slapTrigger));
        slap.velocityZone           = juce::roundToInt (value (ParamIDs::slapVelocityZone));
        slap.triggerCc              = juce::roundToInt (value (ParamIDs::slapTriggerCc));
        slap.ghostCc                = juce::roundToInt (value (ParamIDs::slapGhostCc));
        slap.slapStrength           = value (ParamIDs::slapStrength);
        slap.slapPositionMm         = value (ParamIDs::slapPositionMm);
        slap.thumbHardness          = value (ParamIDs::slapThumbHardness);
        slap.fretContact            = value (ParamIDs::slapFretContact);
        slap.popStrength            = value (ParamIDs::popStrength);
        slap.popPositionMm          = value (ParamIDs::popPositionMm);
        slap.doubleThump            = value (ParamIDs::doubleThumpEnabled) > 0.5f;
        slap.upRatio                = value (ParamIDs::doubleThumpUpRatio);
        slap.ghostLevel             = value (ParamIDs::ghostLevel);
        slap.ghostDamping           = value (ParamIDs::ghostDamping);
        slap.ghostAuto              = value (ParamIDs::ghostAuto) > 0.5f;
        slap.ghostVelocityThreshold = juce::roundToInt (value (ParamIDs::ghostVelocityThreshold));
        slap.force                  = value (ParamIDs::slapForce);
        slap.palmPositionMm         = value (ParamIDs::slapPalmPositionMm);
        slap.stringMask             = juce::roundToInt (value (ParamIDs::slapStringMask));
        slap.ghostMode              = value (ParamIDs::slapGhostMode) > 0.5f;
        slap.reboundGapMs           = value (ParamIDs::slapReboundGap);
        slap.snapBack               = value (ParamIDs::slapSnapBack);
        slap.bodyPart               = (BodyPart) juce::jlimit (0, (int) BodyPart::numParts - 1, (int) value (ParamIDs::slapBodyPart));
        engine.setSlapSettings (slap);
    }

    auto& tech = engine.getTechniqueEngine();
    tech.setLegatoWindowMs (value (ParamIDs::legatoWindow));
    tech.setSlideGuitarMode (value (ParamIDs::slideGuitar) > 0.5f);

    engine.setVibratoRate (value (ParamIDs::vibratoRate));
    engine.setVibratoDepthCents (value (ParamIDs::vibratoDepth));
    {
        EBowSettings ebow;
        ebow.enabled = value (ParamIDs::ebowEnable) > 0.5f;
        ebow.stringMask = juce::roundToInt (value (ParamIDs::ebowStringMask));
        ebow.intensity = value (ParamIDs::ebowIntensity) * 0.01;
        ebow.harmonic = 1 + juce::roundToInt (value (ParamIDs::ebowHarmonic));
        engine.setEBow (ebow);
    }

    // ---- freeze (ambiguity-resolutions 2.1) ------------------------------------
    auto& freeze = engine.getFreezeOverlay();

    // Capture length is read before the enable, so a rising edge this block
    // captures the window length the user actually has dialled in.
    freeze.setCaptureMs (value (ParamIDs::freezeCaptureMs));
    freeze.setLevelDb   (value (ParamIDs::freezeLevel));
    freeze.setAttackMs  (value (ParamIDs::freezeAttackMs));
    freeze.setReleaseMs (value (ParamIDs::freezeReleaseMs));
    freeze.setLowpassHz (value (ParamIDs::freezeLpCutoff));
    freeze.setHighpassHz (value (ParamIDs::freezeHpCutoff));
    freeze.setEnabled   (value (ParamIDs::freezeEnable) > 0.5f);

    // ---- whammy ---------------------------------------------------------------
    auto& wham = engine.getWhammyEngine();
    wham.setRange (value (ParamIDs::whammyDown), value (ParamIDs::whammyUp));
    wham.setSpringAmount (value (ParamIDs::whammySprings));
    wham.setTransposeLock ((int) value (ParamIDs::transposeLock));

    // ---- guitar circuit (volume-knob-interaction.md) ------------------------------
    {
        CircuitComponents circuit;

        circuit.volume = value (ParamIDs::guitarVolume);

        // The Tone macro turns the guitar's own tone control as well as the amp.
        circuit.tone = juce::jlimit (0.0, 1.0, value (ParamIDs::guitarTone) * (0.45 + macroTone * 1.1));

        circuit.volumePot = value (ParamIDs::circuitVolumePot);
        circuit.tonePot   = value (ParamIDs::circuitTonePot);
        circuit.toneCap   = value (ParamIDs::circuitToneCap) * 1.0e-9;
        circuit.taper     = (PotTaper) (int) value (ParamIDs::circuitPotTaper);

        circuit.bleed            = (TrebleBleed) (int) value (ParamIDs::circuitTrebleBleed);
        circuit.bleedResistance  = value (ParamIDs::circuitBleedR);
        circuit.bleedCapacitance = value (ParamIDs::circuitBleedC) * 1.0e-9;
        circuit.bleedSeries      = (int) value (ParamIDs::circuitBleedMode) == 1;

        circuit.active = value (ParamIDs::circuitActive) > 0.5f;

        circuit.cableOn           = value (ParamIDs::cableOn) > 0.5f;
        circuit.cableLength       = value (ParamIDs::cableLength);
        circuit.cableQuality      = (CableQuality) (int) value (ParamIDs::cableQuality);
        circuit.ampInputImpedance = value (ParamIDs::ampInputImpedance);

        engine.setCircuitControls (circuit);
    }

    // ---- amp -------------------------------------------------------------------
    auto& ampEngine = engine.getAmpEngine();

    // Drive macro pushes the amp gain on top of its own control.
    // The Drive macro pushes on top of the knob. It stays inside the stock
    // travel unless the knob itself is already past it (advanced ranges).
    {
        const double knob = value (ParamIDs::ampGain);
        ampEngine.setGain (juce::jlimit (0.0, juce::jmax (1.0, knob), knob + macroDrive * 0.55));
    }
    ampEngine.setBass (value (ParamIDs::ampBass));
    ampEngine.setMid (value (ParamIDs::ampMid));

    // Tone macro tilts treble against bass, the way a single tone knob should.
    {
        const double knob = value (ParamIDs::ampTreble);
        ampEngine.setTreble (juce::jlimit (juce::jmin (0.0, knob), juce::jmax (1.0, knob),
                                           knob + (macroTone - 0.5) * 0.5));
    }
    ampEngine.setPresence (value (ParamIDs::ampPresence));
    ampEngine.setMaster (value (ParamIDs::ampMaster));
    ampEngine.setBrightSwitch (value (ParamIDs::ampBright) > 0.5f);
    ampEngine.setMidBoost (value (ParamIDs::ampMidBoost) > 0.5f);
    ampEngine.setStandby (value (ParamIDs::ampStandby) > 0.5f);

    // ---- cabinet ----------------------------------------------------------------
    auto& cab = engine.getCabinetEngine();
    cab.setEnabled (value (ParamIDs::cabOn) > 0.5f);
    cab.setDualMicEnabled (value (ParamIDs::dualMic) > 0.5f);
    cab.setMicBlend (value (ParamIDs::micBlend));
    cab.setStereoWidth (value (ParamIDs::micWidth));
    cab.setPhaseAlignMm (value (ParamIDs::micPhaseAlign));

    // ---- room --------------------------------------------------------------------
    auto& roomEngine = engine.getRoomEngine();
    roomEngine.setEnabled (value (ParamIDs::roomOn) > 0.5f);

    // Space macro drives the room blend and the post-chain reverb sends together.
    roomEngine.setRoomBlend (juce::jlimit (0.0, 1.0, value (ParamIDs::roomBlend) + macroSpace * 0.45));
    roomEngine.setDecayScale (value (ParamIDs::roomDecay));
    roomEngine.setWidth (value (ParamIDs::roomWidth));

    // ---- master ------------------------------------------------------------------
    engine.getMasterBus().setGainDb (value (ParamIDs::masterGain));
    engine.setInputGainDb (value (ParamIDs::inputGain));
    engine.setOutputMix (value (ParamIDs::outputMix));
    engine.setStereoWidth (value (ParamIDs::stereoWidth));

    // The character macro is the character-wear amount (character-wear.md 0.3),
    // so the CHARACTER tab's amount and Easy's macro are one control.
    engine.getCharacterEngine().setAmount (value (ParamIDs::macroCharacter));
    engine.getMasterBus().setLimiterEnabled (value (ParamIDs::limiterOn) > 0.5f);

    // ---- feedback and doubler ------------------------------------------------------
    {
        FeedbackSettings fb;
        fb.amount = value (ParamIDs::feedbackAmount) * 0.01;
        fb.distanceMetres = value (ParamIDs::feedbackDistance);
        fb.angleDegrees = value (ParamIDs::feedbackAngle);
        fb.focus = value (ParamIDs::feedbackFocus) * 0.01;
        fb.octaveBias = juce::roundToInt (value (ParamIDs::feedbackOctaveBias));
        engine.setFeedback (fb);
    }
    // doubler_on / doubler_amount: kept for saved automation. The doubler is the
    // post-amp Doubler pedal now (ambiguity-resolutions 3); see PresetManager.

    // ---- the hidden effect ---------------------------------------------------------
    auto& wolf = engine.getSecretEffect();
    wolf.setEnabled (value (ParamIDs::secretOn) > 0.5f);
    wolf.setRate (value (ParamIDs::secretRate));
    wolf.setDepth (value (ParamIDs::secretDepth));
    wolf.setFeedback (value (ParamIDs::secretFeedback));
    wolf.setMix (value (ParamIDs::secretMix));

    // ---- pedal parameters -----------------------------------------------------------
    for (int chain = 0; chain < 2; ++chain)
    {
        const bool post = (chain == 1);
        auto& fx = post ? engine.getPostEffects() : engine.getPreEffects();

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            fx.setSlotBypassed (slot, value (ParamIDs::slotBypass (post, slot)) > 0.5f);
            fx.setSlotMix (slot, value (ParamIDs::slotMix (post, slot)));

            if (auto* pedal = fx.getPedal (slot))
            {
                const int numParams = juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams);

                for (int p = 0; p < numParams; ++p)
                    pedal->setParameterNormalised (p, value (ParamIDs::slotParam (post, slot, p)));
            }
        }
    }

    // ---- structural change detection ---------------------------------------------
    const bool structural = readStructuralValues() || ! structuralInitialised;

    if (structural)
    {
        structuralPending.store (true);
        triggerAsyncUpdate();
    }
}

bool ParameterBridge::readStructuralValues() noexcept
{
    auto changed = [] (int& cached, int current) noexcept
    {
        if (cached == current)
            return false;

        cached = current;
        return true;
    };

    bool structural = false;

    structural |= changed (lastGuitarType,     (int) value (ParamIDs::guitarType));
    structural |= changed (lastTuning,         (int) value (ParamIDs::tuningPreset));
    structural |= changed (lastStringMaterial, (int) value (ParamIDs::stringMaterial));
    structural |= changed (lastStringGauge,    (int) value (ParamIDs::stringGauge));
    structural |= changed (lastStringAge,      (int) value (ParamIDs::stringAge));
    structural |= changed (lastBodyMode,       (int) value (ParamIDs::bodyMode));
    structural |= changed (lastBracing,        (int) value (ParamIDs::bodyBracing));
    structural |= changed (lastTopWood,        (int) value (ParamIDs::bodyTopWood));
    structural |= changed (lastBackWood,       (int) value (ParamIDs::bodyBackWood));
    structural |= changed (lastAmpModel,       (int) value (ParamIDs::ampModel));
    structural |= changed (lastCabType,        (int) value (ParamIDs::cabType));
    structural |= changed (lastSpeaker,        (int) value (ParamIDs::cabSpeaker));
    structural |= changed (lastMicType,        (int) value (ParamIDs::micType));
    structural |= changed (lastMicPos,         (int) value (ParamIDs::micPosition));
    structural |= changed (lastMicDist,        (int) value (ParamIDs::micDistance));
    structural |= changed (lastMicType2,       (int) value (ParamIDs::micType2));
    structural |= changed (lastMicPos2,        (int) value (ParamIDs::micPosition2));
    structural |= changed (lastMicDist2,       (int) value (ParamIDs::micDistance2));
    structural |= changed (lastRoomSize,       (int) value (ParamIDs::roomSize));
    structural |= changed (lastRoomMaterial,   (int) value (ParamIDs::roomMaterial));
    structural |= changed (lastBridgeType,     (int) value (ParamIDs::bridgeType));
    structural |= changed (lastPlayingMode,    (int) value (ParamIDs::playingMode));
    structural |= changed (lastTemperament,    (int) value (ParamIDs::temperament));
    structural |= changed (lastCapoFret,       (int) value (ParamIDs::capoFret));
    structural |= changed (lastOversample,     (int) value (ParamIDs::oversample));

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        structural |= changed (lastPickupType[slot],   (int) value (ParamIDs::pickupType (slot)));
        structural |= changed (lastPickupMagnet[slot], (int) value (ParamIDs::pickupMagnet (slot)));
    }

    for (int chain = 0; chain < 2; ++chain)
        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            structural |= changed (lastSlotType[chain][slot],
                                   (int) value (ParamIDs::slotType (chain == 1, slot)));

    return structural;
}

//==============================================================================
void ParameterBridge::adoptPedalTypesFromParameters()
{
    for (int chain = 0; chain < 2; ++chain)
    {
        const bool post = (chain == 1);
        auto& fx = post ? engine.getPostEffects() : engine.getPreEffects();

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const int type = juce::jlimit (0, (int) PedalType::NumTypes - 1,
                                           (int) value (ParamIDs::slotType (post, slot)));

            // Recorded as seen, so the structural path does not treat it as a
            // fresh pick and write the defaults.
            lastSlotType[chain][slot] = type;

            if (fx.getSlotType (slot) != (PedalType) type)
            {
                fx.setSlotType (slot, (PedalType) type);
                pushSlotParameters (post, slot);
            }
        }
    }
}

void ParameterBridge::pushSlotParameters (bool post, int slot)
{
    // A pedal built after this block's parameter pass would otherwise run at
    // its constructor defaults until the next one.
    auto& fx = post ? engine.getPostEffects() : engine.getPreEffects();

    if (auto* pedal = fx.getPedal (slot))
        for (int p = 0; p < juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams); ++p)
            pedal->setParameterNormalised (p, value (ParamIDs::slotParam (post, slot, p)));
}

//==============================================================================
void ParameterBridge::handleAsyncUpdate()
{
    const juce::ScopedLock sl (engineLock);
    applyStructural();
    structuralPending.store (false);
}

void ParameterBridge::applyAllNow()
{
    const juce::ScopedLock sl (engineLock);
    structuralInitialised = false;
    applyToEngine();
    cancelPendingUpdate();
    applyStructural();
    structuralPending.store (false);
}

void ParameterBridge::applyStructural()
{
    const bool firstTime = ! structuralInitialised;
    structuralInitialised = true;

    // Loading a guitar type resets a lot of downstream state, so it goes first and
    // the explicit parameters below then override whatever it set.
    if (firstTime || lastGuitarType != (int) engine.getGuitarType())
    {
        const auto type = (GuitarType) juce::jlimit (0, (int) GuitarType::NumTypes - 1, lastGuitarType);

        if (onLoadGuitarType != nullptr && onLoadGuitarType (type))
            readStructuralValues();   // the guitar's parts are in the parameters now
        else
            engine.setGuitarType (type);
    }

    engine.setTuningPreset ((TuningPreset) juce::jlimit (0, (int) TuningPreset::NumPresets - 1, lastTuning));

    engine.getTuningEngine().setTemperament (
        (Temperament) juce::jlimit (0, (int) Temperament::NumTemperaments - 1, lastTemperament));
    engine.getTuningEngine().setConcertA (value (ParamIDs::concertA));

    /*  ambiguity-resolutions 4.5. The choice index is the fret, because entry 0
        is "Off" and entry n is "Fret n" - so no mapping table can drift out of
        step with the names. */
    engine.getTuningEngine().setCapoFret ((int) value (ParamIDs::capoFret));
    engine.getTuningEngine().setDriftEnabled (value (ParamIDs::tuningDrift) > 0.5f);

    for (int s = 0; s < engine.getNumStrings(); ++s)
        engine.getTuningEngine().setIntonationSlope (s, value (ParamIDs::intonationErr));

    engine.getTuningEngine().randomiseRealismDetune (value (ParamIDs::realismDetune), 0x9E3779B9ull);

    engine.setStringMaterial ((StringMaterial) juce::jlimit (0, (int) StringMaterial::NumMaterials - 1, lastStringMaterial));
    engine.setStringGauge ((StringGauge) juce::jlimit (0, (int) StringGauge::NumGauges - 1, lastStringGauge));
    engine.setStringAge ((StringAge) juce::jlimit (0, (int) StringAge::NumAges - 1, lastStringAge));
    engine.setFretless (value (ParamIDs::fretless) > 0.5f);

    // ---- body ------------------------------------------------------------------
    {
        auto cfg = engine.getBodyEngine().getBodyConfig();
        cfg.bracing = (Bracing) juce::jlimit (0, (int) Bracing::NumBracings - 1, lastBracing);
        cfg.topWood = (Wood) juce::jlimit (0, (int) Wood::NumWoods - 1, lastTopWood);
        cfg.backWood = (Wood) juce::jlimit (0, (int) Wood::NumWoods - 1, lastBackWood);
        cfg.sideWood = cfg.backWood;
        cfg.scaleWidth = value (ParamIDs::bodyWidth);
        cfg.scaleDepth = value (ParamIDs::bodyDepth);
        cfg.topThicknessMm = value (ParamIDs::bodyTopThick);
        cfg.soundHoleScale = value (ParamIDs::bodySoundhole);
        cfg.age = value (ParamIDs::bodyAge);
        engine.getBodyEngine().setBodyConfig (cfg);

        const int mode = juce::jlimit (0, 3, lastBodyMode);
        engine.getBodyEngine().setMode (mode == 0 ? BodyEngine::Mode::Convolution
                                      : mode == 1 ? BodyEngine::Mode::Modal
                                      : mode == 2 ? BodyEngine::Mode::Hybrid
                                                  : BodyEngine::Mode::Bypassed);

        if (mode == 0 || mode == 2)
            engine.reloadBodyIr();
    }

    // ---- pickups ----------------------------------------------------------------
    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        const auto type = (PickupType) juce::jlimit (0, (int) PickupType::NumTypes - 1, lastPickupType[slot]);

        /*  A parts guitar's pickup - its L, R, C, placement, cover - stands
            unless the pickup type has been changed away from the part's;
            then the type's defaults stand in, at the part's placement
            (guitar-workshop.md 9: placement is the guitar's, not a parameter). */
        PickupSpec pspec;

        if (engine.isWorkshopGuitar() && slot < 3)
        {
            const auto& part = engine.getPartsPickup (slot);

            if (part.type == type)
            {
                pspec = part;
            }
            else
            {
                pspec = PickupSpec::makeDefault (type, part.position);
                pspec.heightMm = part.heightMm;
            }
        }
        else
        {
            pspec = PickupSpec::makeDefault (type, engine.getGuitarSpec().pickupPositions[juce::jmin (slot, 2)]);
        }

        pspec.magnet = (MagnetType) juce::jlimit (0, (int) MagnetType::NumMagnets - 1, lastPickupMagnet[slot]);
        pspec.coilTapped = value (ParamIDs::coilTap) > 0.5f;

        engine.getPickupEngine().setPickupSpec (slot, pspec);
    }

    // ---- hardware -----------------------------------------------------------------
    engine.getWhammyEngine().setBridgeType (
        (WhammyEngine::BridgeType) juce::jlimit (0, (int) WhammyEngine::BridgeType::NumTypes - 1, lastBridgeType));

    engine.getMidiInterpreter().setPlayingMode (
        (PlayingMode) juce::jlimit (0, (int) PlayingMode::NumModes - 1, lastPlayingMode));

    engine.getAmpEngine().setModel (
        (AmpModel) juce::jlimit (0, (int) AmpModel::NumModels - 1, lastAmpModel));

    // ---- cabinet -------------------------------------------------------------------
    {
        CabinetConfig a;
        a.cabinet = (CabinetType) juce::jlimit (0, (int) CabinetType::NumCabinets - 1, lastCabType);
        a.speaker = (SpeakerType) juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1, lastSpeaker);
        a.mic = (MicType) juce::jlimit (0, (int) MicType::NumMics - 1, lastMicType);
        a.position = (MicPosition) juce::jlimit (0, (int) MicPosition::NumPositions - 1, lastMicPos);
        a.distance = (MicDistance) juce::jlimit (0, (int) MicDistance::NumDistances - 1, lastMicDist);
        a.speakerAge = value (ParamIDs::cabSpeakerAge);
        engine.getCabinetEngine().setConfigA (a);

        CabinetConfig b = a;
        b.mic = (MicType) juce::jlimit (0, (int) MicType::NumMics - 1, lastMicType2);
        b.position = (MicPosition) juce::jlimit (0, (int) MicPosition::NumPositions - 1, lastMicPos2);
        b.distance = (MicDistance) juce::jlimit (0, (int) MicDistance::NumDistances - 1, lastMicDist2);
        engine.getCabinetEngine().setConfigB (b);

        engine.reloadCabinetIrs();
    }

    // ---- room ----------------------------------------------------------------------
    engine.getRoomEngine().setRoomSize ((RoomSize) juce::jlimit (0, (int) RoomSize::NumRoomSizes - 1, lastRoomSize));
    engine.getRoomEngine().setMaterial ((RoomMaterial) juce::jlimit (0, (int) RoomMaterial::NumMaterials - 1, lastRoomMaterial));

    // ---- oversampling ----------------------------------------------------------------
    {
        const int factors[4] = { 1, 2, 4, 8 };
        engine.setOversamplingFactor (factors[juce::jlimit (0, 3, lastOversample)]);
    }

    // ---- pedal types --------------------------------------------------------------------
    for (int chain = 0; chain < 2; ++chain)
    {
        const bool post = (chain == 1);
        auto& fx = post ? engine.getPostEffects() : engine.getPreEffects();

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const auto type = (PedalType) juce::jlimit (0, (int) PedalType::NumTypes - 1,
                                                        lastSlotType[chain][slot]);

            if (fx.getSlotType (slot) != type)
            {
                fx.setSlotType (slot, type);

                // A pedal whose settings were written with it (a preset, a
                // snapshot, a morph) keeps them; one the player has just picked
                // starts at its own defaults, written back into the parameters
                // so the UI and the preset agree with what is loaded.
                const bool settingsCameWithIt = paramsWritten[(size_t) chain][(size_t) slot].load (std::memory_order_relaxed)
                                              > typeWritten[(size_t) chain][(size_t) slot].load (std::memory_order_relaxed);

                if (settingsCameWithIt)
                    pushSlotParameters (post, slot);
                else if (auto* pedal = fx.getPedal (slot))
                {
                    for (int p = 0; p < juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams); ++p)
                    {
                        const auto& d = pedal->getParameterDescriptor (p);
                        const float norm = (float) d.toNormalised (d.defaultValue);

                        if (auto* param = apvts.getParameter (ParamIDs::slotParam (post, slot, p)))
                            param->setValueNotifyingHost (norm);
                    }
                }
            }
        }
    }

    // Anything that depends on the string physics has to be recomputed last.
    engine.refreshStringPhysics();
}

} // namespace luthier
