#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStorage.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <condition_variable>
#include <mutex>
#include <thread>

// The dedicated harness injects this pause into a copy of the production
// translation unit. Shipping/native sources have no scheduling hook.
#if LUTHIER_MANIFEST_PUBLICATION_PROBE
namespace
{
    std::mutex publicationMutex;
    std::condition_variable publicationCondition;
    bool pausePublication = false, publicationEntered = false, releasePublication = false;
}
void luthierManifestPublicationProbe()
{
    std::unique_lock<std::mutex> lock (publicationMutex);
    if (! pausePublication) return;
    publicationEntered = true;
    publicationCondition.notify_all();
    publicationCondition.wait (lock, [] { return releasePublication; });
}
using namespace luthier::reference;
using luthier::reference::common::LooperStorage;
LUTHIER_TEST (RefPedalLooperManifest, readyFlagDoesNotFinishBeforeTheHashSnapshot)
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("looper-publication-" + juce::Uuid().toString());
    {
        AssetStore store (directory);
        LooperStorage storage;
        LooperStorage::Config config;
        config.pageFrames = 16; config.maxFrames = 16; config.writePages = 4;
        config.readPages = 2; config.readAhead = 1;
        CHECK (storage.prepare (store, config));
        {
            const std::lock_guard<std::mutex> lock (publicationMutex);
            pausePublication = true; publicationEntered = releasePublication = false;
        }
        storage.beginRecord();
        for (int i = 0; i < 16; ++i) CHECK (storage.append (0.25, -0.5));
        CHECK (storage.finishRecord());
        bool reached = false;
        {
            std::unique_lock<std::mutex> lock (publicationMutex);
            reached = publicationCondition.wait_for (lock, std::chrono::seconds (3), [] { return publicationEntered; });
        }
        CHECK (reached);
        AssetHash result; result.bytes[0] = 42;
        std::atomic<bool> started {false}, finished {false}, success {false};
        std::thread reader ([&]
        {
            started.store (true);
            success.store (storage.manifest (result, 1000));
            finished.store (true);
        });
        while (! started.load()) std::this_thread::yield();
        juce::Thread::sleep (50);
        const bool returnedBeforeSnapshot = finished.load();
        {
            const std::lock_guard<std::mutex> lock (publicationMutex);
            releasePublication = true; pausePublication = false;
        }
        publicationCondition.notify_all();
        reader.join();
        CHECK (! returnedBeforeSnapshot);
        CHECK (success.load());
        CHECK (store.hasBlob (result));
        CHECK ((storage.errors() & LooperStorage::ioFailure) == 0);
    }
    CHECK (directory.deleteRecursively());
}
#endif
