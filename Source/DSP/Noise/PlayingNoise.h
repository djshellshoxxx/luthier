#pragma once

/*  The pick and the fretting hand as noise sources (pick-noise.md, string-squeak.md).

    NoiseEngine is the pool; this is what decides when a generator fires and
    what it sounds like. Every decision is a function of physical fields - pick
    material and thickness, winding pitch, hand speed - so the static builders
    below are what the tests check, and the trigger methods are thin wrappers
    that add the probability roll and hand the event to the pool.

    Fret buzz is sensed rather than triggered and lives in FretBuzz.h.
*/

#include "NoiseEngine.h"
#include "../String/Excitation.h"
#include "../../Model/Guitar/StringMaterials.h"

namespace luthier
{

//==============================================================================
/** The pick, in physical units (pick-noise.md 2). */
struct PickSettings
{
    Excitation::Material material = Excitation::Material::PickCelluloid;
    double thicknessMm = 0.73;
    double tipRadiusMm = 1.0;
    double bevel = 0.2;
    double wear = 0.1;
    double angleDegrees = 20.0;

    double clickAmount = 0.5;
    double chirpAmount = 0.4;
    double scrapeAmount = 0.25;

    bool fingers = false;
    double pluckPosition = 0.16;   ///< fraction of the string from the bridge
};

/** The fretting hand (string-squeak.md 3, 6, 7). */
struct SqueakSettings
{
    double amount = 0.25;
    double probability = 0.65;
    double moisture = 0.35;
    double pressure = 0.5;
    double minTravelFrets = 1.5;

    /** slide-guitar.md: a bottleneck is not a fingertip. */
    bool slideMode = false;
};

/** What the noise models need to know about one string. */
struct StringNoiseInfo
{
    bool wound = false;
    double windingPitchPerMm = 0.0;   ///< wraps per mm of the winding
    double windingDepth = 0.0;        ///< 0 plain, up to 1 for a heavy winding
    StringMaterial material = StringMaterial::NickelPlatedSteel;
    double ageRoughness = 1.0;        ///< 1 fresh, up to 1.4 old (string-squeak.md 10)

    static StringNoiseInfo fromSpec (const StringSpec& spec, StringMaterial material, StringAge age) noexcept;
};

//==============================================================================
class PlayingNoise
{
public:
    /*  A plucked note at full velocity peaks at roughly this in the string
        output's units; every noise level is set relative to it, so "30 dB
        below the note" means something. Measured, not assumed - the
        calibration test holds it within a few dB. */
    static constexpr double kNoteReference = 1.75;

    /*  The click enters the string's excitation input rather than its output
        (pick-noise.md 1.2), and the string passes only what lands on its own
        partials. This makes up for that, so the level formula still means
        "dB under the note" at the output. Measured by the calibration test. */
    static constexpr double kClickInjectionGainDb = 1.0;

    //==========================================================================
    // Pick properties (pick-noise.md 2.1). Materials the build's pick list does
    // not have (Ultex, Tortex, stone) have no row.
    struct PickMaterialProperties
    {
        double density;      ///< g/cm3
        double damping;
        double roughness;    ///< surface: chirp and scrape texture
        NoiseTexture texture;
        bool isPick;         ///< false for fingers, thumb, brush and slide
    };

    static PickMaterialProperties getPickMaterial (Excitation::Material m) noexcept;

    /** Squeak brightness and texture per winding (string-squeak.md 4). */
    static double windingBrightness (StringMaterial m) noexcept;
    static NoiseTexture windingTexture (StringMaterial m) noexcept;

    //==========================================================================
    /** The click for a picked note (pick-noise.md 3). Level 0 when nothing clicks. */
    static NoiseEvent makeClick (const PickSettings& pick, int stringIndex, double velocity) noexcept;

    /** The chirp as the pick leaves a wound string (4). Level 0 on a plain string. */
    static NoiseEvent makeChirp (const PickSettings& pick, const StringNoiseInfo& string,
                                 int stringIndex, double velocity) noexcept;

    /** Fingers: the soft fingertip release noise on a wound string (6). */
    static NoiseEvent makeFingertipNoise (const PickSettings& pick, const StringNoiseInfo& string,
                                          int stringIndex, double velocity) noexcept;

    /*  A finger shift along a string (string-squeak.md 3). Level 0 when the
        string is plain, the travel is under the minimum, or Slide Mode is on.
        The probability roll is not here - see onShift. */
    static NoiseEvent makeSqueak (const SqueakSettings& squeak, const StringNoiseInfo& string,
                                  int stringIndex, double travelMm, double seconds,
                                  double travelFrets) noexcept;

    /** Distance along the string between two fret positions, mm. */
    static double fretDistanceMm (double scaleLengthMm, double fromFret, double toFret) noexcept;

    //==========================================================================
    void prepare (double sampleRate);
    void reset() noexcept;

    NoiseEngine& getPool() noexcept { return pool; }
    const NoiseEngine& getPool() const noexcept { return pool; }

    void setPick (const PickSettings& p) noexcept { pick = p; }
    void setSqueak (const SqueakSettings& s) noexcept { squeak = s; }
    const PickSettings& getPick() const noexcept { return pick; }
    const SqueakSettings& getSqueak() const noexcept { return squeak; }

    void setSeed (juce::uint32 seed) noexcept { seed32 = seed; pool.setSeed (seed); }

    //==========================================================================
    /** A picked (or fingered) note starting. Audio thread. */
    void onPluck (int stringIndex, const StringNoiseInfo& string, double velocity) noexcept;

    /*  A fretted note moving along a still-sounding string. Returns true if a
        squeak was started. `shiftIndex` is the deterministic roll's index -
        pass a running count of shifts so a repeated performance repeats. */
    bool onShift (int stringIndex, const StringNoiseInfo& string, double scaleLengthMm,
                  double fromFret, double toFret, double seconds, juce::uint32 shiftIndex) noexcept;

    /*  A deliberate rake along the wound strings (pick-noise.md 5): one
        scrape crossing per wound string, spread over `seconds`, downward from
        the lowest string or upward from the highest. `wound` says which
        strings have a winding to scrape. Audio thread. */
    void startScrape (double seconds, bool downward, const bool* wound, int numStrings) noexcept;

    /** Runs the pool for one sample (and a scrape in progress). See NoiseEngine. */
    double processSample (double* excitationNoise, double* surfaceNoise, int numStrings) noexcept;

private:
    NoiseEngine pool;
    PickSettings pick;
    SqueakSettings squeak;
    juce::uint32 seed32 = 0x5eed1234u;
    double sr = 48000.0;

    // A rake in progress.
    struct Scrape
    {
        bool active = false;
        int crossings = 0, next = 0;
        std::array<int, 12> order {};
        double samplesPerCrossing = 0.0;
        double counter = 0.0;
    } scrape;
};

} // namespace luthier
