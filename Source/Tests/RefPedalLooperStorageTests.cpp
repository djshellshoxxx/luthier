#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStorage.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
#if JUCE_LINUX
#include <pthread.h>
#endif
using namespace luthier::reference;
using luthier::reference::common::LooperStorage;
namespace
{
class ControlledStore final : public IAssetStore
{
public:
    ControlledStore():directory(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("loop-spill-"+juce::Uuid().toString())),real(directory){}
    ~ControlledStore() override {unblock();directory.deleteRecursively();}
    bool putBlob(const AssetHash& h,const uint8_t* data,size_t n) override
    {
#if JUCE_LINUX
        pthread_attr_t attributes;
        if(pthread_getattr_np(pthread_self(),&attributes)==0)
        {
            size_t bytes=0;void* address=nullptr;
            if(pthread_attr_getstack(&attributes,&address,&bytes)==0)observedStack.store(bytes);
            pthread_attr_destroy(&attributes);
        }
#endif
        {std::unique_lock<std::mutex> lock(mutex);++puts;entered.notify_all();gate.wait(lock,[&]{return !blockWrites || permits>0;});if(blockWrites)--permits;if(failWrites)return false;}
        return real.putBlob(h,data,n);
    }
    bool hasBlob(const AssetHash& h) const override{return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override
    {
        {std::unique_lock<std::mutex> lock(mutex);if(blockReads && h!=allowedRead){++blockedReads;entered.notify_all();gate.wait(lock,[&]{return !blockReads;});}}
        return real.getBlobPath(h);
    }
    void putBlobAsync(const AssetHash& h,AssetBlob b,std::function<void(bool)> done) override{real.putBlobAsync(h,std::move(b),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override{real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override{return real.findMissing(h);}
    void unblock(){const std::lock_guard<std::mutex> lock(mutex);blockReads=blockWrites=false;gate.notify_all();}
    void allowOneWrite(){const std::lock_guard<std::mutex> lock(mutex);++permits;gate.notify_all();}
    bool waitForWrites(int count){std::unique_lock<std::mutex> lock(mutex);return entered.wait_for(lock,std::chrono::seconds(2),[&]{return puts>=count;});}
    bool waitForWrite(){std::unique_lock<std::mutex> lock(mutex);return entered.wait_for(lock,std::chrono::seconds(2),[&]{return puts>0;});}
    bool waitForBlockedRead(){std::unique_lock<std::mutex> lock(mutex);return entered.wait_for(lock,std::chrono::seconds(2),[&]{return blockedReads>0;});}
    void setWriteGate(bool block,bool fail=false){const std::lock_guard<std::mutex> lock(mutex);blockWrites=block;failWrites=fail;puts=0;permits=0;}
    void setReadGate(const AssetHash& manifest){const std::lock_guard<std::mutex> lock(mutex);allowedRead=manifest;blockReads=true;}
    juce::File directory;AssetStore real;std::atomic<size_t> observedStack{0};
private:
    mutable std::mutex mutex;mutable std::condition_variable gate,entered;
    bool blockWrites=false,failWrites=false;mutable bool blockReads=false;
    int permits=0;mutable int puts=0,blockedReads=0;AssetHash allowedRead;
};
LooperStorage::Config smallConfig()
{
    LooperStorage::Config c;c.pageFrames=16;c.maxFrames=128;c.writePages=16;c.readPages=4;c.readAhead=2;c.ramBudget=1024*1024;c.storageBudget=1024*1024;return c;
}
template<typename F> bool eventually(F predicate)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(predicate())return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    return false;
}
}
LUTHIER_TEST(RefPedalLooperStorage, fullDurationFitsBoundedRamWithoutTruncation)
{
    ControlledStore store;LooperStorage storage;LooperStorage::Config c;c.maxFrames=48000ull*5400;
    CHECK(storage.prepare(store,c));CHECK(storage.capacity()==259200000ull);
    std::printf("90min/48k stereo: frames=%llu RAM=%zu storage=%llu bytes\n",(unsigned long long)storage.capacity(),storage.reservedBytes(),(unsigned long long)storage.reservedStorageBytes());
    CHECK(storage.reservedBytes()<=c.ramBudget);CHECK(storage.reservedStorageBytes()<=c.storageBudget);
    CHECK(storage.reservedBytes()<32*1024*1024);
    auto invalid=c;invalid.ramBudget=storage.reservedBytes()-1;CHECK(!storage.prepare(store,invalid));CHECK(storage.capacity()==c.maxFrames);
    invalid=c;invalid.storageBudget=storage.reservedStorageBytes()-1;CHECK(!storage.prepare(store,invalid));CHECK(storage.capacity()==c.maxFrames);
}
LUTHIER_TEST(RefPedalLooperStorage, diskPagesAndExternalManifestRoundTrip)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<128;++i)CHECK(storage.append(std::sin(i*.1)*.25,-std::cos(i*.1)*.25));
    CHECK(storage.finishRecord());AssetHash hash;CHECK(storage.manifest(hash));CHECK(!hash.isNull());
    LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));CHECK(restored.length()==128);
    for(int i=0;i<128;++i)
    {
        double l=0,r=0;CHECK(eventually([&]{return restored.read(uint64_t(i),l,r);}));
        CHECK_NEAR(l,std::sin(i*.1)*.25,2e-8);CHECK_NEAR(r,-std::cos(i*.1)*.25,2e-8);
    }
    CHECK((restored.errors()&LooperStorage::ioFailure)==0);
}
LUTHIER_TEST(RefPedalLooperStorage, slowWriterNeverBlocksAudioAndReportsOverrun)
{
    ControlledStore store;store.setWriteGate(true);LooperStorage storage;auto c=smallConfig();c.writePages=2;
    CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<16;++i)CHECK(storage.append(.1,.2));CHECK(store.waitForWrite());
    for(int i=0;i<16;++i)CHECK(storage.append(.1,.2));
    CHECK(!storage.append(.1,.2));CHECK((storage.errors()&LooperStorage::writeOverrun)!=0);CHECK(storage.length()==32);
    store.unblock();AssetHash recovered;CHECK(storage.manifest(recovered));CHECK(!recovered.isNull());
}
LUTHIER_TEST(RefPedalLooperStorage, failedWritesPreservePriorManifestOutput)
{
    ControlledStore store;store.setWriteGate(false,true);LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<16;++i)CHECK(storage.append(.1,.2));
    CHECK(store.waitForWrite());storage.finishRecord();AssetHash previous;previous.bytes[0]=42;
    CHECK(!storage.manifest(previous));CHECK(previous.bytes[0]==42);CHECK((storage.errors()&LooperStorage::ioFailure)!=0);
}
LUTHIER_TEST(RefPedalLooperStorage, slowReaderReturnsSilenceAndRecovers)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<128;++i)CHECK(storage.append(.25,-.5));CHECK(storage.finishRecord());AssetHash hash;CHECK(storage.manifest(hash));
    store.setReadGate(hash);LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));CHECK(store.waitForBlockedRead());
    CHECK(!restored.waitAvailable(0,0));
    double l=99,r=99;CHECK(!restored.read(0,l,r));CHECK(l==0);CHECK(r==0);CHECK((restored.errors()&LooperStorage::readUnderrun)!=0);
    store.unblock();CHECK(restored.waitAvailable(0));CHECK(eventually([&]{return restored.read(0,l,r);}));CHECK_NEAR(l,.25,0);CHECK_NEAR(r,-.5,0);
}
LUTHIER_TEST(RefPedalLooperStorage, corruptManifestIsAtomicAndMissingPageIsReported)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<32;++i)CHECK(storage.append(.25,-.5));CHECK(storage.finishRecord());AssetHash hash;CHECK(storage.manifest(hash));
    auto bad=hash;bad.bytes[0]^=1;CHECK(!storage.restore(bad));CHECK(storage.length()==32);
    const auto page=store.directory.findChildFiles(juce::File::findFiles,false,"*.blob");
    for(const auto& file:page)if(file!=store.real.getBlobPath(hash))CHECK(file.deleteFile());
    LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));
    double l=0,r=0;CHECK(eventually([&]{restored.read(0,l,r);return (restored.errors()&LooperStorage::ioFailure)!=0;}));CHECK(l==0);CHECK(r==0);
}
LUTHIER_TEST(RefPedalLooperStorage, monoPagesAndMalformedManifest)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();c.channels=1;CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<32;++i)CHECK(storage.append(.125,-.75));CHECK(storage.finishRecord());AssetHash hash;CHECK(storage.manifest(hash));
    LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));double l=0,r=0;
    CHECK(eventually([&]{return restored.read(0,l,r);}));CHECK_NEAR(l,.125,0);CHECK_NEAR(r,.125,0);
    const uint8_t junk[]{1,2,3,4};const auto malformed=AssetStore::hashBlob(junk,sizeof(junk));CHECK(store.putBlob(malformed,junk,sizeof(junk)));
    CHECK(!restored.restore(malformed));CHECK(restored.length()==32);
}
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests{long allocationsOnThisThread() noexcept;}
LUTHIER_TEST(RefPedalLooperStorage, audioPagingCallsNeverAllocate)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));
    const auto before=luthier::tests::allocationsOnThisThread();
    storage.beginRecord();for(int i=0;i<128;++i)storage.append(.125,-.25);storage.finishRecord();
    for(int i=0;i<128;++i){double l=0,r=0;storage.read(uint64_t(i),l,r);storage.prefetch(uint64_t(i));}
    LooperStorage::Capture captured;storage.capture(captured);
    storage.beginLayer(128);for(int i=0;i<128;++i){storage.canAdd(uint64_t(i));storage.add(uint64_t(i),.03125,-.03125);}storage.finishRecord();
    const auto count=luthier::tests::allocationsOnThisThread()-before;CHECK(count==0);
}
#endif
LUTHIER_TEST(RefPedalLooperStorage, restoreWaitsForOldJobsAndTimeoutKeepsLoop)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));storage.beginRecord();
    for(int i=0;i<16;++i)CHECK(storage.append(.25,-.5));CHECK(storage.finishRecord());AssetHash hash;CHECK(storage.manifest(hash));
    store.setWriteGate(true);storage.beginRecord();for(int i=0;i<32;++i)CHECK(storage.append(.125,.125));CHECK(storage.finishRecord());CHECK(store.waitForWrite());
    CHECK(!storage.restore(hash));CHECK(storage.length()==32);
    store.unblock();CHECK(storage.restore(hash));CHECK(storage.length()==16);
    double l=0,r=0;CHECK(eventually([&]{return storage.read(0,l,r);}));CHECK_NEAR(l,.25,0);CHECK_NEAR(r,-.5,0);
}
LUTHIER_TEST(RefPedalLooperStorage, sparsePartialOverdubPagesPersistAndAccumulateAcrossWraps)
{
    ControlledStore store;LooperStorage layer;auto c=smallConfig();CHECK(layer.prepare(store,c));CHECK(layer.beginLayer(128));
    double l=99,r=99;CHECK(layer.read(0,l,r));CHECK(l==0);CHECK(r==0);
    CHECK(layer.add(79,.25,-.125));CHECK(layer.add(79,.25,-.125));CHECK(layer.finishRecord());
    AssetHash hash;CHECK(layer.manifest(hash));LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));
    CHECK(restored.read(0,l,r));CHECK(l==0);CHECK(r==0);
    CHECK(eventually([&]{return restored.read(79,l,r);}));CHECK_NEAR(l,.5,0);CHECK_NEAR(r,-.25,0);
    CHECK(layer.beginLayer(64));std::array<double,64> expected{};uint64_t head=23;
    for(int pass=0;pass<7;++pass)for(int i=0;i<64;++i)
    {
        CHECK(eventually([&]{return layer.read(head,l,r)&&layer.canAdd(head);}));
        CHECK(layer.add(head,.03125,-.015625));expected[size_t(head)]+=.03125;head=(head+1)%64;
    }
    CHECK(layer.finishRecord());CHECK(layer.manifest(hash));CHECK(restored.restore(hash));
    for(uint64_t i=0;i<64;++i){CHECK(eventually([&]{return restored.read(i,l,r);}));CHECK_NEAR(l,expected[size_t(i)],0);CHECK_NEAR(r,-expected[size_t(i)]*.5,0);}
}
LUTHIER_TEST(RefPedalLooperStorage, atomicCaptureDistinguishesPendingAndNeverReusesOldHash)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));
    LooperStorage::Capture capture;CHECK(storage.capture(capture));CHECK(capture.status==LooperStorage::CaptureStatus::Empty);CHECK(capture.hash.isNull());
    store.setWriteGate(true);storage.beginRecord();for(int i=0;i<16;++i)CHECK(storage.append(.25,-.25));
    CHECK(storage.capture(capture));CHECK(capture.status==LooperStorage::CaptureStatus::Recording);CHECK(capture.frames==16);CHECK(capture.hash.isNull());
    CHECK(storage.finishRecord());CHECK(store.waitForWrite());CHECK(storage.capture(capture));
    CHECK(capture.status==LooperStorage::CaptureStatus::Pending);CHECK(capture.hash.isNull());
    store.unblock();AssetHash first;CHECK(storage.manifest(first));CHECK(storage.capture(capture));
    CHECK(capture.status==LooperStorage::CaptureStatus::Ready);CHECK(capture.hash==first);const auto generation=capture.generation;
    storage.beginRecord();CHECK(storage.capture(capture));CHECK(capture.generation!=generation);CHECK(capture.status==LooperStorage::CaptureStatus::Recording);CHECK(capture.hash.isNull());
    store.setWriteGate(false,true);for(int i=0;i<16;++i)CHECK(storage.append(.5,-.5));storage.finishRecord();
    CHECK(eventually([&]{return storage.capture(capture)&&capture.status==LooperStorage::CaptureStatus::Failed;}));CHECK(capture.hash.isNull());
}
LUTHIER_TEST(RefPedalLooperStorage, concurrentAtomicCaptureReportsCompleteGenerations)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();CHECK(storage.prepare(store,c));
    std::vector<LooperStorage::Capture> captures;captures.reserve(50000);std::atomic<bool> stop{false};
    std::thread reader([&]{while(!stop.load()){LooperStorage::Capture next;if(storage.capture(next)&&captures.size()<50000)captures.push_back(next);}});
    std::array<AssetHash,101> expected{};
    for(int generation=0;generation<100;++generation)
    {
        storage.beginRecord();for(int i=0;i<16;++i)storage.append(double(generation)/128.,-.25);
        storage.finishRecord();AssetHash hash;CHECK(storage.manifest(hash));LooperStorage::Capture next;CHECK(storage.capture(next));
        CHECK(next.generation>=2 && next.generation<=101);expected[size_t(next.generation-1)]=hash;
    }
    stop=true;reader.join();CHECK(!captures.empty());
    bool valid=true;for(const auto& capture:captures)
    {
        if(capture.status==LooperStorage::CaptureStatus::Ready)
            valid=valid && capture.generation>=2 && capture.generation<=101 && capture.hash==expected[size_t(capture.generation-1)] && capture.frames==16;
        else valid=valid && capture.hash.isNull();
    }
    CHECK(valid);
}
LUTHIER_TEST(RefPedalLooperStorage, workerCompactsSparseLayerAndPersistsNewBase)
{
    ControlledStore store;LooperStorage base,layer;auto c=smallConfig();CHECK(base.prepare(store,c));CHECK(layer.prepare(store,c));
    base.beginRecord();for(int i=0;i<128;++i)CHECK(base.append(.25,-.25));CHECK(base.finishRecord());AssetHash before;CHECK(base.manifest(before));
    CHECK(layer.beginLayer(128));for(int i=24;i<40;++i)CHECK(layer.add(uint64_t(i),.125,-.125));CHECK(layer.finishRecord());AssetHash delta;CHECK(layer.manifest(delta));
    store.setWriteGate(true);const auto token=base.mergeLayer(layer);CHECK(token!=0);CHECK(store.waitForWrite());
    LooperStorage::Capture capture;CHECK(base.capture(capture));CHECK(capture.status==LooperStorage::CaptureStatus::Pending);CHECK(capture.hash.isNull());
    CHECK(base.mergeLayer(layer)==0);CHECK(!layer.beginLayer(128));
    store.unblock();CHECK(eventually([&]{return base.mergeComplete(token);}));CHECK(base.acknowledgeMerge(token));AssetHash after;CHECK(base.manifest(after));CHECK(after!=before);
    LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(after));
    for(uint64_t i=0;i<128;++i)
    {
        double l=0,r=0;uint64_t version=0;CHECK(eventually([&]{return base.read(i,l,r,&version,true);}));CHECK(base.mergedAt(i,version,token));
        CHECK_NEAR(l,i>=24&&i<40?.375:.25,0);CHECK_NEAR(r,-l,0);
        CHECK(eventually([&]{return restored.read(i,l,r);}));CHECK_NEAR(l,i>=24&&i<40?.375:.25,0);CHECK_NEAR(r,-l,0);
    }
    CHECK(layer.beginLayer(128));
}
LUTHIER_TEST(RefPedalLooperStorage, compactionCancelCannotOverwriteNewRecording)
{
    ControlledStore store;LooperStorage base,layer;auto c=smallConfig();CHECK(base.prepare(store,c));CHECK(layer.prepare(store,c));
    base.beginRecord();for(int i=0;i<32;++i)CHECK(base.append(.25,-.25));base.finishRecord();AssetHash hash;CHECK(base.manifest(hash));
    CHECK(layer.beginLayer(32));CHECK(layer.add(0,.125,-.125));layer.finishRecord();CHECK(layer.manifest(hash));
    store.setWriteGate(true);const auto token=base.mergeLayer(layer);CHECK(token!=0);CHECK(store.waitForWrite());
    base.beginRecord();for(int i=0;i<16;++i)CHECK(base.append(.5,-.5));CHECK(base.finishRecord());
    store.unblock();CHECK(base.manifest(hash));CHECK(!base.mergeComplete(token));CHECK(base.length()==16);
    double l=0,r=0;uint64_t version=0;CHECK(eventually([&]{return base.read(0,l,r,&version);}));CHECK_NEAR(l,.5,0);CHECK_NEAR(r,-.5,0);CHECK(!base.mergedAt(0,version,token));
}

LUTHIER_TEST(RefPedalLooperStorage, compactionRevisionHandoffDoesNotDoubleCountRetiredAudio)
{
    ControlledStore store;LooperStorage recorded,base,layer;auto c=smallConfig();CHECK(recorded.prepare(store,c));CHECK(base.prepare(store,c));CHECK(layer.prepare(store,c));
    recorded.beginRecord();for(int i=0;i<128;++i)recorded.append(.25,-.25);recorded.finishRecord();AssetHash hash;CHECK(recorded.manifest(hash));CHECK(base.restore(hash));
    double l=0,r=0;uint64_t version=0;CHECK(eventually([&]{return base.read(0,l,r,&version);}));CHECK(version==1);
    CHECK(layer.beginLayer(128));for(int i=0;i<128;++i)CHECK(layer.add(uint64_t(i),.125,-.125));layer.finishRecord();CHECK(layer.manifest(hash));
    store.setWriteGate(true);
#if LUTHIER_ALLOCATION_COUNTER
    const auto before=luthier::tests::allocationsOnThisThread();
#endif
    const auto token=base.mergeLayer(layer);
#if LUTHIER_ALLOCATION_COUNTER
    const auto allocations=luthier::tests::allocationsOnThisThread()-before;CHECK(allocations==0);
#endif
    CHECK(token!=0);CHECK(store.waitForWrite());store.allowOneWrite();CHECK(store.waitForWrites(2));
    CHECK(base.read(0,l,r,&version,true));CHECK(version==1);CHECK(!base.mergedAt(0,version,token));CHECK_NEAR(l+.125,.375,0);
    store.unblock();CHECK(eventually([&]{return base.mergeComplete(token);}));
    CHECK(eventually([&]{return base.read(0,l,r,&version,false);}));CHECK(version==2);CHECK(base.acknowledgeMerge(token));CHECK(base.mergedAt(0,version,token));CHECK_NEAR(l,.375,0);
    CHECK(layer.beginLayer(128));CHECK(layer.add(0,.0625,-.0625));layer.finishRecord();CHECK(layer.manifest(hash));
    const auto second=base.mergeLayer(layer);CHECK(second!=0);CHECK(eventually([&]{return base.mergeComplete(second);}));CHECK(base.acknowledgeMerge(second));
    CHECK(eventually([&]{return base.read(0,l,r,&version,true);}));CHECK(version==3);CHECK(base.mergedAt(0,version,second));CHECK_NEAR(l,.4375,0);
}
LUTHIER_TEST(RefPedalLooperStorage, compactionSourceLifetimeIsHeldUntilWorkerFinishes)
{
    ControlledStore store;LooperStorage base;auto layer=std::make_unique<LooperStorage>();auto c=smallConfig();CHECK(base.prepare(store,c));CHECK(layer->prepare(store,c));
    base.beginRecord();for(int i=0;i<32;++i)base.append(.25,-.25);base.finishRecord();AssetHash hash;CHECK(base.manifest(hash));
    CHECK(layer->beginLayer(32));CHECK(layer->add(0,.125,-.125));layer->finishRecord();CHECK(layer->manifest(hash));
    store.setWriteGate(true);const auto token=base.mergeLayer(*layer);CHECK(token!=0);CHECK(store.waitForWrite());
    std::atomic<bool> entered{false},destroyed{false};std::thread destroy([&]{entered=true;layer.reset();destroyed=true;});
    CHECK(eventually([&]{return entered.load();}));CHECK(!destroyed.load());store.unblock();destroy.join();CHECK(destroyed.load());
    CHECK(eventually([&]{return base.mergeComplete(token);}));CHECK(base.acknowledgeMerge(token));double l=0,r=0;CHECK(eventually([&]{return base.read(0,l,r);}));CHECK_NEAR(l,.375,0);
}
LUTHIER_TEST(RefPedalLooperStorage, repeatedWorkerCompactionKeepsLongRunOverdubs)
{
    ControlledStore store;LooperStorage base,layer;auto c=smallConfig();CHECK(base.prepare(store,c));CHECK(layer.prepare(store,c));
    CHECK(base.beginLayer(128));base.finishRecord();AssetHash hash;CHECK(base.manifest(hash));std::array<double,128> expected{};
    for(int pass=0;pass<100;++pass)
    {
        CHECK(layer.beginLayer(128));const auto at=uint64_t((pass*31)%128);CHECK(layer.add(at,.00390625,-.00390625));expected[size_t(at)]+=.00390625;
        CHECK(layer.finishRecord());CHECK(layer.manifest(hash));const auto token=base.mergeLayer(layer);CHECK(token!=0);CHECK(eventually([&]{return base.mergeComplete(token);}));CHECK(base.acknowledgeMerge(token));
    }
    CHECK(base.manifest(hash));LooperStorage restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(hash));
    for(uint64_t i=0;i<128;++i){double l=0,r=0;CHECK(eventually([&]{return restored.read(i,l,r);}));CHECK_NEAR(l,expected[size_t(i)],0);CHECK_NEAR(r,-l,0);}
}
LUTHIER_TEST(RefPedalLooperStorage, failedCompactionIsExplicitAndDoesNotClaimOldHashIsLatest)
{
    ControlledStore store;LooperStorage base,layer;auto c=smallConfig();CHECK(base.prepare(store,c));CHECK(layer.prepare(store,c));
    base.beginRecord();for(int i=0;i<32;++i)base.append(.25,-.25);base.finishRecord();AssetHash before;CHECK(base.manifest(before));
    CHECK(layer.beginLayer(32));CHECK(layer.add(0,.125,-.125));layer.finishRecord();AssetHash hash;CHECK(layer.manifest(hash));
    store.setWriteGate(false,true);const auto token=base.mergeLayer(layer);CHECK(token!=0);LooperStorage::Capture capture;
    CHECK(eventually([&]{return base.capture(capture)&&capture.status==LooperStorage::CaptureStatus::Failed;}));CHECK(capture.hash.isNull());CHECK(!base.mergeComplete(token));
    hash=before;CHECK(!base.manifest(hash));CHECK(hash==before);
}

LUTHIER_TEST(RefPedalLooperStorage, reservationIncludesFixedWorkerStackAndRejectsInvalidConfig)
{
    ControlledStore store;LooperStorage storage;auto c=smallConfig();size_t ram=0;uint64_t disk=0;
    CHECK(LooperStorage::reservation(c,ram,disk));CHECK(storage.prepare(store,c));CHECK(storage.reservedBytes()==ram);CHECK(storage.reservedStorageBytes()==disk);
    CHECK(ram>=LooperStorage::workerStackBytes);storage.beginRecord();for(int i=0;i<16;++i)storage.append(.25,-.25);storage.finishRecord();AssetHash hash;CHECK(storage.manifest(hash));
#if JUCE_LINUX
    CHECK(store.observedStack.load()==LooperStorage::workerStackBytes);
#endif
    const auto before=ram;const auto diskBefore=disk;c.sampleRate=0;
    CHECK(!LooperStorage::reservation(c,ram,disk));CHECK(ram==before);CHECK(disk==diskBefore);
}
