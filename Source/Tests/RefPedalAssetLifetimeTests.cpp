#include "TestFramework.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <thread>
#include <stdexcept>

using namespace luthier::tests;
using namespace luthier::reference;

LUTHIER_TEST (RefPedalAssetLifetime, destructionDrainsQueuedIoWithoutUsingOuterOwner)
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("asset-lifetime-" + juce::Uuid().toString());
    const auto blob = std::make_shared<const std::vector<uint8_t>> (std::initializer_list<uint8_t>{1, 2, 3, 4, 5});
    const auto hash = AssetStore::hashBlob (blob->data(), blob->size());
    for (int repetition = 0; repetition < 8; ++repetition)
    {
        auto store = std::make_unique<AssetStore> (directory);
        juce::WaitableEvent entered, resume;
        std::atomic<int> successful {0};
        store->putBlobAsync (hash, blob, [&] (bool ok)
        {
            if (ok) ++successful;
            entered.signal();
            resume.wait (5000);
        });
        CHECK (entered.wait (3000));
        for (int i = 0; i < 32; ++i)
        {
            store->putBlobAsync (hash, blob, [&] (bool ok) { if (ok) ++successful; });
            store->getBlobAsync (hash, [&] (AssetBlob result) { if (result && *result == *blob) ++successful; });
        }
        // Keep work queued while destruction starts. libc++'s unique_ptr clears
        // its member before deleting Impl; worker jobs must not read that member.
        std::thread release ([&] { juce::Thread::sleep (20); resume.signal(); });
        store.reset();
        release.join();
        CHECK (successful.load() == 65);
    }
    CHECK (directory.deleteRecursively());
}

LUTHIER_TEST (RefPedalAssetLifetime, throwingCallbackDoesNotLoseFollowingWork)
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("asset-throw-" + juce::Uuid().toString());
    const auto blob = std::make_shared<const std::vector<uint8_t>> (std::initializer_list<uint8_t>{7, 8});
    const auto hash = AssetStore::hashBlob (blob->data(), blob->size());
    std::atomic<int> completed {0};
    {
        AssetStore store (directory);
        store.putBlobAsync (hash, blob, [] (bool) { throw std::runtime_error ("test callback"); });
        store.getBlobAsync (hash, [&] (AssetBlob result) { if (result && *result == *blob) ++completed; });
    }
    CHECK (completed.load() == 1);
    CHECK (directory.deleteRecursively());
}
