#pragma once

/*  What the strings are sounding right now, for the display (animated-strings.md
    4.1, piano-roll-chord-display.md 2).

    One record per engine string plus the piano roll's 128-bit note set, written
    by the audio thread once per sub-block (LuthierEngine::publishSoundingNotes)
    and read by the message thread. It is a seqlock: the writer bumps `sequence`
    to odd, stores every field relaxed, and bumps it back to even with release
    ordering. It never waits. The reader copies the fields between two reads of
    `sequence` and retries at most three times; on failure it keeps whatever
    snapshot it had (animated-strings 12).

    Header-only and POD-only. Both specs share it: the piano roll reads
    `noteSet`, `midiNote` and `startSample`; the string animation reads the rest.
    A field is added at the end of its struct, never reordered.
*/

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace luthier
{

struct SoundingString            // one per engine string, kMaxStrings = 12
{
    /** animated-strings 2.1: where the string is stopped. */
    enum StopKind : uint8_t { open = 0, fretted = 1, tapped = 2, slide = 3 };

    std::atomic<float>   level { 0.0f };            ///< StringEngine::getLevel()
    std::atomic<float>   stopFret { 0.0f };         ///< absolute frets from the nut (capo included; slide contact under a bar)
    std::atomic<float>   bendCents { 0.0f };        ///< finger-bend part only: no whammy, no slide (2.4)
    std::atomic<float>   pluckPosition { 0.16f };   ///< last Excitation::Params::pluckPosition, from the bridge
    std::atomic<int64_t> startSample { -1 };        ///< note start, engine sample clock (piano roll uses it too)
    std::atomic<int8_t>  midiNote { -1 };           ///< -1 = none (piano roll)
    std::atomic<uint8_t> damping { 0 };             ///< StringEngine::Damping
    std::atomic<uint8_t> harmonicPartial { 0 };     ///< 0/1 = none
    std::atomic<uint8_t> stopKind { open };         ///< StopKind
    std::atomic<float>   fret { 0.0f };             ///< the note's fret from the capo (LuthierEngine::getStringFret)
};

struct SoundingNotes
{
    static constexpr int kMaxStrings = 12;

    std::array<SoundingString, kMaxStrings> strings;
    std::atomic<uint64_t> noteSet[2] { { 0 }, { 0 } };   ///< piano roll's 128-bit set of sounding MIDI notes
    std::atomic<int64_t>  samplePosition { 0 };          ///< engine sample clock at publish
    std::atomic<double>   sampleRate { 44100.0 };
    std::atomic<uint32_t> sequence { 0 };                ///< seqlock: odd while writing
    std::atomic<int32_t>  numStrings { 6 };

    //==========================================================================
    /** A plain copy, for the message thread. */
    struct String
    {
        float level = 0.0f, stopFret = 0.0f, bendCents = 0.0f, pluckPosition = 0.16f;
        int64_t startSample = -1;
        int8_t midiNote = -1;
        uint8_t damping = 0, harmonicPartial = 0, stopKind = 0;
        float fret = 0.0f;
    };

    struct Snapshot
    {
        std::array<String, kMaxStrings> strings {};
        uint64_t noteSet[2] { 0, 0 };
        int64_t samplePosition = 0;
        double sampleRate = 44100.0;
        uint32_t sequence = 0;       ///< 0 = never published
        int numStrings = 6;

        bool isNoteSounding (int midiNote) const noexcept
        {
            return midiNote >= 0 && midiNote < 128 && ((noteSet[midiNote >> 6] >> (midiNote & 63)) & 1u) != 0;
        }
    };

    //==========================================================================
    /** Writer side (audio thread): open and close a publication. Never waits. */
    void beginWrite() noexcept
    {
        sequence.fetch_add (1, std::memory_order_relaxed);   // odd
        std::atomic_thread_fence (std::memory_order_release);
    }

    void endWrite() noexcept
    {
        sequence.fetch_add (1, std::memory_order_release);   // even
    }

    /** Reader side (message thread): a seqlock copy. Returns false, leaving
        `out` as it was, if three attempts all overlapped a write. Allocates
        nothing. */
    bool read (Snapshot& out) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto before = sequence.load (std::memory_order_acquire);

            if ((before & 1u) != 0)
                continue;

            Snapshot copy;

            for (int s = 0; s < kMaxStrings; ++s)
            {
                const auto& src = strings[(std::size_t) s];
                auto& dst = copy.strings[(std::size_t) s];
                dst.level           = src.level.load (std::memory_order_relaxed);
                dst.stopFret        = src.stopFret.load (std::memory_order_relaxed);
                dst.bendCents       = src.bendCents.load (std::memory_order_relaxed);
                dst.pluckPosition   = src.pluckPosition.load (std::memory_order_relaxed);
                dst.startSample     = src.startSample.load (std::memory_order_relaxed);
                dst.midiNote        = src.midiNote.load (std::memory_order_relaxed);
                dst.damping         = src.damping.load (std::memory_order_relaxed);
                dst.harmonicPartial = src.harmonicPartial.load (std::memory_order_relaxed);
                dst.stopKind        = src.stopKind.load (std::memory_order_relaxed);
                dst.fret            = src.fret.load (std::memory_order_relaxed);
            }

            copy.noteSet[0]     = noteSet[0].load (std::memory_order_relaxed);
            copy.noteSet[1]     = noteSet[1].load (std::memory_order_relaxed);
            copy.samplePosition = samplePosition.load (std::memory_order_relaxed);
            copy.sampleRate     = sampleRate.load (std::memory_order_relaxed);
            copy.numStrings     = numStrings.load (std::memory_order_relaxed);

            std::atomic_thread_fence (std::memory_order_acquire);

            if (sequence.load (std::memory_order_relaxed) == before)
            {
                copy.sequence = before;
                out = copy;
                return true;
            }
        }

        return false;
    }
};

} // namespace luthier
