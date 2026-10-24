#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStorage.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
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
        {std::unique_lock<std::mutex> lock(mutex);++puts;entered.notify_all();gate.wait(lock,[&]{return !blockWrites;});if(failWrites)return false;}
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
    bool waitForWrite(){std::unique_lock<std::mutex> lock(mutex);return entered.wait_for(lock,std::chrono::seconds(2),[&]{return puts>0;});}
    bool waitForBlockedRead(){std::unique_lock<std::mutex> lock(mutex);return entered.wait_for(lock,std::chrono::seconds(2),[&]{return blockedReads>0;});}
    void setWriteGate(bool block,bool fail=false){const std::lock_guard<std::mutex> lock(mutex);blockWrites=block;failWrites=fail;puts=0;}
    void setReadGate(const AssetHash& manifest){const std::lock_guard<std::mutex> lock(mutex);allowedRead=manifest;blockReads=true;}
    juce::File directory;AssetStore real;
private:
    mutable std::mutex mutex;mutable std::condition_variable gate,entered;
    bool blockWrites=false,failWrites=false;mutable bool blockReads=false;
    mutable int puts=0,blockedReads=0;AssetHash allowedRead;
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
    store.unblock();
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
    double l=99,r=99;CHECK(!restored.read(0,l,r));CHECK(l==0);CHECK(r==0);CHECK((restored.errors()&LooperStorage::readUnderrun)!=0);
    store.unblock();CHECK(eventually([&]{return restored.read(0,l,r);}));CHECK_NEAR(l,.25,0);CHECK_NEAR(r,-.5,0);
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
