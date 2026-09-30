#pragma once

/*  preset-browser-previews.md 4: plays a finished preview on top of the live
    output.

    Audio thread: one additive mix of a clip prepared in advance - no
    allocation, no lock, no file I/O, no resampling (ground rule 3). Two
    voices, so a replaced clip fades out over 30 ms before the next one starts
    and two clips are never both at full level. Raised-cosine fades: 10 ms in,
    30 ms on stop. Volume smoothed linearly over 20 ms.

    Message thread: start / stop / volume and the Gate snapshot the UI reads to
    decide whether a trigger may play (4.3).
*/

#include "PreviewClipPool.h"
#include <atomic>

namespace luthier
{

class PreviewPlayer
{
public:
    PreviewPlayer() = default;

    //==========================================================================
    // Message thread.

    /** Starts a clip (owned by a PreviewClipPool). Replaces any playing one. */
    void start (const PreviewClip* clip) noexcept;

    /** Fades out whatever is playing. */
    void stop() noexcept;

    void setVolumeDb (double db) noexcept   { targetGain.store ((float) juce::Decibels::decibelsToGain (juce::jlimit (-40.0, 0.0, db))); }
    double getVolumeDb() const noexcept     { return juce::Decibels::gainToDecibels ((double) targetGain.load()); }

    /** True from start() until the clip has finished or faded out. */
    bool isActive() const noexcept;

    /** The playing clip's progress, 0-1, for the row's waveform (-1 when idle). */
    float getProgress() const noexcept      { return progress.load (std::memory_order_relaxed); }

    /** The key of the clip that is sounding (message thread; empty when idle). */
    juce::String getActiveKey() const;

    /** 4.3: what the audio thread saw on its last block. */
    struct Gate
    {
        bool hostPlaying = false;
        bool nonRealtime = false;
        bool killActive = false;
        bool liveNotesHeld = false;
        double msSinceLiveNote = 1.0e9;
        double msSinceProcess = 1.0e9;   ///< > 200: the host is not processing audio
    };

    Gate getGate() const noexcept;

    /** Pointers the pool must keep alive (pending, and each voice's clip). */
    const PreviewClip* getPending() const noexcept          { return pending.load(); }
    const PreviewClip* getInUse (int voice) const noexcept  { return inUse[(size_t) voice].load(); }

    //==========================================================================
    // Audio thread.

    void prepare (double sampleRate) noexcept;

    /** Drops both voices at once (prepareToPlay, releaseResources, a rate change). */
    void dropAll() noexcept;

    /** panic(): fade out and drop (4.1). Any thread. */
    void panic() noexcept { stop(); }

    /** Publishes what the gate needs; call once per block before processBlock. */
    void publishGate (bool hostPlaying, bool nonRealtime, bool killActive,
                      int liveNotesHeld, bool liveNoteOnThisBlock) noexcept;

    /** Adds into main channels 0-1 (a mono bus gets both at -3 dB). */
    void processBlock (juce::AudioBuffer<float>& mainOut, int numSamples) noexcept;

    double getSampleRate() const noexcept { return sampleRate.load(); }

private:
    struct Voice
    {
        const PreviewClip* clip = nullptr;
        int position = 0;
        int fadeInPos = 0;        ///< samples into the 10 ms fade-in
        int fadeOutPos = -1;      ///< -1 = not fading out
    };

    void releaseVoice (int index) noexcept;

    std::atomic<const PreviewClip*> pending { nullptr };
    std::array<std::atomic<const PreviewClip*>, 2> inUse { nullptr, nullptr };
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> startRequested { false };
    std::atomic<bool> active { false };
    std::atomic<float> progress { -1.0f };
    std::atomic<float> targetGain { 0.5011872f };   // -6 dB (section 8)
    std::atomic<double> sampleRate { 48000.0 };

    // The gate snapshot, relaxed atomics (4.3).
    std::atomic<bool> gateHostPlaying { false }, gateNonRealtime { false }, gateKill { false }, gateHeld { false };
    std::atomic<double> lastNoteMs { -1.0e9 }, lastProcessMs { -1.0e9 };

    // Audio-thread only.
    std::array<Voice, 2> voices {};
    float currentGain = 0.5011872f;
    const PreviewClip* queued = nullptr;   ///< waits for the old voice's fade-out

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewPlayer)
};

} // namespace luthier
