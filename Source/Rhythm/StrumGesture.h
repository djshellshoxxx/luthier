#pragma once

/*  One strum as one gesture (strum-dynamics.md).

    A strum is not six notes at once: it is one object crossing the strings
    over 15-60 ms, accelerating as it goes, hitting each string with slightly
    different force and sometimes not hitting one at all. Crossing velocity,
    acceleration, direction, force and misses belong to the gesture; the
    per-string events are derived from it here (ground rule 1).

    Everything in this file is a pure function of its inputs plus a seed and a
    strum index, so a performance repeats exactly (character-wear.md 1) and the
    tests can check each rule on its own. Nothing allocates and nothing locks:
    `plan` runs on the audio thread for every strum the rhythm engine plays.

    What is not here:
      - where the crossing velocity comes from. That is the caller's choice
        (ambiguity-resolutions.md 6): RhythmEngine resolves step, pattern, kit
        and plugin-global; the interpreter uses the measured spread of a live
        chord or the global default.
      - the chuck's sound. A gesture only says how much of a chuck each strike
        is; LuthierEngine::triggerNote damps the string accordingly.
*/

#include "../DSP/Common/DspCommon.h"
#include "../DSP/String/Excitation.h"

namespace luthier
{

//==============================================================================
/** The object doing the crossing (strum-dynamics 5). Saved as a choice index,
    so the order is fixed; append only. */
enum class Striker
{
    pick = 0,
    thumb,
    fingernails,
    flesh,
    thumbpick,
    brush,
    numStrikers
};

const char* getStrikerName (Striker striker) noexcept;

/** The choice parameters' item list. Message thread: it builds a StringArray. */
juce::StringArray getStrikerNames();

/** 5: the striker's crossing speed relative to a pick (thumb x0.6 ... brush x0.4). */
double getStrikerCrossingFactor (Striker striker) noexcept;

/*  5: "the striker also selects which pick-noise.md generator fires". The
    excitation material the striker hits with, as an Excitation::Material
    index, or -1 for the pick: the pick is whatever the player has set up in
    the PICK group, so the gesture leaves it alone. Every other striker is a
    hand, and PlayingNoise gives a hand no pick click. */
int getStrikerMaterial (Striker striker) noexcept;

//==============================================================================
/** The STRUM group's settings (strum-dynamics 7), in their natural units. */
struct StrumSettings
{
    double crossingSps = 200.0;       ///< 1: strings per second, 20 - 800
    double acceleration = 0.35;       ///< 2: 0 even spacing, 1 a strong ease
    double upVelocityRatio = 1.25;    ///< 2.1: how much faster the up-stroke is
    double tilt = 0.15;               ///< 3: toward the strings struck later on a down-strum
    double evenness = 0.75;           ///< 3: 1 every string equal, 0 +-40 %
    double missProbability = 0.04;    ///< 3.1: mean chance a string is not struck
    Striker strikerDown = Striker::pick;
    Striker strikerUp = Striker::pick;
    double chuckAmount = 0.0;         ///< 6.1: 0 a normal strum, 1 a full chuck
    double chuckDamping = 0.92;       ///< 6.1: how hard the fretting hand damps

    /** 4: the guitar column. */
    static StrumSettings guitarDefaults() noexcept;

    /** 4: the bass column. The strings are further apart and there are fewer. */
    static StrumSettings bassDefaults() noexcept;

    static StrumSettings defaultsFor (bool bassFamily) noexcept
    {
        return bassFamily ? bassDefaults() : guitarDefaults();
    }

    /*  bass-techniques.md 8: "these are defaults, not constraints". Moving
        between a guitar and a bass moves every field still sitting on the old
        family's default to the new family's, and leaves every field the user
        has changed alone. */
    static StrumSettings retargetDefaults (const StrumSettings& current,
                                           bool fromBass, bool toBass) noexcept;

    /** Every field clamped to its strum-dynamics 7 range. */
    StrumSettings clamped() const noexcept;
};

//==============================================================================
/*  strum-dynamics 6.3: Easy mode's Feel knob, 0..1, scales crossing velocity
    and evenness together. At 0.5 it is section 4's defaults; across its range
    it maps crossing velocity 60 -> 200 -> 400 sps and evenness
    0.45 -> 0.75 -> 0.95. Three points that do not lie on one line, so the map
    is two straight segments meeting at the default. */
namespace StrumFeel
{
    double crossingSpsFor (double feel) noexcept;
    double evennessFor (double feel) noexcept;

    /** The same map as a factor on whatever source is in charge, 1 at 0.5. */
    inline double crossingScaleFor (double feel) noexcept { return crossingSpsFor (feel) / 200.0; }
    inline double evennessScaleFor (double feel) noexcept { return evennessFor (feel) / 0.75; }
}

//==============================================================================
/** One string's part in a gesture. */
struct StrumStrike
{
    int stringIndex = 0;
    double timeSeconds = 0.0;   ///< from the moment the gesture meets its first string
    double force = 1.0;         ///< multiplies the gesture's base velocity
    bool missed = false;        ///< 3.1: the hand crossed it and did not strike it
};

/** What the caller knows about the strum it wants planned. */
struct StrumRequest
{
    /** Engine string indices in the order they are crossed: the first entry is
        struck first. (Index 0 is the high E, so a down-strum lists the highest
        indices first.) */
    const int* strings = nullptr;
    int numStrings = 0;

    bool down = true;

    /** The crossing velocity from the caller's source, before direction and
        striker are applied (ambiguity-resolutions 6). */
    double sourceSps = 200.0;

    /** Which strum this is. With the seed, it fixes evenness and misses. */
    juce::uint32 strumIndex = 0;

    /*  Scales 3.1's miss chance. The rhythm engine passes its humanise amount,
        so a humanise amount of 0 is a machine that never misses; 0 turns
        misses off entirely, which the live interpreter uses, because a key the
        player pressed should sound. */
    double missScale = 1.0;
};

//==============================================================================
class StrumGesture
{
public:
    void setSeed (juce::uint64 newSeed) noexcept { seed = newSeed; }
    juce::uint64 getSeed() const noexcept { return seed; }

    /*  Plans one strum. Writes one strike per requested string into `out`, in
        crossing order, and returns how many it wrote (never more than
        `maxOut`). Missed strings are written too, flagged, so a caller can
        see the hand crossed them; their time is where they would have sounded.
        Forces are scaled so the strongest string before evenness is 1 on a
        down-strum and kUpStrokeForce on an up-strum.
        Audio thread: no allocation, no locks. */
    int plan (const StrumSettings& settings, const StrumRequest& request,
              StrumStrike* out, int maxOut) const noexcept;

    //==========================================================================
    // The rules, one at a time, for the tests and for the callers that only
    // need one of them.

    /** 1 and 2.1: strings per second after direction and striker. */
    static double effectiveCrossingSps (const StrumSettings& settings,
                                        double sourceSps, bool down) noexcept;

    /** 1: a crossing of `numStrings` at `sps` takes (n - 1) / sps seconds; a
        medium strum crosses six strings in 25-30 ms. */
    static double crossingSeconds (int numStrings, double sps) noexcept;

    /*  2: where in the crossing string u (0 first, 1 last) is struck, as a
        fraction of the total time.

        The spec writes this as a smoothstep blend applied to u directly. Read
        literally that bunches the outer strings together, the opposite of
        what its own prose and test ask for ("at a = 1 the first and last gaps
        are roughly twice the middle ones"). The hand's *position* is what
        follows the smoothstep in time - slow away, fast through the middle,
        easing at the end - so the strike time is the smoothstep's inverse,
        blended the same way. At a = 1 on six strings the first gap is 2.1x
        the middle one. See DECISIONS "strum acceleration". */
    static double ease (double u, double acceleration) noexcept;

    /** 3: the linear force ramp across the strings in crossing order. The
        up-stroke's tilt is the down-stroke's mirrored (+0.15 down / -0.15 up),
        so a positive setting favours the treble either way. */
    static double tiltFactor (int order, int numStrings, double tilt, bool down) noexcept;

    /** 3: the random per-string variation, 1 at evenness 1 and within +-40 %
        at 0. Deterministic in seed, strum and string. */
    static double evennessFactor (juce::uint64 seed, juce::uint32 strumIndex,
                                  int stringIndex, double evenness) noexcept;

    /** 3: +1.5 dB where the hand puts its weight - the first two strings it
        crosses, which are the lowest two on a down-strum and the top two on
        an up-strum. */
    static double accentFactor (int order) noexcept;

    /*  3.1: a string's chance of being missed. The first string in the
        direction of travel is missed three times as often as the rest, and
        the rest are scaled so the mean over the strum is still `probability`. */
    static double missChance (int order, int numStrings, double probability) noexcept;

    /** 6.1: how much of a chuck a strike is, for NoteOnEvent::chuck. A chuck
        step is a full chuck; any other strum is chuck_amount of the way there. */
    static double chuckFor (const StrumSettings& settings, bool isChuckStep) noexcept;

    /** A uniform number in [0, 1) from the seed, strum, string and a salt that
        keeps evenness and misses independent of one another. */
    static double uniform (juce::uint64 seed, juce::uint32 strumIndex,
                           int stringIndex, juce::uint32 salt) noexcept;

    static constexpr double kAccentDb = 1.5;
    static constexpr double kMaxUnevenness = 0.40;
    static constexpr double kLeadingMissWeight = 3.0;

    /*  2.1: the up-stroke meets the strings at a shallower angle and is
        "softer". The spec gives no figure; -1.4 dB is gentler than the 0.78
        the interpreter used before this, which also carried the fall-off that
        tilt now does. See DECISIONS "strum acceleration". */
    static constexpr double kUpStrokeForce = 0.85;

private:
    juce::uint64 seed = 0x5712D0E5ull;
};

} // namespace luthier
