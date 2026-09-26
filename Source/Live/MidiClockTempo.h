#pragma once

/*  Tempo from incoming MIDI clock (SPEC-SWEEP HI-32; host-integration 7:
    "MIDI clock and transport are accepted", used as the internal clock when
    no host transport is running - the standalone case).

    24 clocks to the quarter note. The tempo is the mean spacing of the last
    kWindow clocks, so the jitter of a USB interface averages out; a gap of
    more than half a second (the clock stopped) starts the count again. Start
    and Continue restart it too, and Stop ends it.

    Audio thread only: fixed storage, no allocation.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace luthier
{

class MidiClockTempo
{
public:
    static constexpr int kWindow = 48;   ///< two beats of clocks

    /** Reads the block's clock, start, continue and stop messages. `blockStartSeconds`
        is the block's place on the plugin's own clock. */
    void process (const juce::MidiBuffer& midi, double blockStartSeconds, double sampleRate) noexcept
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const double t = blockStartSeconds + metadata.samplePosition / juce::jmax (1.0, sampleRate);

            if (message.isMidiStart() || message.isMidiContinue() || message.isMidiStop())
            {
                count = 0;
                running = ! message.isMidiStop();
            }
            else if (message.isMidiClock())
            {
                if (count > 0 && t - times[(size_t) ((next + kWindow - 1) % kWindow)] > 0.5)
                    count = 0;

                times[(size_t) next] = t;
                next = (next + 1) % kWindow;
                count = juce::jmin (count + 1, kWindow);
                running = true;
            }
        }
    }

    /** The clock's tempo, or 0 when there is not enough of it (a beat's worth). */
    double getBpm() const noexcept
    {
        if (! running || count < 24)
            return 0.0;

        const double newest = times[(size_t) ((next + kWindow - 1) % kWindow)];
        const double oldest = times[(size_t) ((next + kWindow - count) % kWindow)];
        const double perClock = (newest - oldest) / (count - 1);

        return perClock > 0.0 ? juce::jlimit (20.0, 300.0, 60.0 / (perClock * 24.0)) : 0.0;
    }

    void reset() noexcept { count = 0; next = 0; running = false; }

private:
    std::array<double, kWindow> times {};
    int next = 0, count = 0;
    bool running = false;
};

} // namespace luthier
