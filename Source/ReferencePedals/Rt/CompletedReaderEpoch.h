// OWNER: WP2
#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>

namespace luthier::reference
{
// One non-nested reader; one control-side snapshot consumer. Reset requires
// both to be stopped. Every protected live acquisition must be SC after begin.
template <typename T, std::size_t N>
class CompletedReaderEpoch
{
public:
    using Pointers = std::array<const T*, N>;
    struct Snapshot { uint64_t epoch = 0; Pointers pointers {}; };
    static constexpr uint64_t exhaustedEpoch = std::numeric_limits<uint64_t>::max();
    explicit CompletedReaderEpoch (uint64_t initialEvenEpoch = 0) noexcept { reset (initialEvenEpoch); }
    void reset (uint64_t initialEvenEpoch = 0) noexcept
    {
        assert ((initialEvenEpoch & 1u) == 0);
        for (auto& pointer : retained) pointer.store (nullptr, std::memory_order_relaxed);
        reader.store (initialEvenEpoch, std::memory_order_seq_cst);
        completed.store (initialEvenEpoch, std::memory_order_seq_cst);
    }
    void begin() noexcept
    {
        const auto current = reader.load (std::memory_order_relaxed);
        if (current != exhaustedEpoch) reader.store (current + 1, std::memory_order_seq_cst);
    }
    void finish (const Pointers& pointers) noexcept
    {
        const auto current = reader.load (std::memory_order_relaxed);
        if (current == exhaustedEpoch) return; // never wrap/ABA; reprepare is required
        assert ((current & 1u) != 0);
        stageSnapshot (pointers, current);
        commitSnapshot (current);
    }
    // Control thread, strictly AFTER the SC store removing an owner from live.
    uint64_t retirementEpoch() const noexcept
    {
        const auto current = reader.load (std::memory_order_seq_cst);
        return current == exhaustedEpoch ? current : current + (current & 1u);
    }
    bool exhausted() const noexcept { return reader.load (std::memory_order_seq_cst) == exhaustedEpoch; }
    bool snapshot (Snapshot& result) const noexcept
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto before = completed.load (std::memory_order_seq_cst);
            if (before & 1u) continue;
            Snapshot candidate; candidate.epoch = before;
            for (std::size_t i = 0; i < N; ++i) candidate.pointers[i] = retained[i].load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            if (before == completed.load (std::memory_order_seq_cst)) { result = candidate; return true; }
        }
        return false;
    }
private:
    // Stateless friend seam exercises the actual two publication phases in tests;
    // production finish calls both directly without any callback or test branch.
    friend struct CompletedReaderEpochTestAccess;
    void stageSnapshot (const Pointers& pointers, uint64_t current) noexcept
    {
        completed.store (current, std::memory_order_seq_cst);
        // A reader observing any later payload store must also observe its odd
        // marker at validation (paired with snapshot's acquire fence).
        std::atomic_thread_fence (std::memory_order_release);
        for (std::size_t i = 0; i < N; ++i) retained[i].store (pointers[i], std::memory_order_relaxed);
        reader.store (current + 1, std::memory_order_seq_cst);
    }
    void commitSnapshot (uint64_t current) noexcept
    {
        completed.store (current + 1, std::memory_order_seq_cst);
    }
    std::atomic<uint64_t> reader { 0 }, completed { 0 };
    std::array<std::atomic<const T*>, N> retained {};
};
}
