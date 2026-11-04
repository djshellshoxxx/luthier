// OWNER: WP1 (frozen interface I-8; changes only via errata request to the WP1 owner)
#pragma once

// Wait-free single-producer / single-consumer ring (errata E15). Fixed
// capacity, power of two, no allocation after construction, no locks, every
// operation finishes in a bounded number of steps (no CAS loops), so both
// sides are safe on the audio thread. Replaces Support/CommandQueue.h for
// audio -> UI traffic (that one takes a SpinLock).
//
// Overflow policy (errata E25): a full ring DROPS THE NEW ITEM and counts it.
// "Drop oldest" is not possible without the consumer's cooperation in a
// wait-free SPSC ring; callers that must never lose an item (MIDI clock ticks)
// use a separate ring so a flood of other events cannot starve them.
//
// Exactly one thread may call push(); exactly one (other) thread may call
// pop()/popMany()/clear(). dropped() and sizeApprox() may be read anywhere.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace luthier::reference
{

template <typename T, std::size_t Capacity>
class SpscRing
{
public:
    static_assert (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert (std::is_nothrow_copy_assignable_v<T> && std::is_default_constructible_v<T>,
                   "SpscRing<T>: T must be default constructible and nothrow copy assignable");

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    // Producer thread only. False (and dropped() + 1) when full.
    bool push (const T& item) noexcept
    {
        const std::size_t t = tail.load (std::memory_order_relaxed);
        if (t - cachedHead >= Capacity)
        {
            cachedHead = head.load (std::memory_order_acquire);
            if (t - cachedHead >= Capacity)
            {
                droppedCount.store (droppedCount.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                return false;
            }
        }
        slots[t & (Capacity - 1)] = item;
        tail.store (t + 1, std::memory_order_release);
        return true;
    }

    // Consumer thread only.
    bool pop (T& out) noexcept
    {
        const std::size_t h = head.load (std::memory_order_relaxed);
        if (h == cachedTail)
        {
            cachedTail = tail.load (std::memory_order_acquire);
            if (h == cachedTail)
                return false;
        }
        out = slots[h & (Capacity - 1)];
        head.store (h + 1, std::memory_order_release);
        return true;
    }

    // Consumer thread only; copies up to `cap` items, returns how many.
    std::size_t popMany (T* out, std::size_t cap) noexcept
    {
        std::size_t n = 0;
        while (n < cap && pop (out[n]))
            ++n;
        return n;
    }

    // Consumer thread only: discards everything currently queued.
    void clear() noexcept
    {
        T scratch;
        while (pop (scratch)) {}
    }

    std::size_t sizeApprox() const noexcept
    {
        return tail.load (std::memory_order_acquire) - head.load (std::memory_order_acquire);
    }

    // Items refused because the ring was full (producer-written, monotonic).
    std::uint32_t dropped() const noexcept { return droppedCount.load (std::memory_order_relaxed); }

private:
    static constexpr std::size_t cacheLine = 64;

    alignas (cacheLine) std::atomic<std::size_t> head { 0 };   // consumer-written
    std::size_t cachedTail = 0;                                // consumer-private
    alignas (cacheLine) std::atomic<std::size_t> tail { 0 };   // producer-written
    std::size_t cachedHead = 0;                                // producer-private
    alignas (cacheLine) std::atomic<std::uint32_t> droppedCount { 0 };
    std::array<T, Capacity> slots {};
};

} // namespace luthier::reference
