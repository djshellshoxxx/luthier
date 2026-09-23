#pragma once

/*  Live MIDI out: the alignment helpers midi-export.md 6 needs beyond what
    MidiOutRouter (routing-io 6) already does.

    MidiOutRouter puts pass-through, rhythm, string activity and CC broadcast
    out at their exact offsets. This adds the two sources 6 lists that it has
    no way to carry:

      - LiveMidiClock: where a position in beats lands in a block, from the
        host's own playhead. The tune-builder's playback is scheduled in beats;
        this is how each of its events goes out on the sample it belongs to.

      - LuthierSysExOut: character, noise and workshop events as Luthier SysEx
        - the same bytes a Luthier-profile file carries, so another instance
        reads them with LuthierEvents::decodeSysEx. Queued from the audio
        thread at a sample offset, appended after MidiOutRouter::emit. Fixed
        storage, no allocation, no locks. A host that does not know Luthier
        drops SysEx (6); nothing else in the stream depends on it.
*/

#include "LuthierMidiEvents.h"

#include <array>
#include <atomic>
#include <initializer_list>

namespace luthier
{

//==============================================================================
namespace LiveMidiClock
{
    /** The exact, fractional sample a quarter-note position falls on, given the
        block's own playhead: its first sample, its PPQ position and the tempo. */
    double ppqToSampleExact (double ppq, juce::int64 blockStartSample, double blockStartPpq,
                             double bpm, double sampleRate) noexcept;

    /** The same, rounded to the sample it goes out on: within half a sample. */
    juce::int64 ppqToSample (double ppq, juce::int64 blockStartSample, double blockStartPpq,
                             double bpm, double sampleRate) noexcept;

    double sampleToPpq (juce::int64 sample, juce::int64 blockStartSample, double blockStartPpq,
                        double bpm, double sampleRate) noexcept;

    /** Where an absolute sample falls in [blockStart, blockStart + numSamples),
        or -1 when outside. An event on a block boundary belongs to the later
        block, so none is sent twice and none is skipped. */
    int offsetInBlock (juce::int64 sample, juce::int64 blockStartSample, int numSamples) noexcept;
}

//==============================================================================
class LuthierSysExOut
{
public:
    static constexpr int kMaxEvents = 64;
    static constexpr int kMaxFields = 8;
    static constexpr int kMaxEventBytes = 256;

    /** One tagged field. Keys and words are string literals: nothing is copied
        until the event is encoded, and that happens inside push(). */
    struct Field
    {
        enum class Kind { word, integer, real };

        const char* key = nullptr;
        Kind kind = Kind::integer;
        const char* word = nullptr;
        juce::int64 integer = 0;
        double real = 0.0;

        static Field makeWord (const char* fieldKey, const char* value) noexcept;
        static Field makeInt (const char* fieldKey, juce::int64 value) noexcept;
        static Field makeReal (const char* fieldKey, double value) noexcept;
    };

    LuthierSysExOut() = default;

    /** Audio thread. Encodes the event now, into this block's fixed storage.
        Reals go out to six decimal places. Returns false, and counts a drop,
        when the block's queue is full or the event would not fit. */
    bool push (LuthierEventClass eventClass, int sampleOffset,
               const Field* fields, int numFields, int part = 0) noexcept;

    bool push (LuthierEventClass eventClass, int sampleOffset,
               std::initializer_list<Field> fields, int part = 0) noexcept
    {
        return push (eventClass, sampleOffset, fields.begin(), (int) fields.size(), part);
    }

    /** Adds the block's events to the outgoing buffer at their offsets, then
        forgets them. Call after MidiOutRouter::emit, which clears the buffer. */
    void appendTo (juce::MidiBuffer& destination, int numSamples) noexcept;

    /** Forgets this block's events without sending them (MIDI out is off). */
    void clear() noexcept { numPending = 0; }

    int getNumPending() const noexcept { return numPending; }

    /** Events that did not fit. Diagnostics only. */
    int getDroppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    struct Pending
    {
        int sampleOffset = 0;
        int numBytes = 0;
        std::array<juce::uint8, kMaxEventBytes> bytes {};
    };

    std::array<Pending, kMaxEvents> pending {};
    int numPending = 0;
    std::atomic<int> dropped { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierSysExOut)
};

} // namespace luthier
