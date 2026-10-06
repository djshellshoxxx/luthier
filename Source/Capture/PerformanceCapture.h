#pragma once

/*  Performance capture (notation-export.md 6; TODO 9).

    What fills a PerformanceScore from what the player just played. Until this,
    PerformanceScore was only ever filled by NotationImporter reading a file, so
    the live TAB view had nothing to show and the export dialog nothing to
    export.

    Three halves:

      - The audio thread reports what the engine actually played - after
        voicing and technique resolution, not the incoming MIDI (6.1) - through
        noteOn / noteOff / bend / mark / chordSymbol / bassTechnique /
        slideBar, each stamped against the block's clock (beginBlock). The
        reports go into a CaptureRing: fixed, allocation-free, overwriting the
        oldest when nobody reads (6.2). `off` costs one relaxed load per call
        (6.3, 6.5).

      - The message thread drains the ring at 10 Hz into a take: notes with
        their string, fret, times, velocity, techniques and bend, plus chord
        symbols, tempo and time-signature changes, and the BASS_TECH and slide
        events (6.1). `rolling` keeps the last N minutes (default 10, section 1);
        `armed` clears and starts from the next note (6.3).

      - The take becomes a PerformanceScore (the NOTATION tab, the live TAB
        view, notation export), a MidiPerformance (MIDI export in either
        profile), or a rendered window of ASCII tab. Nothing is quantised on
        the way in (6.5); quantisation is an option of those conversions.

    Times (6.4): quarter notes on the host's clock while its transport runs,
    samples (read as seconds) while it is stopped.

    Wiring (not done here; see the report): LuthierEngine reports from
    triggerNote and applyNoteOff, which is where the string and fret are known;
    until that hook exists, captureStringActivity reads the engine's
    StringActivityQueue after each block, which has the string but not the
    technique.
*/

#include "CaptureRing.h"

#include "../Export/MidiPerformance.h"
#include "../Model/Playing/PlayingEvents.h"
#include "../Notation/NotationExport.h"
#include "../Notation/PerformanceScore.h"

#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

class StringActivityQueue;
class TuningEngine;

//==============================================================================
/** notation-export 6.3. */
enum class CaptureState
{
    off = 0,     ///< nothing written; zero cost
    rolling,     ///< continuous; the take holds the last N minutes. The default.
    armed        ///< cleared, and recording from the next note
};

const char* getCaptureStateName (CaptureState) noexcept;

/** The block's place on the host's clock, from its AudioPlayHead. */
struct CaptureClock
{
    juce::int64 blockStartSample = 0;
    double sampleRate = 48000.0;
    bool transportPlaying = false;
    double blockStartPpq = 0.0;
    double bpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
};

//==============================================================================
/** One note as the engine played it. */
struct CapturedNote
{
    juce::int64 startSample = 0;
    juce::int64 endSample = -1;          ///< -1 while it still sounds

    bool musical = false;                ///< started with the transport running
    double startPpq = 0.0;
    double endPpq = 0.0;

    int stringIndex = 0;                 ///< 0 is the highest string
    int midiNote = 60;
    double fret = 0.0;                   ///< from the capo, as the engine counts it
    double velocity = 0.8;               ///< 0..1

    Technique technique = Technique::Pluck;
    int harmonicPartial = 0;

    /** auto-articulation.md 9 (FEAT-ASSIST): the aa_rules bits that decided it. */
    juce::uint16 autoRules = 0;

    /** Techniques reported while it sounded (palm mute, vibrato, ...). */
    std::vector<ScoreTechnique> marks;

    /** Pitch bend while it sounded: (sample, cents). */
    std::vector<std::pair<juce::int64, double>> bend;

    bool isSounding() const noexcept { return endSample < 0; }
};

struct CapturedChord
{
    juce::int64 sample = 0;
    bool musical = false;
    double ppq = 0.0;
    juce::String name;
};

struct CapturedMeter
{
    juce::int64 sample = 0;
    bool musical = false;
    double ppq = 0.0;
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
};

/** A BASS_TECH or SLIDE_BAR event (6.1), ready for MIDI export. */
struct CapturedEvent
{
    juce::int64 sample = 0;
    bool musical = false;
    double ppq = 0.0;
    LuthierEvent event;
};

//==============================================================================
/** How a take becomes a score (6.4, 6.5: quantisation is an export and edit
    option, never applied on the way in). */
struct CaptureScoreOptions
{
    /** The grid in quarter notes: 0 = none, 0.25 = sixteenths, 1.0 / 3.0 =
        eighth-note triplets. */
    double quantiseBeats = 0.0;

    /** 1 snaps fully; 0.5 moves each note half way to the grid. */
    double quantiseStrength = 1.0;

    /** Only the last this-many seconds of the take; 0 = all of it (section 1's
        "capture the last N minutes"). */
    double lastSeconds = 0.0;
    /** midi-export 4.1's "marked region" (MODEL-GAPS, TODO 9/10): only notes
        starting in these capture samples; empty = no limit. */
    juce::Range<juce::int64> sampleRange;
    /** 4.1's "current section (tune only)": only notes played in time with the
        host that start in these quarter-note positions; empty = no limit. */
    juce::Range<double> ppqRange;
    /** notation-export 4 (MODEL-GAPS, TODO 9): "In Mono mode, chord extraction
        runs offline on the captured PerformanceScore using a template match
        against detected pitch classes per beat." Applied when the take has no
        chord track of its own (Mono mode records none). */
    bool extractChordsWhenMissing = true;

    /** The tempo free-play notes are read at; 0 = the take's own. */
    double freeTempoBpm = 0.0;

    juce::String title { "Captured performance" };
};

//==============================================================================
class PerformanceCapture
{
public:
    static constexpr int kRingCapacity = CaptureRing::kCapacity;
    static constexpr double kDefaultRollingMinutes = 10.0;
    static constexpr int kDrainHz = 10;

    PerformanceCapture();

    //==========================================================================
    // Message thread.

    /** Empties the ring and the take. Not while the audio thread reports. */
    void prepare (double rate);

    /** `armed` clears the take and starts from the next note (6.3). */
    void setState (CaptureState newState);
    CaptureState getState() const noexcept { return (CaptureState) state.load (std::memory_order_relaxed); }

    void setRollingMinutes (double minutes) noexcept;
    double getRollingMinutes() const noexcept { return rollingMinutes; }

    /** The instrument the take is played on: each string's open MIDI note at
        the nut (highest string first), the string count, and the capo with
        the strings it clamps (bit 0 is string 0). Frets read from string
        activity count from the capo, as the engine's do (ambiguity-resolutions
        4.5); the score gets the nut tuning and the capo fret. */
    void setTuning (const std::array<int, kMaxStrings>& openNotesAtNut, int stringCount,
                    int capo, juce::uint32 capoStringMask = 0xFFFFFFFFu);

    /** Each string's open MIDI note at the nut, from the engine's own tuning. */
    static std::array<int, kMaxStrings> getOpenNotes (const TuningEngine& tuning, int stringCount);

    //==========================================================================
    // Audio thread. None of these allocate, lock or wait.

    /** Once per block, before the engine renders it. */
    void beginBlock (const CaptureClock& clock) noexcept;

    /** A voiced note: the string and fret the voicer chose (6.1). */
    void noteOn (int sampleOffset, int stringIndex, int midiNote, double fret, float velocity,
                 Technique playedAs = Technique::Pluck, int harmonicPartial = 0) noexcept;

    void noteOff (int sampleOffset, int stringIndex, bool letRing = false) noexcept;

    /** The bend on a string, in cents, whenever it changes. */
    void bend (int sampleOffset, int stringIndex, double cents) noexcept;

    /** A technique that starts during a note (palm mute, vibrato, ...). */
    void mark (int sampleOffset, int stringIndex, ScoreTechnique::Type type, double value = 0.0,
               double secondValue = 0.0) noexcept;

    /** auto-articulation.md 9 (FEAT-ASSIST): Performance Assist's bits for the
        note just reported on the string. */
    void autoRules (int sampleOffset, int stringIndex, juce::uint16 rules) noexcept;

    /** notation-export 4: the detector's chord, where it changes. At most 15
        characters are kept. The caller must not allocate to produce `name`. */
    void chordSymbol (int sampleOffset, const char* name) noexcept;

    /** bass-techniques: slap, pop, ghost, lhslap, thump, pluck. */
    void bassTechnique (int sampleOffset, int stringIndex, const char* technique, double pluckPosition,
                        double force = 0.8, double fretContact = 0.8) noexcept;   // SPEC-SWEEP BT-24: force, contact

    /** slide-guitar: the bar's position in frets and its pressure (lift, light, full). */
    void slideBar (int sampleOffset, double fretPosition, const char* pressure) noexcept;

    /*  SPEC-SWEEP MX-1 (midi-export 6): a live noise event - what the live
        MIDI out sends as PICK, SQUEAK, BUZZ or CLANK - so an exported take
        carries it too. `kind`: 0 pick, 1 squeak (shift), 2 squeak (drag),
        3 buzz, 4 clank. Audio thread. */
    enum class NoiseKind : juce::uint8 { pick = 0, squeakShift, squeakDrag, buzz, clank };
    void noiseEvent (int sampleOffset, NoiseKind kind, int stringIndex, double durationMs, double level) noexcept;

    /** Until the engine reports from triggerNote: the block's string activity,
        after engine.processBlock. Strings are the voicer's; frets are worked
        out from setTuning's open notes; techniques are not known. */
    void captureStringActivity (const StringActivityQueue& activity) noexcept;

    //==========================================================================
    // Message thread.

    /** 6.2: moves what the audio thread reported into the take. Call at
        kDrainHz. Returns the number of records read. */
    int drain();

    /** 6.2: records lost because nobody read them in time. */
    juce::int64 getDroppedCount() const noexcept { return ring.getDroppedCount(); }

    /** Records reported since prepare(), kept or not. */
    juce::uint64 getRecordsWritten() const noexcept { return ring.getWriteCount(); }

    const std::vector<CapturedNote>& getNotes() const noexcept   { return notes; }
    const std::vector<CapturedChord>& getChords() const noexcept { return chords; }
    const std::vector<CapturedMeter>& getMeters() const noexcept { return meters; }
    const std::vector<CapturedEvent>& getEvents() const noexcept { return events; }

    void clearTake();
    /*  midi-export 4.1 / notation-export 5 (MODEL-GAPS): the marked region -
        mark in, play, mark out - at the newest sample the take has seen.
        Message thread. The region is empty until both marks are set, the
        out after the in. */
    void markIn();
    void markOut();
    void clearMarks();
    bool hasMarkedRegion() const noexcept { return markOutSample > markInSample && markInSample >= 0; }
    juce::Range<juce::int64> getMarkedRegion() const noexcept
    {
        return hasMarkedRegion() ? juce::Range<juce::int64> (markInSample, markOutSample) : juce::Range<juce::int64>();
    }
    juce::int64 getNewestSample() const noexcept { return newestSample; }

    /** The capture samples the notes played in time with the host inside
        `ppq` span, first start to last end; empty when none (MODEL-GAPS: the
        current section, for exports that count in samples). */
    juce::Range<juce::int64> sampleRangeForPpq (juce::Range<double> ppq) const noexcept;

    //==========================================================================
    /** The take as notation. Notes still sounding end at the newest sample
        the take has seen. */
    void toScore (PerformanceScore& score, const CaptureScoreOptions& options = {}) const;

    /** The take for MIDI export (midi-export 0.1): the score's notes and
        techniques as MidiPerformance::fromScore lays them out, plus the
        BASS_TECH and SLIDE_BAR events. */
    MidiPerformance toPerformance (double rate, const CaptureScoreOptions& options = {}) const;

    /** notation-export 3: the last `numBars` bars as ASCII tab. */
    juce::String renderLiveTab (int numBars,
                                NotationExportOptions::SymbolDensity density
                                  = NotationExportOptions::SymbolDensity::full) const;

private:
    CaptureRecord makeRecord (CaptureRecord::Kind kind, int sampleOffset) const noexcept;
    bool isRecording() const noexcept { return state.load (std::memory_order_relaxed) != (int) CaptureState::off; }

    void apply (const CaptureRecord& record);
    void trimToRollingWindow();
    void rebuildSoundingIndex();

    struct Timeline;
    Timeline makeTimeline (const CaptureScoreOptions& options, std::vector<size_t>& selected) const;

    CaptureRing ring;
    std::atomic<int> state { (int) CaptureState::rolling };
    std::atomic<bool> restateMeter { true };

    // Audio thread only.
    CaptureClock clock;
    double lastMeterBpm = -1.0;
    int lastMeterNumerator = -1, lastMeterDenominator = -1;

    // Written by the message thread, read by captureStringActivity: each
    // string's open note with the capo on it.
    std::array<std::atomic<int>, kMaxStrings> cappedOpenNotes;

    // Message thread only.
    double sampleRate = 48000.0;
    double rollingMinutes = kDefaultRollingMinutes;
    std::array<int, kMaxStrings> scoreTuning {};
    int numStrings = 6;
    int capoFret = 0;

    bool waitingForFirstNote = false;
    juce::int64 newestSample = 0;
    juce::int64 markInSample = -1, markOutSample = -1;
    std::atomic<juce::int64> clockSample { 0 };   ///< the audio thread's latest block start (the marks' "now")

    std::vector<CaptureRecord> incoming;
    std::vector<CapturedNote> notes;
    std::vector<CapturedChord> chords;
    std::vector<CapturedMeter> meters;
    std::vector<CapturedEvent> events;
    std::array<int, kMaxStrings> sounding {};     ///< index into notes, or -1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceCapture)
};

} // namespace luthier
