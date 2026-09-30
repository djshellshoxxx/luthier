#pragma once

/*  TapEngine (two-hand-tapping.md 4, engine-technique-layer.md 1).

    A tap is a fret event, not a pluck (0.1): a finger hammers the string onto
    a fret and the segment from that fret to the bridge sounds. While it is
    held it is a movable capo; several taps on one string are capos in series
    and the highest wins the pitch (10). Lifting it returns the string to the
    fretting hand's note - with a lateral flick if the pull-off is plucked
    (2), with none if auto pull-off is off.

    This class keeps the taps and turns triggers into events: which string a
    MIDI tap lands on, the tap strength curve, the concurrency cap, fret snap,
    timed gestures from the fretboard and scripts. LuthierEngine plays the
    events on its strings, because the strings are its (StringEngine's
    excitation and pitch inputs are reused, not changed - 4).

    Triggers (3): notes on the tap channel (an MPE-style second channel), the
    notes played under keyswitch 19, or the fretboard's tap layer. All arrive
    through TechniqueTriggers except the fretboard's, which come through a
    small lock-free queue.

    Audio thread, apart from requestGesture(). Nothing allocates.
*/

#include "../Common/DspCommon.h"
#include "../../Model/Playing/TechniqueTriggers.h"
#include <array>
#include <atomic>

namespace luthier
{

/** 3, "Tap trigger source". Saved as a choice index: append only. */
enum class TapSource { midiChannel = 0, keyswitch, fretboard, numSources };

enum class TapHand { left = 0, right };

/** 1: one tap, as a script or the fretboard describes it. */
struct TapGesture
{
    int stringIndex = 0;
    double fret = 12.0;              ///< continuous: fret snap decides whether it is rounded (3)
    double strength = 0.8;           ///< 0-1
    TapHand hand = TapHand::right;
    bool pullOffAfter = true;
    double pullOffTargetFret = -1.0; ///< -1: the fretting hand's own fret
    double durationMs = -1.0;        ///< -1: the default duration; 0: held until released
};

/** 3: the TAP sub-tab's controls. */
struct TapSettings
{
    bool armed = false;
    TapSource source = TapSource::midiChannel;
    int channel = 2;                 ///< the right hand's channel
    double strengthCurve = 0.0;      ///< -1 soft-touch .. 0 linear .. +1 hard
    bool autoPullOff = true;
    int hammerOnThreshold = 40;      ///< MIDI velocity
    double lateralFlick = 0.5;
    double defaultDurationMs = 200.0;
    int maxConcurrent = 2;
    bool fretSnap = true;

    /** The TechniqueTriggers config these settings imply. */
    TechniqueTriggerConfig triggerConfig() const noexcept;

    /** 3: velocity (0-1) to tap strength through the curve. */
    double strengthFor (double velocity) const noexcept;

    /** 5: the legato window a hammer-on is read within. */
    static constexpr double kHammerOnWindowMs = 150.0;
};

/** What the engine plays. */
struct TapEvent
{
    enum class Kind { tapOn, tapOff };

    Kind kind = Kind::tapOn;
    int stringIndex = 0;
    double fret = 0.0;               ///< tapOn: the tap's fret
    double strength = 0.0;           ///< tapOn: the strike; tapOff: the flick's excitation (0: none)
    double revealFret = 0.0;         ///< tapOff: what the string sounds after (-1: nothing held, it stops)
    int offset = 0;
};

//==============================================================================
class TapEngine
{
public:
    static constexpr int kMaxTapsPerString = 8;
    static constexpr int kMaxEvents = 64;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSettings (const TapSettings& s) noexcept;
    const TapSettings& getSettings() const noexcept { return settings; }

    /** The instrument: string count, open notes (MIDI, for the tuning in use) and frets. */
    void setInstrument (int numStrings, const double* openMidiNotes, int maxFrets) noexcept;

    //==========================================================================
    /** The fretboard or a script (1). Any thread; played at the next block. */
    void requestGesture (const TapGesture& g) noexcept;

    /** 4: a manual tap-off. Any thread. */
    void requestRelease (int stringIndex, double fret) noexcept;

    //==========================================================================
    /*  One block. `lhFrets` / `lhHeld` are what the fretting hand holds per
        string now (its fret and whether a note is down); `blocked` marks a
        string the slide bar holds, which a tap may not take
        (technique-cascade.md 3.4). Fills the events. */
    void processBlock (int numSamples, const TechniqueTriggers& triggers,
                       const double* lhFrets, const bool* lhHeld, const bool* blocked) noexcept;

    int getNumEvents() const noexcept { return numEvents; }
    const TapEvent& getEvent (int i) const noexcept { return events[(size_t) juce::jlimit (0, kMaxEvents - 1, i)]; }

    //==========================================================================
    /** True while at least one tap holds string `s`. */
    bool isTapping (int s) const noexcept;

    /** The fret string `s` sounds at: its highest tap, else `fretted`. */
    double soundingFret (int s, double fretted) const noexcept;

    /** The fretting hand let go of string `s` while a tap holds it. */
    void fretHandReleased (int s) noexcept;

    /** 3: the tap layer on the fretboard is live. */
    bool acceptsFretboardTaps() const noexcept { return settings.armed && settings.source == TapSource::fretboard; }

    //==========================================================================
    // For the fretboard's tap markers (gui-techniques-updates 4). Any thread.
    struct Marker { float fret = -1.0f; int string = -1; bool held = false; juce::uint32 releasedMs = 0; };
    static constexpr int kMaxMarkers = 16;
    Marker getMarker (int i) const noexcept;
    juce::uint32 getFireCount() const noexcept { return fireCount.load (std::memory_order_relaxed); }

    /** Where a MIDI tap on `note` lands: a string and fret, or false. */
    bool placeNote (int note, const double* lhFrets, const bool* lhHeld, int& string, double& fret) const noexcept;

private:
    struct Tap { double fret = 0.0; double strength = 0.0; int note = -1; double remaining = -1.0; bool pullOff = true; double target = -1.0; juce::uint32 order = 0; };

    void tapOn (int s, double fret, double strength, int note, double durationSeconds, bool pullOff,
                double target, int offset, const double* lhFrets, const bool* lhHeld, const bool* blocked) noexcept;
    void tapOff (int s, int slot, int offset, const double* lhFrets, const bool* lhHeld) noexcept;
    void push (const TapEvent& e) noexcept;

    TapSettings settings;
    double sr = 48000.0;
    int numStrings = 6;
    int maxFrets = 24;
    std::array<double, kMaxStrings> openNotes {};

    std::array<std::array<Tap, kMaxTapsPerString>, kMaxStrings> taps {};
    std::array<int, kMaxStrings> numTaps {};
    std::array<bool, kMaxStrings> fretHandGone {};
    juce::uint32 orderCounter = 0;

    std::array<TapEvent, kMaxEvents> events {};
    int numEvents = 0;

    // requestGesture's queue: single consumer, a few producers (message thread).
    static constexpr int kQueue = 32;
    std::array<TapGesture, kQueue> requests {};
    std::array<std::atomic<int>, kQueue> requestState {};   // 0 free, 1 writing, 2 ready tap, 3 ready release
    std::array<std::atomic<juce::uint32>, kQueue> requestOrder {};
    std::atomic<juce::uint32> requestCounter { 0 };

    std::array<std::atomic<float>, kMaxMarkers> markerFret {};
    std::array<std::atomic<int>, kMaxMarkers> markerString {};
    std::array<std::atomic<juce::uint32>, kMaxMarkers> markerReleased {};
    std::atomic<juce::uint32> fireCount { 0 };
};

} // namespace luthier
