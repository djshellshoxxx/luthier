#pragma once

#include "BoundedMidi.h"

namespace luthier
{
/** Audio-thread-only carry for MIDI whose engine callback could not obtain the
    structural lock. Upstream consumers have already seen it; replay only into
    the engine, at the start of the next admitted block. Storage is prepared off
    audio. Overflow replaces the incomplete history with a bounded panic. */
class DeferredEngineMidi
{
public:
    void prepare()
    {
        pending.ensureSize (BoundedMidi::kReserveBytes);
        merged.ensureSize (BoundedMidi::kReserveBytes);
        reset();
    }

    void reset() noexcept { pending.clear(); merged.clear(); }

    void defer (const juce::MidiBuffer& input) noexcept
    {
        for (const auto event : input)
            if (! BoundedMidi::add (pending, event.data, event.numBytes, 0))
            {
                panic (pending);
                return;
            }
    }

    juce::MidiBuffer& take (juce::MidiBuffer& input) noexcept
    {
        if (pending.isEmpty())
            return input;
        merged.clear();
        int dropped = BoundedMidi::addEvents (merged, BoundedMidi::kReserveBytes, pending, 0, -1, 0);
        dropped += BoundedMidi::addEvents (merged, BoundedMidi::kReserveBytes, input, 0, -1, 0);
        pending.clear();
        if (dropped > 0)
            panic (merged);
        return merged;
    }

private:
    juce::MidiBuffer pending, merged;

    static void panic (juce::MidiBuffer& output) noexcept
    {
        output.clear();
        // Release pedals before notes. Include all-sound-off for instruments
        // whose envelope or sustain handling ignores an ordinary all-notes-off.
        for (int channel = 0; channel < 16; ++channel)
            for (int cc : {64, 66, 67, 123, 120})
            {
                const juce::uint8 bytes[] { (juce::uint8) (0xb0 | channel), (juce::uint8) cc, 0 };
                BoundedMidi::add (output, bytes, 3, 0);
            }
    }
};
} // namespace luthier
