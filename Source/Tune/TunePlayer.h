#pragma once

/*  Real-time playback of a tune (tune-builder.md 2, 3.6 and 8).

    The TuneTimeline is the tune as MIDI, built on the message thread. This is
    what walks it on the audio thread and writes its events into the block:
    into the MIDI the engine is about to play, and into the plugin's MIDI out.

    Threads, and what each owns:

      - The message thread builds timelines and hands them over with
        setTimeline(). That only fills a waiting slot under a SpinLock; the
        timeline it displaces is destroyed on the message thread. Transport
        commands (play, pause, seek, skip) are atomics.
      - The audio thread owns the timeline that is playing. It takes the lock
        with ScopedTryLock only to exchange timelines: the waiting one in, the
        finished one out to a retired slot the message thread empties
        (collectGarbage). On a miss it plays on with the timeline it has, and
        the exchange waits for a later block. It never allocates and never
        frees: events go into the caller's MidiBuffers, sized up front.

    When a new timeline comes in (2: "changes take effect on the next bar
    boundary, or immediately if paused"): while playing, the audio thread
    swaps it in at the next bar line, ends the old timeline's notes there and
    restarts whatever the new one holds across that line, so a chord edited
    mid-bar sounds from the next bar and nothing drops out for a bar. Paused,
    it swaps at once.

    Clocks (3.6): "The transport is independent of the host transport when the
    plugin is standalone or when the host is stopped; when the host plays, host
    transport wins." The tune plays once its own Play has been pressed, so
    opening a session in a DAW does not start a tune nobody asked for. Then,
    while the host plays, the tune's position is the host's ppq (bar one of
    the host is bar one of the tune) at the host's tempo. Stopping the host
    pauses the tune, which follows the host again when it restarts, until the
    tune's own Pause or Stop. With the host stopped, the tune runs its own
    clock at its own tempo, counted in if asked.

    Loop mode plays the setlist end to end and repeats (8). A tune that
    improvises (4.4) gets a new timeline every pass: the message thread builds
    the next pass ahead of time (getPassNeedingTimeline / setNextPassTimeline)
    and the audio thread swaps it in at the pass boundary.

    No note is left hanging: stopping, pausing, seeking, a host jump, a new
    timeline or a new pass end every note the player started, on the channels
    it started them, and reset the controllers the tune uses. A note-off is
    only ever sent where its note-on went, so switching the bass between the
    engine and MIDI out mid-note cannot strand it.
*/

#include "TuneMidi.h"

#include <array>
#include <atomic>
#include <memory>

namespace luthier
{

class TunePlayer
{
public:
    /** What the host said about its transport this block. */
    struct HostInfo
    {
        bool hasPosition = false;   ///< The play head answered at all.
        bool isPlaying = false;
        double ppqPosition = 0.0;
        double bpm = 120.0;
    };

    /** Where the tune was at the start of the block. When the tune's own
        clock drives, this is the grid the rhythm engine should follow
        (RhythmEngine strums on the transport it is given). The position runs
        on across loop passes rather than jumping back, so the rhythm grid
        never sees a locate. */
    struct BlockTransport
    {
        bool running = false;         ///< Past any count-in, and playing.
        bool followingHost = false;
        double ppq = 0.0;
        double bpm = 120.0;
    };

    /** The metronome's clicks this block, at sample offsets (3.6 "count-in,
        metronome"). The count-in's are here whether or not the metronome is
        on; the tune's beats only when it is. */
    static constexpr int kMaxClicks = 32;

    struct Clicks
    {
        int count = 0;
        std::array<int, kMaxClicks> offsets {};
        std::array<bool, kMaxClicks> downbeat {};
    };

    /** A note from MIDI in while recording (4.3), placed in the tune. */
    struct RecordedEvent
    {
        double ppq = 0.0;             ///< Tune position, from the start of the pass.
        int span = -1;                ///< The section occurrence it fell in.
        int note = 0;
        int velocity = 0;
        bool isNoteOn = false;
    };

    /** What the caller's MidiBuffers should be given with ensureSize: a full
        release of every note on every channel fits, with room to spare. */
    static constexpr int kRecommendedMidiBytes = 32 * 1024;

    TunePlayer();
    ~TunePlayer();

    void prepare (double sampleRate, int maxBlockSize);

    //==========================================================================
    // Message thread: the timeline

    /** Hands over a new timeline (after an edit, a load, a tempo change). It
        plays from the next bar line, or at once when paused. `tempoBpm` drives
        the internal clock; `beatsPerBar` sets the bar lines and the count-in. */
    void setTimeline (std::unique_ptr<TuneTimeline> timeline, double tempoBpm, double beatsPerBar);

    void clearTimeline();
    bool hasTimeline() const;

    /** The loop pass a prebuilt timeline is wanted for, or -1. Non-negative
        only while the playing timeline needsRebuildEachPass() and the next
        pass has not been built yet. */
    int getPassNeedingTimeline() const;

    /** The timeline for loop pass `pass`, built with that improvisePass. */
    void setNextPassTimeline (std::unique_ptr<TuneTimeline> timeline, int pass);

    /** Destroys the timelines the audio thread has finished with. Call from
        a message-thread timer. */
    void collectGarbage();

    //==========================================================================
    // Message thread: transport (3.6)

    void play();
    void pause();

    /** Pause, and go back to the start of the tune. */
    void stop();

    void togglePlayPause();

    /** Moves to a position in the tune, in ppq from its start. */
    void seek (double ppq);

    /** Seeks to a section occurrence's start (an index into the timeline's
        getSections(), which is the tune's play order). */
    void seekToSection (int spanIndex);

    /** Shift+Space: play from the start of `spanIndex`. */
    void playFromSection (int spanIndex);

    /** Skips back or forward by whole sections. Back from more than a beat
        into a section goes to its own start first, as a transport's back
        button does. */
    void skipSection (int delta);

    void setLoop (bool shouldLoop) noexcept          { loop.store (shouldLoop, std::memory_order_relaxed); }
    bool isLooping() const noexcept                  { return loop.load (std::memory_order_relaxed); }

    /** Bars counted in before playback from the internal clock (3.6). */
    void setCountInBars (int bars) noexcept          { countInBars.store (juce::jlimit (0, 8, bars), std::memory_order_relaxed); }
    int getCountInBars() const noexcept              { return countInBars.load (std::memory_order_relaxed); }

    /** Clicks on the tune's beats (3.6 "metronome"). See getBlockClicks(). */
    void setMetronome (bool shouldClick) noexcept    { metronome.store (shouldClick, std::memory_order_relaxed); }

    /** tune-builder 14 (TUNE-HELP-ONBOARDING): Tune Tempo Drift, as a factor on
        the tune's own clock (1 = as written). A host that plays keeps its tempo. */
    void setTempoScale (double scale) noexcept       { tempoScale.store (juce::jlimit (0.5, 2.0, scale), std::memory_order_relaxed); }
    double getTempoScale() const noexcept            { return tempoScale.load (std::memory_order_relaxed); }
    bool isMetronomeOn() const noexcept              { return metronome.load (std::memory_order_relaxed); }

    /** tune-builder 6: the bass plays through the engine only when the current
        instrument is a bass. Otherwise it goes to MIDI out alone. */
    void setBassToEngine (bool shouldPlay) noexcept  { bassToEngine.store (shouldPlay, std::memory_order_relaxed); }
    bool isBassToEngine() const noexcept             { return bassToEngine.load (std::memory_order_relaxed); }

    /** Record (4.3): while armed, captureInput() places MIDI in's notes in
        the tune for popRecordedEvents(). */
    void setRecordArmed (bool armed) noexcept        { recordArmed.store (armed, std::memory_order_relaxed); }
    bool isRecordArmed() const noexcept              { return recordArmed.load (std::memory_order_relaxed); }

    /** Copies out up to `maxEvents` recorded notes; returns how many. */
    int popRecordedEvents (RecordedEvent* destination, int maxEvents);

    /** The rhythm settings that came into force since the last call: a section
        boundary crossed, a new timeline, or where playback started. The caller
        applies them to the RhythmEngine on the message thread, which is where
        its pattern and kit are set; a UI tick of latency is the price. */
    bool takePendingRhythmChange (TuneRhythmChange& change);

    //==========================================================================
    // Any thread: state for the UI (gui-engine-dataflow 24)

    /** True while the user's Play is in force. */
    bool isPlaying() const noexcept         { return wantPlaying.load (std::memory_order_relaxed); }

    bool isCountingIn() const noexcept      { return countingIn.load (std::memory_order_relaxed); }
    bool isFollowingHost() const noexcept   { return followingHost.load (std::memory_order_relaxed); }

    /** Position within the current pass, in ppq from the tune's start. */
    double getPositionPpq() const noexcept  { return position.load (std::memory_order_relaxed); }

    /** The section occurrence playing (an index into getSections()), or -1. */
    int getPlayingSpan() const noexcept     { return playingSpan.load (std::memory_order_relaxed); }

    /** The section playing (an index into the tune's sections), or -1. */
    int getPlayingSection() const noexcept  { return playingSection.load (std::memory_order_relaxed); }

    int getPass() const noexcept            { return pass.load (std::memory_order_relaxed); }

    //==========================================================================
    // Audio thread

    /** Adds this block's events: everything to `toMidiOut`, and everything but
        a non-bass instrument's bass line to `toEngine` (merge it with the
        incoming MIDI before the engine runs). Neither buffer is cleared, and
        both should have kRecommendedMidiBytes reserved. */
    void renderBlock (int numSamples, const HostInfo& host,
                      juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** After renderBlock: where this block started, for the rhythm engine. */
    const BlockTransport& getBlockTransport() const noexcept { return blockTransport; }

    /** After renderBlock: this block's metronome clicks. */
    const Clicks& getBlockClicks() const noexcept { return clicks; }

    /** After renderBlock: true when a section that asks for a state boundary
        (8) started in this block, for the caller to reset mod envelopes and
        the rhythm engine's phase on the audio thread. */
    bool crossedStateBoundary() const noexcept { return stateBoundaryThisBlock; }

    /** After renderBlock, with the block's incoming MIDI: records its notes
        while armed. */
    void captureInput (const juce::MidiBuffer& incoming) noexcept;

    /** Notes the player has started and not yet ended, for tests. */
    int getNumSoundingNotes() const noexcept;

private:
    using NoteTable = std::array<std::array<uint8_t, 128>, 16>;

    static constexpr int kRetiredSlots = 4;
    static constexpr int kRecordCapacity = 1024;

    // --- audio thread -------------------------------------------------------------
    /** Walks [from, to) in pieces: a pass, up to a bar line where a waiting
        timeline goes in, or to the block's end. */
    void renderSegments (double from, double to, bool canSwap, bool hostDriving,
                         juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** Replaces the playing timeline with `incoming` at `atAbsolute` (sample
        `offset`). On the internal clock the place in the tune is kept, which
        moves the clock's numbers by `shift` when the length changed. False,
        with nothing changed, when there is no retired slot free. */
    bool swapIn (std::unique_ptr<TuneTimeline>& incoming, double atAbsolute, int offset, double& shift,
                 juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** swapIn for the timeline setTimeline left waiting, with its tempo and bar. */
    bool swapInWaiting (double atAbsolute, int offset, double& shift,
                        juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** Played to the end without Loop. */
    void finishAtEnd (int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    void emit (const TuneEvent& e, int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** Restarts the notes the playing timeline holds across `localPpq`: on a
        start, a seek, a host jump or a new timeline mid-tune. */
    void chase (double localPpq, int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    /** Ends every note the player started, and resets the tune's controllers
        on the channels it touched. */
    void releaseAll (int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept;

    void publishRhythm (int index) noexcept;
    void publishRhythmAt (double localPpq) noexcept;
    void publishState() noexcept;
    void addClick (int offset, bool downbeat) noexcept;

    double clockPosition() const noexcept { return clockBase + (double) clockSamples / clockSamplesPerQuarter; }
    void setClock (double ppq) noexcept;
    int offsetOf (double absolute) const noexcept;
    double localOf (double absolute) const noexcept;

    double sampleRate = 48000.0;

    // --- shared with the message thread, under timelineLock --------------------------
    mutable juce::SpinLock timelineLock;

    /*  The playing timeline. Written only by the audio thread, and only while
        it holds the lock, so the audio thread reads it freely and the message
        thread reads it under the lock. Timelines are immutable once built, so
        reading one from both threads at once is safe. */
    std::unique_ptr<TuneTimeline> active;

    std::unique_ptr<TuneTimeline> waiting;       ///< From setTimeline, for the next bar line.
    bool hasWaiting = false;                     ///< (A waiting nullptr clears the timeline.)
    double waitingTempo = 120.0, waitingBarBeats = 4.0;

    std::unique_ptr<TuneTimeline> nextPass;      ///< Improvise's prebuilt next pass.
    int nextPassNumber = -1;

    std::array<std::unique_ptr<TuneTimeline>, kRetiredSlots> retired;

    int activeSerial = 0;                        ///< Bumped on every swap.

    // --- commands --------------------------------------------------------------------
    std::atomic<bool> wantPlaying { false };
    std::atomic<bool> hostArmed { false };       ///< Follow the host when it next plays.
    std::atomic<double> pendingSeek { -1.0 };
    std::atomic<bool> pendingRewind { false };
    std::atomic<bool> loop { true };
    std::atomic<int> countInBars { 0 };
    std::atomic<bool> metronome { false };
    std::atomic<double> tempoScale { 1.0 };
    std::atomic<bool> bassToEngine { false };
    std::atomic<bool> recordArmed { false };

    // --- published state ----------------------------------------------------------
    std::atomic<double> position { 0.0 };
    std::atomic<int> playingSpan { -1 };
    std::atomic<int> playingSection { -1 };
    std::atomic<int> pass { 0 };
    std::atomic<bool> countingIn { false };
    std::atomic<bool> followingHost { false };

    /*  The latest rhythm change, as the timeline it belongs to and its index,
        and a counter that moves on every publish (the same change crossed
        again on the next pass is news too). */
    std::atomic<int> rhythmCounter { 0 };
    std::atomic<juce::int64> rhythmKey { -1 };
    int lastTakenRhythmCounter = 0;   // message thread

    juce::AbstractFifo recordFifo { kRecordCapacity };
    std::array<RecordedEvent, kRecordCapacity> recordRing {};

    // --- audio thread only ---------------------------------------------------------
    bool running = false;
    bool wasFollowingHost = false;
    double tempo = 120.0;
    double barBeats = 4.0;
    int currentPass = 0;

    /*  The internal clock: a base position plus whole samples since it was
        set, divided by the samples per quarter in force then. Counting samples
        rather than adding a fraction of a beat per block means a block's end
        and the next block's start are the same number, and a beat falls on
        the sample it should however many blocks it took to get there.

        Positions on either clock are "absolute": pass * length + position in
        the pass, so the loop is just a larger number. */
    double clockBase = 0.0;
    juce::int64 clockSamples = 0;
    double clockSamplesPerQuarter = 24000.0;

    double lastTo = 0.0;                 ///< The previous block's end, on the host's clock.
    double suppressBefore = -1.0e300;    ///< Events before this are not played (a count-in, a seek).
    double countInBeats = 0.0;           ///< The count-in running now, ending at suppressBefore.
    bool chasePending = false;
    double chaseAt = 0.0;

    // The block being rendered.
    double blockFrom = 0.0;
    double blockSamplesPerQuarter = 24000.0;
    int blockLength = 0;
    BlockTransport blockTransport;
    Clicks clicks;
    bool stateBoundaryThisBlock = false;
    bool blockRecordable = false;

    NoteTable heldEngine {}, heldOut {};
    uint16_t touchedChannels = 0;

    // chase()'s scratch: how many of each note are on, and the event that last started it.
    NoteTable chaseCount {};
    std::array<std::array<int, 128>, 16> chaseEvent {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePlayer)
};

} // namespace luthier
