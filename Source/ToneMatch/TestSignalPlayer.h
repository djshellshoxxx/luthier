#pragma once

/*  The Cab Match test signal on its way out to the rig (SPEC-SWEEP TM-17;
    tone-match 2: "the plugin plays the test signal out of Aux 1 (DI) while
    the reference comes back on the sidechain").

    The wizard hands over a generated signal on the message thread and arms
    it; the audio thread starts playing it and starts the capture in the same
    block, so the recording and the signal share sample zero - the
    deconvolution then sees only the rig's own delay.

    Lock-free handover as IrSlot's: the buffer being replaced is kept until
    the next start, by when the audio thread has long stopped reading it.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace luthier
{

class TestSignalPlayer
{
public:
    /** Message thread: the signal to play, and how long the capture should run. */
    void arm (std::vector<float> signal, double captureSeconds)
    {
        stop();

        retired = std::move (live);
        live = std::make_unique<std::vector<float>> (std::move (signal));
        seconds.store (captureSeconds, std::memory_order_relaxed);

        active.store (live.get(), std::memory_order_release);
        armed.store (true, std::memory_order_release);
    }

    /** Message thread. */
    void stop() noexcept
    {
        armed.store (false, std::memory_order_relaxed);
        playing.store (false, std::memory_order_relaxed);
    }

    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
    bool isArmed() const noexcept   { return armed.load (std::memory_order_relaxed); }

    /** Audio thread: true once, in the block the signal starts, with the
        capture's length - the caller starts the capture there. */
    bool takeStart (double& captureSeconds) noexcept
    {
        if (! armed.exchange (false, std::memory_order_acq_rel))
            return false;

        position = 0;
        playing.store (true, std::memory_order_relaxed);
        captureSeconds = seconds.load (std::memory_order_relaxed);
        return true;
    }

    /** Audio thread: the next `numSamples` of the signal into `destination`
        (silence after its end). False when not playing. */
    bool render (float* destination, int numSamples) noexcept
    {
        if (! isPlaying())
            return false;

        const auto* signal = active.load (std::memory_order_acquire);

        if (signal == nullptr)
        {
            playing.store (false, std::memory_order_relaxed);
            return false;
        }

        const int length = (int) signal->size();

        for (int i = 0; i < numSamples; ++i)
            destination[i] = position + i < length ? (*signal)[(size_t) (position + i)] : 0.0f;

        position += numSamples;

        if (position >= length)
            playing.store (false, std::memory_order_relaxed);

        return true;
    }

private:
    std::unique_ptr<std::vector<float>> live, retired;
    std::atomic<const std::vector<float>*> active { nullptr };
    std::atomic<bool> armed { false }, playing { false };
    std::atomic<double> seconds { 6.0 };
    int position = 0;
};

} // namespace luthier
