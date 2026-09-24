#pragma once

/*  The riff audition player (riff-library.md 5.3).

    A MIDI-level player: it adds no audio module and plays through the
    engine, whose triggerNote the notes reach as they reach it from a played
    note (so capture, fretboard, piano roll and meters all see them). It owns
    nothing on the audio thread that it has to free.

    Hand-over, TunePlayer's pattern:
      - the message thread calls setCompiled(), which fills a waiting slot
        under a SpinLock;
      - the audio thread takes it with a ScopedTryLock: at once while
        stopped or when asked to (a new riff, a guitar swap), otherwise at
        the next beat;
      - the displaced riff goes to a retired slot, which the processor's
        timer empties (collectGarbage). The audio thread never frees.
    Transport commands (play, stop, loop, start quantise, clock, tempo) are
    atomics.

    Clock: Auto follows the host while it plays (starting on its next bar
    line, or beat, and looping on whole bars; a host stop ends the riff, a
    host jump ends every note and re-locates) and runs its own clock
    otherwise; Own always runs its own, at the riff's tempo times a factor
    (0.25-2) or at an absolute bpm. Strum spacing stays in seconds.

    Bends go out as BendEvents, at most one per bending string per 64
    samples, interpolated from the compiled breakpoints plus the vibrato
    sine. Riff events take at most 96 of the queue's 192 slots a sub-block:
    past that bend points are dropped, never notes, and an overflow counter
    goes to Diagnostics.
*/

#include "RiffCompiler.h"

#include <array>
#include <atomic>
#include <memory>

namespace luthier
{

class RiffPlayer
{
public:
    enum class ClockMode { automatic = 0, own };
    enum class StartQuantise { nextBar = 0, nextBeat, immediate };

    static constexpr int kMaxSlotsPerSubBlock = 96;
    static constexpr int kBendIntervalSamples = 64;
    static constexpr int kRetiredSlots = 4;

    /** One sub-block's output: the events, and for each note-on the bend it
        starts at (a prebend), in cents, which the engine builds its pitch from. */
    struct Output
    {
        PlayEventQueue queue;
        std::array<double, PlayEventQueue::kCapacity> startCents {};

        void clear() noexcept { queue.clear(); }
    };

    RiffPlayer();
    ~RiffPlayer();

    void prepare (double sampleRate);

    //==========================================================================
    // Message thread.

    /** Hands a compiled riff over. `immediate` swaps at the next sub-block,
        ending the old riff's notes (a new riff, a guitar change); otherwise it
        swaps at the next beat while playing. nullptr clears. */
    void setCompiled (std::shared_ptr<const CompiledRiff> compiled, bool immediate = false);

    /** Frees retired riffs. Call from a timer on the message thread. */
    void collectGarbage();

    /** The newest riff handed over (waiting or playing), or nullptr. */
    std::shared_ptr<const CompiledRiff> getCompiled() const;

    void play() noexcept;
    void stop() noexcept;
    void togglePlay() noexcept { if (isPlaying() || isWaiting()) stop(); else play(); }

    /** An engine reset (a preset load) silenced the strings: the notes the
        player thought were sounding are gone, and playback continues from
        the next event. */
    void notifyEngineReset() noexcept { resetPending.store (true, std::memory_order_relaxed); }

    void setLooping (bool shouldLoop) noexcept          { looping.store (shouldLoop, std::memory_order_relaxed); }
    void setClockMode (ClockMode mode) noexcept         { clockMode.store ((int) mode, std::memory_order_relaxed); }
    void setStartQuantise (StartQuantise q) noexcept    { startQuantise.store ((int) q, std::memory_order_relaxed); }
    void setTempoFactor (double factor) noexcept;
    /** An absolute own-clock tempo, or <= 0 to use the riff's tempo x the factor. */
    void setAbsoluteBpm (double bpm) noexcept;

    bool isLooping() const noexcept              { return looping.load (std::memory_order_relaxed); }
    ClockMode getClockMode() const noexcept      { return (ClockMode) clockMode.load (std::memory_order_relaxed); }
    StartQuantise getStartQuantise() const noexcept { return (StartQuantise) startQuantise.load (std::memory_order_relaxed); }
    double getTempoFactor() const noexcept       { return tempoFactor.load (std::memory_order_relaxed); }
    double getAbsoluteBpm() const noexcept       { return absoluteBpm.load (std::memory_order_relaxed); }

    //==========================================================================
    // Any thread: what the audio thread last published.

    bool isPlaying() const noexcept   { return playingFlag.load (std::memory_order_relaxed); }
    bool isWaiting() const noexcept   { return waitingFlag.load (std::memory_order_relaxed); }
    /** The riff beat under the playhead; -1 when stopped. */
    double getBeatPosition() const noexcept { return beatPosition.load (std::memory_order_relaxed); }
    /** Counts every render that moved the playhead (the stale rule's heartbeat). */
    juce::uint32 getPositionStamp() const noexcept { return positionStamp.load (std::memory_order_relaxed); }
    /** The tempo the last render played at. */
    double getPlayingBpm() const noexcept   { return playingBpm.load (std::memory_order_relaxed); }
    bool isFollowingHost() const noexcept   { return followingHostFlag.load (std::memory_order_relaxed); }
    int getOverflowCount() const noexcept   { return overflowCount.load (std::memory_order_relaxed); }
    /** Strings the player has a note sounding on, as a mask (bit 0 = string 0). */
    int getSoundingMask() const noexcept    { return soundingFlag.load (std::memory_order_relaxed); }

    //==========================================================================
    // Audio thread.

    /** Renders one sub-block into `out` (cleared by the caller). `hostPpq`
        is the host position at the sub-block's first sample. */
    void renderSubBlock (int numSamples, double hostPpq, bool hostPlaying, double hostBpm, Output& out) noexcept;

private:
    bool swapInWaiting (Output& out, int offset) noexcept;
    void releaseAll (Output& out, int offset) noexcept;
    void relocate (double beat) noexcept;
    void advance (Output& out, double sampleFrom, double samplesPerBeat, int numSamples) noexcept;
    void playSpan (Output& out, double beatFrom, double beatTo, double sampleFrom, double samplesPerBeat,
                   int numSamples) noexcept;
    void emit (const RiffEvent& e, double offset, double samplesPerBeat, int numSamples, Output& out) noexcept;
    void emitBends (double beat, int offset, double samplesPerBeat, Output& out) noexcept;
    bool addBend (int stringIndex, double cents, int offset, Output& out, bool force) noexcept;
    double segmentCentsAt (const RiffBendSegment& seg, double beat, double samplesPerBeat) const noexcept;
    double ownBpm() const noexcept;

    double sampleRate = 48000.0;

    // Hand-over.
    mutable juce::SpinLock lock;
    std::shared_ptr<const CompiledRiff> waiting;
    bool hasWaiting = false;
    bool waitingImmediate = false;
    std::shared_ptr<const CompiledRiff> newest;          ///< message thread's view
    std::array<std::shared_ptr<const CompiledRiff>, kRetiredSlots> retired;

    // Audio thread only.
    std::shared_ptr<const CompiledRiff> active;
    bool playingNow = false, armed = false, hostLocked = false;
    double position = 0.0;            ///< riff beats
    double anchorPpq = 0.0;           ///< host ppq of riff beat 0 (host clock)
    double expectedPpq = -1.0;
    size_t eventIndex = 0;
    int sounding = 0;                 ///< string mask
    std::array<int, kMaxStrings> activeSegment {};
    std::array<double, kMaxStrings> lastCents {};
    std::array<int, kMaxStrings> lateUntil {};      ///< samples past this block a strum's note-on still waits
    int lastBlockSamples = 0;
    int slotsUsed = 0;
    std::atomic<bool> pendingSwap { false };

    // Commands and settings.
    std::atomic<bool> playRequested { false }, stopRequested { false }, resetPending { false };
    std::atomic<bool> looping { true };
    std::atomic<int> clockMode { 0 }, startQuantise { 0 };
    std::atomic<double> tempoFactor { 1.0 }, absoluteBpm { 0.0 };

    // Published.
    std::atomic<bool> playingFlag { false }, waitingFlag { false }, followingHostFlag { false };
    std::atomic<double> beatPosition { -1.0 }, playingBpm { 0.0 };
    std::atomic<juce::uint32> positionStamp { 0 };
    std::atomic<int> overflowCount { 0 }, soundingFlag { 0 };

    JUCE_DECLARE_NON_COPYABLE (RiffPlayer)
};

} // namespace luthier
