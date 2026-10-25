// OWNER: WP1 (frozen interface I-8; changes only via errata request to the WP1 owner)
#pragma once

// Deferred destruction for objects the audio thread may still be reading
// (retired Topologies, retired DSP objects), BOARD_CORE 4.1. MESSAGE THREAD
// ONLY: never touch it from the audio thread, never take a lock shared with it.
//
// Rule: an item is tagged with `lastUsableVersion`, the highest topology
// version that can still reference it. The audio thread publishes the OLDEST
// version it may still read (the current topology, or the outgoing one during
// a structural transition). reclaim(oldestInUse) frees every item whose
// lastUsableVersion is older. Versions compare with wrap-around arithmetic.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace luthier::reference
{

template <typename T>
class RetireList
{
public:
    // Message thread: secure the vector slot before publishing a replacement.
    // Failure leaves every existing owner and borrowed audio pointer intact.
    void reserveOne()
    {
        if (entries.size() < entries.capacity()) return;
        const auto maximum = entries.max_size();
        if (entries.size() == maximum) throw std::length_error ("Retirement storage exhausted");
        const auto grown = entries.capacity() <= maximum / 2 ? entries.capacity() * 2 : maximum;
        entries.reserve (std::max (entries.size() + 1, grown));
    }

    // Rvalue reference keeps the caller's unique_ptr intact if reserve fails.
    // With reserveOne completed, the following ownership transfer cannot allocate.
    void retire (std::unique_ptr<T>&& item, std::uint32_t lastUsableVersion)
    {
        if (item == nullptr)
            return;
        reserveOne();
        entries.push_back ({ std::move (item), lastUsableVersion });
        peak = entries.size() > peak ? entries.size() : peak;
    }

    // Frees items no topology at or after `oldestInUse` can reference.
    // Returns the number freed.
    int reclaim (std::uint32_t oldestInUse)
    {
        int freed = 0;
        for (std::size_t i = 0; i < entries.size();)
        {
            if (older (entries[i].version, oldestInUse))
            {
                entries.erase (entries.begin() + (std::ptrdiff_t) i);
                ++freed;
            }
            else
            {
                ++i;
            }
        }
        return freed;
    }

    // Barrier for "audio is stopped" (releaseResources / prepareToPlay).
    int reclaimAll()
    {
        const int n = (int) entries.size();
        entries.clear();
        return n;
    }

    std::size_t size() const noexcept { return entries.size(); }
    std::size_t highWater() const noexcept { return peak; }   // T-RETIRE: stays <= 8

private:
    struct Entry
    {
        std::unique_ptr<T> object;
        std::uint32_t version;
    };
    static bool older (std::uint32_t a, std::uint32_t b) noexcept { return (std::int32_t) (a - b) < 0; }

    std::vector<Entry> entries;
    std::size_t peak = 0;
};

} // namespace luthier::reference
