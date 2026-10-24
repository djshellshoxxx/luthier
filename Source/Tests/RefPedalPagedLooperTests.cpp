// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
#include <condition_variable>
#include <mutex>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace
{
struct PagedFixture
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("paged-core-"+juce::Uuid().toString());
    AssetStore store{path};
    ~PagedFixture(){path.deleteRecursively();}
};
class GatedPagedStore final : public IAssetStore
{
public:
    AssetStore& real;std::mutex mutex;std::condition_variable cv;bool block=false,fail=false;int writes=0;
    explicit GatedPagedStore(AssetStore& store):real(store){}
    ~GatedPagedStore() override {release();}
    bool putBlob(const AssetHash& h,const uint8_t* data,size_t n) override
    {std::unique_lock<std::mutex> lock(mutex);++writes;cv.notify_all();cv.wait(lock,[&]{return !block;});if(fail)return false;lock.unlock();return real.putBlob(h,data,n);}
    bool hasBlob(const AssetHash& h) const override{return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override{return real.getBlobPath(h);}
    void putBlobAsync(const AssetHash& h,AssetBlob b,std::function<void(bool)> done) override{real.putBlobAsync(h,std::move(b),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override{real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override{return real.findMissing(h);}
    void release(){const std::lock_guard<std::mutex> lock(mutex);block=false;cv.notify_all();}
    bool waitWrite(){std::unique_lock<std::mutex> lock(mutex);return cv.wait_for(lock,std::chrono::seconds(2),[&]{return writes>0;});}
};
PagedLooperCore::Config tiny()
{
    PagedLooperCore::Config c;c.sampleRate=1000;c.seconds=.064;c.pageFrames=16;c.writePages=16;c.readPages=8;c.readAhead=3;c.ramBudget=8*1024*1024;c.storageBudget=1024*1024;return c;
}
bool ready(PagedLooperCore& core,PagedLooperCore::Graph* out=nullptr)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do
    {
        core.process(nullptr,nullptr,0);PagedLooperCore::Graph graph;
        if(core.capture(graph) && graph.status==PagedLooperCore::CaptureStatus::Ready && core.playbackReady() && graph.retiredCount==0){if(out)*out=graph;return true;}
        std::this_thread::yield();
    }while(std::chrono::steady_clock::now()<until);
    return false;
}
void process(PagedLooperCore& core,int n,double inL,double inR,std::vector<double>* lOut=nullptr)
{
    std::vector<double> l(size_t(n),inL),r(size_t(n),inR);core.process(l.data(),r.data(),n);if(lOut)*lOut=std::move(l);
}
}
LUTHIER_TEST(RefPedalPagedLooper, fullDurationAggregateReservationIncludesAllStreams)
{
    PagedFixture f;PagedLooperCore core;PagedLooperCore::Config c;c.sampleRate=48000;c.seconds=5400;c.undoDepth=8;c.ramBudget=256*1024*1024;c.storageBudget=32ull*1024*1024*1024;
    CHECK(core.prepare(f.store,c));CHECK(core.capacity()==259200000ull);CHECK(core.reservedBytes()<=c.ramBudget);CHECK(core.reservedStorageBytes()<=c.storageBudget);
    std::printf("paged90min/48k depth8 streams11 RAM=%zu storage=%llu\n",core.reservedBytes(),(unsigned long long)core.reservedStorageBytes());
    auto tooSmall=c;tooSmall.ramBudget=core.reservedBytes()-1;CHECK(!core.prepare(f.store,tooSmall));CHECK(core.capacity()==259200000ull);
    PagedLooperCore rc5;auto high=c;high.sampleRate=192000;high.undoDepth=1;high.pageFrames=32768;high.writePages=4;high.readAhead=12;high.readPages=14;high.ramBudget=64*1024*1024;high.storageBudget=32ull*1024*1024*1024;
    CHECK(rc5.prepare(f.store,high));CHECK(rc5.capacity()==1036800000ull);CHECK(double(high.pageFrames)*high.readAhead/high.sampleRate>=2);
    std::printf("pagedRC5/192k depth1 streams4 lookahead=%.3fs RAM=%zu storage=%llu\n",double(high.pageFrames)*high.readAhead/high.sampleRate,rc5.reservedBytes(),(unsigned long long)rc5.reservedStorageBytes());
    CHECK(rc5.reservedBytes()<=high.ramBudget);CHECK(rc5.reservedStorageBytes()<=high.storageBudget);
    auto ditto=tiny();ditto.sampleRate=192000;ditto.seconds=300;ditto.channels=1;ditto.pageFrames=4096;ditto.writePages=8;ditto.readPages=8;ditto.ramBudget=32*1024*1024;ditto.storageBudget=4ull*1024*1024*1024;
    PagedLooperCore mono;CHECK(mono.prepare(f.store,ditto));CHECK(mono.capacity()==57600000ull);CHECK(mono.reservedBytes()<=ditto.ramBudget);
    std::printf("pagedDitto/192k depth1 streams4 RAM=%zu storage=%llu\n",mono.reservedBytes(),(unsigned long long)mono.reservedStorageBytes());
    auto rc1=ditto;rc1.seconds=720;rc1.channels=2;rc1.storageBudget=8ull*1024*1024*1024;PagedLooperCore stereo;CHECK(stereo.prepare(f.store,rc1));CHECK(stereo.capacity()==138240000ull);CHECK(stereo.reservedBytes()<=rc1.ramBudget);
    std::printf("pagedRC1/192k depth1 streams4 RAM=%zu storage=%llu\n",stereo.reservedBytes(),(unsigned long long)stereo.reservedStorageBytes());
    for(double rate:{44100.,48000.,96000.,192000.})
    {
        auto native=high;native.sampleRate=rate;native.pageFrames=rate<=48000?8192:rate<=96000?16384:32768;
        PagedLooperCore checked;CHECK(checked.prepare(f.store,native));CHECK(checked.capacity()==uint64_t(rate*5400));CHECK(checked.reservedBytes()<=64*1024*1024);
        CHECK(double(native.pageFrames)*native.readAhead/rate>=2);
    }
}
LUTHIER_TEST(RefPedalPagedLooper, timestampRecordAndStereoDryRemainExact)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));
    CHECK(core.enqueue({LooperAction::Record,3}));CHECK(core.enqueue({LooperAction::Play,35}));
    std::vector<double> l(40,.25),r(40,-.5);core.process(l.data(),r.data(),40);
    CHECK(core.length()==32);CHECK(core.state()==PagedLooperCore::State::Playing);CHECK(core.samplePosition()==40);
    for(int i=0;i<35;++i){CHECK_NEAR(l[size_t(i)],.25,0);CHECK_NEAR(r[size_t(i)],-.5,0);}
    CHECK(ready(core));process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.25,0);
    core.apply(LooperAction::Stop);process(core,6,.125,-.25,&l);CHECK_NEAR(l.back(),.125,0);CHECK(core.playhead()==0);
}
LUTHIER_TEST(RefPedalPagedLooper, undoRedoCancelAndWorkerCompactionPreserveAcceptedLayers)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));
    core.apply(LooperAction::Record);process(core,32,.25,-.25);core.apply(LooperAction::Play);CHECK(ready(core));process(core,32,0,0);
    core.apply(LooperAction::Overdub);process(core,32,.125,-.125);core.apply(LooperAction::Play);CHECK(ready(core));
    std::vector<double> l;process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.375,0);
    core.apply(LooperAction::Undo);process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.25,0);
    core.apply(LooperAction::Redo);process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.375,0);
    core.apply(LooperAction::Overdub);process(core,7,.5,-.5);core.apply(LooperAction::CancelOverdub);process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.375,0);
    for(int pass=0;pass<20;++pass)
    {
        core.apply(LooperAction::Overdub);CHECK(core.state()==PagedLooperCore::State::Overdubbing);process(core,32,.00390625,-.00390625);core.apply(LooperAction::Play);CHECK(ready(core));
    }
    process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.453125,0);
    CHECK((core.faults()&(LooperStorage::writeOverrun|LooperStorage::ioFailure))==0);
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));CHECK(graph.layerCount==1);CHECK(!graph.base.isNull());
    PagedLooperCore restored;CHECK(restored.prepare(f.store,c));CHECK(restored.restore(graph));CHECK(ready(restored));restored.apply(LooperAction::Play);
    for(int attempt=0;attempt<100;++attempt){process(restored,32,0,0,&l);if(l.back()!=0)break;std::this_thread::yield();}
    process(restored,64,0,0,&l);for(size_t i=32;i<l.size();++i)CHECK_NEAR(l[i],.453125,0);
}
LUTHIER_TEST(RefPedalPagedLooper, capacityAutoClosesAndClearNeverReadsStalePages)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));core.apply(LooperAction::Record);process(core,64,.25,-.25);
    CHECK(core.length()==64);CHECK(core.state()==PagedLooperCore::State::Playing);CHECK(ready(core));core.apply(LooperAction::Clear);
    std::vector<double> l;process(core,64,.125,-.5,&l);for(double x:l)CHECK_NEAR(x,.125,0);CHECK(core.length()==0);
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));CHECK(graph.status==PagedLooperCore::CaptureStatus::Empty);CHECK(graph.base.isNull());
}
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests{long allocationsOnThisThread() noexcept;}
LUTHIER_TEST(RefPedalPagedLooper, callbacksAndAtomicGraphCaptureDoNotAllocate)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));std::array<double,32> l{},r{};
    const auto before=luthier::tests::allocationsOnThisThread();core.apply(LooperAction::Record);core.process(l.data(),r.data(),32);core.apply(LooperAction::Play);
    core.apply(LooperAction::Overdub);core.process(l.data(),r.data(),32);core.apply(LooperAction::CancelOverdub);core.apply(LooperAction::Undo);core.apply(LooperAction::Redo);
    PagedLooperCore::Graph graph;core.capture(graph);core.enqueue({LooperAction::Stop,65});core.reset();core.process(l.data(),r.data(),32);
    CHECK(luthier::tests::allocationsOnThisThread()-before==0);
}
#endif
LUTHIER_TEST(RefPedalPagedLooper, concurrentCaptureNeverPublishesMixedOrPartialAssetGraphs)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));std::atomic<bool> stop{false},valid{true};std::atomic<int> captures{0};
    std::thread reader([&]
    {
        while(!stop.load())
        {
            PagedLooperCore::Graph graph;if(!core.capture(graph))continue;++captures;
            if(graph.status==PagedLooperCore::CaptureStatus::Ready)
            {
                bool complete=graph.frames==32 && !graph.base.isNull() && f.store.hasBlob(graph.base) && graph.layerCount>=0 && graph.layerCount<=1;
                for(int i=0;i<graph.layerCount;++i)complete=complete && !graph.layers[size_t(i)].isNull() && f.store.hasBlob(graph.layers[size_t(i)]);
                if(!complete)valid=false;
            }
            else if(!graph.base.isNull() || graph.layerCount!=0)valid=false;
        }
    });
    for(int generation=0;generation<100;++generation)
    {
        core.apply(LooperAction::Record);process(core,32,.25+double(generation)/512.,-.25);core.apply(LooperAction::Play);CHECK(ready(core));
        core.apply(LooperAction::Overdub);process(core,16,.03125,-.03125);core.apply(LooperAction::Play);CHECK(ready(core));
        core.apply(LooperAction::Undo);core.apply(LooperAction::Redo);
        core.apply(LooperAction::Overdub);process(core,7,.5,-.5);core.apply(LooperAction::CancelOverdub);CHECK(ready(core));
        if(generation%10==0)
        {
            PagedLooperCore::Graph graph;CHECK(core.capture(graph));PagedLooperCore restored;CHECK(restored.prepare(f.store,c));CHECK(restored.restore(graph));CHECK(ready(restored));
            PagedLooperCore::Graph after;CHECK(restored.capture(after));CHECK(after.base==graph.base);CHECK(after.layers==graph.layers);CHECK(after.frames==graph.frames);
        }
    }
    stop=true;reader.join();CHECK(captures.load()>0);CHECK(valid.load());
}

LUTHIER_TEST(RefPedalPagedLooper, blockedWriterAutoClosesAcceptedTakeAndKeepsDry)
{
    PagedFixture f;GatedPagedStore store(f.store);PagedLooperCore core;auto c=tiny();c.writePages=2;CHECK(core.prepare(store,c));
    {const std::lock_guard<std::mutex> lock(store.mutex);store.block=true;}core.apply(LooperAction::Record);process(core,16,.25,-.5);CHECK(store.waitWrite());process(core,16,.25,-.5);
    std::array<double,1> l{.125},r{-.25};core.process(l.data(),r.data(),1);CHECK_NEAR(l[0],.125,0);CHECK_NEAR(r[0],-.25,0);
    CHECK(core.length()==32);CHECK(core.state()==PagedLooperCore::State::Playing);CHECK((core.faults()&LooperStorage::writeOverrun)!=0);
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));CHECK(graph.status==PagedLooperCore::CaptureStatus::Pending);CHECK(graph.base.isNull());
    store.release();
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(core.capture(graph)&&graph.status==PagedLooperCore::CaptureStatus::Ready)break;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    CHECK(graph.status==PagedLooperCore::CaptureStatus::Ready); // no further audio callback needed for completed I/O
    CHECK(ready(core));CHECK(core.capture(graph));CHECK(graph.status==PagedLooperCore::CaptureStatus::Ready);CHECK(graph.frames==32);
}
LUTHIER_TEST(RefPedalPagedLooper, failedStoreStopsWetAndPublishesExplicitFailedCurrentTake)
{
    PagedFixture f;GatedPagedStore store(f.store);PagedLooperCore core;auto c=tiny();CHECK(core.prepare(store,c));
    core.apply(LooperAction::Record);process(core,32,.25,-.5);core.apply(LooperAction::Play);CHECK(ready(core));PagedLooperCore::Graph before;CHECK(core.capture(before));
    {const std::lock_guard<std::mutex> lock(store.mutex);store.fail=true;}
    core.apply(LooperAction::Record);process(core,16,.5,-.5);core.apply(LooperAction::Play);
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);do{core.process(nullptr,nullptr,0);if(core.faults()&LooperStorage::ioFailure)break;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    CHECK((core.faults()&LooperStorage::ioFailure)!=0);CHECK(core.state()==PagedLooperCore::State::Stopped);
    std::array<double,16> l{},r{};l.fill(.125);r.fill(-.25);core.process(l.data(),r.data(),16);for(size_t i=0;i<16;++i){CHECK_NEAR(l[i],.125,0);CHECK_NEAR(r[i],-.25,0);}
    PagedLooperCore::Graph after;CHECK(core.capture(after));CHECK(after.status==PagedLooperCore::CaptureStatus::Failed);CHECK(after.base.isNull());CHECK(after.generation!=before.generation);
}
LUTHIER_TEST(RefPedalPagedLooper, saveDuringCompactionRestoresCompleteGraphWithoutAnotherAudioCallback)
{
    PagedFixture f;GatedPagedStore store(f.store);PagedLooperCore core;auto c=tiny();CHECK(core.prepare(store,c));
    core.apply(LooperAction::Record);process(core,32,.25,-.25);core.apply(LooperAction::Play);CHECK(ready(core));
    PagedLooperCore::Graph original;CHECK(core.capture(original));
    core.apply(LooperAction::Overdub);process(core,32,.125,-.125);core.apply(LooperAction::Play);CHECK(ready(core));
    {const std::lock_guard<std::mutex> lock(store.mutex);store.block=true;store.writes=0;}
    core.apply(LooperAction::Overdub);process(core,32,.0625,-.0625);core.apply(LooperAction::Play);CHECK(store.waitWrite());
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));CHECK(graph.status==PagedLooperCore::CaptureStatus::Pending);CHECK(graph.base.isNull());
    store.release();const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(core.capture(graph)&&graph.status==PagedLooperCore::CaptureStatus::Ready)break;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    CHECK(graph.status==PagedLooperCore::CaptureStatus::Ready);CHECK(graph.retiredCount==1);CHECK(graph.layerCount==1);CHECK(graph.base==original.base);CHECK(!graph.retired[0].isNull());
    PagedLooperCore restored;CHECK(restored.prepare(store,c));CHECK(restored.restore(graph));CHECK(ready(restored));restored.apply(LooperAction::Play);std::vector<double> l;
    process(restored,64,0,0,&l);for(size_t i=32;i<64;++i)CHECK_NEAR(l[i],.4375,0);
    restored.apply(LooperAction::Undo);process(restored,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.375,0);
}
