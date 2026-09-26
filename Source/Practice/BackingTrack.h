#pragma once

/*  The backing track player (practice-tools.md section 3).

    practice-tools 0.4 is the rule that shapes this: tracks stream from disk and
    are never loaded whole. An hour of stereo 48 kHz audio is 1.4 GB, and a
    practice tool that needs that much resident to play along to a backing track
    is a practice tool nobody can use.

    JUCE's AudioTransportSource over a BufferingAudioSource does exactly the
    streaming this needs - a reader thread filling a ring ahead of the audio
    thread - so this wraps those rather than reimplementing them, and adds what
    the spec asks for on top: loop points snapped to zero crossings, section
    markers, independent tone controls, and a tempo estimate on load.
*/

#include "../DSP/Common/DspCommon.h"
#include "TimePitchShifter.h"   // SPEC-SWEEP PT-28/29

// The streaming chain spans three modules: the reader and its source come from
// juce_audio_formats, the buffering source from juce_audio_basics, and the
// transport that drives them from juce_audio_devices.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/** A named position in a track (practice-tools 3). */
struct TrackMarker
{
    juce::String name;
    double seconds = 0.0;

    juce::var toVar() const;
    static TrackMarker fromVar (const juce::var& state);
};

//==============================================================================
class BackingTrackPlayer
{
public:
    /** practice-tools 3: a four-second ring per file. */
    static constexpr int kRingBufferSeconds = 4;

    BackingTrackPlayer();
    ~BackingTrackPlayer();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    /** Opens a file and starts streaming it. Message thread. Returns false if
        the format is not one the plugin can read. */
    bool load (const juce::File& file);

    void unload();

    bool isLoaded() const noexcept { return loaded.load (std::memory_order_relaxed); }
    juce::File getFile() const { return currentFile; }
    juce::String getTitle() const { return currentFile.getFileNameWithoutExtension(); }

    double getLengthSeconds() const noexcept { return lengthSeconds; }

    //==========================================================================
    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;

    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }

    void setPositionSeconds (double seconds);
    double getPositionSeconds() const;

    //==========================================================================
    void setLevelDb (double db) noexcept;
    double getLevelDb() const noexcept { return levelDb.load (std::memory_order_relaxed); }

    void setPan (double pan) noexcept;
    double getPan() const noexcept { return pan.load (std::memory_order_relaxed); }

    void setMonoSum (bool shouldSum) noexcept { monoSum.store (shouldSum, std::memory_order_relaxed); }
    bool isMonoSummed() const noexcept { return monoSum.load (std::memory_order_relaxed); }

    void setLowCutHz (double hz) noexcept;
    void setHighCutHz (double hz) noexcept;

    double getLowCutHz() const noexcept { return lowCutHz.load (std::memory_order_relaxed); }
    double getHighCutHz() const noexcept { return highCutHz.load (std::memory_order_relaxed); }

    //==========================================================================
    // Loop points (practice-tools 3).

    void setLoopEnabled (bool shouldLoop) noexcept { looping.store (shouldLoop, std::memory_order_relaxed); }
    bool isLoopEnabled() const noexcept { return looping.load (std::memory_order_relaxed); }

    /** Sets the loop, snapping each end to the nearest zero crossing so the loop
        does not click on the way round. */
    void setLoopSeconds (double startSeconds, double endSeconds);

    double getLoopStartSeconds() const noexcept { return loopStart.load (std::memory_order_relaxed); }
    double getLoopEndSeconds() const noexcept { return loopEnd.load (std::memory_order_relaxed); }

    //==========================================================================
    // Pitch and tempo (practice-tools 3).

    /** -12 to +12 semitones, without changing the tempo. */
    void setPitchShiftSemitones (double semitones) noexcept;
    double getPitchShiftSemitones() const noexcept { return pitchSemis.load (std::memory_order_relaxed); }

    /** 0.25 to 2.0, without changing the pitch. */
    void setTempoRatio (double ratio) noexcept;
    double getTempoRatio() const noexcept { return tempoRatio.load (std::memory_order_relaxed); }

    //==========================================================================
    // Markers (practice-tools 3).

    void addMarker (const juce::String& name, double seconds);
    bool removeMarker (int index);
    int getNumMarkers() const { return (int) markers.size(); }
    TrackMarker getMarker (int index) const;

    /** Jumps to a marker. Returns false if there is no such marker. */
    bool jumpToMarker (int index);

    //==========================================================================
    /** practice-tools 3: the tempo is estimated on load, so the metronome can
        follow the track. Returns 0 when nothing convincing was found. */
    double getDetectedTempo() const noexcept { return detectedTempo; }

    //==========================================================================
    // The playlist (practice-tools 3).

    void setPlaylist (const juce::Array<juce::File>& files);
    const juce::Array<juce::File>& getPlaylist() const noexcept { return playlist; }

    bool nextInPlaylist();
    bool previousInPlaylist();
    int getPlaylistPosition() const noexcept { return playlistPosition; }

    //==========================================================================
    /** Renders the track into a stereo buffer, which the caller mixes. Written,
        not added to. Audio thread. */
    void processBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept;

private:
    /** Finds the nearest sample either side of `seconds` where the waveform
        crosses zero, so a loop point does not land mid-cycle. */
    double snapToZeroCrossing (double seconds) const;

    /** A rough tempo estimate from the spacing of energy onsets. Nothing like a
        full beat tracker; enough to set a metronome from. */
    double estimateTempo();

    juce::AudioFormatManager formats;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::BufferingAudioSource> bufferingSource;
    std::unique_ptr<juce::AudioTransportSource> transport;

    /** Held by load/unload while they swap the three sources above; the audio
        thread only ever try-locks it and plays silence for that block. Without
        it, loading the next track freed the transport mid-callback. */
    juce::SpinLock transportLock;
    juce::TimeSliceThread readerThread { "Luthier backing track" };

    juce::File currentFile;
    double lengthSeconds = 0.0;
    double fileSampleRate = 44100.0;
    double detectedTempo = 0.0;

    double sr = 44100.0;
    int blockSize = 512;

    std::atomic<bool> loaded { false };
    std::atomic<bool> playing { false };

    std::atomic<double> levelDb { -6.0 };
    std::atomic<double> pan { 0.0 };
    std::atomic<bool> monoSum { false };
    std::atomic<double> lowCutHz { 20.0 };
    std::atomic<double> highCutHz { 20000.0 };

    std::atomic<bool> looping { false };
    std::atomic<double> loopStart { 0.0 }, loopEnd { 0.0 };

    std::atomic<double> pitchSemis { 0.0 };
    std::atomic<double> tempoRatio { 1.0 };

    // SPEC-SWEEP PT-28/29: the pitch and tempo shift, bypassed when neutral.
    // A seek, stop or new file throws away what it had read ahead.
    TimePitchShifter shifter;
    bool shifterActive = false;
    std::atomic<bool> shifterResetPending { false };

    /** Pulls `count` (<= blockSize) samples from the transport into `left` and
        `right`, and carries out the loop points and the end of the stream. */
    void pullFromTransport (float* left, float* right, int count) noexcept;

    Biquad lowCutL, lowCutR, highCutL, highCutR;
    double lastLowCut = -1.0, lastHighCut = -1.0;

    std::vector<TrackMarker> markers;

    juce::Array<juce::File> playlist;
    int playlistPosition = 0;

    /** A short window of the file's start, kept for zero-crossing snapping and
        tempo estimation without going back to the disk. */
    juce::AudioBuffer<float> analysisBuffer;
    double analysisSampleRate = 44100.0;

    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackingTrackPlayer)
};

} // namespace luthier
