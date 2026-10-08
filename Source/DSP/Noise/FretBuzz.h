#pragma once

/*  Setup geometry and sensed fret buzz (fret-buzz.md).

    The guitar acquires a setup: action at the 12th fret on each side, neck
    relief, nut slot depth per string, fret height. From that, the clearance
    between each string and each fret ahead of the finger, and from the
    string's modelled amplitude at that fret, whether it is touching.

    Buzz is sensed, never scheduled (0.1): each block, for each sounding
    string, the fret with the largest `amplitude - clearance` is found, and a
    positive excess drives a buzz generator in the shared NoiseEngine pool.
    As the note decays the excess goes negative and the buzz stops, which is
    the "rattles on the attack, cleans up as it rings" behaviour without any
    special case.
*/

#include "NoiseEngine.h"
#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** The tech's measurements, in mm (fret-buzz.md 1). */
struct SetupGeometry
{
    static constexpr int kMaxStrings = 12;
    static constexpr int kMaxFrets = 24;

    double actionTreble = 1.6;     ///< string to fret top at fret 12, highest string
    double actionBass = 2.0;       ///< same, lowest string
    double relief = 0.20;          ///< bow at fret 7; negative is back-bow
    std::array<double, kMaxStrings> nutDepth {};   ///< open-string clearance over fret 1
    double fretHeight = 1.0;
    double buzzThreshold = 0.35;   ///< sensitivity trim, 3.2
    bool sitarMode = false;

    /*  SPEC-SWEEP FB-21 (fret-buzz.md 8, character-wear.md 3): how far each
        fret's crown has worn below a new one, mm. A worn fret is lower, so the
        string clears it more easily - but a note fretted on it starts lower
        too, so the frets ahead of it come closer. Wear moves buzz around
        rather than removing it. Index 0 is the nut and is ignored. */
    std::array<double, kMaxFrets + 1> fretWearMm {};

    double scaleLengthMm = 648.0;
    int numStrings = 6;
    int numFrets = 22;

    SetupGeometry() { nutDepth.fill (0.45); }

    /** Distance from the nut to fret n, mm. */
    double fretPositionMm (double fret) const noexcept;

    /** The 12th-fret action for a string, interpolated from treble (string 0) to bass. */
    double actionFor (int stringIndex) const noexcept;

    /*  The gap between string and the top of fret `fret`, with the string
        held at `frettedAt` (0 = open). Only frets past the fretted one can be
        touched; for any other this returns a large number. */
    double clearanceMm (int stringIndex, double frettedAt, int fret) const noexcept;
};

/** fret-buzz.md 6.1's setup styles, in its order. */
struct SetupStyle
{
    const char* name;
    double actionTreble, actionBass, relief;
};

const SetupStyle& getSetupStyle (int index) noexcept;
inline constexpr int kNumSetupStyles = 6;
inline constexpr int kDefaultSetupStyle = 1;   ///< Player-friendly, onboarding.md 1

//==============================================================================
class FretBuzz
{
public:
    /*  How many mm of string displacement one unit of the string engine's
        level follower is. The string model is not calibrated in millimetres
        (3.2 says as much). Set from the six styles in 6.1: at 2.4 a hard
        (velocity 127) open low E swings about 1.8 mm, so "Needs a tech" buzzes
        at velocity 100, "Player-friendly" only when attacked hard, and
        "Clean / high" never. The engine tests hold all three. */
    static constexpr double kMmPerLevelUnit = 2.4;

    void setGeometry (const SetupGeometry& g) noexcept { geometry = g; }
    const SetupGeometry& getGeometry() const noexcept { return geometry; }

    /*  The block-rate test (3.1). `level` is the string's current amplitude
        in its level follower's units; `pluckPosition` the fraction of the
        vibrating length from the bridge, which sets the mode mix. Returns the
        worst fret and its excess in mm (negative when clear). */
    struct Contact { int fret = -1; double excessMm = -1.0e9; };

    Contact sense (int stringIndex, double frettedAt, double level, double pluckPosition) const noexcept;

    /** The string's displacement at a point `u` along its vibrating length (0 nut side, 1 bridge). */
    static double displacementMm (double level, double u, double pluckPosition) noexcept;

    /*  Runs sense() for every string and starts, updates or releases each
        string's buzz generator in `pool`. `levels`, `fretted` and
        `fundamentalHz` are per string. Audio thread, once a block. */
    /*  CW-12, character-wear.md 3: a worn fret sits lower than its neighbour, so
        the string grazes it more easily. `wornMultiplier`, one per string
        (1 = fresh, higher = more worn - CharacterEngine::getFretBuzzMultiplier),
        nudges that string's contact closer to buzzing; null skips the bias. */
    void process (NoiseEngine& pool, const double* levels, const double* fretted,
                  const double* fundamentalHz, int numStrings, double pluckPosition,
                  const double* wearMultiplier = nullptr) noexcept;

    /*  SPEC-SWEEP: CW-12 - character-wear 3: a worn fret sits low, so the
        string stopped on it is this much closer to the fret in front, per unit
        of CharacterEngine::getFretBuzzMultiplier above 1. */
    static constexpr double kWearClearanceMm = 0.1;

    void reset() noexcept;

    /*  SPEC-SWEEP FB-26 (fret-buzz.md 8): a bend pushes the string across the
        frets and lifts it slightly: more clearance just past the finger, less
        further up. Set each block from the string's bend. Audio thread. */
    void setBendCents (int stringIndex, double cents) noexcept
    {
        if (juce::isPositiveAndBelow (stringIndex, SetupGeometry::kMaxStrings))
            bendCents[(size_t) stringIndex] = cents;
    }

    /** The clearance with the bend's lift, mm. */
    double clearanceFor (int stringIndex, double frettedAt, int fret) const noexcept;

    //==========================================================================
    /*  The heatmap's source (6.2): the last block's excess for each string at
        each fret, in mm. Written on the audio thread, read by the UI; each cell
        is an atomic so a torn read cannot happen. */
    float getHeat (int stringIndex, int fret) const noexcept;

    /** The fret each string's generator was last triggered for, -1 if none. */
    int getBuzzingFret (int stringIndex) const noexcept;

    /** The level a contact of `excessMm` at `fret` produces (4). For tests. */
    double levelFor (double excessMm) const noexcept;

private:
    SetupGeometry geometry;
    std::array<double, SetupGeometry::kMaxStrings> bendCents {};   // SPEC-SWEEP FB-26

    std::array<int, SetupGeometry::kMaxStrings> generatorIndex {};
    std::array<std::atomic<int>, SetupGeometry::kMaxStrings> buzzingFret {};
    std::array<std::array<std::atomic<float>, SetupGeometry::kMaxFrets + 1>, SetupGeometry::kMaxStrings> heat {};

public:
    FretBuzz();
};

} // namespace luthier
