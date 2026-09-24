#pragma once

/*  From parts to engine numbers (part-acoustics.md).

    `mapSpec` is the one function where a physical field becomes a DSP value
    (0.1). It is evaluated once per guitar change, on the message thread, and
    its result is what the engine consumes; nothing on the audio path reads a
    part field.

    The result is expressed in the engine's own vocabulary where one exists -
    a GuitarSpec (the compiled description, guitar-workshop.md 3.1), a
    BodyConfig, PickupSpecs, circuit components, setup geometry - so every
    existing engine path keeps working and the parts model re-points it, which
    is the "widening, not replacement" 3.1 asks for. Values the engine had no
    slot for (magnet pull, pickup covers, termination brightness, the
    composed coupling) are carried beside them.

    Every constant below says where it came from (0.5).
*/

#include "PartLibrary.h"
#include "../Guitar/GuitarLibrary.h"
#include "../../DSP/Circuit/GuitarCircuit.h"
#include "../../DSP/Noise/FretBuzz.h"

namespace luthier
{

//==============================================================================
/** part-acoustics.md 1: one row of the wood table. */
struct WoodData
{
    double densityKgM3, youngsGPa, lossTangent;   ///< tan(delta), not x 1e-3
};

bool lookUpWood (const juce::String& woodId, WoodData& out);

/** part-acoustics.md 6.2. */
struct MagnetData { double pull, damping; };
MagnetData lookUpMagnet (const juce::String& magnetId);

//==============================================================================
struct PickupDerived
{
    PickupSpec spec;                 ///< the engine's pickup, electrically from the part
    double magnetPull = 0.8;         ///< 6.2
    double magnetDamping = 0.032;
    double coverLossDbAt4k = 0.0;    ///< 6: nickel -0.8 dB
    double poleBrightness = 1.0;     ///< 6: steel dulls, ceramic brightest
    double heightMm = 2.5;           ///< mean of treble and bass
    double positionMm = 100.0;       ///< from the saddle
    bool isPiezo = false;
};

struct DerivedAcoustics
{
    /** The compiled-shape description the existing engine paths consume. */
    GuitarSpec spec {};

    int numStrings = 6;
    int stringExcess = 0;
    std::array<double, 12> gaugesIn {};   ///< per string, 0 = the set's default
    StringMaterial stringMaterial = StringMaterial::NickelPlatedSteel;

    /** workshop-ui.md 3.3: per string, -1 = the set's; else a StringMaterial / 0 plain, 1 wound. */
    std::array<int, 12> stringMaterialOverride { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<int, 12> stringWoundOverride { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    BodyConfig body;

    /** Pickups in the engine's slot order: bridge first. */
    std::array<PickupDerived, 3> pickups {};
    int numPickups = 0;
    bool hasPiezo = false;

    // --- composed values (part-acoustics.md 10) --------------------------------
    double couplingFraction = 0.5;       ///< neck joint x bridge coupling
    double terminationMassG = 100.0;     ///< bridge + tailpiece + pickguard
    double sustainScale = 1.0;           ///< from termination mass and chambering
    double fretBrightness = 0.70;        ///< 4: fret material
    double nutBrightness = 0.75;         ///< 4: open strings only
    double bodyGainDb = 0.0;             ///< 2.1: chambering's modes gain
    double airResonanceHz = 0.0;         ///< 2.1: 0 for solid
    double airResonanceQ = 0.0;
    double feedbackGain = 0.0;           ///< 2.1: into ambiguity-resolutions 1's loop
    double finishDampingDb = 0.0;        ///< 9: thick gloss on an acoustic top

    std::array<double, 12> tensionNewtons {};   ///< 3 and 8, for the tests and the UI
    std::array<double, 12> windingPitchPerMm {};

    CircuitComponents wiring;    ///< 7: the circuit's controls from the wiring part
    SetupGeometry setup;         ///< fret-buzz.md geometry from setup, frets and nut

    bool operator== (const DerivedAcoustics&) const;
};

/** part-acoustics.md 0.1: the whole mapping. Deterministic and allocation-light. */
DerivedAcoustics mapSpec (const WorkshopGuitar& guitar);

/** 3: T = (2 L f)^2 mu, in newtons, for one string. */
double stringTensionNewtons (double scaleLengthMm, double frequencyHz, double linearDensityKgPerM);

/** 8: mass per length of a string from its gauge and whether it is wound. */
double stringLinearDensity (double gaugeInches, bool wound, StringMaterial material);

/** 6.1: the frequency of the first comb null for a pickup at `positionMm`. */
double firstCombNullHz (double positionMm, double scaleLengthMm, double openHz);

} // namespace luthier
