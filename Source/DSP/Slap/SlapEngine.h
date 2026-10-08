#pragma once

/*  Slap (string-slap-technique.md, and the slap, pop, double thump and ghost
    mechanics of bass-techniques.md 2-5).

    A slap is a strike, not a loud pluck (string-slap 0.2, bass-techniques 0.2).
    The thumb drives the string into the frets or the finger yanks it off them.
    That is two things at once, and both are modelled where they already live:

    - the string excitation: Excitation::Kind::Slap, with the steep front edge
      the excitation model already gives percussive kinds, placed at the
      contact point and shaped by the thumb or the fingernail;
    - the clack: the collision with the frets, handed to the fret-buzz
      generator in the NoiseEngine pool with a large excess
      (bass-techniques 2.1.3). Slap's clack comes from the same code as a bad
      setup's rattle, because physically it is the same event.

    This class decides what a note becomes (a thumb slap, a pop, a ghost, or
    nothing) and builds those two pieces; LuthierEngine applies them, because
    the string and the pool are its. What is not a note is timed here: a
    button strike, the double thump's up-stroke, a palm slap across muted
    strings, a body tap. Those wait in a small queue and land on their sample.

    A palm slap silences the strings under the hand and makes a broadband
    clack and thump; the strings are not excited, which is why it has no
    pitch. A body tap bypasses the strings entirely: it drives the body stage
    directly (string-slap 2). body-coupling.md is not on disk yet, so the tap
    drives the existing body response with a knock whose spectrum is weighted
    toward the part it lands on (top, side, back), rather than a mode bank.

    Triggers arrive as GestureEvents from TechniqueTriggers, the technique
    layer's shared MIDI front: a keyswitch or CC held is a modifier (the notes
    under it are slapped), the velocity zone and the MPE zone classify notes
    as they come, and keyswitches 16-18 are ghost, body tap and palm slap.

    Audio thread throughout. Nothing allocates after prepare(); nothing here is
    random, so a performance repeats exactly.
*/

#include "../../Model/Playing/TechniqueTriggers.h"
#include "../Noise/FretBuzz.h"
#include "../String/StringEngine.h"
#include <array>
#include <atomic>
#include <limits>

namespace luthier
{

//==============================================================================
/** 1, "Slap type". Saved as a choice index: append only. */
enum class SlapType
{
    thumb = 0,
    pop,
    palm,
    bodyTap,
    numTypes
};

/** 1, "Body tap resonance": which part of the body the hand lands on. */
enum class BodyPart
{
    top = 0,
    side,
    back,
    numParts
};

/** The factory slaps (4). */
enum class SlapPreset
{
    bassStandard = 0,
    bassAggressive,
    funkGuitarPalm,
    acousticBodyTap,
    percussiveFingerstyle,
    numPresets
};

//==============================================================================
/** Every slap control, in natural units. */
struct SlapSettings
{
    bool armed = false;
    SlapType type = SlapType::thumb;
    TriggerSource trigger = TriggerSource::keyswitch;
    int velocityZone = 100;          ///< velocity-zone trigger: MIDI velocity at or above this slaps
    int triggerCc = 86;              ///< free in engine.md 2's map (scrape has 85)
    int ghostCc = 87;                ///< 1: ghost mode's dedicated CC
    int zoneChannel = 15;            ///< the MPE zone's channel (scrape listens on 16)

    // bass-techniques.md 2.2-5: its fields, read as they are (string-slap 3).
    double slapStrength = 0.70;      ///< the thumb's contact force
    double slapPositionMm = 60.0;    ///< from the last fret toward the bridge
    double thumbHardness = 0.55;
    double fretContact = 0.80;
    double popStrength = 0.75;
    double popPositionMm = 40.0;
    bool doubleThump = false;        ///< string-slap 1's "Rebound"
    double upRatio = 0.65;
    double ghostLevel = 0.45;
    double ghostDamping = 0.94;
    bool ghostAuto = true;
    int ghostVelocityThreshold = 32;

    // string-slap-technique.md 1.
    double force = 0.6;              ///< contact force for a palm slap and a body tap
    double palmPositionMm = 100.0;   ///< from the last fret, like the others
    int stringMask = 0;              ///< bit per string, bit 0 the high E; 0 is the type's default
    bool ghostMode = false;
    double reboundGapMs = 60.0;
    double snapBack = 0.5;           ///< bass only: the pop's collision with the board
    BodyPart bodyPart = BodyPart::top;

    /** Section 4's factory settings, armed. */
    static SlapSettings fromPreset (SlapPreset preset, int numStrings, bool bassFamily) noexcept;
    static const char* getPresetName (SlapPreset preset) noexcept;

    /** What the technique layer's MIDI front listens for on the slap's behalf. */
    TechniqueTriggerConfig triggerConfig() const noexcept;
};

/** What a note, or a strike with no note, turns into. */
struct SlapStrike
{
    SlapType type = SlapType::thumb;
    bool strike = false;             ///< struck by the thumb or popped, rather than played as it came
    bool ghost = false;              ///< the fretting hand is resting on the string
    bool rebound = false;            ///< the double thump's up-stroke
    int stringIndex = 0;
    double velocity = 0.8;           ///< for the excitation, 0-1, ghost level included
    double force = 0.7;              ///< the contact force the strike was made with
    double contactMm = 60.0;         ///< contact point, mm from the last fret toward the bridge

    /** A palm slap or a body tap: the note is replaced by the hand, not played. */
    bool isPercussive() const noexcept { return strike && (type == SlapType::palm || type == SlapType::bodyTap); }
};

/** Something the slap has to do at a given sample. */
struct SlapAction
{
    enum class Kind { strike, palmSlap, bodyTap };

    Kind kind = Kind::strike;
    SlapStrike strike;               ///< Kind::strike
    int mask = 0;                    ///< Kind::palmSlap
    double force = 0.6;              ///< palm slap and body tap
    juce::int64 due = 0;
};

//==============================================================================
class SlapEngine
{
public:
    static constexpr int kMaxActions = 32;

    SlapEngine() = default;

    /** How much louder than an ordinary fret buzz the slap's clack is: it is
        the same contact, driven far harder (bass-techniques 2.1.3, 0.5). */
    static constexpr double kClackGainDb = 8.0;

    /** 7: "plain strings buzz less against frets". A plain string's clack is
        driven with this fraction of a wound one's excess. */
    static constexpr double kPlainStringContact = 0.35;

    /** Snap-back is a bass control; on a guitar the pop uses this. */
    static constexpr double kGuitarSnapBack = 0.5;

    /** A body tap's knock peaks at this, at force 1, before the body stage. */
    static constexpr double kBodyTapReference = 0.5;

    static constexpr double kBodyTapSeconds = 0.25;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSettings (const SlapSettings& s) noexcept;
    const SlapSettings& getSettings() const noexcept { return settings; }

    /** The instrument, each block: string count, scale, frets, and whether it is a bass. */
    void setInstrument (int numStrings, double scaleLengthMm, int numFrets, bool bassFamily) noexcept;
    bool isBassFamily() const noexcept { return bass; }

    //==========================================================================
    /*  3's interface. Reads this block's slap GestureEvents from the front
        and queues what they fire, at blockStart + their offset. */
    void processBlock (int numSamples, juce::int64 blockStartSample, const TechniqueTriggers& triggers) noexcept;

    /** 3: a strike with no note, at a given sample (the button, the tests). */
    void trigger (const SlapStrike& strike, juce::int64 atSample) noexcept;

    /*  What a note-on becomes. `underBar` is whether the slide bar holds the
        string (technique-cascade.md 3.4: the bar blocks the thumb). A result
        with neither `strike` nor `ghost` set means "play it as it came". */
    SlapStrike classify (const NoteOnEvent& e, bool underBar) const noexcept;

    /** A strike on a string at its current pitch, as the button fires it. */
    SlapStrike strikeFor (int stringIndex, double dynamics) const noexcept;

    /*  After the engine has played a strike: queues the double thump's
        up-stroke if it is on. `atSample` is the strike's absolute sample. */
    void noteStruck (const SlapStrike& strike, juce::int64 atSample) noexcept;

    /*  technique-cascade.md 2-3: another technique took the string - a scrape
        or a tap started, or a new strike landed. Anything this slap still
        had to do on it (the up-stroke) is dropped. */
    void preempt (int stringIndex) noexcept;

    //==========================================================================
    // The pieces the engine applies.

    /*  The strike's excitation: the kind, the contact point on a string
        fretted at `fret`, the thumb or the nail, the level. */
    void shapeExcitation (const SlapStrike& strike, double fret, Excitation::Params& p) const noexcept;

    /*  bass-techniques 5: a ghost is the fretting hand resting on the string
        before the strike - strum-dynamics 6.1's chuck, by the same code
        (Damping::Chuck). The default 0.94 ends the note in about 15 ms: a
        thump with no pitch. */
    static void applyGhostDamping (StringEngine& string, double damping) noexcept;

    /*  The collision with the frets, as a fret-buzz event (bass-techniques
        2.1.3). `buzz` supplies fret-buzz.md 4's level for an excess, so the
        slap's clack and a bad setup's rattle are on one scale. Level 0 (and
        so no generator) when the contact is 0. */
    NoiseEvent makeContactBuzz (const SlapStrike& strike, bool wound, double fundamentalHz,
                                const FretBuzz& buzz) const noexcept;

    /** A palm slap on one string: its clack against the frets and the hand's thump. */
    void makePalmEvents (int stringIndex, bool wound, double fundamentalHz, double force, const FretBuzz& buzz,
                         NoiseEvent& clack, NoiseEvent& thump) const noexcept;

    /** The strings a palm slap covers by default: all of them. */
    int palmMask() const noexcept;

    //==========================================================================
    // The queue. hasDue is one comparison, so the engine can ask every sample.

    bool hasDue (juce::int64 now) const noexcept { return earliestDue <= now; }
    bool popDue (juce::int64 now, SlapAction& out) noexcept;
    int getNumQueued() const noexcept { return numActions; }

    //==========================================================================
    // The body tap (string-slap 2): bypasses the strings, drives the body.

    void startBodyTap (double force, BodyPart part) noexcept;
    bool isBodyTapSounding() const noexcept { return bodySamplesLeft > 0; }

    /** One sample of the knock, into the body stage's input. */
    double nextBodyDrive() noexcept;

    //==========================================================================
    // The hand's state, for the tests and the Techniques tab's indicators.

    bool isModifierHeld() const noexcept { return modifierHeld; }
    bool isGhostHeld() const noexcept { return ghostHeld; }
    juce::uint32 getFiredCount() const noexcept { return firedCount.load (std::memory_order_relaxed); }

    /** The contact point for a string: `mmFromLastFret` toward the bridge, as a fraction of the vibrating length. */
    double positionFraction (double mmFromLastFret, double fret) const noexcept;

    /** The velocity whose excitation peak is `ratio` of `velocity`'s (Excitation's level law, 8). */
    static double velocityForLevelRatio (double velocity, double ratio) noexcept;

    /** The type's default mask when the setting is 0. */
    int effectiveMask() const noexcept;

private:
    void queue (const SlapAction& a) noexcept;
    void recomputeEarliest() noexcept;
    void queuePercussive (SlapType type, double dynamics, juce::int64 at) noexcept;

    double sr = 48000.0;
    SlapSettings settings;

    int numStrings = 6;
    double scaleLengthMm = 648.0;
    int numFrets = 22;
    bool bass = false;

    bool modifierHeld = false;
    bool ghostHeld = false;

    std::array<SlapAction, kMaxActions> actions {};
    int numActions = 0;
    juce::int64 earliestDue = std::numeric_limits<juce::int64>::max();

    // The body tap's knock: three resonances weighted by where it lands.
    std::array<Biquad, 3> bodyModes;
    std::array<double, 3> bodyGains {};
    double bodyKick = 0.0;
    int bodySamplesLeft = 0;

    std::atomic<juce::uint32> firedCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapEngine)
};

} // namespace luthier
