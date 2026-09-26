#pragma once

/*  Body woods, shapes and the modal profiles derived from them.

    The build spec asks for "a stored modal profile" per wood and for the modes to
    shift when the user changes the body dimensions. Storing 100 hand-written
    tables would satisfy the first half and fail the second, so the profiles are
    *computed* instead, from plate and Helmholtz theory:

      - the air resonance comes from the Helmholtz equation for the soundhole
        area, the body volume and the hole's effective length;
      - the top and back plate modes come from thin-plate theory, so they move
        correctly when the user changes thickness, bout width or wood;
      - bracing enters as a stiffness multiplier on the plate modes.

    A bigger virtual body therefore really does ring lower and bigger, which is
    the whole point of offering modal synthesis alongside convolution.
*/

#include "../../DSP/Common/DspCommon.h"
#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
enum class Wood
{
    SitkaSpruce, Cedar, Mahogany, Maple, Alder, Ash, Rosewood, Koa,
    Basswood, Korina, Poplar, Walnut, Ebony, Sapele, Agathis, Nato,
    NumWoods
};

enum class BodyShape
{
    // Acoustic
    Parlor, Concert, Auditorium, Dreadnought, Jumbo, Classical, Flamenco, Resonator,
    TwelveStringDread,
    // Electric
    SolidThin, SolidStandard, SolidHeavy, SemiHollow, Hollow, Offset, Chambered,
    // Bass
    BassSolid, BassHollow,
    NumShapes
};

enum class Bracing
{
    XBrace, ForwardShiftedX, FanBrace, LadderBrace, VBrace,
    SolidBodyNone, SemiHollowBlock, HollowParallel,
    NumBracings
};

enum class BodySize
{
    Small, Medium, Large, ExtraLarge, NumSizes
};

//==============================================================================
struct WoodProperties
{
    const char* name;
    double densityKgM3;
    double youngsModulusPa;   ///< Along the grain.
    double lossFactor;        ///< Internal damping; lower = higher Q, more ring.
    double brightnessTilt;    ///< dB of high-shelf character this wood imposes.
};

//==============================================================================
struct BodyShapeProperties
{
    const char* name;
    double lowerBoutMm;      ///< Width across the lower bout.
    double depthMm;          ///< Body depth at the tail block.
    double soundHoleMm;      ///< Diameter; 0 for a solid body.
    double topThicknessMm;
    double backThicknessMm;
    double volumeLitres;     ///< Enclosed air volume; 0 for a solid body.
    bool   acoustic;
};

//==============================================================================
/** One resonance of the body. */
struct BodyMode
{
    double frequencyHz = 100.0;
    double q           = 30.0;
    double gain        = 1.0;
};

/** Everything that defines one body, before it is turned into modes. */
struct BodyConfig
{
    BodyShape shape       = BodyShape::Dreadnought;
    Wood      topWood     = Wood::SitkaSpruce;
    Wood      backWood    = Wood::Rosewood;
    Wood      sideWood    = Wood::Rosewood;
    Bracing   bracing     = Bracing::XBrace;

    double scaleWidth     = 1.0;   ///< Multiplier on the lower bout.
    double scaleDepth     = 1.0;   ///< Multiplier on the body depth.
    double topThicknessMm = 0.0;   ///< 0 = use the shape default.
    double soundHoleScale = 1.0;   ///< Multiplier on the soundhole diameter.
    double age            = 0.3;   ///< 0 new, 1 vintage. Old wood damps less.
    double resonanceTrim  = 1.0;   ///< User multiplier on every mode frequency.

    // SPEC-SWEEP: part-acoustics.md 2.1 and 9, set by the Workshop mapping
    // (mapSpec). All neutral at their defaults, so a compiled guitar's body is
    // unchanged.
    double airHzOverride  = 0.0;   ///< PA-14: chambering's air mode, Hz; 0 = computed from the shape
    double airQOverride   = 0.0;   ///< PA-14: that mode's Q; 0 = the shape's
    double modeGainDb     = 0.0;   ///< PA-15: chambering's gain on the body modes, against solid
    double topDampingDb   = 0.0;   ///< PA-56: finish damping on the top modes (<= 0), Q x0.92 at -0.5 dB
};

//==============================================================================
class BodyModels
{
public:
    static const WoodProperties& getWood (Wood w) noexcept;
    static const BodyShapeProperties& getShape (BodyShape s) noexcept;

    static const char* getWoodName (Wood w) noexcept;
    static const char* getShapeName (BodyShape s) noexcept;
    static const char* getBracingName (Bracing b) noexcept;

    /** Stiffness multiplier a bracing pattern applies to the top plate modes. */
    static double getBracingStiffness (Bracing b) noexcept;

    /** The Helmholtz air resonance for a configuration, in Hz. Returns 0 for a
        solid body, which has no enclosed air. */
    static double computeAirResonance (const BodyConfig& cfg) noexcept;

    /** The fundamental top-plate mode, in Hz. */
    static double computeTopFundamental (const BodyConfig& cfg) noexcept;

    /** Builds the full modal profile. Appends to `dest`, which is cleared first.
        Produces between 30 and 48 modes depending on the body. */
    static void buildModes (const BodyConfig& cfg, std::vector<BodyMode>& dest);

    /** Maximum number of modes buildModes will ever produce. */
    static constexpr int kMaxModes = 48;

    /** Suggested body-IR filename for the convolution path. */
    static juce::String irFileNameFor (const BodyConfig& cfg);
};

} // namespace luthier
