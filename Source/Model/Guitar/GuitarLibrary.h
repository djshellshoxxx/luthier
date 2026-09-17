#pragma once

/*  The factory instruments (build spec "Guitar types").

    Each entry is a complete instrument: body, woods, bracing, scale length, fret
    count, pickups and their positions, string set, tuning, bridge hardware, and
    the amp and cabinet the instrument is normally heard through. Loading a guitar
    type sets every one of those at once, which is what makes the type selector
    behave like picking up a different instrument rather than nudging a filter.
*/

#include "BodyModels.h"
#include "StringMaterials.h"
#include "../../DSP/Pickup/PickupEngine.h"
#include "../../DSP/Whammy/WhammyEngine.h"
#include "../../DSP/Amp/AmpEngine.h"
#include "../../DSP/Amp/CabinetEngine.h"
#include "../Playing/TuningEngine.h"

namespace luthier
{

//==============================================================================
enum class GuitarType
{
    // Electric
    Stratocaster, Telecaster, LesPaul, SG, ES335, Jazzmaster, Explorer,
    IbanezRG, SevenString, EightString, BaritoneElectric,

    // Acoustic
    Dreadnought, Auditorium, Jumbo, Parlor, Classical, Flamenco,
    TwelveString, Resonator,

    // Bass
    PrecisionBass, JazzBass, Rickenbacker, FiveStringBass, FretlessBass,

    Custom,
    NumTypes
};

enum class GuitarCategory { Electric, Acoustic, Bass, Custom, NumCategories };

//==============================================================================
struct GuitarSpec
{
    const char* name;
    const char* description;
    GuitarCategory category;

    int    numStrings;
    bool   twelveString;        ///< Six courses of paired strings.
    double scaleLengthMm;
    int    maxFrets;
    double fretActionMm;
    bool   fretless;

    BodyShape bodyShape;
    Wood      topWood;
    Wood      backWood;
    Wood      sideWood;
    Wood      neckWood;
    Bracing   bracing;

    int          numPickups;
    PickupType   pickupTypes[PickupEngine::kMaxPickups];
    double       pickupPositions[PickupEngine::kMaxPickups];
    MagnetType   pickupMagnets[PickupEngine::kMaxPickups];
    PickupSelector defaultSelector;
    bool         hasPiezo;
    bool         hasInternalMic;

    StringMaterial stringMaterial;
    StringGauge    stringGauge;
    TuningPreset   tuning;

    WhammyEngine::BridgeType bridge;

    AmpModel    defaultAmp;
    CabinetType defaultCabinet;
    SpeakerType defaultSpeaker;
    MicType     defaultMic;

    double defaultPluckPosition;   ///< Where the picking hand normally sits.
    double bodyAmount;             ///< How much body colour this instrument has.
    double couplingAmount;         ///< Sympathetic resonance strength.
};

//==============================================================================
class GuitarLibrary
{
public:
    static const GuitarSpec& get (GuitarType t) noexcept;
    static const char* getName (GuitarType t) noexcept;
    static const char* getCategoryName (GuitarCategory c) noexcept;

    static int getNumTypes() noexcept { return (int) GuitarType::NumTypes; }

    /** Builds the body configuration a guitar type implies. */
    static BodyConfig makeBodyConfig (const GuitarSpec& spec) noexcept;

    /** Builds one pickup's full electrical spec for a slot of a guitar type. */
    static PickupSpec makePickupSpec (const GuitarSpec& spec, int slot) noexcept;

    /** For a 12-string, which course a string index belongs to, and whether it is
        the octave string of that pair. */
    static int courseForString (int stringIndex) noexcept { return stringIndex / 2; }
    static bool isOctaveString (int stringIndex) noexcept { return (stringIndex % 2) == 1; }

    /** The octave offset applied to the second string of each course on a
        12-string. The top two courses are unison; the lower four are octaves. */
    static double twelveStringOctaveOffset (int stringIndex) noexcept;
};

} // namespace luthier
