#pragma once

/*  Sung / hummed melody capture (tune-builder.md 13). TUNE-HELP-ONBOARDING.

    "If the standalone app has a mic input and the user chooses Record -> Voice,
    the plugin runs a monophonic pitch tracker on the input and converts the
    sung line into a melody track ... rejects samples with confidence below
    0.6, snaps to key by default, quantises timing on release."

    Two halves, the usual split:
      AUDIO THREAD  pushAudio() mixes the input to mono into a lock-free ring
                    while armed. Nothing else, and nothing that allocates.
      MESSAGE       process() (the UI timer) drains the ring and runs the
                    PitchTracker every hop; finish() turns the frames into
                    notes - confident, voiced frames, median-smoothed, grouped
                    into runs of one semitone - positioned in the section's
                    beats, then quantised by the tune builder's own Record
                    quantiser (4.3), so a sung take and a played take land
                    the same way.
*/

#include "TuneMelody.h"
#include "../DSP/Common/PitchTracker.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace luthier
{

class TuneHumCapture
{
public:
    static constexpr double kMinConfidence = 0.6;     ///< tune-builder 13
    static constexpr double kMinNoteSeconds = 0.06;
    static constexpr double kRingSeconds = 4.0;
    static constexpr double kHopSeconds = 0.005;

    struct Frame
    {
        double seconds = 0.0;      ///< the frame's centre, from begin()
        double midi = 0.0;         ///< fractional MIDI note; 0 unvoiced
        double confidence = 0.0;
        double rms = 0.0;
    };

    /** Message thread, before audio runs. */
    void prepare (double sampleRate);

    /** Starts a take: `startBeat` is where in the section the first sample
        lands and `tempoBpm` turns seconds into beats. Clears the last one. */
    void begin (double tempoBpm, double startBeat);

    void setArmed (bool shouldCapture) noexcept { armed.store (shouldCapture, std::memory_order_release); }
    bool isArmed() const noexcept               { return armed.load (std::memory_order_acquire); }

    /** Audio thread: the input, mixed to mono, into the ring while armed. */
    void pushAudio (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Message thread: drains the ring and analyses what arrived. */
    void process();

    const std::vector<Frame>& getFrames() const noexcept { return frames; }

    /** The take as notes in section beats, before quantising. */
    std::vector<RecordedNote> segment() const;

    /** process(), then the take quantised into the section (4.3's quantiser). */
    std::vector<MelodyNote> finish (const Tune& tune, int sectionIndex, QuantiseGrid grid, bool snapToKey);

    double getSampleRate() const noexcept { return sampleRate; }

private:
    double sampleRate = 48000.0;
    double tempo = 120.0, startBeat = 0.0;
    int hop = 240;

    std::atomic<bool> armed { false };
    std::unique_ptr<juce::AbstractFifo> fifo;
    std::vector<float> ring, mono;

    PitchTracker tracker;
    std::vector<float> pending;   ///< drained, not yet analysed
    juce::int64 consumed = 0;     ///< samples analysed-past since begin()
    std::vector<Frame> frames;
};

} // namespace luthier
