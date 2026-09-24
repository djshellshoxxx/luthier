#pragma once

/*  The looper and the session recorder (practice-tools.md sections 2 and 8).

    Both record the plugin's own output into a buffer that was allocated before
    the audio thread ever saw it, which is rule 5 of practice-tools 0 and rule 2
    of engine.md section 0. Nothing here allocates once prepare() has run:
    recording writes into a layer that already exists, and a layer that has not
    been used yet is silent rather than absent.

    practice-tools 2 asks for each layer to keep the MIDI alongside the audio, so
    that changing the tone re-renders the loop rather than replaying a recording
    of the old tone. The MIDI is captured here; the re-render is the caller's,
    because only the caller owns an engine to render with.
*/

#include "../DSP/Common/DspCommon.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/** What a layer does when the loop comes round (practice-tools 2). */
enum class LayerMode
{
    overdub = 0,   ///< Adds to what is there.
    replace,       ///< Overwrites it.
    playOnce,      ///< Plays once, then mutes itself.
    numModes
};

const char* getLayerModeName (LayerMode mode) noexcept;

//==============================================================================
/** One recorded layer. */
class LoopLayer
{
public:
    /** practice-tools 2: undo and redo, per layer. One level each way is enough
        to cover the mistake a player actually makes, which is recording over a
        take they wanted. */
    static constexpr int kUndoDepth = 1;

    void prepare (int maxSamples);
    void reset() noexcept;

    bool hasContent() const noexcept { return recordedSamples > 0; }
    int getRecordedSamples() const noexcept { return recordedSamples; }

    //==========================================================================
    void setMode (LayerMode m) noexcept { mode.store ((int) m, std::memory_order_relaxed); }
    LayerMode getMode() const noexcept { return (LayerMode) mode.load (std::memory_order_relaxed); }

    void setLevelDb (double db) noexcept;
    double getLevelDb() const noexcept { return levelDb.load (std::memory_order_relaxed); }

    void setPan (double p) noexcept;
    double getPan() const noexcept { return pan.load (std::memory_order_relaxed); }

    void setMuted (bool m) noexcept { muted.store (m, std::memory_order_relaxed); }
    bool isMuted() const noexcept { return muted.load (std::memory_order_relaxed); }

    /** practice-tools 2: reverse and half-speed are audio-only, because
        re-rendering the MIDI at half speed would change its pitch. */
    void setReversed (bool r) noexcept { reversed.store (r, std::memory_order_relaxed); }
    bool isReversed() const noexcept { return reversed.load (std::memory_order_relaxed); }

    void setHalfSpeed (bool h) noexcept { halfSpeed.store (h, std::memory_order_relaxed); }
    bool isHalfSpeed() const noexcept { return halfSpeed.load (std::memory_order_relaxed); }

    void setLowCutHz (double hz) noexcept;
    void setHighCutHz (double hz) noexcept;

    double getLowCutHz() const noexcept { return lowCutHz.load (std::memory_order_relaxed); }
    double getHighCutHz() const noexcept { return highCutHz.load (std::memory_order_relaxed); }

    void prepareFilters (double sampleRate) noexcept;

    //==========================================================================
    /** Writes into the layer at `position`, mixing or replacing according to the
        mode. Audio thread. */
    void record (const float* left, const float* right, int position, int numSamples) noexcept;

    /** Adds the layer's contribution into a stereo pair. Audio thread. */
    void playInto (float* left, float* right, int position, int numSamples,
                   int loopLength) noexcept;

    //==========================================================================
    /** Copies the layer's audio aside so it can be restored. Message thread. */
    void pushUndo();
    bool undo();
    bool redo();

    bool canUndo() const noexcept { return undoFilled; }

    /** Empties the layer but keeps its audio in the undo buffer. Message thread. */
    void clearKeepingUndo();
    bool canRedo() const noexcept { return redoFilled; }

    //==========================================================================
    /** The MIDI recorded alongside the audio (practice-tools 2). */
    juce::MidiMessageSequence& getMidi() noexcept { return midi; }
    const juce::MidiMessageSequence& getMidi() const noexcept { return midi; }

    const float* readLeft() const noexcept { return audio.getReadPointer (0); }
    const float* readRight() const noexcept { return audio.getReadPointer (1); }

    juce::AudioBuffer<float>& getAudio() noexcept { return audio; }

    void setRecordedSamples (int samples) noexcept;

    juce::var settingsToVar() const;
    void settingsFromVar (const juce::var& state);

private:
    juce::AudioBuffer<float> audio, undoBuffer, redoBuffer;
    juce::MidiMessageSequence midi;

    int capacity = 0;
    int recordedSamples = 0;
    int undoSamples = 0, redoSamples = 0;
    bool undoFilled = false, redoFilled = false;

    std::atomic<int> mode { (int) LayerMode::overdub };
    std::atomic<double> levelDb { 0.0 };
    std::atomic<double> pan { 0.0 };
    std::atomic<bool> muted { false };
    std::atomic<bool> reversed { false };
    std::atomic<bool> halfSpeed { false };
    std::atomic<double> lowCutHz { 20.0 };
    std::atomic<double> highCutHz { 20000.0 };

    Biquad lowCutL, lowCutR, highCutL, highCutR;
    double lastLowCut = -1.0, lastHighCut = -1.0;
    double sr = 44100.0;

    /** Position in the layer's own audio, for half-speed playback, which reads
        at a fractional rate. */
    double readPosition = 0.0;

    /** Play-once layers mute themselves after one pass. */
    bool playedOnce = false;

    JUCE_LEAK_DETECTOR (LoopLayer)
};

//==============================================================================
class Looper
{
public:
    /** practice-tools 2: up to eight layers, and up to four minutes. */
    static constexpr int kMaxLayers = 8;
    static constexpr double kMaxLoopSeconds = 240.0;
    static constexpr double kMinLoopSeconds = 1.0;

    enum class State { stopped = 0, recordingFirst, playing, overdubbing };

    Looper();
    ~Looper();

    /** Allocates every layer at the maximum loop length. This is the one
        expensive call, and it happens on the message thread. */
    void prepare (double sampleRate, double maxSeconds = kMaxLoopSeconds);

    void reset() noexcept;

    //==========================================================================
    State getState() const noexcept { return (State) state.load (std::memory_order_relaxed); }

    /** The transport: one button, which is how a looper pedal works. First press
        records, second closes the loop and plays, third overdubs. */
    void press() noexcept;

    void stop() noexcept;

    /*  Empties every layer, keeping each one's audio in its undo buffer so a
        clear can be taken back (action-and-undo.md 3.14: deleting a layer is
        restorable, recording one is not). Message thread. */
    void clear();

    /** Puts back what the last clear() removed. False if nothing to restore. */
    bool restoreCleared();

    /** practice-tools 2: a loop is quantised to bars when the metronome is
        running. The caller supplies the bar length; zero means free. */
    void setBarLengthSamples (int samples) noexcept
    {
        barLengthSamples.store (juce::jmax (0, samples), std::memory_order_relaxed);
    }

    int getLoopLengthSamples() const noexcept { return loopLength.load (std::memory_order_relaxed); }
    int getPlayPosition() const noexcept { return playPosition.load (std::memory_order_relaxed); }

    double getLoopSeconds() const noexcept
    {
        return (double) getLoopLengthSamples() / juce::jmax (1.0, sr);
    }

    //==========================================================================
    int getNumLayers() const noexcept { return kMaxLayers; }
    LoopLayer& getLayer (int index) noexcept;
    const LoopLayer& getLayer (int index) const noexcept;

    int getActiveLayer() const noexcept { return activeLayer.load (std::memory_order_relaxed); }
    void setActiveLayer (int index) noexcept;

    /** How many layers hold anything. */
    int getNumRecordedLayers() const noexcept;

    //==========================================================================
    /** Records the block into the active layer if recording, and mixes every
        layer's playback into the buffer. Audio thread. */
    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    /** Feeds the looper the MIDI to store alongside the audio. */
    void captureMidi (const juce::MidiBuffer& midi, int numSamples) noexcept;

    //==========================================================================
    /** practice-tools 2: bounce all layers to one file, or each to its own. */
    bool exportMixdown (const juce::File& file) const;
    bool exportStems (const juce::File& directory) const;

    /** The `.luthierloop` file: settings, MIDI and the audio of every layer. */
    bool save (const juce::File& file) const;
    bool load (const juce::File& file);

    static const char* const kFileExtension;
    static juce::File getUserDirectory();

private:
    bool writeLayersToFile (const juce::File& file,
                            const juce::Array<int>& layerIndices) const;

    double sr = 44100.0;
    int capacity = 0;

    std::array<LoopLayer, kMaxLayers> layers;

    std::atomic<int> state { (int) State::stopped };
    std::atomic<int> loopLength { 0 };
    int clearedLoopLength = 0;   // action-and-undo.md 3.14
    std::atomic<int> playPosition { 0 };
    std::atomic<int> activeLayer { 0 };
    std::atomic<int> barLengthSamples { 0 };

    /** Set by press() and acted on by the audio thread at the loop boundary, so
        that closing a loop lands on the beat rather than on the key press. */
    std::atomic<bool> pendingClose { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Looper)
};

//==============================================================================
/** The session recorder (practice-tools.md section 8).

    A ring buffer that is always a fixed number of minutes long, so that the
    player can decide a take was worth keeping after they have played it. The
    whole point is that it never allocates while running: the buffer is sized
    once, and writing into it is a copy into memory that already exists.

    Disabled by default, which is what the spec asks for - a recorder that is on
    without being asked is a surprise nobody wants. */
class SessionRecorder
{
public:
    static constexpr double kDefaultMinutes = 60.0;

    // Declared rather than defaulted in-class: JUCE_DECLARE_NON_COPYABLE below
    // declares a deleted copy constructor, and declaring any constructor
    // suppresses the implicit default one.
    SessionRecorder();
    ~SessionRecorder();

    /** Allocates the ring buffer. Message thread: it is the only call here that
        touches memory. Sixty minutes of 48 kHz stereo float is about 1.4 GB, so
        the size is checked against what is actually available first. */
    bool prepare (double sampleRate, double minutes = kDefaultMinutes);

    void reset() noexcept;

    void setEnabled (bool shouldRecord) noexcept
    {
        enabled.store (shouldRecord, std::memory_order_relaxed);
    }

    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    double getCapacityMinutes() const noexcept
    {
        return (double) capacity / juce::jmax (1.0, sr) / 60.0;
    }

    /** How much of the buffer holds audio. */
    int getRecordedSamples() const noexcept { return recorded.load (std::memory_order_relaxed); }

    /** Writes the block into the ring. Audio thread; never allocates. */
    void processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    void captureMidi (const juce::MidiBuffer& midi, int numSamples) noexcept;

    /** practice-tools 8: freezes the buffer to a WAV and a MIDI file, named by
        timestamp. Message thread. */
    bool saveLastTake (const juce::File& directory, double seconds = 0.0) const;

    /** practice-tools 8: temp files older than a day go, unless they were saved. */
    static void cleanUpOldTempFiles (const juce::File& directory, double olderThanHours = 24.0);

    static juce::File getSessionDirectory();
    static juce::File getTempDirectory();

private:
    juce::AudioBuffer<float> ring;
    juce::MidiMessageSequence midi;
    juce::CriticalSection midiLock;

    double sr = 44100.0;
    int capacity = 0;

    std::atomic<bool> enabled { false };
    std::atomic<int> writePosition { 0 };
    std::atomic<int> recorded { 0 };

    int64_t samplesSeen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionRecorder)
};

} // namespace luthier
