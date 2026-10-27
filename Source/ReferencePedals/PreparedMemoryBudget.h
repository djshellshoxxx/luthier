// OWNER: WP2
#pragma once
#include "PreparedMemory.h"
#include <atomic>
#include <memory>
#include <utility>
namespace luthier::reference
{
// Off-audio ownership. A reservation follows the retained runtime, including
// RCU retirement/persistence pins; its counter outlives the pool if necessary.
class PreparedMemoryBudget
{
    struct State
    {
        explicit State (uint64_t maximum) : limit (maximum) {}
        const uint64_t limit;
        std::atomic<uint64_t> used { 0 };
    };
public:
    class Reservation
    {
    public:
        Reservation() = default;
        Reservation (const Reservation&) = delete;
        Reservation& operator= (const Reservation&) = delete;
        Reservation (Reservation&& other) noexcept : state (std::move (other.state)), amount (std::exchange (other.amount, 0)) {}
        Reservation& operator= (Reservation&& other) noexcept
        {
            if (this != &other) { release(); state = std::move (other.state); amount = std::exchange (other.amount, 0); }
            return *this;
        }
        ~Reservation() { release(); }
        uint64_t bytes() const noexcept { return amount; }
        bool shrink (uint64_t retained) noexcept
        {
            if (! state || retained > amount) return false;
            state->used.fetch_sub (amount - retained, std::memory_order_relaxed); amount = retained; return true;
        }
        void release() noexcept
        {
            if (state) { state->used.fetch_sub (amount, std::memory_order_relaxed); state.reset(); amount = 0; }
        }
    private:
        friend class PreparedMemoryBudget;
        std::shared_ptr<State> state;
        uint64_t amount = 0;
    };
    explicit PreparedMemoryBudget (uint64_t maximum = kPreparedPedalMemoryLimit) : state (std::make_shared<State> (maximum)) {}
    bool reserve (uint64_t bytes, Reservation& result) noexcept
    {
        if (result.state || bytes == kUnknownPreparedMemoryBytes) return false;
        auto current = state->used.load (std::memory_order_relaxed);
        do
        {
            if (current > state->limit || bytes > state->limit - current) return false;
        }
        while (! state->used.compare_exchange_weak (current, current + bytes, std::memory_order_relaxed));
        result.state = state; result.amount = bytes; return true;
    }
    uint64_t used() const noexcept { return state->used.load (std::memory_order_relaxed); }
    uint64_t limit() const noexcept { return state->limit; }
private:
    std::shared_ptr<State> state;
};
}
