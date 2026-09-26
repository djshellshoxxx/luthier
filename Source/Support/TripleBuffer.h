#pragma once

#include <atomic>

/**
    SPEC-SWEEP (RE-2): a lock-free, wait-free triple buffer for handing a value
    that is too big (or owns heap memory) to live in an atomic from one writer
    thread to one reader thread.

    The writer fills its private slot and publishes it; the reader, at a point of
    its choosing (the top of an audio block), picks up the newest published slot
    and then reads it by const reference for as long as it likes. Neither side
    ever waits, the reader never copies, and - because only the writer assigns
    into slots - a value that owns memory (a juce::StringArray, say) is only
    ever freed on the writer's thread.

    Writer calls must be serialised by the caller (one writer thread, or a lock
    held by every writer). Reader calls must all come from one thread.
*/
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer() = default;

    /** Sets every slot. Only safe while no reader or writer is active
        (construction, prepare). */
    void resetAll (const T& value)
    {
        for (auto& s : slots)
            s = value;
    }

    //==========================================================================
    // Writer side

    /** Copies @p value into the writer's slot and publishes it. */
    void write (const T& value)
    {
        slots[writeIndex] = value;
        publish();
    }

    /** The writer's private slot, to fill in place before publish(). */
    T& writerSlot() noexcept { return slots[writeIndex]; }

    void publish() noexcept
    {
        writeIndex = middle.exchange (writeIndex | kDirty, std::memory_order_acq_rel) & kIndexMask;
    }

    //==========================================================================
    // Reader side

    /** Picks up the newest published value if there is one, and returns the
        reader's slot. Real-time safe: two atomic operations at most. */
    const T& acquire() noexcept
    {
        if ((middle.load (std::memory_order_relaxed) & kDirty) != 0)
            readIndex = middle.exchange (readIndex, std::memory_order_acq_rel) & kIndexMask;

        return slots[readIndex];
    }

    /** The reader's slot as of its last acquire(). Reader thread only. */
    const T& current() const noexcept { return slots[readIndex]; }

private:
    static constexpr int kDirty = 4;
    static constexpr int kIndexMask = 3;

    T slots[3];
    int writeIndex = 0;
    int readIndex = 1;
    std::atomic<int> middle { 2 };
};
