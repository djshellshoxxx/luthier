#include "GuitarLibrary.h"

namespace luthier
{

namespace
{
    using PT = PickupType;
    using MT = MagnetType;
    using PS = PickupSelector;
    using BT = WhammyEngine::BridgeType;

    // Common scale lengths, in millimetres.
    constexpr double kFenderScale    = 647.7;   // 25.5"
    constexpr double kGibsonScale    = 628.65;  // 24.75"
    constexpr double kMartinScale    = 645.2;   // 25.4"
    constexpr double kParlorScale    = 632.5;   // 24.9"
    constexpr double kClassicalScale = 650.0;
    constexpr double kSevenScale     = 673.1;   // 26.5"
    constexpr double kEightScale     = 685.8;   // 27"
    constexpr double kBaritoneScale  = 727.1;   // 28.625"
    constexpr double kBassScale      = 863.6;   // 34"
    constexpr double kRickScale      = 844.6;   // 33.25"
    constexpr double kResonatorScale = 635.0;   // 25"

    const GuitarSpec kGuitars[(size_t) GuitarType::NumTypes] =
    {
        //======================================================================
        // ELECTRIC
        //======================================================================
        { "Vintage Double-Cut", "Alder body, maple neck, three single-coils, 25.5\" scale",
          GuitarCategory::Electric,
          6, false, kFenderScale, 22, 1.6, false,
          BodyShape::SolidStandard, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Maple,
          Bracing::SolidBodyNone,
          3, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.13, 0.25, 0.40 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Regular, TuningPreset::Standard,
          BT::VintageTrem,
          AmpModel::FenderTwin, CabinetType::Cab2x12Open, SpeakerType::JensenC12, MicType::SM57,
          0.16, 0.22, 0.85 },

        { "Classic T-Style", "Ash body, maple neck, two single-coils, 25.5\" scale",
          GuitarCategory::Electric,
          6, false, kFenderScale, 22, 1.5, false,
          BodyShape::SolidStandard, Wood::Ash, Wood::Ash, Wood::Ash, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.11, 0.42, 0.42 },
             { MT::Alnico5, MT::Alnico3, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Regular, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::FenderTweed, CabinetType::Cab1x12Open, SpeakerType::JensenC12, MicType::SM57,
          0.13, 0.20, 0.80 },

        { "Vintage Single-Cut", "Mahogany body with maple cap, two humbuckers, 24.75\" scale",
          GuitarCategory::Electric,
          6, false, kGibsonScale, 22, 1.7, false,
          BodyShape::SolidHeavy, Wood::Maple, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.14, 0.38, 0.38 },
             { MT::Alnico5, MT::Alnico2, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Regular, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::MarshallPlexi, CabinetType::Cab4x12, SpeakerType::Greenback, MicType::SM57,
          0.17, 0.25, 0.90 },

        { "SG", "All-mahogany, thin body, two humbuckers, 24.75\" scale",
          GuitarCategory::Electric,
          6, false, kGibsonScale, 22, 1.6, false,
          BodyShape::SolidThin, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.15, 0.39, 0.39 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Regular, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::MarshallPlexi, CabinetType::Cab4x12, SpeakerType::Greenback, MicType::SM57,
          0.17, 0.26, 0.92 },

        { "Semi-Hollow 335", "Semi-hollow maple with a centre block, two humbuckers",
          GuitarCategory::Electric,
          6, false, kGibsonScale, 22, 1.7, false,
          BodyShape::SemiHollow, Wood::Maple, Wood::Maple, Wood::Maple, Wood::Mahogany,
          Bracing::SemiHollowBlock,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.14, 0.37, 0.37 },
             { MT::Alnico2, MT::Alnico2, MT::Alnico5 },
          PS::Neck, false, false,
          StringMaterial::Flatwound, StringGauge::Medium, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::FenderTwin, CabinetType::Cab2x12Open, SpeakerType::AlnicoBlue, MicType::RibbonR121,
          0.22, 0.48, 1.00 },

        { "Offset Modern", "Offset alder body, two wide single-coils, floating trem",
          GuitarCategory::Electric,
          6, false, kFenderScale, 21, 1.8, false,
          BodyShape::Offset, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::P90, PT::P90, PT::P90 },
             { 0.15, 0.41, 0.41 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Neck, false, false,
          StringMaterial::PureNickel, StringGauge::Medium, TuningPreset::Standard,
          BT::VintageTrem,
          AmpModel::FenderDeluxe, CabinetType::Cab1x12Open, SpeakerType::JensenC12, MicType::MD421,
          0.19, 0.24, 0.88 },

        { "Angular Korina", "Korina body, two humbuckers, long upper horn",
          GuitarCategory::Electric,
          6, false, kGibsonScale, 22, 1.6, false,
          BodyShape::SolidStandard, Wood::Korina, Wood::Korina, Wood::Korina, Wood::Mahogany,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.13, 0.38, 0.38 },
             { MT::Ceramic, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Medium, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::MarshallJCM800, CabinetType::Cab4x12, SpeakerType::Vintage30, MicType::SM57,
          0.15, 0.23, 0.86 },

        { "Superstrat", "Basswood body, HSH pickups, thin neck, locking trem",
          GuitarCategory::Electric,
          6, false, kFenderScale, 24, 1.3, false,
          BodyShape::SolidStandard, Wood::Basswood, Wood::Basswood, Wood::Basswood, Wood::Maple,
          Bracing::SolidBodyNone,
          3, { PT::Humbucker, PT::SingleCoil, PT::Humbucker },
             { 0.12, 0.25, 0.39 },
             { MT::Ceramic, MT::Alnico5, MT::Ceramic },
          PS::Bridge, false, false,
          StringMaterial::Cobalt, StringGauge::Light, TuningPreset::Standard,
          BT::FloydRose,
          AmpModel::MesaRectifier, CabinetType::Cab4x12, SpeakerType::Vintage30, MicType::SM57,
          0.11, 0.18, 0.78 },

        { "7-String", "Extended range with a low B, 26.5\" scale",
          GuitarCategory::Electric,
          7, false, kSevenScale, 24, 1.4, false,
          BodyShape::SolidStandard, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.12, 0.38, 0.38 },
             { MT::Ceramic, MT::Alnico5, MT::Ceramic },
          PS::Bridge, false, false,
          StringMaterial::StainlessSteel, StringGauge::Medium, TuningPreset::SevenString,
          BT::Fixed,
          AmpModel::DiezelVH4, CabinetType::Cab4x12, SpeakerType::Vintage30, MicType::SM57,
          0.10, 0.18, 0.80 },

        { "8-String", "Extended range with a low F#, 27\" scale",
          GuitarCategory::Electric,
          8, false, kEightScale, 24, 1.4, false,
          BodyShape::SolidStandard, Wood::Ash, Wood::Mahogany, Wood::Mahogany, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.11, 0.37, 0.37 },
             { MT::Ceramic, MT::Ceramic, MT::Ceramic },
          PS::Bridge, false, false,
          StringMaterial::StainlessSteel, StringGauge::Heavy, TuningPreset::EightString,
          BT::Fixed,
          AmpModel::DiezelVH4, CabinetType::Cab4x12, SpeakerType::Vintage30, MicType::SM57,
          0.09, 0.17, 0.78 },

        { "Baritone Electric", "28.625\" scale tuned to B, built for low tunings",
          GuitarCategory::Electric,
          6, false, kBaritoneScale, 22, 1.6, false,
          BodyShape::SolidStandard, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::Humbucker },
             { 0.13, 0.38, 0.38 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Heavy, TuningPreset::BaritoneB,
          BT::Fixed,
          AmpModel::BognerEcstasy, CabinetType::Cab4x12, SpeakerType::G12H, MicType::MD421,
          0.12, 0.21, 0.84 },

        //======================================================================
        // ACOUSTIC
        //======================================================================
        { "Dreadnought", "Sitka spruce top, rosewood back and sides, X-braced",
          GuitarCategory::Acoustic,
          6, false, kMartinScale, 20, 2.2, false,
          BodyShape::Dreadnought, Wood::SitkaSpruce, Wood::Rosewood, Wood::Rosewood, Wood::Mahogany,
          Bracing::XBrace,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, true,
          StringMaterial::PhosphorBronze, StringGauge::AcousticLight, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::U87,
          0.24, 1.00, 1.00 },

        { "Auditorium", "Smaller body, spruce over mahogany, balanced and articulate",
          GuitarCategory::Acoustic,
          6, false, kMartinScale, 20, 2.1, false,
          BodyShape::Auditorium, Wood::SitkaSpruce, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany,
          Bracing::XBrace,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, true,
          StringMaterial::PhosphorBronze, StringGauge::AcousticLight, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::C414,
          0.26, 1.00, 1.00 },

        { "Jumbo", "Large maple body, big low end and plenty of volume",
          GuitarCategory::Acoustic,
          6, false, kFenderScale, 20, 2.3, false,
          BodyShape::Jumbo, Wood::SitkaSpruce, Wood::Maple, Wood::Maple, Wood::Mahogany,
          Bracing::ForwardShiftedX,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, true,
          StringMaterial::Bronze8020, StringGauge::AcousticMedium, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::U87,
          0.25, 1.00, 1.00 },

        { "Parlor", "Small ladder-braced body, dry midrange bark",
          GuitarCategory::Acoustic,
          6, false, kParlorScale, 19, 2.0, false,
          BodyShape::Parlor, Wood::SitkaSpruce, Wood::Mahogany, Wood::Mahogany, Wood::Mahogany,
          Bracing::LadderBrace,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, true,
          StringMaterial::SilkAndSteel, StringGauge::AcousticLight, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::C414,
          0.28, 1.00, 1.00 },

        { "Classical", "Cedar top, fan-braced, nylon strings, wide neck",
          GuitarCategory::Acoustic,
          6, false, kClassicalScale, 19, 3.0, false,
          BodyShape::Classical, Wood::Cedar, Wood::Rosewood, Wood::Rosewood, Wood::Mahogany,
          Bracing::FanBrace,
          1, { PT::InternalMic, PT::Piezo, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, true,
          StringMaterial::Nylon, StringGauge::ClassicalNormal, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::C414,
          0.32, 1.00, 1.00 },

        { "Flamenco", "Thin cypress body, spruce top, fast and percussive",
          GuitarCategory::Acoustic,
          6, false, kClassicalScale, 19, 2.4, false,
          BodyShape::Flamenco, Wood::SitkaSpruce, Wood::Maple, Wood::Maple, Wood::Mahogany,
          Bracing::FanBrace,
          1, { PT::InternalMic, PT::Piezo, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, true,
          StringMaterial::Fluorocarbon, StringGauge::ClassicalHard, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::C414,
          0.30, 1.00, 1.00 },

        { "12-String", "Six courses, the lower four tuned in octaves",
          GuitarCategory::Acoustic,
          12, true, kFenderScale, 20, 2.3, false,
          BodyShape::TwelveStringDread, Wood::SitkaSpruce, Wood::Rosewood, Wood::Rosewood, Wood::Mahogany,
          Bracing::XBrace,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, true,
          StringMaterial::PhosphorBronze, StringGauge::AcousticLight, TuningPreset::Standard,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::U87,
          0.24, 1.00, 1.00 },

        { "Resonator", "Spun metal cone, the classic blues and bluegrass bark",
          GuitarCategory::Acoustic,
          6, false, kResonatorScale, 19, 3.5, false,
          BodyShape::Resonator, Wood::Maple, Wood::Maple, Wood::Maple, Wood::Mahogany,
          Bracing::LadderBrace,
          1, { PT::Piezo, PT::InternalMic, PT::SingleCoil },
             { 0.0, 0.0, 0.0 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, true, false,
          StringMaterial::Bronze8020, StringGauge::AcousticMedium, TuningPreset::OpenG,
          BT::Fixed,
          AmpModel::AcousticDI, CabinetType::AcousticDI, SpeakerType::Greenback, MicType::SM57,
          0.20, 1.00, 1.00 },

        //======================================================================
        // BASS
        //======================================================================
        { "P-Style Bass", "Alder body, split-coil pickup, 34\" scale",
          GuitarCategory::Bass,
          4, false, kBassScale, 20, 2.2, false,
          BodyShape::BassSolid, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Maple,
          Bracing::SolidBodyNone,
          1, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.28, 0.28, 0.28 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::Flatwound, StringGauge::BassStandard, TuningPreset::BassStandard,
          BT::Fixed,
          AmpModel::AmpegSVT, CabinetType::Cab8x10Bass, SpeakerType::BassCeramic, MicType::D112,
          0.18, 0.16, 0.70 },

        { "J-Style Bass", "Alder body, two single-coils, slim neck",
          GuitarCategory::Bass,
          4, false, kBassScale, 20, 2.0, false,
          BodyShape::BassSolid, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.16, 0.32, 0.32 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::All, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::BassStandard, TuningPreset::BassStandard,
          BT::Fixed,
          AmpModel::AmpegSVT, CabinetType::Cab4x10Bass, SpeakerType::BassCeramic, MicType::D112,
          0.15, 0.15, 0.72 },

        { "Violin-Style Bass", "Maple through-neck, bright and cutting",
          GuitarCategory::Bass,
          4, false, kRickScale, 20, 2.1, false,
          BodyShape::BassHollow, Wood::Maple, Wood::Maple, Wood::Maple, Wood::Maple,
          Bracing::HollowParallel,
          2, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.14, 0.34, 0.34 },
             { MT::Alnico5, MT::Ceramic, MT::Alnico5 },
          PS::All, false, false,
          StringMaterial::StainlessSteel, StringGauge::BassStandard, TuningPreset::BassStandard,
          BT::Fixed,
          AmpModel::AmpegSVT, CabinetType::Cab4x10Bass, SpeakerType::BassCeramic, MicType::D112,
          0.12, 0.30, 0.80 },

        { "5-String Bass", "Extended range with a low B, 34\" scale",
          GuitarCategory::Bass,
          5, false, kBassScale, 24, 2.1, false,
          BodyShape::BassSolid, Wood::Ash, Wood::Ash, Wood::Ash, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::Humbucker, PT::SingleCoil },
             { 0.15, 0.31, 0.31 },
             { MT::Ceramic, MT::Ceramic, MT::Alnico5 },
          PS::All, false, false,
          StringMaterial::StainlessSteel, StringGauge::BassHeavy, TuningPreset::BassFiveString,
          BT::Fixed,
          AmpModel::AmpegSVT, CabinetType::Cab8x10Bass, SpeakerType::BassCeramic, MicType::D112,
          0.14, 0.15, 0.70 },

        { "Fretless Bass", "No frets: continuous pitch, vocal slides, mwah",
          GuitarCategory::Bass,
          4, false, kBassScale, 24, 1.4, true,
          BodyShape::BassSolid, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Ebony,
          Bracing::SolidBodyNone,
          2, { PT::SingleCoil, PT::SingleCoil, PT::SingleCoil },
             { 0.16, 0.32, 0.32 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::All, false, false,
          StringMaterial::Flatwound, StringGauge::BassStandard, TuningPreset::BassStandard,
          BT::Fixed,
          AmpModel::AmpegSVT, CabinetType::Cab1x15Bass, SpeakerType::BassCeramic, MicType::D112,
          0.20, 0.18, 0.75 },

        //======================================================================
        { "Custom", "Built from scratch: choose every wood, pickup and dimension",
          GuitarCategory::Custom,
          6, false, kFenderScale, 24, 1.6, false,
          BodyShape::SolidStandard, Wood::Alder, Wood::Alder, Wood::Alder, Wood::Maple,
          Bracing::SolidBodyNone,
          2, { PT::Humbucker, PT::SingleCoil, PT::Humbucker },
             { 0.13, 0.25, 0.39 },
             { MT::Alnico5, MT::Alnico5, MT::Alnico5 },
          PS::Bridge, false, false,
          StringMaterial::NickelPlatedSteel, StringGauge::Regular, TuningPreset::Standard,
          BT::VintageTrem,
          AmpModel::MarshallPlexi, CabinetType::Cab4x12, SpeakerType::Vintage30, MicType::SM57,
          0.16, 0.22, 0.85 }
    };
}

//==============================================================================
const GuitarSpec& GuitarLibrary::get (GuitarType t) noexcept
{
    return kGuitars[(size_t) juce::jlimit (0, (int) GuitarType::NumTypes - 1, (int) t)];
}

const char* GuitarLibrary::getName (GuitarType t) noexcept
{
    return get (t).name;
}

const char* GuitarLibrary::getCategoryName (GuitarCategory c) noexcept
{
    switch (c)
    {
        case GuitarCategory::Electric: return "Electric";
        case GuitarCategory::Acoustic: return "Acoustic";
        case GuitarCategory::Bass:     return "Bass";
        case GuitarCategory::Custom:   return "Custom";
        case GuitarCategory::NumCategories:
        default:                       return "Electric";
    }
}

//==============================================================================
BodyConfig GuitarLibrary::makeBodyConfig (const GuitarSpec& spec) noexcept
{
    BodyConfig cfg;

    cfg.shape = spec.bodyShape;
    cfg.topWood = spec.topWood;
    cfg.backWood = spec.backWood;
    cfg.sideWood = spec.sideWood;
    cfg.bracing = spec.bracing;
    cfg.scaleWidth = 1.0;
    cfg.scaleDepth = 1.0;
    cfg.topThicknessMm = 0.0;     // use the shape default
    cfg.soundHoleScale = 1.0;
    cfg.age = 0.3;
    cfg.resonanceTrim = 1.0;

    return cfg;
}

PickupSpec GuitarLibrary::makePickupSpec (const GuitarSpec& spec, int slot) noexcept
{
    const int s = juce::jlimit (0, PickupEngine::kMaxPickups - 1, slot);

    auto pickup = PickupSpec::makeDefault (spec.pickupTypes[s], spec.pickupPositions[s]);
    pickup.magnet = spec.pickupMagnets[s];

    // A bridge pickup is normally set closer to the strings than a neck pickup, to
    // even out the level between the two: the strings move less near the bridge.
    pickup.heightMm = (spec.pickupPositions[s] < 0.20) ? 2.0 : 2.8;

    return pickup;
}

double GuitarLibrary::twelveStringOctaveOffset (int stringIndex) noexcept
{
    // Courses run high to low: 0 = high E, 5 = low E. The top two courses are
    // tuned in unison; the lower four have the second string an octave up.
    if (! isOctaveString (stringIndex))
        return 0.0;

    const int course = courseForString (stringIndex);

    return (course >= 2) ? 12.0 : 0.0;
}

} // namespace luthier
