#pragma once

/*  The capture ring (notation-export.md 6.2).

    One writer - the audio thread - and one reader - a message-thread drain at
    10 Hz. A fixed number of fixed-size records, allocated once. The writer
    never waits and never allocates; when the reader falls behind, the writer
    simply laps it, and the reader finds out and counts what it lost. That is
    6.2's "ring overflow drops the oldest": the newest records are always the
    ones still there, which is what the live TAB view needs.

    Each slot carries a sequence stamp, written odd before the record and even
    after it (a seqlock). The reader accepts a record only if the stamp says it
    is the one it expected and did not change while it was read, so a record
    the writer overwrote mid-read is counted as dropped rather than returned
    torn. The record itself is stored as relaxed atomic words, which keeps the
    whole exchange free of data races in the C++ sense and not only in
    practice.

    The session recorder (practice-tools.md 8) has the same shape - a fixed ring
    the audio thread writes and the message thread freezes - but holds audio in
    an AudioBuffer rather than records, so the primitive here is its own.
*/

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

namespace luthier
{

//==============================================================================
/** One thing the audio thread saw. 48 bytes, trivially copyable. */
struct CaptureRecord
{
    enum class Kind : juce::uint8
    {
        noteOn = 0,
        noteOff,
        bend,
        mark,            ///< a notation technique on the note sounding on a string
        chord,
        meter,           ///< tempo and time signature
        bassTechnique,
        slideBar,
        autoRules,       ///< auto-articulation.md 9 (FEAT-ASSIST): the sounding note's Assist bits
        noise            ///< SPEC-SWEEP MX-1: a playing-noise trigger (code = the kind below)
    };

    Kind kind = Kind::noteOn;
    bool musical = false;          ///< the host transport was running
    juce::int8 stringIndex = -1;
    juce::uint8 midiNote = 0;
    juce::uint8 code = 0;          ///< Technique, ScoreTechnique::Type, or a meter's numerator
    juce::uint8 code2 = 0;         ///< a harmonic partial, or a meter's denominator
    juce::uint8 flags = 0;         ///< note-off: 1 = let ring
    juce::uint8 reserved = 0;

    juce::int64 sample = 0;        ///< absolute, on the audio thread's clock
    double ppq = 0.0;              ///< quarter notes on the host's clock, when musical

    float value = 0.0f;            ///< velocity, cents, a mark's value, bpm, a bar's position
    float fret = 0.0f;             ///< a note's fret; a bass technique's pluck position

    char text[16] = {};            ///< a chord name, a bass technique, a bar's pressure

    void setText (const char* source) noexcept
    {
        size_t i = 0;

        if (source != nullptr)
            for (; i + 1 < sizeof (text) && source[i] != 0; ++i)
                text[i] = source[i];

        for (; i < sizeof (text); ++i)
            text[i] = 0;
    }
};

static_assert (std::is_trivially_copyable<CaptureRecord>::value, "records are copied as words");
static_assert (sizeof (CaptureRecord) % sizeof (juce::uint64) == 0, "records are copied as words");

//==============================================================================
class CaptureRing
{
public:
    /** 6.2: 8192 records, about twelve minutes of dense playing. */
    static constexpr int kCapacity = 8192;

    CaptureRing()
        : slots (new Slot[(size_t) kCapacity])
    {
        reset();
    }

    /** Empties the ring. Not while the writer is running. */
    void reset() noexcept
    {
        for (int i = 0; i < kCapacity; ++i)
            slots[(size_t) i].stamp.store (0, std::memory_order_relaxed);

        writeSeq.store (0, std::memory_order_release);
        readSeq = 0;
        dropped.store (0, std::memory_order_relaxed);
    }

    //==========================================================================
    /** Audio thread. Never waits, never allocates, never fails: when the ring
        is full the oldest record goes. */
    void push (const CaptureRecord& record) noexcept
    {
        const auto seq = writeSeq.load (std::memory_order_relaxed);
        auto& slot = slots[(size_t) (seq & (juce::uint64) (kCapacity - 1))];

        juce::uint64 words[kWords];
        std::memcpy (words, &record, sizeof (record));

        slot.stamp.store (2 * seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);

        for (int i = 0; i < kWords; ++i)
            slot.words[(size_t) i].store (words[i], std::memory_order_relaxed);

        slot.stamp.store (2 * seq + 2, std::memory_order_release);
        writeSeq.store (seq + 1, std::memory_order_release);
    }

    //==========================================================================
    /** Reader. Appends every record written since the last call, oldest first,
        and counts the ones the writer overwrote before they could be read. */
    int drain (std::vector<CaptureRecord>& destination)
    {
        const auto written = writeSeq.load (std::memory_order_acquire);
        juce::int64 lost = 0;

        if (written - readSeq > (juce::uint64) kCapacity)
        {
            lost += (juce::int64) (written - (juce::uint64) kCapacity - readSeq);
            readSeq = written - (juce::uint64) kCapacity;
        }

        int count = 0;

        for (auto seq = readSeq; seq < written; ++seq)
        {
            const auto& slot = slots[(size_t) (seq & (juce::uint64) (kCapacity - 1))];
            const auto expected = 2 * seq + 2;

            if (slot.stamp.load (std::memory_order_acquire) != expected)
            {
                ++lost;       // lapped while this drain was running
                continue;
            }

            juce::uint64 words[kWords];

            for (int i = 0; i < kWords; ++i)
                words[i] = slot.words[(size_t) i].load (std::memory_order_relaxed);

            std::atomic_thread_fence (std::memory_order_acquire);

            if (slot.stamp.load (std::memory_order_relaxed) != expected)
            {
                ++lost;
                continue;
            }

            CaptureRecord record;
            std::memcpy (&record, words, sizeof (record));
            destination.push_back (record);
            ++count;
        }

        readSeq = written;

        if (lost > 0)
            dropped.fetch_add (lost, std::memory_order_relaxed);

        return count;
    }

    /** Reader. Forgets everything written before `sequence` without counting
        it as dropped: an armed capture starting clean (6.3). */
    void skipTo (juce::uint64 sequence) noexcept
    {
        if (sequence > readSeq)
            readSeq = juce::jmin (sequence, writeSeq.load (std::memory_order_acquire));
    }

    /** Records written since the last reset, whether or not they survived. */
    juce::uint64 getWriteCount() const noexcept { return writeSeq.load (std::memory_order_acquire); }

    /** 6.2: the counter the UI shows. */
    juce::int64 getDroppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    static constexpr int kWords = (int) (sizeof (CaptureRecord) / sizeof (juce::uint64));

    struct Slot
    {
        std::atomic<juce::uint64> stamp { 0 };
        std::array<std::atomic<juce::uint64>, (size_t) kWords> words;
    };

    std::unique_ptr<Slot[]> slots;
    std::atomic<juce::uint64> writeSeq { 0 };
    juce::uint64 readSeq = 0;
    std::atomic<juce::int64> dropped { 0 };

    JUCE_DECLARE_NON_COPYABLE (CaptureRing)
};

} // namespace luthier
