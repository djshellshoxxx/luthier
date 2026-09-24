#pragma once

/*  String scraping (string-scraping.md): a pick, nail or thumb drawn along the
    length of a wound string, catching each winding in turn.

    No sample and no convolution (0.1). The pick's position moves along the
    string; every time it crosses from one winding to the next, that is a
    catch, and a catch is a short impulse into the string's excitation input
    at the pick's position (1). So:

    - a plain string has no windings to cross and stays silent (0.2) - it
      falls out of the winding count, it is not a special case;
    - the catch rate is windings per mm x speed, so a fast scrape is a zipper
      and a slow one is a ratchet of separate clicks (0.3);
    - a swept gesture (mod wheel, pedal, aftertouch, a CC) catches exactly as
      far as the controller moved it, no more (2).

    The position is applied the way a pluck position is: an impulse at a
    fraction b of the vibrating length from the bridge reaches the bridge
    twice, once directly and once inverted after b of a period. That comb is
    what makes a catch near the 12th fret sound different from one near the
    bridge, and why nothing much happens right at the saddle.

    One voice per string. A second trigger on a string that is still scraping
    queues behind it (technique-cascade.md 2, scrape x scrape = queue); a tap,
    a slap or a slide landing on it preempts it with a 10 ms fade (3.3).

    Audio thread, apart from the request*() calls. Nothing allocates after
    prepare(); every random choice is a hash of the seed and the winding
    index, so a repeated performance repeats and a reversed scrape is the
    forward one mirrored (6).
*/

#include "PlayingNoise.h"
#include <array>
#include <atomic>
#include <limits>
#include <vector>

namespace luthier
{

//==============================================================================
/** What touches the winding (2, "Tool"). */
enum class ScrapeTool
{
    pick = 0,
    nail,
    thumb,
    numTools
};

/** 2, "Direction". Hold-and-sweep hands the position to the sweep source. */
enum class ScrapeDirection
{
    bridgeToNut = 0,
    nutToBridge,
    holdAndSweep,
    numDirections
};

/** 2, "Sweep source": what drives the pick's position during the gesture. */
enum class ScrapeSweepSource
{
    automatic = 0,  ///< a fixed-duration move from one end of the range to the other
    modWheel,       ///< CC 1
    expression,     ///< CC 4, the foot controller (engine.md 2's map)
    aftertouch,     ///< channel pressure or polyphonic aftertouch, any channel
    customCc,       ///< ScrapeSettings::sweepCc
    numSources
};

/** 2, "Scrape trigger". Only one MIDI source listens at a time, so a
    keyswitch never steals a note from a player who is not using it. */
enum class ScrapeTriggerSource
{
    keyswitch = 0,  ///< kScrapeKeyswitch; kRakeDown/UpKeyswitch for the pick-noise rake
    controller,     ///< ScrapeSettings::triggerCc, on at 64 and above
    mpeZone,        ///< any note on kZoneChannel
    buttonOnly,     ///< the Techniques tab / Playing strip only
    numSources
};

/** The factory scrapes (4). */
enum class ScrapePreset
{
    classicRock = 0,
    metalZipper,
    slowRatchet,
    nailScrape,
    modwheelSweep,
    numPresets
};

//==============================================================================
/** One scrape on one string (1). Positions are mm from the bridge saddle. */
struct ScrapeGesture
{
    int stringIndex = 0;
    double startPositionMm = 200.0;
    double endPositionMm = 900.0;
    double durationMs = 600.0;
    double pressure = 0.5;              ///< 0-1, how hard the pick pushes
    ScrapeTool tool = ScrapeTool::pick;
    double angleDegrees = 20.0;         ///< positive tilts toward the direction of travel

    /*  Not in 1's struct: whether the position follows the sweep source
        rather than moving by itself over durationMs. A controlled gesture
        runs until it is released. */
    bool controlled = false;
};

/** Section 2's controls, as the parameters set them. */
struct ScrapeSettings
{
    bool armed = false;

    ScrapeTriggerSource trigger = ScrapeTriggerSource::keyswitch;
    ScrapeDirection direction = ScrapeDirection::bridgeToNut;
    ScrapeSweepSource sweepSource = ScrapeSweepSource::automatic;
    int triggerCc = 85;                 ///< undefined in the MIDI spec, free in engine.md 2's map
    int sweepCc = 16;                   ///< general purpose 1

    double startPositionMm = 200.0;
    double endPositionMm = 900.0;
    double durationMs = 600.0;
    double pressure = 0.5;
    ScrapeTool tool = ScrapeTool::pick;
    double angleDegrees = 20.0;

    /** Bit per string, bit 0 the high E. 0 means "the wound strings" (2's default). */
    int stringMask = 0;

    /** 2: a trigger this soon after the last one is dropped. */
    double retriggerMs = 200.0;

    /*  Level trim, 1 at pick_scrape_amount's default of 0.25 (coverage C-29:
        the pick-noise scrape amount stays as the scrape's level, so there is
        one scrape level control). 0 is silent and free (pick-noise.md 0.5). */
    double level = 1.0;

    /** Section 4's factory settings, armed. "Low 3" is counted from the lowest string. */
    static ScrapeSettings fromPreset (ScrapePreset preset, int numStrings) noexcept;
    static const char* getPresetName (ScrapePreset preset) noexcept;
};

//==============================================================================
class ScrapeEngine
{
public:
    /*  engine-technique-layer.md 3.6: keyswitches from a range nothing plays.
        A five-string bass's low B is MIDI 23 and a drop-A bass 21; these sit
        an octave under that. Notes 13 and 14 are pick-noise.md 5's rake. */
    static constexpr int kScrapeKeyswitch = 12;
    static constexpr int kRakeDownKeyswitch = 13;
    static constexpr int kRakeUpKeyswitch = 14;

    /** The MPE zone trigger's channel: the upper zone's manager channel. */
    static constexpr int kZoneChannel = 16;

    /** Where a scrape can reach: this far inside the saddle and the nut. */
    static constexpr double kEdgeMm = 5.0;

    /** A controlled gesture nobody releases stops by itself after this long. */
    static constexpr double kMaxHoldSeconds = 10.0;

    /** technique-cascade.md 3.3: a preempted gesture fades over 10 ms. */
    static constexpr double kPreemptFadeSeconds = 0.010;

    /*  A catch's peak into the string's excitation input at pressure 1, a pick
        square to the string, a full-depth winding. The string passes about
        what lands on its partials, so this is set by ear against a pluck;
        the tests hold the relations, not this number. */
    static constexpr double kCatchReference = 0.6;

    /** 1: the pick loading the string raises its tension. Cents at pressure 1. */
    static constexpr double kLoadCentsAtFullPressure = 6.0;

    static constexpr int kMaxEventsPerBlock = 64;
    static constexpr int kMaxRecordsPerBlock = kMaxStrings * 2;

    ScrapeEngine();

    /** Allocates the per-string buffers and position lines. Message thread. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Silences everything, drops queued gestures, snaps every smoother. */
    void reset() noexcept;

    /** The seed every per-winding irregularity derives from (character-wear.md 0.1). */
    void setSeed (juce::uint32 seed) noexcept { seed32 = seed; }

    void setSettings (const ScrapeSettings& s) noexcept;
    const ScrapeSettings& getSettings() const noexcept { return settings; }

    //==========================================================================
    // What the engine tells it about the strings, each block it is busy.

    void setNumStrings (int n) noexcept { numStrings = juce::jlimit (1, kMaxStrings, n); }
    void setScaleLengthMm (double mm) noexcept { scaleLengthMm = juce::jmax (100.0, mm); }

    /*  One string's winding, sounding pitch, fret and bend. The fret sets the
        vibrating length (a pick past the fretting finger barely moves the
        string); the bend stretches the winding slightly (5). */
    void setString (int stringIndex, const StringNoiseInfo& info, double frequencyHz,
                    double fret, double bendCents) noexcept;

    /*  technique-cascade.md 3.4: a string under the slide bar belongs to the
        slide. New scrapes on it are dropped and one in progress fades out. */
    void setStringBlocked (int stringIndex, bool blocked) noexcept;

    /** 5: scraping through a palm mute is duller and thumpier. 0-1. */
    void setMuteAmount (double amount) noexcept { muteAmount = juce::jlimit (0.0, 1.0, amount); }

    //==========================================================================
    // Triggers.

    /*  3's interface: starts (or queues) one gesture at this sample of the
        next processBlock. Not subject to the retrigger threshold, which is a
        property of the player's triggers, not of the engine. */
    void trigger (const ScrapeGesture& gesture, int sampleOffset = 0) noexcept;

    /*  The settings' gesture on every masked string, at this sample of the
        next processBlock, as a keyswitch, the trigger CC, the zone or the
        button fires it. Dropped silently inside the retrigger threshold (6);
        getDroppedTriggerCount() counts those. */
    void triggerFromSettings (int sampleOffset) noexcept;

    /** Lets go of every controlled gesture (the keyswitch or zone note-off). */
    void releaseControlled (int sampleOffset) noexcept;

    /** technique-cascade.md 3: a tap, slap or slide took this string. */
    void preempt (int stringIndex) noexcept;

    /** Stops everything now, for panic. */
    void stopAll() noexcept;

    /*  The sweep source's value, 0-1, from this sample on. handleMidi feeds
        it from the chosen source; the tests and the modulation matrix can
        drive it directly. */
    void setSweepValue (double value, int sampleOffset) noexcept;
    double getSweepValue() const noexcept { return sweepValue; }

    // From the UI (the Techniques tab's button, the Playing strip, the Easy-mode
    // rake). Any thread; picked up at the start of the next block.
    void requestTrigger() noexcept   { uiRequests.fetch_or (requestTriggerBit); }
    void requestRelease() noexcept   { uiRequests.fetch_or (requestReleaseBit); }
    void requestRake (bool downward) noexcept { uiRequests.fetch_or (downward ? requestRakeDownBit : requestRakeUpBit); }

    //==========================================================================
    /*  Reads the block's MIDI for the scrape's triggers and sweep controller
        (input-routing.md 5: techniques consume their keyswitches before the
        rhythm engine sees the notes). Returns `in` untouched if nothing was
        consumed, otherwise `filtered` holding everything else - which must
        have been sized in advance so copying into it does not allocate.
        Disarmed, it returns `in` without looking at it. */
    const juce::MidiBuffer& handleMidi (const juce::MidiBuffer& in, juce::MidiBuffer& filtered) noexcept;

    /** True when there is anything to do this block: the engine only feeds
        setString() and reads the outputs when it is. One test when idle. */
    bool isBusy() const noexcept
    {
        return activeVoices > 0 || numEvents > 0 || numPendingTriggers > 0 || loadSettling
                 || uiRequests.load (std::memory_order_relaxed) != 0;
    }

    /** 3: schedules and renders this block's catches. */
    void processBlock (int numSamples) noexcept;

    /** True if the last processBlock wrote the outputs below. */
    bool hasOutput() const noexcept { return renderedThisBlock; }

    /** A string's catches for the last block, into its excitation input. Valid
        for the block's length when hasOutput(). */
    const double* getExcitation (int stringIndex) const noexcept
    {
        return excitation[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)].data();
    }

    /** The catch trains summed, before the position comb, for Aux 8 (pick-noise.md 1.3). */
    const double* getNoiseOutput() const noexcept { return noiseOut.data(); }

    /** 1: the pick's load on the string, cents, for the block just rendered. */
    double getPitchOffsetCents (int stringIndex) const noexcept
    {
        return loadCents[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    }

    /*  While a mod-wheel or aftertouch sweep is in progress those controllers
        are the pick's, not the vibrato's (the interpreter maps both to
        vibrato depth by default). */
    bool ownsVibratoControllers() const noexcept;

    /*  pick-noise.md 5's rake, asked for by keyswitch or UI this block. The
        engine hands it to PlayingNoise::startScrape. */
    bool takeRake (bool& downward, double& seconds) noexcept;

    //==========================================================================
    /*  midi-export.md 6: one record per gesture started this block, at its
        sample, for the Luthier SysEx out and the noise-event strip. */
    struct GestureRecord
    {
        int stringIndex = 0;
        int offset = 0;
        float level = 0.0f;
        float durationMs = 0.0f;
    };

    int getNumRecords() const noexcept { return numRecords; }
    const GestureRecord& getRecord (int i) const noexcept
    {
        return records[(size_t) juce::jlimit (0, kMaxRecordsPerBlock - 1, i)];
    }

    //==========================================================================
    // Live state, for the fretboard's scrape trail and the pill's flash
    // (gui-techniques-updates.md 2, 4). Any thread.

    /** The pick's position on a string in mm from the bridge, or -1 when it is not on it. */
    float getUiPositionMm (int stringIndex) const noexcept
    {
        return uiPosition[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)].load (std::memory_order_relaxed);
    }

    /** Counts gestures started; the pill flashes when it moves. */
    juce::uint32 getFiredCount() const noexcept { return firedCount.load (std::memory_order_relaxed); }

    //==========================================================================
    // For the tests.

    bool isStringActive (int stringIndex) const noexcept;
    bool isStringMoving (int stringIndex) const noexcept;
    juce::int64 getCatchCount (int stringIndex) const noexcept;
    int getDroppedTriggerCount() const noexcept { return droppedTriggers; }

    /** The catch level a gesture would have on a string, before per-winding irregularity. */
    double catchLevelFor (const ScrapeGesture& g, int stringIndex) const noexcept;

private:
    enum RequestBits : int
    {
        requestTriggerBit   = 1,
        requestReleaseBit   = 2,
        requestRakeDownBit  = 4,
        requestRakeUpBit    = 8
    };

    struct Event
    {
        enum class Type { triggerOn, triggerOff, sweep, rakeDown, rakeUp };
        Type type = Type::triggerOn;
        int offset = 0;
        double value = 0.0;
    };

    struct Voice
    {
        bool active = false;         ///< anything left to render: movement, a pulse, the comb's tail
        bool moving = false;         ///< the pick is on the string
        bool controlled = false;
        bool blocked = false;

        double positionMm = 0.0;
        double fromMm = 0.0, toMm = 0.0;
        double mmPerSample = 0.0;    ///< automatic gestures
        double nearMm = 0.0, farMm = 0.0;
        bool reversed = false;       ///< controlled: 0 is the far end
        int samplesLeft = 0;

        juce::int64 winding = 0;     ///< floor (position x windings per mm)
        double level = 0.0;          ///< catch peak before irregularity
        double widthMs = 0.1;
        double angleDegrees = 0.0;
        double pressure = 0.0;
        double depth = 0.0;

        int pulseIndex = 0, pulseLength = 0;
        double pulseAmp = 0.0;

        double fade = 1.0, fadeStep = 0.0;
        int tail = 0;                ///< samples the comb still owes after the last pulse

        bool hasQueued = false;
        ScrapeGesture queued;
    };

    struct StringState
    {
        StringNoiseInfo info;
        double hz = 110.0;
        double fret = 0.0;
        double bendCents = 0.0;
    };

    void applyEvent (const Event& e, int sampleOffset) noexcept;
    void fireFromSettings (int offset) noexcept;
    void startVoice (const ScrapeGesture& g, int offset) noexcept;
    void finishVoice (int stringIndex, int sampleOffset) noexcept;
    ScrapeGesture gestureFor (int stringIndex) const noexcept;
    ScrapeSweepSource effectiveSweepSource() const noexcept;
    int effectiveMask() const noexcept;
    double windingsPerMm (int stringIndex) const noexcept;
    double vibratingLengthMm (int stringIndex) const noexcept;
    double clampPosition (double mm) const noexcept;
    double controlledTarget (const Voice& v) const noexcept;
    void pushEvent (const Event& e) noexcept;
    void renderSample (int stringIndex, int i) noexcept;

    static double toolLevel (ScrapeTool t) noexcept;
    static double toolWidthMs (ScrapeTool t) noexcept;
    static double angleFactor (double degrees) noexcept;

    double sr = 48000.0;
    int maxBlock = 512;
    int numStrings = 6;
    double scaleLengthMm = 648.0;
    double muteAmount = 0.0;
    juce::uint32 seed32 = 0x5c4a9e11u;

    ScrapeSettings settings;

    std::array<Voice, kMaxStrings> voices {};
    std::array<StringState, kMaxStrings> strings {};
    int activeVoices = 0;

    // The position comb: each string's catch train, delayed by b of a period.
    std::array<std::vector<double>, kMaxStrings> rings;
    int ringMask = 0;
    int ringWrite = 0;

    std::array<std::vector<double>, kMaxStrings> excitation;
    std::vector<double> noiseOut;
    bool renderedThisBlock = false;

    std::array<double, kMaxStrings> loadCents {};
    bool loadSettling = false;   ///< a load is still easing off, so a block must run

    // The sweep: the controller's value, and the position smoothing that keeps
    // a 7-bit controller's steps from firing ten windings at once.
    double sweepValue = 0.0;
    double sweepSmoothed = 0.0;
    double sweepCoefficient = 0.0;

    std::array<Event, kMaxEventsPerBlock> events {};
    int numEvents = 0;

    // trigger() calls land here and start inside the next processBlock.
    std::array<ScrapeGesture, kMaxStrings> pendingGestures {};
    std::array<int, kMaxStrings> pendingOffsets {};
    int numPendingTriggers = 0;

    std::atomic<int> uiRequests { 0 };

    bool rakeWanted = false, rakeDownward = true;
    bool triggerCcDown = false;                   ///< the trigger CC's last state, for its edges

    juce::int64 clock = 0;                        ///< samples since reset
    juce::int64 lastTriggerAt = std::numeric_limits<juce::int64>::min() / 2;
    int droppedTriggers = 0;
    std::array<juce::int64, kMaxStrings> catchTotals {};   ///< windings crossed since reset

    std::array<GestureRecord, kMaxRecordsPerBlock> records {};
    int numRecords = 0;

    std::array<std::atomic<float>, kMaxStrings> uiPosition;
    std::atomic<juce::uint32> firedCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScrapeEngine)
};

} // namespace luthier
