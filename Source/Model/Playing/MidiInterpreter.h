#pragma once

/*  MIDI Interpreter (engine spec 2).

    Turns the host's MidiBuffer into typed, string-addressed events. It owns the
    three playing modes, MPE handling, the technique controller map, and the strum
    simulation.

    Chord grouping: in Poly mode, note-ons that land within a short window are
    voiced together as one chord. The window is held across block boundaries so a
    chord split by a buffer edge still voices as a chord; the cost is that Poly mode
    carries up to `chordWindowMs` of extra latency (2 ms by default), which is
    reported to the host along with everything else.
*/

#include "PlayingEvents.h"
#include "TuningEngine.h"
#include "TechniqueEngine.h"
#include "ChordVoicer.h"
#include "RubricVoicer.h"
#include "AutoArticulator.h"   // FEAT-ASSIST: auto-articulation.md
#include "../../Rhythm/StrumGesture.h"
#include <array>

namespace luthier
{

//==============================================================================
/** What a performance controller is wired to. These are the continuous gestures
    that belong to the instrument, as distinct from generic parameter automation,
    which the MIDI Learn system handles separately. */
enum class MidiTarget
{
    None,
    VibratoDepth,
    VibratoRate,
    WhammyBar,
    Expression,
    MasterLevel,
    PalmMute,
    MutedPick,
    PickPosition,
    SlideToggle,
    SlideGuitarToggle,
    PinchHarmonic,
    NaturalHarmonic,
    Tap,
    StrumSpeed,
    StrumDirection,
    Humanize,
    Drive,
    Tone,
    Space,
    Body,
    Attack,

    // harmonic-realism.md 6 and fingerstyle-attack.md 5 (REALISM-B), appended.
    ArtificialHarmonic,
    TappedHarmonic,
    RightHandTool,
    RestStroke,
    NumTargets
};

const char* getMidiTargetName (MidiTarget t) noexcept;

//==============================================================================
class MidiInterpreter
{
public:
    /** The CC map starts at its defaults here, not in prepare(): a host
        prepares after restoring a session and must not undo its MIDI map. */
    MidiInterpreter() noexcept { resetCcMapToDefaults(); }

    void prepare (double sampleRate, int numStrings);
    void reset() noexcept;

    void setNumStrings (int n) noexcept;
    int getNumStrings() const noexcept { return numStrings; }

    void setEngines (TuningEngine* tuning, TechniqueEngine* technique, RubricVoicer* voicer) noexcept;

    //==========================================================================
    void setPlayingMode (PlayingMode m) noexcept;
    PlayingMode getPlayingMode() const noexcept { return mode; }

    void setMpeEnabled (bool e) noexcept { mpeEnabled = e; }
    bool isMpeEnabled() const noexcept { return mpeEnabled; }

    /** Pitch-bend range in semitones. MPE controllers default to 48. */
    void setPitchBendRange (double semitones) noexcept;
    double getPitchBendRange() const noexcept { return bendRangeSemitones; }

    /** Per-string bend range, for guitar controller mode. */
    void setStringBendRange (int stringIndex, double semitones) noexcept;

    //==========================================================================
    /*  Which MIDI channel drives which string in guitar-controller mode
        (controllers.md section 4).

        The default is the common convention - channel 1 is the high E, and the
        strings run upward from there - but it is only a convention: a Roland GK
        puts the high E on channel 11, and a TriplePlay can be configured to put
        it anywhere. So the map is data rather than arithmetic, and a controller
        profile sets it.
    */
    void setChannelForString (int stringIndex, int channel) noexcept;
    int getChannelForString (int stringIndex) const noexcept;

    /** Restores the default map: channel 1 is string 0, and upward. */
    void resetChannelMap() noexcept;

    /*  controllers.md 5: some controllers emit a constant low-magnitude wobble
        around a sustained note's pitch. Bends smaller than this are ignored. */
    void setPitchDeadZoneCents (double cents) noexcept;
    double getPitchDeadZoneCents() const noexcept { return pitchDeadZoneCents; }

    /*  controllers.md 5: some controllers send lazy note-offs. A note is held for
        at least this long regardless of when its note-off arrives, which stops a
        stuttering controller from machine-gunning the string engine. */
    void setMinimumNoteDurationMs (double ms) noexcept;
    double getMinimumNoteDurationMs() const noexcept { return minNoteDurationMs; }

    //==========================================================================
    void setCcTarget (int ccNumber, MidiTarget target) noexcept;
    MidiTarget getCcTarget (int ccNumber) const noexcept;
    void resetCcMapToDefaults() noexcept;

    /** Aftertouch can drive vibrato depth (default) or bend. */
    void setAftertouchTarget (MidiTarget t) noexcept { aftertouchTarget = t; }
    MidiTarget getAftertouchTarget() const noexcept { return aftertouchTarget; }

    //==========================================================================
    void setChordWindowMs (double ms) noexcept;
    double getChordWindowMs() const noexcept { return chordWindowMs; }

    void setStrumSpeedMs (double msPerString) noexcept;
    double getStrumSpeedMs() const noexcept { return strumSpeedMs; }

    void setStrumDirection (StrumDirection d) noexcept { strumDirection = d; }
    StrumDirection getStrumDirection() const noexcept { return strumDirection; }

    /** strum-dynamics 7: the gesture's shape for live chords. Their speed comes
        from setStrumSpeedMs, which the bridge feeds from strum_crossing_sps. */
    void setStrumSettings (const StrumSettings& s) noexcept { strumSettings = s.clamped(); }

    /*  harmonic-realism.md 4 (REALISM-B): the artificial and tapped offsets
        (choice indices, harmonics::offsetFretsForChoice) and the note mapping. */
    struct HarmonicSettings
    {
        int  artificialOffsetChoice = 0;
        int  tappedOffsetChoice = 0;
        bool soundingPitch = false;   ///< 4.2; false = touch-fret mapping (4.1)
        double inharmonicityB[kMaxStrings] {};
    };

    void setHarmonicSettings (const HarmonicSettings& h) noexcept { harmonicSettings = h; }
    const HarmonicSettings& getHarmonicSettings() const noexcept { return harmonicSettings; }

    /*  fingerstyle-attack.md 5: CC 102's live tool override (0 Off, 1 Pick,
        2 Finger, 3 Thumb, 4 Thumbpick, 5 Slap, 6 Pop) and CC 105's rest. */
    int  getRightHandToolOverride() const noexcept { return rightHandTool; }
    bool isRestStrokeForced() const noexcept { return restStrokeHeld; }

    /** True once CC 70 (PickPosition) has moved since the last reset: the
        pinch harmonic then follows it (harmonic-realism.md 3). */
    bool hasPickPositionController() const noexcept { return pickPositionMoved; }

    /*  string-interaction.md 6: the muted-string thump's level; 0 is off and
        emits nothing. */
    void setMutedThumpLevel (double level) noexcept { mutedThumpLevel = juce::jlimit (0.0, 1.0, level); }

    /** Latency the chord window adds, in samples. */
    int getLatencySamples() const noexcept;

    //==========================================================================
    /** Humanisation, applied to the events as they are generated. */
    struct Humanisation
    {
        double timingJitterMs = 3.0;
        double velocityVariation = 0.08;
        double microDetuneCents = 2.5;
        double attackVariation = 0.10;
        double stringNoiseProbability = 0.25;
        double strumSpeedVariation = 0.20;
        double amount = 1.0;           ///< Master scaler on all of the above.
    };

    void setHumanisation (const Humanisation& h) noexcept { humanise = h; }
    const Humanisation& getHumanisation() const noexcept { return humanise; }

    //==========================================================================
    /** Parses one block. `blockStartSample` is the absolute sample position of the
        start of the block, used for technique timing across blocks. */
    void processBlock (const juce::MidiBuffer& midi,
                       int numSamples,
                       int64_t blockStartSample,
                       PlayEventQueue& out) noexcept;

    //==========================================================================
    // Live state the engine and UI read back.

    bool isSustainPedalDown() const noexcept { return sustainDown; }
    bool isSostenutoDown() const noexcept { return sostenutoDown; }
    double getVibratoDepth() const noexcept { return vibratoDepth; }
    double getVibratoRate() const noexcept { return vibratoRate; }
    double getWhammyPosition() const noexcept { return whammyPosition; }
    double getExpression() const noexcept { return expressionValue; }
    double getPickPosition() const noexcept { return pickPosition; }

    /** Per-string bend in cents, including MPE per-note bend. */
    double getStringBendCents (int stringIndex) const noexcept;

    /** True if any note arrived during the last block, for the MIDI-in LED. */
    bool consumeActivityFlag() noexcept { const bool a = activity; activity = false; return a; }

    /** Notes currently sounding, for the fretboard display. */
    int getActiveNoteCount() const noexcept { return activeNoteCount; }

    /** Which MIDI note is on a given string, or -1. */
    int getStringMidiNote (int stringIndex) const noexcept;

    /** The last chord the voicer identified, for the UI. */
    juce::String getLastChordName() const;

    /** Panic: releases everything. */
    void allNotesOff (PlayEventQueue& out) noexcept;

    // ==== BEGIN FEAT-ASSIST ====
    /*  Performance Assist (auto-articulation.md 4). The hooks are in
        MidiInterpreterAssist.cpp; every one is behind isAssistEffective(), so
        with Assist off the interpreter is exactly what it was (0.1). */
    void setAutoArticulation (const AutoArticulationSettings& s) noexcept { autoArt.setSettings (s); }
    const AutoArticulationSettings& getAutoArticulation() const noexcept { return autoArt.getSettings(); }
    AutoArticulator& getAutoArticulator() noexcept { return autoArt; }
    const AutoArticulator& getAutoArticulator() const noexcept { return autoArt; }

    /** LuthierEngine, before each processBlock (4.2). */
    void setAssistContext (const AssistExplicitContext& c, const AssistTransport& t,
                           int64_t blockStartSample) noexcept;

    /** Settings on, and neither Guitar Controller mode, MPE nor a
        pre-articulated import (5). */
    bool isAssistEffective() const noexcept;

    /** The notice line's reason (5, 7.2). The rhythm engine's is the engine's to add. */
    AssistBypass getAssistBypass() const noexcept;
    // ==== END FEAT-ASSIST ====

private:
    struct PendingNote
    {
        int midiNote = 60;
        int channel = 1;
        double velocity = 0.8;
        int64_t timestamp = 0;
        bool used = false;
        int64_t releasedAt = -1;   ///< note-off seen while still waiting, or -1
    };

    struct StringSlot
    {
        int midiNote = -1;
        int channel = -1;
        bool held = false;
        bool sostenutoHeld = false;
        double bendCents = 0.0;
        double pressure = 0.0;
        double timbre = 0.0;

        /*  When the note started, and when a note-off that arrived too early is
            allowed to take effect (controllers.md 5).

            Some controllers send a note-off almost immediately after the note-on
            and then another when the player actually lets go. Obeying the first
            one turns every note into a click. A minimum duration holds the note
            open until it has at least sounded. */
        int64_t startedAt = 0;
        int64_t releaseDueAt = -1;
        bool releaseWasLetRing = false;

        /*  The key went up while a pedal held the string open: it is still
            ringing, and lifting the pedal is what releases it. */
        bool pedalRinging = false;
        int  pedalRingingNote = -1;
    };

    /** Releases (damps) a string left ringing by a pedal, if nothing else still
        holds it open. */
    void releasePedalRinging (int stringIndex, int blockOffset, PlayEventQueue& out) noexcept;

    void handleNoteOn (int midiNote, int channel, double velocity,
                       int64_t timestamp, int blockOffset, PlayEventQueue& out) noexcept;
    void handleNoteOff (int midiNote, int channel, int blockOffset, PlayEventQueue& out) noexcept;
    void handleController (int cc, int value, int channel, int blockOffset, PlayEventQueue& out) noexcept;
    void applyTarget (MidiTarget target, double value, int blockOffset, PlayEventQueue& out) noexcept;

    void flushChordGroup (int64_t upToSample, int blockOffset, int numSamples, PlayEventQueue& out) noexcept;

    /** Releases any note whose deferred note-off has now come due
        (controllers.md 5). */
    void flushDeferredReleases (int64_t blockStartSample, int numSamples,
                                PlayEventQueue& out) noexcept;
    void emitVoicedNote (const VoicedNote& note, int64_t timestamp, int blockOffset,
                         int extraDelaySamples, PlayEventQueue& out) noexcept;

    int stringForChannel (int channel) const noexcept;
    void releaseString (int stringIndex, int blockOffset, PlayEventQueue& out) noexcept;

    double sr = 44100.0;
    int numStrings = 6;

    TuningEngine* tuning = nullptr;
    TechniqueEngine* technique = nullptr;
    RubricVoicer* voicer = nullptr;

    PlayingMode mode = PlayingMode::Poly;

    /** channelMap[stringIndex] is the 1-based MIDI channel that plays it. */
    std::array<int, kMaxStrings> channelMap {};

    double pitchDeadZoneCents = 0.0;
    double minNoteDurationMs = 0.0;

    /** The absolute sample position of the message being handled, so that a
        note-off can tell how long its note has been sounding. */
    int64_t currentTimestamp = 0;

    /** The block being interpreted, so a chord group sounds at its own sample. */
    int64_t blockStart = 0;
    int blockLength = 0;
    bool mpeEnabled = false;

    double bendRangeSemitones = 2.0;
    std::array<double, kMaxStrings> stringBendRange {};

    std::array<MidiTarget, 128> ccMap {};
    MidiTarget aftertouchTarget = MidiTarget::VibratoDepth;

    std::array<StringSlot, kMaxStrings> slots {};

    // Chord grouping
    static constexpr int kMaxPending = 16;
    std::array<PendingNote, kMaxPending> pending {};
    int numPending = 0;
    double chordWindowMs = 2.0;
    int chordWindowSamples = 96;

    double strumSpeedMs = 9.0;
    StrumDirection strumDirection = StrumDirection::Down;
    bool nextStrumIsUp = false;

    StrumSettings strumSettings;
    HarmonicSettings harmonicSettings;
    int rightHandTool = 0;
    bool restStrokeHeld = false;
    bool pickPositionMoved = false;
    double mutedThumpLevel = 0.0;

    /** harmonic-realism.md 4.2: plays a harmonic-armed note named by the pitch
        heard. True if it was handled (located or played artificially). */
    bool emitSoundingHarmonic (int midiNote, int channel, double velocity, int64_t timestamp,
                               int blockOffset, PlayEventQueue& out) noexcept;
    bool isHarmonicArmed (double velocity) const noexcept;
    StrumGesture strumGesture;
    juce::uint32 strumCount = 0;

    // Controller state
    bool sustainDown = false;
    bool sostenutoDown = false;
    double vibratoDepth = 0.0;
    double vibratoRate = 5.0;
    double whammyPosition = 0.0;
    double expressionValue = 0.5;
    double pickPosition = 0.5;
    double globalBendCents = 0.0;

    bool activity = false;
    int activeNoteCount = 0;
    int lastMonoString = -1;

    /*  The last chord's notes, not its name: naming it builds a String, which
        the audio thread must not do, and the UI used to copy the String while
        the audio thread reassigned it. The UI names it (getLastChordName). */
    mutable juce::SpinLock lastChordLock;
    std::array<int, 16> lastChordNotes {};
    int lastChordCount = 0;

    RtRandom rng { 0x4D1D1ull };
    Humanisation humanise;

    // ==== BEGIN FEAT-ASSIST ====
    AutoArticulator autoArt;

    /** The plan of the note emitVoicedNote is emitting, or null (unassisted). */
    const AssistPlan* currentPlan = nullptr;
    int64_t currentArrival = 0;

    /** 3.3: a legato note waiting for its source's release or the slide's
        minimum overlap, whichever comes first. */
    struct PendingLegato
    {
        bool active = false;
        AutoArticulator::SinglePlan plan;
        int channel = 1;
        int64_t arrival = 0;
    };

    PendingLegato pendingLegato;

    /** 3.1 late join: the last chord group. */
    int64_t lastGroupArrival = -1000000000;
    juce::uint32 lastGroupMask = 0;
    int lastGroupSize = 0;

    /** A note-on shares the sample of the note-off being handled (3.8's fall). */
    bool offSharesNoteOn = false;

    void assistBeginBlock() noexcept;
    void assistEndBlock (int numSamples, PlayEventQueue& out) noexcept;
    void assistBeforeEvent (int64_t timestamp, PlayEventQueue& out) noexcept;
    bool assistMonoNoteOn (int midiNote, int channel, double velocity, int64_t timestamp,
                           int blockOffset, PlayEventQueue& out) noexcept;
    bool assistNoteOff (int midiNote, int channel, int blockOffset, PlayEventQueue& out) noexcept;
    bool assistFlushSingle (int midiNote, double velocity, int64_t arrival, int64_t releasedAt,
                            int64_t groupTimestamp, int blockOffset, PlayEventQueue& out) noexcept;
    void assistEmit (const VoicedNote& note, const AssistPlan& plan, int64_t arrival, int64_t timestamp,
                     int blockOffset, int extraDelay, PlayEventQueue& out) noexcept;
    void assistResolvePending (int64_t atSample, bool sourceReleased, PlayEventQueue& out) noexcept;
    void assistDecorate (NoteOnEvent& e, Technique decided, bool explicitTech, int64_t soundSample) noexcept;
    bool assistNoteIsExplicit (double velocity) const noexcept;
    void assistPlanChordNotes (const ChordVoicing& voicing, int64_t groupTimestamp, bool playedSpread,
                               AssistPlan& chordPlan) noexcept;
    bool assistPlanStrum (StrumRequest& request, int* order, int numOrdered, const ChordVoicing& voicing,
                          int64_t groupTimestamp, double speedVariation, AssistPlan& chordPlan,
                          std::array<double, kMaxStrings>& delays) noexcept;
    void assistShapeStrikes (StrumStrike* strikes, int count, const AssistPlan& chordPlan) const noexcept;
    void assistEmitChordNote (const VoicedNote& note, const AssistPlan& chordPlan, int voicingIndex,
                              const int64_t* arrivals, const int* notes, int count, int64_t groupTimestamp,
                              int blockOffset, int delaySamples, PlayEventQueue& out) noexcept;
    // ==== END FEAT-ASSIST ====

    JUCE_LEAK_DETECTOR (MidiInterpreter)
};

} // namespace luthier
