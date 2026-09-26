#pragma once

/*  The rhythm engine (rhythm-engine.md).

    A MIDI transformer, not a DSP module. It watches the notes the player is
    holding, works out what chord they make, voices that chord onto the
    fretboard, and then plays the voicing according to a pattern locked to the
    host transport. Nothing here touches audio.

    The six ground rules from section 0 are what the design is arranged around:

      1. It rewrites the event stream between the interpreter and the technique
         engine, so the rest of the instrument needs no knowledge of it.
      2. Nothing allocates once prepare() has run. The scheduler's output goes
         straight into the caller's PlayEventQueue, which is itself fixed-size.
      3. Scheduling is sample-accurate: a step's position is computed in beats
         from the host's own ppq position and converted to a sample offset.
      4. With the transport stopped it is silent, unless free-run is switched on
         deliberately.
      5. Patterns hold exact grid positions; humanisation is applied when an
         event is scheduled, never stored.
      6. Bypassing takes effect on the next block and releases anything the
         engine was holding, so it cannot leave a note ringing.
*/

#include "ChordDetector.h"
#include "Patterns.h"
#include "StrumGesture.h"
#include "BassStepGrid.h"
#include "../Support/TripleBuffer.h"   // SPEC-SWEEP RE-2

#include "../Model/Playing/PlayingEvents.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../Model/Playing/TuningEngine.h"

#include <atomic>

namespace luthier
{

//==============================================================================
/** What the host says about the transport this block. */
struct RhythmTransport
{
    double bpm = 120.0;
    double ppqPosition = 0.0;
    bool isPlaying = false;
};

//==============================================================================
/** How much a real hand differs from the grid (rhythm-engine 4). Read from the
    instrument's existing humanise settings rather than owned here. */
struct RhythmHumanise
{
    double timingMs = 8.0;        ///< Gaussian sigma on event timing.
    double velocityPercent = 12.0;
    double missPercent = 0.0;     ///< Chance a scheduled stroke simply does not happen.
    double ghostPercent = 0.0;    ///< Chance an extra muted stroke lands before a hit.
    double amount = 1.0;          ///< Scales all of the above.
};

//==============================================================================
/** Voicing preferences (rhythm-engine 3). */
enum class VoicingStyle
{
    open = 0, barre, triad, shell, drop2, drop3, power, rootless, wide,
    bass,      ///< ambiguity-resolutions 4.3; appended, so saved indices keep their meaning
    numStyles
};

const char* getVoicingStyleName (VoicingStyle style) noexcept;

//==============================================================================
class RhythmEngine
{
public:
    RhythmEngine();

    void prepare (double sampleRate, int maxBlockSize,
                  TuningEngine* tuning, RubricVoicer* voicer) noexcept;

    void reset() noexcept;

    void setNumStrings (int n) noexcept { numStrings = juce::jlimit (1, kMaxStrings, n); }
    int getNumStrings() const noexcept { return numStrings; }

    //==========================================================================
    void setEnabled (bool shouldBeEnabled) noexcept;
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    /** rhythm-engine 0.4: with the transport stopped the engine is silent unless
        this is switched on. */
    void setFreeRun (bool shouldFreeRun) noexcept { freeRun.store (shouldFreeRun, std::memory_order_relaxed); }
    bool isFreeRunning() const noexcept { return freeRun.load (std::memory_order_relaxed); }

    /** Installs a pattern. Message thread; the audio thread sees it on its next
        block through a double buffer. */
    void setPattern (const RhythmPattern& pattern);
    RhythmPattern getPattern() const;

    /*  bass-techniques 9 (MODEL-GAPS): the bass step grid. Message thread; the
        audio thread sees it next block. On a bass (setBassFamily) a grid with
        any step in it plays instead of the strum pattern; an empty one leaves
        the pattern in charge. */
    void setBassGrid (const BassStepGrid& grid);
    BassStepGrid getBassGrid() const;

    /** Whether the loaded guitar is a bass. The engine sets it every block. */
    void setBassFamily (bool isBass) noexcept { bassFamily.store (isBass, std::memory_order_relaxed); }
    bool isBassFamily() const noexcept { return bassFamily.load (std::memory_order_relaxed); }

    /** True when the grid, not the strum pattern, is what plays. */
    bool isBassGridActive() const noexcept;

    void setHumanise (const RhythmHumanise& h) noexcept;
    RhythmHumanise getHumanise() const noexcept;

    void setVoicingStyle (VoicingStyle style) noexcept { voicingStyle.store ((int) style, std::memory_order_relaxed); }

    /*  ambiguity-resolutions 4.3 / 4.7 (MODEL-GAPS): what the Bass style voices -
        the root only, root and fifth, or a walking approach. Saved with the
        rhythm engine's state; the RHYTHM tab shows it when the style is Bass. */
    void setBassPattern (RubricBassPattern p) noexcept { bassPattern.store ((int) p, std::memory_order_relaxed); }
    RubricBassPattern getBassPattern() const noexcept { return (RubricBassPattern) bassPattern.load (std::memory_order_relaxed); }
    VoicingStyle getVoicingStyle() const noexcept { return (VoicingStyle) voicingStyle.load (std::memory_order_relaxed); }

    /** 0..100: how many of the held notes get voiced (rhythm-engine 3). */
    void setVoicingDensity (double percent) noexcept { voicingDensity.store (juce::jlimit (0.0, 100.0, percent), std::memory_order_relaxed); }
    double getVoicingDensity() const noexcept { return voicingDensity.load (std::memory_order_relaxed); }

    void setHandPositionHint (int fret) noexcept { handPositionHint.store (juce::jlimit (0, 22, fret), std::memory_order_relaxed); }
    int getHandPositionHint() const noexcept { return handPositionHint.load (std::memory_order_relaxed); }

    /** SPEC-SWEEP (RE-12, rhythm-engine 3): the widest stretch, in frets, a
        voicing may ask of the hand (3-7, default 5; the Wide style adds one). */
    void setHandSpan (int frets) noexcept { handSpan.store (juce::jlimit (3, 7, frets), std::memory_order_relaxed); }
    int getHandSpan() const noexcept { return handSpan.load (std::memory_order_relaxed); }

    /*  Where the capo sits, 0 for none (rhythm-engine 3, 8.2).

        This used to be the rhythm engine's own field, and it was one of three
        capos in the build that did not know about each other: this one, which
        only moved the voicer's lowest fret; the fretboard's right-click capo,
        which only drew itself; and none at all in TuningEngine, so no capo
        anywhere changed the pitch of a note. gui-integration 19 names
        TuningEngine as the home, so that is where it lives now and this
        delegates.

        The setter is kept because rhythm-engine 8.2 asks for capo up/down here,
        but it is a view: setting it moves the one capo, and everything that reads
        a capo reads the same one. Before `prepare` there is no tuning engine to
        delegate to and it is a no-op, which is the same as it was. */
    void setCapoFret (int fret) noexcept;
    int getCapoFret() const noexcept;

    /** How even a strum is across its strings, 0..1 (rhythm-engine 4). */
    void setStrumEvenness (double evenness) noexcept { strumEvenness.store (juce::jlimit (0.0, 1.0, evenness), std::memory_order_relaxed); }

    /*  A genre kit's (or tune section's) six-string crossing time, ms: the kit
        default of ambiguity-resolutions 6, under the pattern's crossing_sps and
        over the global parameter. 0 or less clears it. */
    void setStrumDurationMs (double ms) noexcept { strumDurationMs.store (ms > 0.0 ? juce::jlimit (1.0, 250.0, ms) : 0.0, std::memory_order_relaxed); }
    double getStrumDurationMs() const noexcept { return strumDurationMs.load (std::memory_order_relaxed); }

    void setSeed (uint64_t seed) noexcept;

    //==========================================================================
    // strum-dynamics.md

    /** ambiguity-resolutions 6: where a strum's crossing velocity comes from. */
    enum class CrossingSource { step, pattern, kit, global };

    /** 7: the STRUM group's settings, pushed every block by the ParameterBridge.
        Their crossing velocity is the plugin-global default (1.1 source 4). */
    void setStrumSettings (const StrumSettings& s) noexcept { strumSettings = s.clamped(); }

    /** REALISM-B, string-interaction.md 6: the muted-string thump's level; 0 emits nothing. */
    void setMutedThumpLevel (double level) noexcept { mutedThumpLevel = juce::jlimit (0.0, 1.0, level); }
    StrumSettings getStrumSettings() const noexcept { return strumSettings; }

    double getStrumEvenness() const noexcept { return strumEvenness.load (std::memory_order_relaxed); }

    /** 6.3: Easy mode's Feel, 0..1. It scales whichever crossing source is in
        charge, and the evenness; 0.5 leaves both alone. */
    void setStrumFeel (double feel) noexcept { strumFeel.store (juce::jlimit (0.0, 1.0, feel), std::memory_order_relaxed); }
    double getStrumFeel() const noexcept { return strumFeel.load (std::memory_order_relaxed); }

    /** ambiguity-resolutions 6: the step's crossing_sps, else the pattern's,
        else the kit's default, else the global parameter. Before feel,
        direction and striker. */
    double resolveCrossingSps (const StrumStep& step, const RhythmPattern& pattern,
                               CrossingSource* source = nullptr) const noexcept;

    /** For the STRUM group: what the live pattern's plain steps take their
        crossing from. Message thread (reads the pattern under its lock). */
    CrossingSource getCrossingSource (double& sps) const;

    //==========================================================================
    /** Feeds the engine the block's MIDI so it can track held notes. Call before
        processBlock. */
    void handleMidi (const juce::MidiBuffer& midi, int64_t blockStartSample) noexcept;

    /** Generates this block's events. Returns the number of note-ons written.
        When the engine is bypassed or silent it writes nothing and returns 0,
        and the caller uses the interpreter's own events instead. */
    int processBlock (int numSamples, const RhythmTransport& transport,
                      PlayEventQueue& out) noexcept;

    /** True when the engine is producing the event stream this block, so the
        caller knows to suppress the interpreter's. */
    bool isDriving() const noexcept { return driving; }

    //==========================================================================
    // Live state, for the UI.

    ChordSymbol getCurrentChord() const noexcept { return currentChord; }

    /** The detector's reading of what is held now, whether or not the engine is
        on (notation-export 4: the capture's chord track). Audio thread. */
    ChordSymbol detectHeldChord() const noexcept { return detector.detect (detector.getHeldNotes(), detector.getNumHeldNotes()); }
    const ChordVoicing& getCurrentVoicing() const noexcept { return currentVoicing; }
    int getCurrentStep() const noexcept { return lastStepPlayed.load (std::memory_order_relaxed); }
    StrumType getNextStrumType() const noexcept { return (StrumType) nextStrumType.load (std::memory_order_relaxed); }

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    /** Re-detects the chord and re-voices it. Audio thread; the voicer is
        documented as running in microseconds. */
    void revoice() noexcept;

    // selectNotesForStyle is retired (ambiguity-resolutions 4, MODEL-GAPS):
    // the rubric voicer chooses the notes, style and density included.

    void scheduleStrum (const StrumStep& step, double sourceSps, int sampleOffset,
                        PlayEventQueue& out) noexcept;

    void scheduleFingerpick (const FingerpickStep& step, int sampleOffset,
                             PlayEventQueue& out) noexcept;

    void emitNote (int stringIndex, double velocity, bool muted, double chuck,
                   int strikerMaterial, int sampleOffset, PlayEventQueue& out, int finger = -1) noexcept;

    void releaseAll (int sampleOffset, PlayEventQueue& out) noexcept;

    // bass-techniques 9 (MODEL-GAPS).
    int processBassGrid (const BassStepGrid& grid, double startBeats, double endBeats,
                         double beatsPerSample, int numSamples, PlayEventQueue& out) noexcept;
    void emitBassStep (const BassStep& step, int sampleOffset, int lengthSamples, PlayEventQueue& out) noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    int numStrings = 6;

    TuningEngine* tuning = nullptr;
    RubricVoicer* voicer = nullptr;

    ChordDetector detector;
    ChordSymbol currentChord;
    ChordVoicing currentVoicing;
    bool voicingValid = false;

    std::atomic<bool> enabled { false };
    std::atomic<bool> freeRun { false };
    std::atomic<int> voicingStyle { (int) VoicingStyle::open };
    std::atomic<int> bassPattern { (int) RubricBassPattern::root };
    std::atomic<double> voicingDensity { 100.0 };
    std::atomic<int> handPositionHint { 0 };
    std::atomic<int> handSpan { 5 };   // SPEC-SWEEP RE-12
    // No capoFret here any more: TuningEngine owns the one capo. See setCapoFret.
    std::atomic<double> strumEvenness { 0.75 };   // strum-dynamics 4 / 7
    std::atomic<double> strumDurationMs { 0.0 };  // no kit crossing until a kit sets one

    StrumSettings strumSettings;
    double mutedThumpLevel = 0.0;   // REALISM-B
    std::atomic<double> strumFeel { 0.5 };
    StrumGesture gesture;
    juce::uint32 strumCount = 0;

    /** A kit's strum_duration_ms is the time to cross six strings, the strum
        strum-dynamics 1's table is written for. */
    static constexpr int kKitReferenceStrings = 6;

    // --- pattern, humanise and bass grid: message thread -> audio thread -----------
    // SPEC-SWEEP (RE-2): each is a TripleBuffer, so the audio thread picks up the
    // newest value at the top of a block and reads it by reference: no copy of a
    // pattern's name/tags, no lock and no free on the audio thread. The writer
    // keeps its own copy for message-thread getters.
    mutable juce::CriticalSection patternLock;   // serialises writers only
    RhythmPattern writtenPattern;
    TripleBuffer<RhythmPattern> patternBuffer;

    BassStepGrid writtenBassGrid;
    TripleBuffer<BassStepGrid> bassGridBuffer;
    std::atomic<bool> bassGridHasSteps { false };
    std::atomic<bool> bassFamily { false };

    mutable juce::CriticalSection humaniseLock;  // serialises writers only
    RhythmHumanise writtenHumanise;
    TripleBuffer<RhythmHumanise> humaniseBuffer;

    // --- transport ------------------------------------------------------------------
    double freeRunPpq = 0.0;
    double lastPpq = -1.0;
    bool wasPlaying = false;
    std::atomic<bool> pendingRelease { false };   // SPEC-SWEEP: set from the message thread (setEnabled)

    /** Which strings the engine currently has ringing, so it can release them. */
    uint16_t soundingMask = 0;

    bool driving = false;

    std::atomic<int> lastStepPlayed { -1 };
    std::atomic<int> nextStrumType { (int) StrumType::rest };

    RtRandom rng { 0x12345678ull };
    uint64_t seed = 0x12345678ull;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmEngine)
};

} // namespace luthier
