#pragma once

/*  Jam mode (jam-mode.md): a synthesized drummer and bass player who follow
    the chords and play in time.

    The band belongs to the processor, not the guitar (0.2). JamEngine sits
    beside the Metronome and the Looper; it never touches the guitar's path
    and never writes into its event queue. It reads the block's MIDI (the
    player's notes, the looper's playback, the tune's chord channel through
    the rhythm engine), the transport, and a tune's chord map, and it renders
    two stems - drums (stereo) and bass (mono, panned) - for the processor to
    mix and route.

    The audio path, per block:
      1. commands (START / STOP / FILL / panic / taps) and parameter edges;
      2. the clock: host, tune or own (JamConductor.h), locate detection;
      3. MIDI: first-note starts, the chord follower, silence and dynamics;
      4. the scheduler: every step whose grid sample falls before the block's
         end (plus the humanise look-ahead) becomes queued events at
         grid + humanise + L, L being the plugin's latency (0.6);
      5. rendering: events fire on their sample, the kit and the bass render
         between them;
      6. the mixer and the status snapshot.

    Real-time safe (0.4): everything is sized in prepare; styles and chord
    maps arrive by atomic pointer; no locks, no allocation, no strings.
    Deterministic (0.5): the only randomness is RtRandom seeded by (jam seed,
    bar, lane).
*/

#include "JamStyle.h"
#include "JamConductor.h"
#include "JamChordFollower.h"
#include "JamBassLine.h"
#include "JamCapture.h"
#include "JamStatus.h"
#include "../DSP/Jam/JamDrumKit.h"
#include "../DSP/Jam/JamBassVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace luthier
{

class JamEngine
{
public:
    //==========================================================================
    /** The 34 parameters (jam-mode 10), as plain values. */
    struct Settings
    {
        bool enabled = false, play = false, fillNow = false;
        int style = 0, variation = 0, intensity = 3, fillEvery = 3;
        int follow = 1;
        bool predict = true;
        int chordSource = 0, startMode = 0, countInBars = 1;
        bool stopOnSilence = true;
        int silenceBars = 2;
        bool ending = true, dynamicsFollow = true;
        double swing = 0.0, humanise = 50.0;
        int kit = 0;
        bool kitAuto = true;
        double kitTuning = 0.0, kitDamping = 40.0, kitRoom = 25.0, kitWidth = 70.0;
        int perspective = 0, bassVoice = 0;
        double bassTone = 0.5, volumeDb = -6.0, balance = 0.0, drumsPan = 0.0, bassPan = 0.0;
        bool drumsMute = false, bassMute = false;
        int output = 0;
    };

    enum class StartMode { automatic = 0, hostTransport, firstNote, countIn, tapIn };
    enum class ChordSource { automatic = 0, live, tune };
    enum class Output { main = 0, separate, mainAndSeparate };

    /** What the processor knows this block. */
    struct BlockContext
    {
        int latency = 0;

        bool hasPlayHead = false, hostHasPpq = false, hostPlaying = false;
        double hostPpq = 0.0, hostBpm = 120.0;
        bool hostHasBarStart = false;
        double hostBarStartPpq = 0.0;
        bool hostHasMeter = false;
        int hostNumerator = 4, hostDenominator = 4;

        bool tuneRunning = false, tuneFollowingHost = false, tunePlaying = false;
        double tunePpq = 0.0, tuneBpm = 120.0, tuneBeatsPerBar = 4.0;
        bool progressionPlaying = false;

        /** LuthierAudioProcessor::getEffectiveTempo(): tap, last host, 120. */
        double effectiveTempo = 120.0;

        bool rhythmDriving = false;
        ChordSymbol rhythmChord;
        bool playerIsBass = false;

        /** The tune's bass is to play through the Jam bass (11). */
        bool tuneBassActive = false;
    };

    static constexpr int kMaxEvents = 1024;
    static constexpr int kFillEveryBars[] = { 0, 2, 4, 8, 16 };

    JamEngine();
    ~JamEngine();

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize);

    /** Back to Armed: every voice cleared, the clock stopped (13: a re-prepare). */
    void reset() noexcept;

    //==========================================================================
    // Message thread.

    /** Installs the style the audio thread will use for choice `index`
        (0-9 factory, 10 User). The library keeps it alive. */
    void setStyleSlot (int index, const JamStyle* style) noexcept;
    const JamStyle* getStyleSlot (int index) const noexcept;

    /** The tune's chord map, by pointer swap; the old one is freed by
        collectGarbage on the message thread. */
    void setChordMap (std::unique_ptr<JamChordMap> map);
    void collectGarbage();

    void setSeed (uint64_t seed) noexcept { seedValue.store (seed, std::memory_order_relaxed); }
    uint64_t getSeed() const noexcept     { return seedValue.load (std::memory_order_relaxed); }

    /** MIDI out channels (9). */
    void setMidiChannels (int drums, int bass) noexcept
    {
        drumChannel.store (juce::jlimit (1, 16, drums), std::memory_order_relaxed);
        bassChannel.store (juce::jlimit (1, 16, bass), std::memory_order_relaxed);
    }

    /** CPU relief step (13). */
    void setReducedCymbals (bool reduced) noexcept { reducedCymbals.store (reduced, std::memory_order_relaxed); }

    //==========================================================================
    // Commands, from any thread; acted on at the next block.

    /** START / STOP as the J key, the pill and the button do it (2.1, 2.2):
        armed starts, playing or counting stops, ending cuts. */
    void requestToggle() noexcept  { toggleRequests.fetch_add (1, std::memory_order_acq_rel); }
    void requestStart() noexcept   { startRequested.store (true, std::memory_order_release); }
    void requestStop() noexcept    { stopRequests.fetch_add (1, std::memory_order_acq_rel); }
    void requestFill() noexcept    { fillRequested.store (true, std::memory_order_release); }
    void requestPanic() noexcept   { panicRequested.store (true, std::memory_order_release); }

    /** A tap for Tap In, stamped on the engine's sample clock. */
    void tapAtSample (int64_t sample) noexcept;

    /** The sample clock (the next block's first sample). */
    int64_t getSampleClock() const noexcept { return sampleClockAtomic.load (std::memory_order_acquire); }

    //==========================================================================
    // Audio thread.

    /** Per block, before process. */
    void setSettings (const Settings& s) noexcept { pendingSettings = s; }

    /** Count-in clicks the tune asked for, as the band's sticks (11). */
    void addStickClicks (const int* offsets, const bool* downbeats, int count) noexcept;

    /** Runs one block. `notes` is the MIDI the band listens to; `tuneBass`
        the tune's bass-channel notes when they play through the Jam bass. */
    void process (const BlockContext& context, const juce::MidiBuffer& notes,
                  const juce::MidiBuffer* tuneBass, int numSamples) noexcept;

    /** The stems of the last block, after the Jam mixer. */
    const float* getDrums (int channel) const noexcept { return drumsOut.getReadPointer (juce::jlimit (0, 1, channel)); }
    const float* getBass (int channel) const noexcept  { return bassOut.getReadPointer (juce::jlimit (0, 1, channel)); }

    /** The block's band MIDI (sample offsets, including L), for MIDI out. */
    const juce::MidiBuffer& getMidiOut() const noexcept { return midiOut; }

    //==========================================================================
    // State, for the processor and the UI.

    JamState getState() const noexcept { return (JamState) stateAtomic.load (std::memory_order_relaxed); }
    bool isBandRunning() const noexcept
    {
        const auto s = getState();
        return s == JamState::counting || s == JamState::playing || s == JamState::ending;
    }

    /** 11: the drums are heard (running, not muted, the balance not all bass). */
    bool areDrumsAudible() const noexcept { return drumsAudible.load (std::memory_order_relaxed); }

    /** True while the Jam bass plays the tune's bass line (11). */
    bool isTuneBassPlaying() const noexcept { return tuneBassPlaying.load (std::memory_order_relaxed); }

    /** The band's position when it runs its own clock, for the rhythm engine (2.3). */
    bool getOwnClock (double& ppq, double& bpm) const noexcept;

    JamStatusChannel& getStatusChannel() noexcept { return statusChannel; }
    const JamCapture& getCapture() const noexcept  { return capture; }
    JamCapture& getCapture() noexcept              { return capture; }

    //==========================================================================
    // For the tests.

    ChordSymbol getHeardChord() const noexcept { return heardChord; }
    ChordSymbol getBassChord() const noexcept  { return bassChord; }
    const JamPredictor& getPredictor() const noexcept { return predictor; }
    JamDrumKit& getKit() noexcept { return kit; }
    JamBassVoice& getBassVoice() noexcept { return bass; }
    int getEffectiveIntensity() const noexcept { return effectiveIntensity; }
    int getLatchedStyle() const noexcept { return latchedStyleIndex; }
    int getLatchedVariation() const noexcept { return latchedVariation; }
    int getLatchedKit() const noexcept { return kit.getKit(); }
    bool isFillActive() const noexcept { return fillActive; }
    double getTempo() const noexcept { return currentBpm; }
    const JamCursor& getCursor() const noexcept { return cursor; }
    int64_t getBandStartSample() const noexcept { return bandStartSample; }

private:
    //==========================================================================
    enum class EventType : uint8_t { drum, drumOff, bassStep, bassOff, stateChange, endingDone, cymbalChoke };

    struct Event
    {
        int64_t sample = 0;        ///< when it fires (includes L)
        int64_t musical = 0;       ///< its grid sample (no L, no humanise)
        double ppq = 0.0;
        EventType type = EventType::drum;
        uint8_t priority = 0;
        int16_t value = 0;         ///< drum sound, bass token, new state
        int16_t note = 0;
        float velocity = 0.0f;
        int16_t walkLeft = 0;
        int32_t order = 0;
    };

    struct PendingChord
    {
        int64_t musical = 0;
        double ppq = 0.0;
        ChordSymbol chord;
        bool applied = false;
        bool anticipated = false;
    };

    //==========================================================================
    void applySettingsEdges() noexcept;
    void handleCommands (const BlockContext& ctx, int64_t blockStart) noexcept;
    void updateClock (const BlockContext& ctx, int64_t blockStart, int numSamples) noexcept;
    void handleMidi (const BlockContext& ctx, const juce::MidiBuffer& notes, int64_t blockStart) noexcept;
    void handleLiveChange (const JamChordFollower::Change& change) noexcept;
    void scheduleAnticipated (double upToPpq) noexcept;
    void schedule (const BlockContext& ctx, int64_t horizon) noexcept;
    void beginBar() noexcept;
    void processStep() noexcept;
    void latchIntensity (double ppq, int64_t musical) noexcept;
    void render (const BlockContext& ctx, const juce::MidiBuffer* tuneBass, int64_t blockStart, int numSamples) noexcept;
    void fire (const Event& e, int64_t blockStart) noexcept;
    void mix (int numSamples) noexcept;
    void publishStatus (const BlockContext& ctx) noexcept;

    void startBand (int64_t beatOneSample, double bpm, int countInBars) noexcept;
    void startWithExternalClock() noexcept;
    void requestEnding (bool immediate) noexcept;
    void cut (double seconds) noexcept;
    void goArmed() noexcept;
    void locate() noexcept;

    bool queue (const Event& e) noexcept;
    void clearQueue (bool keepOffs) noexcept;
    void addChordChange (int64_t musical, double ppq, const ChordSymbol& chord, bool anticipated) noexcept;
    void applyChordsUpTo (int64_t musical) noexcept;

    const JamStyle* activeStyle() const noexcept;
    const JamMeterSet* activeMeter() const noexcept;
    const JamPattern& patternFor (int step) noexcept;
    void buildGenericBar() noexcept;
    bool usingTuneSource() const noexcept;
    ChordSymbol nextKnownChord (double afterPpq, double withinPpq, bool& found) const noexcept;
    int walkStepsLeft (const JamPattern& pattern, int step) const noexcept;
    uint64_t laneSeed (int64_t bar, int lane) const noexcept;
    void emitMidi (int offset, bool noteOn, int channel, int note, int velocity) noexcept;
    void noteCapture (int64_t sample, double ppq, int part, int note, int velocity) noexcept;
    void playBassNote (int note, double velocity, bool ghost, int64_t sample, double ppq) noexcept;
    void releaseBass (int64_t sample, double ppq) noexcept;

    //==========================================================================
    double sr = 48000.0;
    int maxBlock = 512;

    Settings settings, pendingSettings, previousSettings;
    bool haveSettings = false;

    std::array<std::atomic<const JamStyle*>, jam::kNumStyleChoices> styleSlots;
    std::atomic<JamChordMap*> chordMapIncoming { nullptr };
    JamChordMap* chordMap = nullptr;              ///< audio thread's
    std::atomic<JamChordMap*> chordMapRetired { nullptr };
    std::unique_ptr<JamChordMap> ownedMap;        ///< the message thread's copy of the live pointer's ownership
    std::vector<std::unique_ptr<JamChordMap>> retiredMaps;

    std::atomic<uint64_t> seedValue { 4849997 };
    std::atomic<int> drumChannel { 10 }, bassChannel { 11 };
    std::atomic<bool> reducedCymbals { false };

    std::atomic<int> toggleRequests { 0 }, stopRequests { 0 };
    std::atomic<bool> startRequested { false }, fillRequested { false }, panicRequested { false };

    static constexpr int kTapQueue = 16;
    std::array<std::atomic<int64_t>, kTapQueue> tapQueue;
    std::atomic<int> tapWrite { 0 };
    int tapRead = 0;
    std::array<int64_t, 4> taps {};
    int numTaps = 0;

    //==========================================================================
    // Transport and clock.
    JamState state = JamState::armed;
    std::atomic<int> stateAtomic { (int) JamState::off };
    JamStatus::Clock clockSource = JamStatus::Clock::own;
    JamClockMapping own, mapping;
    double currentBpm = 120.0;
    bool externalWasRunning = false;
    double lastBlockEndPpq = 0.0;
    bool haveLastBlockEnd = false;
    int64_t sampleClock = 0;
    std::atomic<int64_t> sampleClockAtomic { 0 };
    int64_t bandStartSample = 0;
    double tuneOffset = 0.0;
    bool pendingJoinAtBar = false;
    bool endingPending = false, cutRequested = false;
    int64_t endingBar = 0;
    double pendingOwnBpm = 0.0;

    JamCursor cursor;
    bool barBegun = false, midBarStart = false, repluckPending = false;
    double stepPpq = 0.0;
    bool hostBarStartKnown = false;
    double hostBarStart = 0.0;
    bool playerIsBass = false;
    int latency = 0;
    int64_t lookahead = 0;
    int64_t blockEnd = 0;

    //==========================================================================
    // What plays.
    const JamStyle* latchedStyle = nullptr;
    int latchedStyleIndex = 0, latchedVariation = 0;
    int latchedBassVoice = 0;
    int baseIntensity = 3, effectiveIntensity = 3, dynamicsOffset = 0, hintIntensity = 0;
    bool genericGroove = false;
    JamPattern genericBar;
    bool fillActive = false;
    int fillStartStep = 0;
    const JamPattern* fillPattern = nullptr;
    int fillPatternOffset = 0;
    bool fillNowPending = false;
    int64_t fillNowNextBar = -1;       ///< the bar that plays the "next bar" Fill Now
    bool crashNextDownbeat = false, crashThisDownbeat = false;
    int markerStyle = -1, markerVariation = -1, markerIntensity = -1, markerKit = -1;
    int lastSection = -1;
    int64_t firstGrooveBar = 0;

    std::array<RtRandom, jam::numDrumLanes + 1> laneRandom;

    //==========================================================================
    // Chords.
    JamChordFollower follower;
    JamPredictor predictor;
    ChordSymbol heardChord, bassChord;
    std::array<PendingChord, 16> pendingChords {};
    int numPendingChords = 0;
    double anticipatedUpTo = -1.0e300;
    int64_t lastBassStepMusical = INT64_MIN;
    JamBassLine bassLine;
    int bassMidiNote = -1;
    bool bassSounding = false;
    ChordSymbol lastRhythmChord;

    //==========================================================================
    // Silence and dynamics.
    double lastNoteOnPpq = 0.0;
    int64_t lastNoteOnSample = INT64_MIN;
    struct Hit { int64_t sample; int velocity; };
    std::array<Hit, 1024> hits {};
    int hitWrite = 0, hitCount = 0;

    //==========================================================================
    // Events.
    std::array<Event, kMaxEvents> events {};
    int numEvents = 0;
    int32_t eventOrder = 0;

    //==========================================================================
    // DSP and output.
    JamDrumKit kit;
    JamBassVoice bass;
    juce::AudioBuffer<double> drumsBuffer, bassBuffer;
    juce::AudioBuffer<float> drumsOut, bassOut;
    juce::MidiBuffer midiOut;
    LinSmoother drumsGainL, drumsGainR, bassGainL, bassGainR;
    std::atomic<bool> drumsAudible { false }, tuneBassPlaying { false };
    bool renderedSomething = false;

    JamCapture capture;
    JamStatusChannel statusChannel;
    JamStatus status;
    std::array<ChordSymbol, 8> barChordHistory {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamEngine)
};

} // namespace luthier
