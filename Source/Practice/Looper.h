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
#include "../Export/MidiProfiles.h"

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
    void clear();

    /** practice-tools 2: a loop is quantised to bars when the metronome is
        running. The caller supplies the bar length; zero means free. */
    void setBarLengthSamples (int samples) noexcept
    {
        barLengthSamples.store (juce::jmax (0, samples), std::memory_order_relaxed);
    }

    /** practice-tools 11.2 "Looper: default length". Above zero, the first
        recording closes itself at this many seconds (rounded to the bar when
        the metronome quantises, at least one bar) without a second press; zero
        leaves the length to the second press, as a looper pedal does. */
    void setDefaultLengthSeconds (double seconds) noexcept
    {
        defaultLengthSeconds.store (seconds > 0.0 ? juce::jlimit (kMinLoopSeconds, kMaxLoopSeconds, seconds) : 0.0,
                                    std::memory_order_relaxed);
    }

    double getDefaultLengthSeconds() const noexcept { return defaultLengthSeconds.load (std::memory_order_relaxed); }

    /** The samples the first recording will close at on its own, after bar
        quantisation; zero when the length is left to the second press. */
    int getDefaultLengthTargetSamples() const noexcept;

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
    std::atomic<int> playPosition { 0 };
    std::atomic<int> activeLayer { 0 };
    std::atomic<int> barLengthSamples { 0 };
    std::atomic<double> defaultLengthSeconds { 0.0 };

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
    once, and writing into it is a copy into memory that already exists. The
    MIDI beside it is the same shape - a ring of fixed-size events, sized with
    the audio - so the audio thread never takes a lock or grows a sequence.

    Disabled by default, which is what the spec asks for - a recorder that is on
    without being asked is a surprise nobody wants.

    practice-tools 11.2 sets three things about it on the PRACTICE tab: whether
    it records audio, MIDI or both, and whether stopping it saves the take. The
    saved MIDI is written by Source/Export's writer, so a take opens anywhere a
    MIDI OUT export does and comes back as a Luthier-profile file. */
class SessionRecorder
{
public:
    static constexpr double kDefaultMinutes = 60.0;

    /** MIDI events the ring holds per second of audio: fast strumming with a
        bend on every string is well under this. */
    static constexpr int kMidiEventsPerSecond = 60;

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

    //==========================================================================
    // 11.2 "Session recorder setup": what to record, and what stopping does.

    void setRecordAudio (bool should) noexcept { recordAudio.store (should, std::memory_order_relaxed); }
    bool isRecordingAudio() const noexcept    { return recordAudio.load (std::memory_order_relaxed); }

    void setRecordMidi (bool should) noexcept  { recordMidi.store (should, std::memory_order_relaxed); }
    bool isRecordingMidi() const noexcept     { return recordMidi.load (std::memory_order_relaxed); }

    void setAutoSaveOnStop (bool should) noexcept { autoSave.store (should, std::memory_order_relaxed); }
    bool isAutoSaveOnStop() const noexcept        { return autoSave.load (std::memory_order_relaxed); }

    /** Where stop() auto-saves and where the drawer's Save writes: the user's
        Sessions folder (practice-tools 10) unless a test points it elsewhere. */
    void setSaveDirectory (const juce::File& directory) { saveDirectory = directory; }
    juce::File getSaveDirectory() const                 { return saveDirectory; }

    /** The tempo the saved MIDI file's map carries: note timing is at the
        samples played whatever this says, but a DAW's bars follow it. */
    void setTempoBpm (double bpm) noexcept { tempoBpm.store (juce::jlimit (20.0, 300.0, bpm), std::memory_order_relaxed); }
    double getTempoBpm() const noexcept    { return tempoBpm.load (std::memory_order_relaxed); }

    /** The MIDI OUT defaults (midi-export 8) the take is written with: profile,
        PPQ, split. Message thread; the range is set per take. */
    void setMidiExportOptions (const MidiExportOptions& options) { exportOptions = options; }
    const MidiExportOptions& getMidiExportOptions() const noexcept { return exportOptions; }

    double getCapacityMinutes() const noexcept
    {
        return (double) capacity / juce::jmax (1.0, sr) / 60.0;
    }

    /** How much of the buffer holds a take. Counts while MIDI-only recording
        too, because a take has a length whether or not its audio was kept. */
    int getRecordedSamples() const noexcept { return recorded.load (std::memory_order_relaxed); }

    /** MIDI events held, after the ring's own wrap. */
    int getRecordedMidiEvents() const noexcept;

    //==========================================================================
    /** Stamps the block's MIDI against the audio seen so far, so call it before
        processBlock in the same block. Audio thread; never allocates. */
    void captureMidi (const juce::MidiBuffer& midi, int numSamples) noexcept;

    /** Writes the block into the ring. Audio thread; never allocates. */
    void processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    //==========================================================================
    /** What saveLastTake wrote: either file is File() when that side was not
        recorded or held nothing. */
    struct SavedTake
    {
        juce::File wav, midi;

        bool isEmpty() const noexcept { return wav == juce::File() && midi == juce::File(); }
    };

    /** practice-tools 8: freezes the buffer to a WAV and a MIDI file, named by
        timestamp (`session-YYYYMMDD-HHMMSS.wav` / `.mid`, a `-2` on a clash),
        the MIDI in the Luthier profile with the notes at the samples they were
        played against the WAV's start. `seconds` above zero keeps only the
        last that many. False when there was nothing to write. Message thread. */
    bool saveLastTake (const juce::File& directory, SavedTake* saved = nullptr, double seconds = 0.0) const;

    /** The drawer's stop: the recorder goes off and, with auto-save on, the take
        is written to the save directory (11.2 "Auto-save on stop"). Returns
        whether a take was written. Message thread. */
    bool stop (SavedTake* saved = nullptr);

    /** midi-export 4.2 for the drawer's Save button: the take as files under
        the session temp folder (practice-tools 10), for an external drag. The
        WAV as recorded and the MIDI in the Luthier profile, or Generic when
        `forceGeneric` (Alt while dragging). Empty when there is nothing. */
    juce::Array<juce::File> writeDragOutFiles (bool forceGeneric) const;

    /** practice-tools 8: temp files older than a day go, unless they were saved. */
    static void cleanUpOldTempFiles (const juce::File& directory, double olderThanHours = 24.0);

    static juce::File getSessionDirectory();
    static juce::File getTempDirectory();

private:
    struct MidiEvent
    {
        int64_t sample = 0;
        juce::uint8 bytes[3] = {};
        juce::uint8 numBytes = 0;
    };

    /** The last `wanted` samples' MIDI as a performance starting at zero. */
    MidiPerformance takeMidi (int wanted) const;
    bool writeTake (const juce::File& wav, const juce::File& mid, int wanted,
                    const MidiExportOptions& options, SavedTake& saved) const;

    juce::AudioBuffer<float> ring;
    std::vector<MidiEvent> midiRing;
    int midiCapacity = 0;

    double sr = 44100.0;
    int capacity = 0;

    std::atomic<bool> enabled { false };
    std::atomic<bool> recordAudio { true };
    std::atomic<bool> recordMidi { true };
    std::atomic<bool> autoSave { false };
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<int> writePosition { 0 };
    std::atomic<int> recorded { 0 };
    std::atomic<int> midiWriteIndex { 0 };
    std::atomic<int> midiWritten { 0 };

    /** Audio samples the recorder has been shown since reset: the clock the
        MIDI is stamped against, and where a take ends. Written by the audio
        thread, read by a save. */
    std::atomic<int64_t> samplesSeen { 0 };

    juce::File saveDirectory { getSessionDirectory() };
    MidiExportOptions exportOptions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionRecorder)
};

} // namespace luthier
