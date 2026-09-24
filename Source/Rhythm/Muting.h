#pragma once

/*  Muting as a rhythmic voice (muting-rhythm.md).

    A mute is not silence: a muted string struck still makes a sound, a thump
    of pick and hand plus a very short damped ring (ground rule 2). So a mute
    is always attached to a strike - a note-on carries its mute type, and the
    engine damps that string as the strike lands (4). The types, their
    damping, the rules that pick one for a strike, and the grid presets live
    here; nothing in this file touches audio.

    Where a strike's mute type comes from, in order (muting-rhythm 2, 3):
      1. the master mute mode, when muting is armed and the mode is not Off -
         "overrides all pattern mute types with this one";
      2. the pattern step's own mute_type (rhythm engine), or the live grid's
         step at the note's time (live play, armed, transport running);
      3. the chuka source: an open strum softer than 0.3 becomes a chuka
         (armed only, so an existing pattern plays as it always did);
      4. random mute humanise, which may flip open and palm mute.

    technique-cascade.md 3.5: muting is additive damping and never conflicts.
    A strike that is also a chuck keeps the chuck, which is already the
    hardest damping there is.

    Audio thread for everything but the name lookups; nothing allocates.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>

namespace luthier
{

//==============================================================================
/** muting-rhythm 1. Saved by id and as a choice index: append only. */
enum class MuteType
{
    open = 0,
    palmLight,
    palmHeavy,
    palmExtreme,
    ghost,
    chuka,
    fretMute,
    numTypes
};

/** The pattern file's id: "open", "palm_mute_light", ... (muting-rhythm 2). */
const char* getMuteTypeId (MuteType type) noexcept;

/** A name for people: "Open", "Palm Mute Light", ... */
const char* getMuteTypeName (MuteType type) noexcept;

/** Two letters for a grid cell. Empty for open. */
const char* getMuteTypeGlyph (MuteType type) noexcept;

/** Unknown ids are open: a newer file's type plays unmuted rather than failing. */
MuteType muteTypeFromId (const juce::String& id) noexcept;

inline bool isPalmMute (MuteType t) noexcept
{
    return t == MuteType::palmLight || t == MuteType::palmHeavy || t == MuteType::palmExtreme;
}

//==============================================================================
/** One grid step's mute (muting-rhythm 1: "each type has a position and
    pressure default; the user can override per pattern step"). */
struct MuteStep
{
    MuteType type = MuteType::open;
    double pressure = -1.0;       ///< 0-1, or -1 for the type's default
    double positionMm = -1.0;     ///< palm distance from the bridge, or -1 for the default

    bool isOpen() const noexcept { return type == MuteType::open; }
};

//==============================================================================
enum class FrettingMuteStyle { rockSpread = 0, classicalFingertip, numStyles };
enum class ChukaSource { patternOnly = 0, softStrums, numSources };

/** muting-rhythm 3's controls, in their natural units. */
struct MuteSettings
{
    bool armed = false;
    int masterMode = -1;                       ///< -1 Off, else a MuteType
    double palmPositionMm = 35.0;
    double palmPressure = 0.5;
    FrettingMuteStyle frettingStyle = FrettingMuteStyle::rockSpread;
    ChukaSource chukaSource = ChukaSource::softStrums;
    double humanise = 0.0;                     ///< chance a step flips open <-> palm mute
    double ghostVelocity = 0.4;                ///< ghost level relative to an open note

    /** 3: "Default: any strum with dynamics < 0.3". */
    static constexpr double kChukaDynamic = 0.3;

    static constexpr double kMinPositionMm = 5.0;
    static constexpr double kMaxPositionMm = 100.0;
};

/** The master mode's choice list: Off, then every type. Message thread. */
juce::StringArray getMuteMasterModeNames();

//==============================================================================
/** What a mute does to the string it lands on. */
struct MuteDamping
{
    double t60Seconds = 0.0;          ///< 0: the strike is not damped
    double cutoffHz = 0.0;            ///< the loop's brightness under the hand
    double velocityScale = 1.0;       ///< ghosts are quieter than open notes (3)
    double releaseAfterSeconds = -1.0;///< fret mute: the finger lets go this long after the strike
    double releaseT60Seconds = 0.0;   ///< ... and the note stops this fast
    bool chuck = false;               ///< chuka: played as strum-dynamics 6.1's chuck

    bool dampsAtStrike() const noexcept { return t60Seconds > 0.0; }
};

//==============================================================================
namespace Muting
{
    /*  1: T60 on a low E at the default palm (35 mm, pressure 0.5). Light
        ~150 ms, heavy ~50 ms, extreme ~20 ms; a ghost has no pitch at all. */
    inline constexpr double kPalmLightT60   = 0.150;
    inline constexpr double kPalmHeavyT60   = 0.050;
    inline constexpr double kPalmExtremeT60 = 0.020;
    inline constexpr double kGhostT60       = 0.012;

    /*  1: a fret mute is "a short pitched sound then silence". The spec gives
        no figures; 35 ms of ring (three periods of a low E, still a pitch)
        and a 10 ms stop is a classical staccato at any tempo, and short
        enough that the room's tail of the note is gone with it (MutingTests). */
    inline constexpr double kFretMuteRingSeconds = 0.035;
    inline constexpr double kFretMuteStopT60     = 0.010;

    /*  How a mute type sounds with this palm. Pressure and position change it
        the way the hand does: pressing harder or resting further from the
        saddle (over more of the vibrating length) both stop the string
        faster and darker. At pressure 0.5 and 35 mm the figures are 1's. */
    MuteDamping dampingFor (MuteType type, const MuteSettings& settings,
                            double pressureOverride = -1.0,
                            double positionOverrideMm = -1.0) noexcept;

    /** The factor pressure and position put on a palm's T60, 1 at the defaults. */
    double palmFactor (double pressure, double positionMm) noexcept;

    /*  The mute a strike gets (the order in this file's header). `step` is the
        grid's own; `isStrum` and `dynamic` feed the chuka source; `loop` and
        `stepIndex` make humanise deterministic per seed. */
    MuteStep resolve (const MuteStep& step, const MuteSettings& settings,
                      bool isStrum, double dynamic,
                      juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept;

    /*  3: "probability that a step's mute type shifts one step (open <-> palm
        mute)". Open becomes a light palm mute; a palm mute of any weight
        becomes open. Ghost, chuka and fret mute are deliberate and never move. */
    MuteType humanise (MuteType type, double probability,
                       juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept;

    /** A uniform number in [0, 1) from the inputs; stateless. */
    double uniform (juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept;
}

//==============================================================================
/** muting-rhythm 6: the mute-grid presets, one letter per sixteenth. */
struct MuteGridPreset
{
    const char* name;
    const char* cells;   ///< 16 of: o open, l h x palm light/heavy/extreme, g ghost, c chuka, f fret mute
};

int getNumMuteGridPresets() noexcept;
const MuteGridPreset& getMuteGridPreset (int index) noexcept;

/** A preset letter's type; anything unknown is open. */
MuteType muteTypeFromLetter (char letter) noexcept;

/** The live grid's length: 16 sixteenths, one bar of 4/4 (muting-rhythm 2). */
inline constexpr int kLiveMuteSteps = 16;

} // namespace luthier
