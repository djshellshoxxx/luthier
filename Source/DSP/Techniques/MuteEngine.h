#pragma once

/*  MuteEngine (engine-technique-layer.md 1, muting-rhythm.md 2-4).

    Muting as a rhythmic voice. The rules for what a mute is and which one a
    strike gets live in Rhythm/Muting; this is the engine side:

      - the live mute grid (muting-rhythm 2): sixteen sixteenths the user
        paints, synced to host tempo, applied to played notes while the
        transport runs and muting is armed;
      - stamping: every note-on of a block gets its final mute - the master
        mode, the pattern step's or the live grid's, the chuka source, the
        humanise - before it is scheduled (engine-technique-layer 2, step 2c);
      - what the strike then does to the string (Muting::dampingFor), and the
        fret mute's timed release;
      - the rock-spread fretting hand: spare fingers deaden the strings a
        muted strike does not play (string-interaction.md's palm-mute spread).

    Idle (disarmed and no note carrying a mute) is one flag test per note.
    The grid is written by the message thread through atomics; everything
    else runs on the audio thread and never allocates.
*/

#include "../../Rhythm/Muting.h"
#include "../../Model/Playing/PlayingEvents.h"
#include <array>
#include <atomic>

namespace luthier
{

class MuteEngine
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    //==========================================================================
    void setSettings (const MuteSettings& s) noexcept { settings = s; }
    const MuteSettings& getSettings() const noexcept { return settings; }

    /** strum-dynamics 6.1's chuck damping, which a chuka plays with. */
    void setChuckDamping (double d) noexcept { chuckDamping = juce::jlimit (0.0, 1.0, d); }

    void setSeed (juce::uint64 s) noexcept { seed = s; }

    //==========================================================================
    // The live grid (muting-rhythm 2). Any thread.

    void setLiveStep (int step, MuteType type) noexcept;
    MuteType getLiveStep (int step) const noexcept;
    void clearLiveGrid() noexcept;

    /** 6: writes a grid preset's sixteen cells. */
    void applyGridPreset (int presetIndex) noexcept;

    /** The step the live grid last applied, for the editor's playhead; -1 when stopped. */
    int getPlayingStep() const noexcept { return playingStep.load (std::memory_order_relaxed); }

    //==========================================================================
    /*  Stamps every note-on in `queue` with its final mute (4). `fromRhythm`
        is true for the rhythm engine's stream, whose notes carry their
        pattern step's mute already; played notes read the live grid.
        Audio thread. */
    void apply (PlayEventQueue& queue, double sampleRate, double bpm, double ppq,
                bool playing, bool fromRhythm) noexcept;

    /** What a stamped note does to its string. */
    MuteDamping dampingFor (const NoteOnEvent& e) const noexcept;

    /** True when the string's other strings are deadened by the fretting hand (3). */
    bool deadensOtherStrings (const NoteOnEvent& e) const noexcept;

    //==========================================================================
    // Fret mute (1): the note rings, then the finger lets go.

    void noteStruck (int stringIndex, const MuteDamping& damping, juce::int64 atSample, double sampleRate) noexcept;
    void noteReleased (int stringIndex) noexcept;

    /** True once, when string `s`'s fret mute is due by `upToSample`; `t60` is its stop. */
    bool takeDueRelease (int s, juce::int64 upToSample, double& t60) noexcept;

    //==========================================================================
    /** For the fretboard's mute-zone shading and the pill's firing dot. */
    int getLastMuteType() const noexcept { return lastMuteType.load (std::memory_order_relaxed); }
    juce::uint32 getFireCount() const noexcept { return fireCount.load (std::memory_order_relaxed); }

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    MuteSettings settings;
    double chuckDamping = 0.92;
    juce::uint64 seed = 0x6D757465ull;
    double sr = 48000.0;

    std::array<std::atomic<int>, kLiveMuteSteps> liveGrid {};
    std::atomic<int> playingStep { -1 };
    std::atomic<int> lastMuteType { 0 };
    std::atomic<juce::uint32> fireCount { 0 };

    std::array<juce::int64, kMaxStrings> releaseAt {};
    std::array<double, kMaxStrings> releaseT60 {};
};

} // namespace luthier
