#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStorage.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
using namespace luthier::reference;
using namespace luthier::reference::common;
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests {long allocationsOnThisThread() noexcept;}
#endif
namespace
{
using Direction=LooperStorage::Direction;
// Real immutable hashed assets. Gate only worker reads, never owner manifest I/O.
class DirectionStore final : public IAssetStore
{
public:
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("looper-direction-"+juce::Uuid().toString());
    AssetStore real{path};
    const std::thread::id owner=std::this_thread::get_id();
    ~DirectionStore() override {release();path.deleteRecursively();}
    bool putBlob(const AssetHash& h,const uint8_t* data,size_t size) override
    {return real.putBlob(h,data,size);}
    bool hasBlob(const AssetHash& h) const override {return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override
    {
        if(std::this_thread::get_id()!=owner)
        {
            std::unique_lock<std::mutex> lock(mutex);
            if(block){entered=true;cv.notify_all();cv.wait(lock,[&]{return !block;});}
        }
        return real.getBlobPath(h);
    }
    void putBlobAsync(const AssetHash& h,AssetBlob blob,std::function<void(bool)> done) override
    {real.putBlobAsync(h,std::move(blob),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override
    {real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override
    {return real.findMissing(h);}
    void hold(){const std::lock_guard<std::mutex> lock(mutex);block=true;entered=false;}
    void release(){const std::lock_guard<std::mutex> lock(mutex);block=false;cv.notify_all();}
    bool waitForWorker()
    {std::unique_lock<std::mutex> lock(mutex);return cv.wait_for(lock,std::chrono::seconds(3),[&]{return entered;});}
private:
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    mutable bool block=false,entered=false;
};
template<typename F> bool directionEventually(F predicate)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(predicate())return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    return false;
}
LooperStorage::Config directionConfig()
{
    LooperStorage::Config c;c.sampleRate=1000;c.maxFrames=144;c.pageFrames=16;
    c.writePages=16;c.readPages=8;c.readAhead=3;c.ramBudget=1024*1024;c.storageBudget=1024*1024;
    return c;
}
AssetHash recordDirectionAsset(DirectionStore& store,luthier::tests::TestContext& ctx,uint64_t frames)
{
    LooperStorage recorded;CHECK(recorded.prepare(store,directionConfig()));recorded.beginRecord();
    for(uint64_t frame=0;frame<frames;++frame)
        CHECK(recorded.append(double(frame+1)/256.,-double(frame+1)/512.));
    CHECK(recorded.finishRecord());AssetHash hash;CHECK(recorded.manifest(hash));return hash;
}
bool reverseWindowReady(LooperStorage& storage,uint64_t page,int ahead)
{
    const auto pages=(storage.length()+15)/16;
    for(int i=0;i<=ahead;++i)
    {
        if(!storage.available(page*16))return false;
        page=page==0?pages-1:page-1;
    }
    return true;
}
void checkDirectionSample(LooperStorage& storage,uint64_t frame,Direction direction,luthier::tests::TestContext& ctx)
{
    double l=0,r=0;uint64_t version=0;
#if LUTHIER_ALLOCATION_COUNTER
    const auto before=luthier::tests::allocationsOnThisThread();
#endif
    const bool read=storage.read(frame,l,r,&version,false,direction);
#if LUTHIER_ALLOCATION_COUNTER
    const auto allocated=luthier::tests::allocationsOnThisThread()-before;CHECK(allocated==0);
#endif
    CHECK(read);CHECK(version==1);
    CHECK_NEAR(l,double(frame+1)/256.,0);CHECK_NEAR(r,-double(frame+1)/512.,0);
}
}

LUTHIER_TEST(RefPedalLooperStorageDirection, freshReverseWrapKeepsSameVersionPagesAndExactDescendingStereo)
{
    DirectionStore store;const auto hash=recordDirectionAsset(store,ctx,144);
    LooperStorage restored;CHECK(restored.prepare(store,directionConfig()));
    store.hold();const bool loaded=restored.restore(hash);const bool blocked=loaded && store.waitForWorker();
    restored.prefetch(0,Direction::Reverse);store.release();CHECK(loaded);CHECK(blocked);
    const bool ready=directionEventually([&]{return restored.available(0) && restored.available(128)
        && restored.available(112) && restored.available(96);});
    CHECK_MSG(ready,"reverse window [0,8,7,6] must coexist after fresh restore");
    LooperStorage::Capture capture;CHECK(restored.capture(capture));
    CHECK(capture.status==LooperStorage::CaptureStatus::Ready);CHECK(capture.hash==hash);
    if(!ready)return;
    checkDirectionSample(restored,0,Direction::Reverse,ctx);
    for(uint64_t frame=144;frame-- >96;)checkDirectionSample(restored,frame,Direction::Reverse,ctx);
    CHECK(restored.errors()==0);
}

LUTHIER_TEST(RefPedalLooperStorageDirection, partialTailAndFullWindowEvictionStayExactAcrossReverseWraps)
{
    for(uint64_t frames:{1ull,17ull,129ull,133ull,144ull})
    {
        DirectionStore store;const auto hash=recordDirectionAsset(store,ctx,frames);
        auto config=directionConfig();config.readAhead=7;
        LooperStorage restored;CHECK(restored.prepare(store,config));
        store.hold();const bool loaded=restored.restore(hash);const bool blocked=loaded && store.waitForWorker();
        restored.prefetch(0,Direction::Reverse);store.release();CHECK(loaded);CHECK(blocked);
        const auto pages=(frames+15)/16;uint64_t page=0;
        for(uint64_t step=0;step<pages*3;++step)
        {
            const auto start=page*16,end=std::min(frames,start+16)-1;
#if LUTHIER_ALLOCATION_COUNTER
            const auto before=luthier::tests::allocationsOnThisThread();
#endif
            restored.prefetch(end,Direction::Reverse);
#if LUTHIER_ALLOCATION_COUNTER
            const auto allocated=luthier::tests::allocationsOnThisThread()-before;CHECK(allocated==0);
#endif
            const bool ready=directionEventually([&]{return reverseWindowReady(restored,page,7);});
            CHECK_MSG(ready,"entire reverse window must survive eviction, including a partial final page");
            if(!ready)return;
            for(uint64_t frame=end+1;frame-- >start;)checkDirectionSample(restored,frame,Direction::Reverse,ctx);
            page=page==0?pages-1:page-1;
        }
        CHECK(restored.errors()==0);
    }
}

LUTHIER_TEST(RefPedalLooperStorageDirection, reverseReadPrimesAtActualPartialPageEnd)
{
    DirectionStore store;const auto hash=recordDirectionAsset(store,ctx,133);
    auto config=directionConfig();config.readPages=2;config.readAhead=1;
    LooperStorage restored;CHECK(restored.prepare(store,config));
    store.hold();const bool loaded=restored.restore(hash);const bool blocked=loaded && store.waitForWorker();
    restored.prefetch(0,Direction::Reverse);store.release();CHECK(loaded);CHECK(blocked);
    const bool ready=directionEventually([&]{return restored.available(0) && restored.available(128);});
    CHECK(ready);if(!ready)return;
    store.hold();checkDirectionSample(restored,132,Direction::Reverse,ctx);
    store.release();
    // No explicit prefetch here: reading the five-frame tail's end must queue page7.
    const bool previous=directionEventually([&]{return restored.available(112);});
    CHECK_MSG(previous,"reverse read at partial-page end132 must prefetch preceding page7");
    if(previous)checkDirectionSample(restored,127,Direction::Reverse,ctx);
    CHECK(restored.errors()==0);
}

LUTHIER_TEST(RefPedalLooperStorageDirection, reverseSparseReadPrimesFromImplicitZeroPartialPage)
{
    DirectionStore store;auto config=directionConfig();config.readPages=2;config.readAhead=1;
    AssetHash hash;
    {
        LooperStorage recorded;CHECK(recorded.prepare(store,config));CHECK(recorded.beginLayer(133));
        CHECK(recorded.add(112,.25,-.125));CHECK(recorded.finishRecord());CHECK(recorded.manifest(hash));
    }
    LooperStorage restored;CHECK(restored.prepare(store,config));CHECK(restored.restore(hash));
    store.hold();double l=99,r=99;uint64_t version=99;
    const bool read=restored.read(132,l,r,&version,false,Direction::Reverse);store.release();
    CHECK(read);CHECK(l==0);CHECK(r==0);CHECK(version==0);
    const bool previous=directionEventually([&]{return restored.available(112);});
    CHECK_MSG(previous,"implicit-zero reverse entry must prefetch preceding nonzero page7");
    if(previous)
    {
        CHECK(restored.read(112,l,r,&version,false,Direction::Reverse));
        CHECK_NEAR(l,.25,0);CHECK_NEAR(r,-.125,0);CHECK(version==1);
    }
    CHECK(restored.errors()==0);
}

LUTHIER_TEST(RefPedalLooperStorageDirection, queuedOppositeDirectionsCannotPublishObsoleteSlots)
{
    for(auto finalDirection:{Direction::Forward,Direction::Reverse})
    {
        DirectionStore store;const auto hash=recordDirectionAsset(store,ctx,133);
        auto config=directionConfig();config.readPages=4;config.readAhead=3;
        LooperStorage restored;CHECK(restored.prepare(store,config));
        store.hold();const bool loaded=restored.restore(hash);const bool blocked=loaded && store.waitForWorker();
        restored.prefetch(0,Direction::Reverse);
        restored.prefetch(64,Direction::Forward);
        restored.prefetch(32,Direction::Reverse);
        if(finalDirection==Direction::Forward)restored.prefetch(64,Direction::Forward);
        store.release();CHECK(loaded);CHECK(blocked);
        const std::array<uint64_t,4> reverse{32,16,0,128},forward{64,80,96,112};
        const auto& window=finalDirection==Direction::Reverse?reverse:forward;
        const bool ready=directionEventually([&]
        {for(auto frame:window)if(!restored.available(frame))return false;return true;});
        CHECK_MSG(ready,"latest requested window must survive older queued jobs in either direction");
        if(!ready)return;
        for(auto frame:window)checkDirectionSample(restored,frame,finalDirection,ctx);
        LooperStorage::Capture capture;CHECK(restored.capture(capture));
        CHECK(capture.status==LooperStorage::CaptureStatus::Ready);CHECK(capture.hash==hash);
        CHECK(capture.faults==0);CHECK(restored.errors()==0);
    }
}

LUTHIER_TEST(RefPedalLooperStorageDirection, reverseMissReportsUnderrunAndRecoversWithoutRelabellingAssets)
{
    DirectionStore store;const auto hash=recordDirectionAsset(store,ctx,133);
    LooperStorage restored;CHECK(restored.prepare(store,directionConfig()));
    store.hold();const bool loaded=restored.restore(hash);const bool blocked=loaded && store.waitForWorker();
    double l=99,r=99;uint64_t version=99;
    const bool read=restored.read(132,l,r,&version,false,Direction::Reverse);
    LooperStorage::Capture blockedCapture;const bool captured=restored.capture(blockedCapture);
    store.release();CHECK(loaded);CHECK(blocked);CHECK(!read);CHECK(l==0);CHECK(r==0);CHECK(version==0);
    CHECK(captured);CHECK(blockedCapture.status==LooperStorage::CaptureStatus::Ready);
    CHECK(blockedCapture.hash==hash);CHECK(blockedCapture.faults==LooperStorage::readUnderrun);
    const bool ready=directionEventually([&]{return restored.available(128) && restored.available(112)
        && restored.available(96) && restored.available(80);});
    CHECK_MSG(ready,"reverse miss must request [8,7,6,5] without another prefetch call");
    if(!ready)return;
    checkDirectionSample(restored,132,Direction::Reverse,ctx);
    CHECK(restored.errors()==LooperStorage::readUnderrun);
    LooperStorage::Capture recovered;CHECK(restored.capture(recovered));
    CHECK(recovered.status==LooperStorage::CaptureStatus::Ready);CHECK(recovered.hash==hash);
    CHECK(recovered.faults==LooperStorage::readUnderrun);
}
