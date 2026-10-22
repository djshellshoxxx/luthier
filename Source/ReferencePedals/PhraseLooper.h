// OWNER: WP15 (prototype looper, removed when WP15 ends; WP1 added this line only)
#pragma once

// Stereo dry-through phrase recording primitive for future reference looper
// pedals. This is NOT yet an RC-1 or Ditto clone: foot-gesture decoding,
// overdub undo/redo, persistence, and multi-minute disk-backed storage remain
// intentionally outside this minimal, tested real-time core.
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstddef>

namespace luthier::reference
{
class PhraseLooper final
{
public:
    enum class State : int { empty, recording, playing, overdubbing, stopped };
    enum class Command : int { none, record, play, overdub, stop, clear };

    void prepare (double sampleRate, double maxSeconds)
    {
        // Allocates only on the host's non-realtime prepare thread.
        const double sr = std::isfinite (sampleRate) ? std::clamp (sampleRate, 8000.0, 192000.0) : 48000.0;
        const double seconds = std::isfinite (maxSeconds) ? std::clamp (maxSeconds, 0.01, 60.0) : 5.0;
        const auto count = std::max<size_t> (1, (size_t) std::ceil (sr * seconds));
        dataL.assign (count, 0.0f);
        dataR.assign (count, 0.0f);
        reset();
    }
    void reset() noexcept
    {
        pending.store (Command::none, std::memory_order_relaxed);
        state.store (State::empty, std::memory_order_release);
        recordedSamples.store (0, std::memory_order_release);
        length = 0;
        head = 0;
    }

    // Safe on GUI/MIDI thread. The latest single request is adopted at the
    // start of the next block. Sample-accurate events will use a fixed queue.
    void request (Command command) noexcept { pending.store (command, std::memory_order_release); }
    State getState() const noexcept { return state.load (std::memory_order_acquire); }
    int getRecordedSamples() const noexcept { return recordedSamples.load (std::memory_order_acquire); }
    int getCapacity() const noexcept { return (int) dataL.size(); }
    void setPlaybackLevel (double value) noexcept
    {
        if (std::isfinite (value))
            playbackLevel.store (std::clamp (value, 0.0, 2.0), std::memory_order_release);
    }

    // Audio-thread only. Never allocates, waits, reads files, or touches UI.
    void processStereo (double* left, double* right, int frames) noexcept
    {
        if (!left || !right || frames <= 0 || dataL.empty()) return;
        State mode = state.load (std::memory_order_relaxed);
        switch (pending.exchange (Command::none, std::memory_order_acq_rel))
        {
            case Command::record:
                if (mode == State::empty || mode == State::stopped)
                {
                    head = 0; length = 0; mode = State::recording;
                }
                else if (mode == State::playing) mode = State::overdubbing;
                break;
            case Command::play:
                if (mode == State::recording)
                {
                    mode = length > 0 ? State::playing : State::empty;
                    head = 0;
                }
                else if (mode == State::overdubbing) mode = State::playing;
                else if (mode == State::stopped && length > 0)
                {
                    head = 0;
                    mode = State::playing;
                }
                break;
            case Command::overdub:
                if (mode == State::playing) mode = State::overdubbing;
                else if (mode == State::overdubbing) mode = State::playing;
                break;
            case Command::stop:
                mode = length > 0 ? State::stopped : State::empty;
                head = 0;
                break;
            case Command::clear:
                // Immediate logical clear. No O(length) memory fill in callback.
                length = 0;
                head = 0;
                mode = State::empty;
                break;
            case Command::none: break;
        }

        const auto playbackGain = playbackLevel.load (std::memory_order_relaxed);
        const int capacity = (int) dataL.size();
        for (int i = 0; i < frames; ++i)
        {
            const double inL = std::isfinite (left[i]) ? std::clamp (left[i], -8.0, 8.0) : 0.0;
            const double inR = std::isfinite (right[i]) ? std::clamp (right[i], -8.0, 8.0) : 0.0;

            if (mode == State::recording)
            {
                if (head < capacity)
                {
                    dataL[(size_t) head] = (float) inL;
                    dataR[(size_t) head] = (float) inR;
                    ++head;
                    length = std::max (length, head);
                }
                if (head == capacity)
                {
                    head = 0;
                    mode = State::playing;
                }
            }
            else if ((mode == State::playing || mode == State::overdubbing) && length > 0)
            {
                const double prevL = dataL[(size_t) head], prevR = dataR[(size_t) head];
                left[i] = std::clamp (inL + playbackGain * prevL, -16.0, 16.0);
                right[i] = std::clamp (inR + playbackGain * prevR, -16.0, 16.0);
                if (mode == State::overdubbing)
                {
                    // Previous layer plus new input. One-way destructive
                    // overdubbing until separate layer-based undo is built.
                    dataL[(size_t) head] = (float) std::clamp (prevL + inL, -8.0, 8.0);
                    dataR[(size_t) head] = (float) std::clamp (prevR + inR, -8.0, 8.0);
                }
                ++head;
                if (head >= length) head = 0;
            }
            // Recording/stopped/empty: direct input passes without modification.
        }
        recordedSamples.store (length, std::memory_order_release);
        state.store (mode, std::memory_order_release);
    }

private:
    std::vector<float> dataL, dataR;
    std::atomic<Command> pending {Command::none};
    std::atomic<State> state {State::empty};
    std::atomic<int> recordedSamples {0};
    std::atomic<double> playbackLevel {1.0};
    int length = 0;  // audio thread only
    int head = 0;    // audio thread only
};
} // namespace luthier::reference
