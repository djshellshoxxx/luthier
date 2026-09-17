#pragma once

/*  String materials, gauges and age (build spec "String types").

    This is where a user-facing choice ("phosphor bronze, medium, broken in")
    becomes the physical numbers StringEngine needs: linear mass density, tension,
    inharmonicity, sustain and contact brightness.

    Nothing here is a lookup of a "sound". Every output is computed from the
    material's density and Young's modulus, the wire diameter and the scale
    length, using the same equations a string manufacturer uses.
*/

#include "../../DSP/String/StringEngine.h"
#include <array>

namespace luthier
{

//==============================================================================
enum class StringMaterial
{
    NickelPlatedSteel,
    PureNickel,
    StainlessSteel,
    Cobalt,
    PhosphorBronze,
    Bronze8020,
    SilkAndSteel,
    Nylon,
    Fluorocarbon,
    Flatwound,
    Halfwound,
    Coated,
    NumMaterials
};

enum class StringGauge
{
    ExtraLight,    ///< .008 - .038
    Light,         ///< .009 - .042
    Regular,       ///< .010 - .046
    Medium,        ///< .011 - .048
    Heavy,         ///< .012 - .052
    AcousticLight, ///< .012 - .053
    AcousticMedium,///< .013 - .056
    ClassicalNormal,
    ClassicalHard,
    BassStandard,  ///< .045 - .105
    BassHeavy,     ///< .050 - .110
    Custom,
    NumGauges
};

enum class StringAge
{
    Fresh,
    BrokenIn,
    Old,
    NumAges
};

//==============================================================================
/** Everything the physics needs about one material. */
struct MaterialProperties
{
    const char* name;
    double densityKgM3;      ///< Bulk density of the wire.
    double woundDensityKgM3; ///< Density of a WOUND string of this type. For nylon
                             ///< and fluorocarbon the winding is silver-plated
                             ///< copper over a floss core, which is nothing like
                             ///< the density of the plain trebles.
    double youngsModulusPa;  ///< Stiffness, drives inharmonicity.
    double woundMassFactor;  ///< Effective density of a wound string vs a solid one.
    double coreRatio;        ///< Core diameter / outer diameter, for wound strings.
    double sustainSeconds;   ///< Reference open-string T60 on a 648 mm scale.
    double brightnessHz;     ///< Reference loop-filter cutoff when fresh.
    double squeakFactor;     ///< How much finger-slide noise the winding produces.
    bool   alwaysWound;      ///< Flatwound/halfwound sets wind every string.
    bool   plainTrebles;     ///< Nylon sets use plain trebles and wound basses.
};

//==============================================================================
/** The physical result of "this material, this gauge, this age, this string". */
struct StringSpec
{
    double diameterInches   = 0.046;
    double diameterMm       = 1.168;
    double coreDiameterMm   = 0.52;
    double linearDensity    = 0.0063;   ///< kg/m
    double tensionNewtons   = 76.0;
    double inharmonicityB   = 0.00015;
    double sustainSeconds   = 4.5;
    double brightnessHz     = 5000.0;
    double squeak           = 1.0;
    double ageDetuneCents   = 0.0;
    bool   wound            = false;
};

//==============================================================================
class StringMaterials
{
public:
    static const MaterialProperties& get (StringMaterial m) noexcept;
    static const char* getMaterialName (StringMaterial m) noexcept;
    static const char* getGaugeName (StringGauge g) noexcept;
    static const char* getAgeName (StringAge a) noexcept;

    /** Number of strings a gauge set is defined for. */
    static int getGaugeStringCount (StringGauge g) noexcept;

    /** Wire diameter in inches for one string of a gauge set. */
    static double getGaugeDiameterInches (StringGauge g, int stringIndex) noexcept;

    /** Computes everything StringEngine needs.

        @param material     wire material
        @param gauge        gauge set (ignored if diameterInchesOverride > 0)
        @param age          fresh / broken in / old
        @param stringIndex  0 = highest string
        @param targetHz     the pitch this string is tuned to
        @param scaleLengthMm nut-to-bridge distance
        @param diameterInchesOverride  per-string custom gauge, or 0 to use the set
    */
    static StringSpec computeSpec (StringMaterial material,
                                   StringGauge gauge,
                                   StringAge age,
                                   int stringIndex,
                                   double targetHz,
                                   double scaleLengthMm,
                                   double diameterInchesOverride = 0.0) noexcept;

    /** Fills a StringEngine::Physical from a StringSpec. */
    static StringEngine::Physical toPhysical (const StringSpec& spec, double scaleLengthMm) noexcept;

    //==========================================================================
    /** Identity rule 1: a string must sit in a playable tension range.

        The engine spec quotes 30 to 90 N, which is the figure for an ELECTRIC
        guitar. It is not universal: a medium acoustic set reaches about 130 N on
        the low E, and a bass string normally runs 130 to 270 N - a bass neck is
        built for it. Applying the electric figure to every instrument would flag
        every bass in the factory bank as unplayable, which would be wrong.

        So the range scales with the instrument, keyed on scale length. */
    struct TensionRange
    {
        double comfortableMin;
        double comfortableMax;
        double absoluteMin;
        double absoluteMax;
    };

    /** Scale lengths at or above this belong to bass instruments. */
    static constexpr double kBassScaleThresholdMm = 760.0;

    static TensionRange getTensionRange (double scaleLengthMm) noexcept
    {
        if (scaleLengthMm >= kBassScaleThresholdMm)
            return { 110.0, 300.0, 40.0, 450.0 };

        // The upper figure is set by a medium acoustic set, whose middle
        // strings genuinely reach the mid 140s of newtons.
        return { 25.0, 165.0, 8.0, 240.0 };
    }

    static bool isTensionPlayable (double newtons, double scaleLengthMm = 648.0) noexcept
    {
        const auto range = getTensionRange (scaleLengthMm);
        return newtons >= range.comfortableMin && newtons <= range.comfortableMax;
    }

    // Widest limits across every instrument, for the validator's hard clamp.
    static constexpr double kMinPlayableTension = 25.0;
    static constexpr double kMaxPlayableTension = 165.0;
    static constexpr double kAbsoluteMinTension = 8.0;
    static constexpr double kAbsoluteMaxTension = 450.0;

    /** Suggests a gauge whose tension lands in range for a given pitch and scale. */
    static double suggestDiameterInches (StringMaterial material,
                                         double targetHz,
                                         double scaleLengthMm,
                                         bool wound) noexcept;
};

} // namespace luthier
