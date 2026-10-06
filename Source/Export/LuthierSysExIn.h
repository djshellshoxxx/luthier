#pragma once

/*
    SPEC-SWEEP (input-routing IR-15, §1.5): Luthier-profile SysEx arriving at a
    running plugin.

    Decoding an event builds strings and vectors, which the audio thread may
    not do. So the audio thread only recognises a Luthier event SysEx (three
    header bytes) and copies its bytes into a fixed ring; the message thread
    drains the ring, decodes each event with LuthierEvents::decodeSysEx and
    hands it to the processor, which applies the classes that describe
    non-parameter state (CHARACTER, SNAPSHOT, RANGES). Other SysEx is ignored,
    and nothing is taken out of the MIDI stream.
*/

#include "LuthierMidiEvents.h"

#include <array>
#include <functional>

namespace luthier
{

class LuthierSysExIn
{
public:
    static constexpr int kRingBytes = 8192;
    static constexpr int kMaxEventBytes = 1024;

    /** Audio thread: copies this block's Luthier event SysEx into the ring. An
        event that does not fit is dropped and counted. */
    void capture (const juce::MidiBuffer& midi) noexcept;

    /** Message thread: decodes every queued event and passes it to @p apply.
        Returns how many decoded. */
    int drain (const std::function<void (const LuthierEvent&)>& apply);

    int getDroppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo { kRingBytes };
    std::array<juce::uint8, kRingBytes> ring {};
    std::atomic<int> dropped { 0 };

    void read (juce::uint8* data, int n) noexcept;
};

} // namespace luthier
