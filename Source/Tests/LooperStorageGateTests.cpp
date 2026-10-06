#include "TestFramework.h"
#include "../Practice/StorageAccessGate.h"

#include <atomic>
#include <thread>

using namespace luthier::tests;

namespace
{
    bool waitFor (const std::atomic<bool>& value, bool expected = true)
    {
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000.0;

        while (value.load (std::memory_order_acquire) != expected
               && juce::Time::getMillisecondCounterHiRes() < deadline)
            std::this_thread::yield();

        return value.load (std::memory_order_acquire) == expected;
    }
}

LUTHIER_TEST (PracticeLooperConcurrency, exclusiveStorageWaitsForCallbacksAndClosesAdmission)
{
    luthier::detail::StorageAccessGate gate;
    std::atomic<bool> workerStarted { false };
    std::atomic<bool> exclusiveAcquired { false };
    std::atomic<bool> releaseExclusive { false };
    std::thread storageThread;

    {
        luthier::detail::StorageAccessGate::CallbackAccess existingCallback (gate);
        CHECK_MSG ((bool) existingCallback, "callback should enter before exclusive storage access");

        storageThread = std::thread ([&]
        {
            workerStarted.store (true, std::memory_order_release);
            luthier::detail::StorageAccessGate::ExclusiveAccess exclusive (gate);
            exclusiveAcquired.store (true, std::memory_order_release);

            while (! releaseExclusive.load (std::memory_order_acquire))
                std::this_thread::yield();
        });

        CHECK_MSG (waitFor (workerStarted), "storage thread did not start");

        bool admissionClosed = false;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000.0;

        while (! admissionClosed && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
            luthier::detail::StorageAccessGate::CallbackAccess probe (gate);
            admissionClosed = ! (bool) probe;
            std::this_thread::yield();
        }

        CHECK_MSG (admissionClosed, "exclusive operation did not close callback admission");
        CHECK_MSG (! exclusiveAcquired.load (std::memory_order_acquire),
                   "exclusive operation must wait for callbacks already inside");
    }

    CHECK_MSG (waitFor (exclusiveAcquired), "storage operation did not proceed after callback exit");

    luthier::detail::StorageAccessGate::CallbackAccess rejectedCallback (gate);
    CHECK_MSG (! (bool) rejectedCallback, "callbacks must stay out while storage owns the buffers");

    releaseExclusive.store (true, std::memory_order_release);
    storageThread.join();

    luthier::detail::StorageAccessGate::CallbackAccess callbackAfterRelease (gate);
    CHECK_MSG ((bool) callbackAfterRelease, "callback admission should resume after storage access releases");
}
