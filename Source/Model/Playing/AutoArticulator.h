#pragma once

/*  Performance Assist (auto-articulation.md).

    Turns plain MIDI into a guitar performance: it picks the string and the
    position, turns legato into hammer-ons, pull-offs and slides, and adds
    delayed vibrato, pick attack, alternate picking, strum direction and speed,
    palm-muted chugs and the occasional ornament. Owned by MidiInterpreter,
    like its StrumGesture, and called from the interpreter's own decision
    points (4.2).

    Ground rules this class keeps (0):
      - Off is today: nothing here runs, and nothing here draws from the
        interpreter's RtRandom, unless isEffective(). Its own randomness is a
        counter-based hash of the note's ordinal and pitch.
      - Explicit wins: decorate() is told whether a controller chose the
        technique, and keeps it.
      - Deterministic: every decision reads the absolute sample stamps of the
        notes (the NoteRecord log), the settings, the tuning and the host
        transport at those samples - never the interpreter's flush-time slot
        state, which depends on the chord window and the block size (4.2).
      - No allocation, no locks: fixed arrays sized by kMaxStrings.

    The per-string auto pitch (bend-into, fall) and vibrato curves live here
    and are read by LuthierEngine::updatePerBlockModulation from absolute time.
*/

#include "PlayingEvents.h"
#include "AutoArticulationStyles.h"
#include "AutoArticulationFeed.h"
#include "ChordVoicer.h"
#include "../../Rhythm/StrumGesture.h"

#include <array>

namespace luthier
{

class TuningEngine;

//==============================================================================
/** The four parameters (6), as the bridge resolves them. */
struct AutoArticulationSettings
{
    bool   enabled = false;
    int    style = 0;              ///< AssistStyle
    double amount = 0.6;           ///< 0..1
    int    rules = AssistRule::all;

    bool operator== (const AutoArticulationSettings& o) const noexcept
    {
        return enabled == o.enabled && style == o.style
            && juce::exactlyEqual (amount, o.amount) && rules == o.rules;
    }
    bool operator!= (const AutoArticulationSettings& o) const noexcept { return ! operator== (o); }
};

/** What counts as explicit (5), filled by LuthierEngine before each block. The
    interpreter adds what it knows itself (controller vibrato, bends, strum CCs). */
struct AssistExplicitContext
{
    bool preArticulated = false;       ///< Luthier-profile import: notes already carry techniques
    bool slapHeld = false;             ///< the slap modifier is down
    bool slapVelocityZone = false;     ///< slap armed on the velocity zone
    int  slapZoneVelocity = 128;       ///< MIDI velocity at or above which a note slaps
    juce::uint32 scrapeMask = 0;       ///< strings with a scrape active
    bool tapArmed = false;             ///< two-hand-tapping.md 5 (TECHNIQUES)
    bool muteGridActive = false;       ///< muting-rhythm.md (TECHNIQUES)
    bool bassFamily = false;
    bool bassFingers = false;          ///< a bass played with fingers: BassFingerstyle alternates
    bool fretless = false;
    bool slideMode = false;            ///< CC 65 Slide Mode
    bool rhythmDriving = false;        ///< this pass's notes are replaced by the rhythm engine's (5)
};

/** The host transport at the block's first sample (3.7). */
struct AssistTransport
{
    bool   playing = false;
    double ppqAtBlockStart = 0.0;
    double bpm = 120.0;
};

/** Why Performance Assist is bypassed, for the notice line (5, 7.2). */
enum class AssistBypass { none, off, guitarControllerOrMpe, rhythmDriving };

//==============================================================================
/** One note's plan, from planSingle / planStrum, carried into decorate(). */
struct AssistPlan
{
    bool assisted = false;             ///< decorate() runs for this note

    Technique legato = Technique::Pluck;   ///< HammerOn, PullOff or Slide from the legato rules
    double slideFromFret = -1.0;
    int    chainCount = 0;

    AssistOrnament ornament = AssistOrnament::none;
    int    bendSemitones = 0;          ///< bend-into: played at fret - k and bent up k
    int    slideInFrets = 0;

    bool   chordMember = false;
    int    chordSize = 1;
    bool   chordInRegister = false;    ///< every note of a <= 3-note chord is in the palm-mute register
    bool   lateJoin = false;

    bool   strummed = false;
    bool   strumUp = false;
    juce::uint16 strumMask = 0;        ///< set on the chord's first note only, for the feed
    double strumForceScale = 1.0;
};

//==============================================================================
class AutoArticulator
{
public:
    static constexpr int kLogSize = 32;

    AutoArticulator() noexcept;

    void prepare (double sampleRate, int numStrings) noexcept;

    /** Transport start, preset load, panic (MidiInterpreter::reset). Clears the
        history and the hand position; running curves stop. */
    void reset() noexcept;

    void setNumStrings (int n) noexcept;
    void setTuning (const TuningEngine* t) noexcept { tuning = t; }

    //==========================================================================
    void setSettings (const AutoArticulationSettings& s) noexcept;
    const AutoArticulationSettings& getSettings() const noexcept { return settings; }

    void setExplicitContext (const AssistExplicitContext& c) noexcept { context = c; }
    const AssistExplicitContext& getExplicitContext() const noexcept { return context; }

    void setTransport (const AssistTransport& t, juce::int64 blockStartSample) noexcept
    {
        transport = t;
        transportBlockStart = blockStartSample;
    }

    /** Settings on, amount > 0 and rules != 0 (0.1). The playing mode is the
        interpreter's to add. */
    bool isEnabledBySettings() const noexcept
    {
        return settings.enabled && settings.amount > 0.0 && settings.rules != 0;
    }

    bool rule (int bit) const noexcept { return (settings.rules & bit) != 0; }
    const AutoArticulationStyle& style() const noexcept { return AutoArticulationStyles::get (settings.style); }

    /** 2: the Amount's three scalings. */
    double windowScale() const noexcept { return 0.5 + settings.amount; }
    double depthScale() const noexcept  { return 0.5 + settings.amount; }
    double probabilityScale() const noexcept { return juce::jmin (1.0, 2.0 * settings.amount); }

    /** Test hook (AA-21, AA-22): forces every ornament probability to this,
        or < 0 for the style's own. */
    void setProbabilityOverride (double p) noexcept { probabilityOverride = p; }

    //==========================================================================
    // The interpreter's hooks (4.1, 4.2).

    /** Records the controller state the interpreter owns, for 5's explicit rows. */
    void noteControllerVibrato (double depth) noexcept { controllerVibrato = depth; }
    void noteBend (int stringIndex, juce::int64 sample) noexcept;
    void noteStrumController (juce::int64 sample) noexcept { lastStrumCcSample = sample; }
    bool strumControllerRecent (juce::int64 sample) const noexcept;

    /** 3.1 - 3.3 and 3.8: places a single note. Invalid when the note is not
        playable on any string (13: the caller falls back to its voicer).
        `deferUntil` > arrival asks the caller to wait for the source's release
        or that sample, whichever comes first, and then call resolveLegato. */
    struct SinglePlan
    {
        VoicedNote note;
        AssistPlan plan;
        juce::int64 deferUntil = -1;
        int sourceString = -1;
        int sourceMidi = -1;
    };

    SinglePlan planSingle (int midiNote, double velocity, juce::int64 arrival,
                           bool lateJoin, juce::uint32 lateJoinGroupMask) noexcept;

    /** A deferred legato candidate (3.3): the source was released at `at`
        (sourceReleased) or is still held at the deadline. Decides HammerOn /
        PullOff or Slide. */
    void resolveLegato (SinglePlan& p, bool sourceReleased) noexcept;

    /** 3.1 for chords: the hand position the chord voicer should prefer, and
        the voicing's hand position after it. */
    int getHandPosition() const noexcept { return hand; }
    void setHandPositionFromVoicing (int handFret) noexcept;

    /** 3.7: fills the StrumRequest for an assisted chord. `peakVelocity` is the
        group's 0..1 peak, `speedVariation` Humanize's multiplier. Returns
        false when the chord's CC 76/77 were moved recently (the CC wins) or
        the strum rule is off. */
    bool planStrum (StrumRequest& request, juce::int64 sample, double peakVelocity,
                    double speedVariation, AssistPlan& chordPlan) noexcept;

    /** 3.7's style exceptions: Fingerstyle's pinch roll and Bass's
        together, instead of StrumGesture. Fills delaySeconds per string of
        `strings` (engine string indices); returns true when it applied. */
    bool planRollOrTogether (const int* strings, const int* midiNotes, int count,
                             double* delaySeconds) const noexcept;

    /** 3.2 - 3.8 on the NoteOnEvent the interpreter built, after
        TechniqueEngine::decide. `explicitTech` is decide's explicitOut. */
    void decorate (NoteOnEvent& e, Technique decided, bool explicitTech,
                   juce::int64 arrival, juce::int64 soundSample, const AssistPlan& plan) noexcept;

    /** Records a note the interpreter sounded without assistance (a fallback,
        an explicit-only note), so the history stays complete. */
    void recordUnassisted (const NoteOnEvent& e, juce::int64 arrival, juce::int64 soundSample) noexcept;

    /** A key went up at `sample` for the note on `stringIndex` (or -1 when not
        voiced yet). Returns the extra release delay for a fall (3.8), in
        samples, or 0. `sharedNoteOn`: a note-on shares this sample. */
    int onNoteOff (int midiNote, int stringIndex, juce::int64 sample, bool sharedNoteOn) noexcept;

    /** The held state from stamps (3). */
    bool isStringHeldAt (int stringIndex, juce::int64 sample) const noexcept;
    int  numStringsHeldAt (juce::int64 sample) const noexcept;
    bool anyHeldAt (juce::int64 sample) const noexcept;

    //==========================================================================
    // The engine's per-block reads (4.2), from absolute time.

    double autoPitchCents (int stringIndex, juce::int64 sample) const noexcept;
    double autoVibratoCents (int stringIndex, juce::int64 sample) noexcept;
    double autoVibratoDepth (int stringIndex, juce::int64 sample) const noexcept;

    /** Performance Assist switched off (or back on) at this block: effects
        running ramp out over 150 ms (3.4). */
    void setEffectiveNow (bool effective, juce::int64 sample) noexcept;
    bool wasEffectiveLastBlock() const noexcept { return effectiveNow; }

    /** One-shot events for the feed and the capture, taken by the engine once
        per block: bit 0 vibrato started, bit 1 fall started. */
    enum PendingEvent : int { vibratoStarted = 1, fallStarted = 2 };
    int takePendingEvents (int stringIndex) noexcept;

    /** The note's vibrato as rate and depth, for the capture mark. */
    double getVibratoRate (int stringIndex) const noexcept;
    double getVibratoDepthCents (int stringIndex) const noexcept;
    juce::int64 getVibratoStartSample (int stringIndex) const noexcept;

    /** The note sounding on the string (for the feed), or -1. */
    double getSoundingFret (int stringIndex) const noexcept;

    //==========================================================================
    AutoArticulationFeed& getFeed() noexcept { return feed; }

    /** Audio thread: one feed entry (7.3). */
    void pushFeed (juce::int64 sample, int stringIndex, double fret, AssistLabel label, int ruleBit,
                   juce::uint16 mask = 0) noexcept;

    /** Number of hash draws so far: the tests compare it, and nothing else. */
    juce::uint64 getNoteOrdinal() const noexcept { return noteOrdinal; }

    /** 3.6 palm-mute register: semitones above the lowest open string. */
    int semitonesAboveLowestOpen (int midiNote) const noexcept;

    /** A hash in [0, 1) from two integers; the counter-based randomness of 0.1. */
    static double hash01 (juce::uint64 a, juce::uint64 b) noexcept;

private:
    struct NoteRecord
    {
        int    midi = -1;
        int    string = -1;
        double fret = 0.0;
        double velocity = 0.0;
        juce::int64 arrival = 0;
        juce::int64 sound = 0;
        juce::int64 off = -1;          ///< -1 while the key is held
        Technique technique = Technique::Pluck;
        int    chain = 0;
        bool   assisted = false;
        bool   autoMuted = false;
        bool   picked = false;
        bool   chord = false;
        bool   upStroke = false;
        juce::uint64 ordinal = 0;      ///< the hash's counter when it was planned
    };

    struct PitchCurve
    {
        bool active = false;
        juce::int64 start = 0;
        juce::int64 length = 1;
        double from = 0.0, to = 0.0;
        double hold = 0.0;             ///< after the curve
        int logIndex = -1;             ///< the note it belongs to
    };

    struct Vibrato
    {
        bool candidate = false;        ///< the note may get vibrato
        bool started = false;
        int logIndex = -1;
        double rate = 5.0;
        double depth = 0.0;            ///< cents, scaled
        juce::int64 startAt = 0;       ///< when the ramp begins
        juce::int64 releasedAt = -1;   ///< Assist switched off: ramps out from here
    };

    int addRecord (const NoteRecord& r) noexcept;
    const NoteRecord* record (int index) const noexcept;
    NoteRecord* record (int index) noexcept;
    int lastSingleIndex() const noexcept { return lastSingle; }

    bool heldAt (const NoteRecord& r, juce::int64 sample) const noexcept;
    bool legatoEligible (const NoteRecord& p, int midiNote, double velocity, juce::int64 arrival) const noexcept;
    double openBias (int stringIndex) const noexcept;
    bool explicitOnString (int stringIndex, int midiNote, double velocity) const noexcept;
    double ppqAt (juce::int64 sample) const noexcept;
    bool nothingHeldFor (juce::int64 sample, double ms) const noexcept;
    bool isLeadAt (int stringIndex, juce::int64 sample) const noexcept;
    double probability (double styleProbability) const noexcept;

    void startVibratoCandidate (int stringIndex, int logIndex, juce::int64 sound, int midiNote) noexcept;

    double sr = 48000.0;
    int numStrings = 6;
    const TuningEngine* tuning = nullptr;

    AutoArticulationSettings settings;
    AssistExplicitContext context;
    AssistTransport transport;
    juce::int64 transportBlockStart = 0;
    double probabilityOverride = -1.0;

    std::array<NoteRecord, kLogSize> log {};
    int logCount = 0;
    int logHead = 0;                   ///< next slot
    std::array<int, kMaxStrings> current {};   ///< log index sounding on the string, or -1
    int lastSingle = -1;
    int lastStruck = -1;               ///< the last note voiced, any kind (3.1's previous string)

    int hand = 0;                      ///< H, the index-finger fret from the capo (3.1)

    juce::int64 lastPickedArrival = -1000000000;
    bool lastPickedUp = false;
    juce::int64 lastStrumSample = -1000000000;
    bool lastStrumUp = false;
    juce::int64 lastStrumCcSample = -1000000000;
    std::array<juce::int64, kMaxStrings> lastBendSample {};
    juce::int64 lastGlobalBendSample = -1000000000;
    double controllerVibrato = 0.0;

    std::array<PitchCurve, kMaxStrings> pitch {};
    std::array<Vibrato, kMaxStrings> vibrato {};
    std::array<int, kMaxStrings> pendingEvents {};

    bool effectiveNow = false;
    juce::int64 offSince = -1;         ///< Assist went off here: running vibrato ramps out

    juce::uint64 noteOrdinal = 0;

    AutoArticulationFeed feed;
};

} // namespace luthier
