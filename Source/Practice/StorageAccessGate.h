#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace luthier::detail
{

/** Coordinates lock-free audio callbacks with exclusive message-thread access
    to looper storage. Callback admission and the exclusive bit share one atomic
    word, so a callback cannot slip between a pause request and the drain check. */
class StorageAccessGate
{
public:
    class CallbackAccess
    {
    public:
        explicit CallbackAccess (const StorageAccessGate& ownerIn) noexcept : owner (&ownerIn)
        {
            entered = owner->tryEnterCallback();
        }

        ~CallbackAccess()
        {
            if (entered)
                owner->leaveCallback();
        }

        CallbackAccess (const CallbackAccess&) = delete;
        CallbackAccess& operator= (const CallbackAccess&) = delete;
        explicit operator bool() const noexcept { return entered; }

    private:
        const StorageAccessGate* owner = nullptr;
        bool entered = false;
    };

    class ExclusiveAccess
    {
    public:
        explicit ExclusiveAccess (const StorageAccessGate& ownerIn) noexcept
            : owner (&ownerIn), serialiser (owner->exclusiveMutex)
        {
            owner->beginExclusive();
        }

        ~ExclusiveAccess()
        {
            if (owner != nullptr)
                owner->endExclusive();
        }

        ExclusiveAccess (const ExclusiveAccess&) = delete;
        ExclusiveAccess& operator= (const ExclusiveAccess&) = delete;

        void release() noexcept
        {
            if (owner == nullptr)
                return;

            owner->endExclusive();
            owner = nullptr;
            serialiser.unlock();
        }

    private:
        const StorageAccessGate* owner = nullptr;
        std::unique_lock<std::mutex> serialiser;
    };

private:
    static constexpr std::uint32_t exclusiveBit = std::uint32_t { 1 } << 31;
    static constexpr std::uint32_t callbackMask = ~exclusiveBit;

    bool tryEnterCallback() const noexcept
    {
        auto observed = state.load (std::memory_order_acquire);

        while ((observed & exclusiveBit) == 0)
        {
            if ((observed & callbackMask) == callbackMask)
                return false;

            if (state.compare_exchange_weak (observed, observed + 1,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
                return true;
        }

        return false;
    }

    void leaveCallback() const noexcept
    {
        state.fetch_sub (1, std::memory_order_release);
    }

    void beginExclusive() const noexcept
    {
        state.fetch_or (exclusiveBit, std::memory_order_acq_rel);

        while ((state.load (std::memory_order_acquire) & callbackMask) != 0)
            std::this_thread::yield();
    }

    void endExclusive() const noexcept
    {
        state.store (0, std::memory_order_release);
    }

    mutable std::atomic<std::uint32_t> state { 0 };
    mutable std::mutex exclusiveMutex;
};

} // namespace luthier::detail
