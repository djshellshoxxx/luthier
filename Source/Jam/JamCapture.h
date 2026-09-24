#pragma once

/*  The band's recent past as MIDI (jam-mode.md 9): a fixed ring of 8192
    note events (at most the last 64 bars), written on the audio thread with
    no allocation and copied out on the message thread for drag-out and
    export. The reader copies under a sequence check: entries the writer may
    have overwritten while it copied are dropped.

    It also carries the style changes the Luthier profile's text metas need.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

struct JamCaptureEvent
{
    int64_t sample = 0;        ///< absolute, including the latency offset
    double ppq = 0.0;          ///< the band's musical position (no offset)
    double bpm = 120.0;
    int barLengthQuarters = 4; ///< numerator * 4 / denominator, rounded
    int8_t part = 0;           ///< 0 drums, 1 bass, 2 a style marker
    uint8_t note = 0;
    uint8_t velocity = 0;      ///< 0 = note-off
    int16_t style = 0, variation = 0, intensity = 0, kit = 0;   ///< markers only
};

class JamCapture
{
public:
    static constexpr int kCapacity = 8192;
    static constexpr int kMaxBars = 64;

    void reset() noexcept
    {
        written.store (0, std::memory_order_release);
    }

    /** Audio thread. */
    void add (const JamCaptureEvent& e) noexcept
    {
        const auto n = written.load (std::memory_order_relaxed);
        ring[(size_t) (n % kCapacity)] = e;
        written.store (n + 1, std::memory_order_release);
    }

    /** Message thread: the events of the last `bars` bars (0 = all held),
        oldest first. */
    std::vector<JamCaptureEvent> copyLastBars (int bars) const
    {
        std::vector<JamCaptureEvent> out;
        const auto end = written.load (std::memory_order_acquire);
        const auto begin = end > (uint64_t) kCapacity ? end - (uint64_t) kCapacity : (uint64_t) 0;

        out.reserve ((size_t) (end - begin));

        for (auto i = begin; i < end; ++i)
            out.push_back (ring[(size_t) (i % kCapacity)]);

        // Anything the writer lapped while this copied is not trustworthy.
        const auto after = written.load (std::memory_order_acquire);
        const auto lapped = after > (uint64_t) kCapacity ? after - (uint64_t) kCapacity : (uint64_t) 0;

        if (lapped > begin)
            out.erase (out.begin(), out.begin() + (std::ptrdiff_t) juce::jmin ((uint64_t) out.size(), lapped - begin));

        if (bars > 0 && ! out.empty())
        {
            // Bars counted back from the last event's bar.
            double lastPpq = 0.0;

            for (const auto& e : out)
                lastPpq = juce::jmax (lastPpq, e.ppq);

            const double barLength = (double) juce::jmax (1, out.back().barLengthQuarters);
            const double lastBarStart = std::floor (lastPpq / barLength) * barLength;
            const double from = lastBarStart - (bars - 1) * barLength;

            out.erase (std::remove_if (out.begin(), out.end(),
                                       [from] (const JamCaptureEvent& e) { return e.ppq < from - 1.0e-9; }),
                       out.end());
        }

        return out;
    }

    uint64_t getNumWritten() const noexcept { return written.load (std::memory_order_acquire); }

private:
    std::array<JamCaptureEvent, kCapacity> ring {};
    std::atomic<uint64_t> written { 0 };
};

} // namespace luthier
