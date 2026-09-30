#pragma once

/*  RT-SAFETY (docs/review/CODEX_RTSAFETY.md P1): MIDI copies on the audio
    thread that never grow a buffer.

    A juce::MidiBuffer grows its storage when an event does not fit, and that
    growth is a heap allocation. The audio thread's own MIDI buffers are sized
    in prepare (kReserveBytes, about ten thousand short events); the helpers
    here copy into them only while the event fits in what was reserved, so a
    burst larger than any real controller sends is trimmed rather than
    allocated for.

    The overflow policy is deterministic and never strands a note. "Releases"
    are note-offs, note-ons at velocity 0, sustain/sostenuto/soft pedal up,
    and the channel-mode messages (all sound off, all notes off, ...). A
    block copy (addEvents) measures its releases first and keeps room for all
    of them, dropping ordinary events instead; a single add() keeps the last
    kReleaseHeadroomBytes for releases. A release is dropped only when the
    releases alone exceed the reserve. */

#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier::BoundedMidi
{
    /** What the audio thread's own MIDI buffers are given with ensureSize. */
    constexpr int kReserveBytes = 64 * 1024;

    /** The tail of each buffer only a release may use. */
    constexpr int kReleaseHeadroomBytes = 16 * 1024;

    /** JUCE's per-event cost in a MidiBuffer: an int32 time, a uint16 size, the bytes. */
    constexpr int eventCost (int numBytes) noexcept
    {
        return numBytes + (int) sizeof (juce::int32) + (int) sizeof (juce::uint16);
    }

    /** Whether a message ends sound (so it must survive an overflow). */
    inline bool isRelease (const juce::uint8* d, int numBytes) noexcept
    {
        if (d == nullptr || numBytes < 1)
            return false;

        const int status = d[0] & 0xf0;

        if (status == 0x80)
            return true;

        if (status == 0x90)
            return numBytes >= 3 && d[2] == 0;

        if (status == 0xb0 && numBytes >= 3)
        {
            const int cc = d[1];

            if (cc >= 120)                                    // channel mode: all notes off etc.
                return true;

            if ((cc == 64 || cc == 66 || cc == 67) && d[2] < 64)   // pedals up
                return true;
        }

        return false;
    }

    /** The bytes a buffer holds. (juce::Array does not expose its capacity, so
        the caller states what it reserved with ensureSize; storage is at least that.) */
    inline int bytesUsed (const juce::MidiBuffer& b) noexcept { return b.data.size(); }

    /** Adds one event if it fits within @p reservedBytes (what @p dest was given
        with ensureSize) under the policy above. Returns false, adding nothing,
        when it was dropped. */
    inline bool add (juce::MidiBuffer& dest, int reservedBytes,
                     const juce::uint8* data, int numBytes, int samplePosition) noexcept
    {
        if (data == nullptr || numBytes <= 0)
            return true;   // nothing to add, nothing lost

        const int needed = bytesUsed (dest) + eventCost (numBytes);
        const int limit = isRelease (data, numBytes) ? reservedBytes
                                                     : reservedBytes - juce::jmin (kReleaseHeadroomBytes, reservedBytes / 4);

        if (needed > limit)
            return false;

        dest.addEvent (data, numBytes, samplePosition);
        return true;
    }

    /** add() with the standard reserve. */
    inline bool add (juce::MidiBuffer& dest, const juce::uint8* data, int numBytes, int samplePosition) noexcept
    {
        return add (dest, kReserveBytes, data, numBytes, samplePosition);
    }

    /** addEvents without growth: the events of @p source in [start, start + count)
        (count < 0: to the end), shifted by @p delta, into a buffer reserved with
        @p reservedBytes. Order is kept. A first pass measures the releases in
        the range, and an ordinary event is only taken while the releases still
        to come would fit after it - so every release survives whenever the
        releases alone fit. Returns how many were dropped. */
    inline int addEvents (juce::MidiBuffer& dest, int reservedBytes, const juce::MidiBuffer& source,
                          int start, int count, int delta) noexcept
    {
        auto inRange = [start, count] (int pos) { return pos >= start && (count < 0 || pos < start + count); };

        int releaseBytesToCome = 0;

        for (const auto metadata : source)
            if (inRange (metadata.samplePosition) && isRelease (metadata.data, metadata.numBytes))
                releaseBytesToCome += eventCost (metadata.numBytes);

        int dropped = 0;

        for (const auto metadata : source)
        {
            if (! inRange (metadata.samplePosition) || metadata.data == nullptr || metadata.numBytes <= 0)
                continue;

            const int cost = eventCost (metadata.numBytes);
            const bool release = isRelease (metadata.data, metadata.numBytes);

            if (release)
                releaseBytesToCome -= cost;

            const int limit = release ? reservedBytes : reservedBytes - releaseBytesToCome;

            if (bytesUsed (dest) + cost > limit)
            {
                ++dropped;
                continue;
            }

            dest.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition + delta);
        }

        return dropped;
    }

    /** A MidiMessage to inspect, built without the heap: a message longer than
        a MidiMessage keeps inline (a long SysEx) comes back as an empty SysEx,
        which is none of note, controller, program change or pitch wheel.
        Callers that pass the event on copy metadata.data, not this. */
    inline juce::MidiMessage inspect (const juce::MidiMessageMetadata& m) noexcept
    {
        if (m.data == nullptr || m.numBytes <= 0 || m.numBytes > (int) sizeof (juce::uint8*))
            return {};

        return m.getMessage();
    }
}
