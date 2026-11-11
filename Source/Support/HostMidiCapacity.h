// OWNER: WP23
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
namespace luthier
{
// Pinned JUCE 8.0.10 wrappers reserve 2048 before callbacks. Standalone's
// 65536-byte constructor reserve is verified/applied by configure using
// scripts/patch_juce_midi_capacity.py before compiling juce_audio_utils.
inline constexpr int kJuceWrapperMidiReserve = 2048;
inline constexpr int kStandaloneMidiReserve = 65536;
enum class MidiCapacityContract { Unknown, JuceWrapper, Standalone };
class HostMidiCapacity
{
public:
    // Off audio while processing is stopped. The buffer must remain alive and
    // retain its storage through processBlock; swaps/shrinks invalidate this.
    void reserveExternal (juce::MidiBuffer& buffer, int bytes)
    {
        bytes = std::clamp (bytes, 0, kStandaloneMidiReserve);
        buffer.ensureSize ((size_t) bytes);
        external = &buffer; storage = buffer.data.getRawDataPointer(); reserved = bytes;
    }
    int lowerBound (const juce::MidiBuffer& buffer, MidiCapacityContract contract) const noexcept
    {
        int capacity = contract == MidiCapacityContract::JuceWrapper ? kJuceWrapperMidiReserve
            : contract == MidiCapacityContract::Standalone ? kStandaloneMidiReserve : 0;
        if (&buffer == external && buffer.data.getRawDataPointer() == storage) capacity = std::max (capacity, reserved);
        return std::max (capacity, buffer.data.size()); // used bytes always fit
    }
private:
    const juce::MidiBuffer* external = nullptr;
    const juce::uint8* storage = nullptr;
    int reserved = 0;
};
}
