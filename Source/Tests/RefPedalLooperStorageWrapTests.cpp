#include "TestFramework.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
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
// Keep real hashed assets and file I/O. Only worker page reads are held until
// the owner has queued the entire wrapping window; manifest reads may proceed.
class GatedWrapStore final : public IAssetStore
{
public:
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("looper-wrap-"+juce::Uuid().toString());
    AssetStore real{path};
    const std::thread::id owner=std::this_thread::get_id();
    ~GatedWrapStore() override {release();path.deleteRecursively();}
    bool putBlob(const AssetHash& hash,const uint8_t* data,size_t size) override
    {return real.putBlob(hash,data,size);}
    bool hasBlob(const AssetHash& hash) const override {return real.hasBlob(hash);}
    juce::File getBlobPath(const AssetHash& hash) const override
    {
        if(std::this_thread::get_id()!=owner)
        {
            std::unique_lock<std::mutex> lock(mutex);
            if(block){entered=true;cv.notify_all();cv.wait(lock,[&]{return !block;});}
        }
        return real.getBlobPath(hash);
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
template<typename F> bool wrapEventually(F predicate)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(predicate())return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    return false;
}
LooperStorage::Config wrapConfig()
{
    LooperStorage::Config c;c.sampleRate=1000;c.maxFrames=144;c.pageFrames=16;
    c.writePages=16;c.readPages=8;c.readAhead=3;c.ramBudget=1024*1024;c.storageBudget=1024*1024;
    return c;
}
AssetHash recordWrapAsset(GatedWrapStore& store,luthier::tests::TestContext& ctx)
{
    LooperStorage recorded;CHECK(recorded.prepare(store,wrapConfig()));recorded.beginRecord();
    for(int frame=0;frame<144;++frame)
        CHECK(recorded.append((frame+1)/256.,-(frame+1)/512.));
    CHECK(recorded.finishRecord());AssetHash hash;CHECK(recorded.manifest(hash));return hash;
}
}

LUTHIER_TEST(RefPedalLooperStorageWrap, freshRestoreKeepsEntireForwardWindowAcrossNinePageWrap)
{
    GatedWrapStore store;const auto hash=recordWrapAsset(store,ctx);
    LooperStorage restored;CHECK(restored.prepare(store,wrapConfig()));
    store.hold();const bool loaded=restored.restore(hash);
    const bool blocked=loaded && store.waitForWorker();
    restored.prefetch(128); // All of [8,0,1,2] must coexist, with no recorder caches.
    store.release();CHECK(loaded);CHECK(blocked);
    const bool ready=wrapEventually([&]{return restored.available(128) && restored.available(0)
        && restored.available(16) && restored.available(32);});
    CHECK_MSG(ready,"fresh forward window [8,0,1,2] must all remain available after queued I/O");
    LooperStorage::Capture captured;CHECK(restored.capture(captured));
    CHECK(captured.status==LooperStorage::CaptureStatus::Ready);CHECK(captured.hash==hash);
    if(!ready)return;
    for(uint64_t start:{128ull,0ull,16ull,32ull})for(uint64_t frame=start;frame<start+16;++frame)
    {
        double l=0,r=0;CHECK(restored.read(frame,l,r));
        CHECK_NEAR(l,double(frame+1)/256.,0);CHECK_NEAR(r,-double(frame+1)/512.,0);
    }
    CHECK(restored.errors()==0);
}

LUTHIER_TEST(RefPedalLooperStorageWrap, freshCoreResumePrimesRaggedWindowBeforeForwardPlayback)
{
    GatedWrapStore store;const auto hash=recordWrapAsset(store,ctx);
    PagedLooperCore::Config config;config.sampleRate=1000;config.seconds=.144;
    config.pageFrames=16;config.writePages=16;config.readPages=8;config.readAhead=3;
    config.ramBudget=8*1024*1024;config.storageBudget=8*1024*1024;
    PagedLooperCore::Graph graph;graph.status=PagedLooperCore::CaptureStatus::Ready;
    graph.frames=144;graph.base=hash;graph.runtime.available=true;
    graph.runtime.state=PagedLooperCore::State::Playing;graph.runtime.head=128;
    graph.runtime.sample=400;graph.runtime.gain=1;
    PagedLooperCore restored;CHECK(restored.prepare(store,config));restored.setSeamMilliseconds(0);
    store.hold();const bool loaded=restored.restore(graph,PagedLooperCore::RestorePolicy::ResumeCaptured);
    const bool blocked=loaded && store.waitForWorker();store.release();CHECK(loaded);CHECK(blocked);
    // No process/prefetch retry can create a transient successful readiness result.
    const bool ready=wrapEventually([&]{return restored.playbackReady();});
    CHECK_MSG(ready,"fresh core must prime head128 and wrapping read-ahead before activation");
    PagedLooperCore::Graph captured;CHECK(restored.capture(captured));
    CHECK(captured.status==PagedLooperCore::CaptureStatus::Ready);
    CHECK(captured.runtime.head==128);CHECK(captured.runtime.sample==400);
    if(!ready)return;
    std::array<double,64> left{},right{};restored.process(left.data(),right.data(),64,1,0);
    for(int i=0;i<64;++i)
    {
        const auto frame=(128+i)%144;
        CHECK_NEAR(left[size_t(i)],double(frame+1)/256.,0);
        CHECK_NEAR(right[size_t(i)],-double(frame+1)/512.,0);
    }
    CHECK(restored.playhead()==48);CHECK(restored.samplePosition()==464);CHECK(restored.faults()==0);
}

LUTHIER_TEST(RefPedalLooperStorageWrap, fullCacheWindowSurvivesEvictionAndRepeatedForwardWraps)
{
    GatedWrapStore store;const auto hash=recordWrapAsset(store,ctx);
    auto config=wrapConfig();config.readAhead=7; // Every slot in a version lane is needed.
    LooperStorage restored;CHECK(restored.prepare(store,config));
    store.hold();const bool loaded=restored.restore(hash);
    const bool blocked=loaded && store.waitForWorker();restored.prefetch(128);
    store.release();CHECK(loaded);CHECK(blocked);
    for(int step=0;step<27;++step)
    {
        const auto page=uint64_t((8+step)%9),start=page*16;
#if LUTHIER_ALLOCATION_COUNTER
        const auto beforeRequest=luthier::tests::allocationsOnThisThread();
#endif
        restored.prefetch(start);
#if LUTHIER_ALLOCATION_COUNTER
        const auto requestAllocations=luthier::tests::allocationsOnThisThread()-beforeRequest;
        CHECK(requestAllocations==0);
#endif
        const bool ready=wrapEventually([&]
        {
            for(uint64_t ahead=0;ahead<8;++ahead)
                if(!restored.available(((page+ahead)%9)*16))return false;
            return true;
        });
        CHECK_MSG(ready,"all eight pages must coexist through slot eviction and wrap");
        if(!ready)return;
        for(uint64_t frame=start;frame<start+16;++frame)
        {
            double l=0,r=0;uint64_t version=0;
#if LUTHIER_ALLOCATION_COUNTER
            const auto beforeRead=luthier::tests::allocationsOnThisThread();
#endif
            const bool read=restored.read(frame,l,r,&version);
#if LUTHIER_ALLOCATION_COUNTER
            const auto readAllocations=luthier::tests::allocationsOnThisThread()-beforeRead;
            CHECK(readAllocations==0);
#endif
            CHECK(read);CHECK(version==1);
            CHECK_NEAR(l,double(frame+1)/256.,0);CHECK_NEAR(r,-double(frame+1)/512.,0);
        }
    }
    CHECK(restored.errors()==0);
}
