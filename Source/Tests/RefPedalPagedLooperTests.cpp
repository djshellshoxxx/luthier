// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
#include <condition_variable>
#include <mutex>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
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
    PagedLooperCore rc5;auto high=c;high.sampleRate=192000;high.undoDepth=1;high.pageFrames=32768;high.writePages=4;high.readAhead=13;high.readPages=14;high.ramBudget=64*1024*1024;high.storageBudget=32ull*1024*1024*1024;
    CHECK(rc5.prepare(f.store,high));CHECK(rc5.capacity()==1036800000ull);CHECK(double(high.pageFrames)*high.readAhead/high.sampleRate>=2);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(rc5.capacity(),high.pageFrames,high.readAhead)>=uint64_t(high.sampleRate*2));
    std::printf("pagedRC5/192k depth1 streams4 nominal=%.3fs minimum=%.3fs RAM=%zu storage=%llu\n",double(high.pageFrames)*high.readAhead/high.sampleRate,double(PagedLooperCore::minimumForwardWindowFrames(rc5.capacity(),high.pageFrames,high.readAhead))/high.sampleRate,rc5.reservedBytes(),(unsigned long long)rc5.reservedStorageBytes());
    CHECK(rc5.reservedBytes()<=high.ramBudget);CHECK(rc5.reservedBytes()<=64000000);CHECK(rc5.reservedStorageBytes()<=high.storageBudget);
    auto ditto=tiny();ditto.sampleRate=192000;ditto.seconds=300;ditto.channels=1;ditto.pageFrames=4096;ditto.writePages=8;ditto.readPages=8;ditto.ramBudget=32*1024*1024;ditto.storageBudget=4ull*1024*1024*1024;
    PagedLooperCore mono;CHECK(mono.prepare(f.store,ditto));CHECK(mono.capacity()==57600000ull);CHECK(mono.reservedBytes()<=ditto.ramBudget);
    std::printf("pagedDitto/192k depth1 streams4 RAM=%zu storage=%llu\n",mono.reservedBytes(),(unsigned long long)mono.reservedStorageBytes());
    auto rc1=ditto;rc1.seconds=720;rc1.channels=2;rc1.storageBudget=8ull*1024*1024*1024;PagedLooperCore stereo;CHECK(stereo.prepare(f.store,rc1));CHECK(stereo.capacity()==138240000ull);CHECK(stereo.reservedBytes()<=rc1.ramBudget);
    std::printf("pagedRC1/192k depth1 streams4 RAM=%zu storage=%llu\n",stereo.reservedBytes(),(unsigned long long)stereo.reservedStorageBytes());
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        auto native=high;native.sampleRate=rate;native.pageFrames=rate<=48000?8192:rate<=96000?16384:32768;
        PagedLooperCore checked;CHECK(checked.prepare(f.store,native));CHECK(checked.capacity()==uint64_t(rate*5400));CHECK(checked.reservedBytes()<=64*1024*1024);
        CHECK(double(native.pageFrames)*native.readAhead/rate>=2);
        CHECK(PagedLooperCore::minimumForwardWindowFrames(checked.capacity(),native.pageFrames,native.readAhead)>=uint64_t(rate*2));
        CHECK(uint64_t(2)+uint64_t(native.readAhead-1)*uint64_t(native.pageFrames)>=uint64_t(rate*2));
        CHECK(checked.reservedBytes()<=64000000);
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
                bool complete=graph.frames==32 && !graph.base.isNull() && f.store.hasBlob(graph.base) && graph.layerCount>=0 && graph.layerCount<=1
                    && graph.redoCount>=0 && graph.layerCount+graph.redoCount<=1;
                for(int i=0;i<graph.layerCount;++i)complete=complete && !graph.layers[size_t(i)].isNull() && f.store.hasBlob(graph.layers[size_t(i)]);
                for(int i=0;i<graph.redoCount;++i)complete=complete && !graph.redo[size_t(i)].isNull() && f.store.hasBlob(graph.redo[size_t(i)]);
                if(!complete)valid=false;
            }
            else if(!graph.base.isNull() || graph.layerCount!=0 || graph.redoCount!=0)valid=false;
        }
    });
    for(int generation=0;generation<100;++generation)
    {
        core.apply(LooperAction::Record);process(core,32,.25+double(generation)/512.,-.25);core.apply(LooperAction::Play);CHECK(ready(core));
        core.apply(LooperAction::Overdub);process(core,16,.03125,-.03125);core.apply(LooperAction::Play);CHECK(ready(core));
        core.apply(LooperAction::Undo);CHECK(ready(core));core.apply(LooperAction::Redo);
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
LUTHIER_TEST(RefPedalPagedLooper, provisionalHoldCancelPreservesRedoUntilTakeIsCommitted)
{
    PagedFixture f;PagedLooperCore core;auto c=tiny();CHECK(core.prepare(f.store,c));core.apply(LooperAction::Record);process(core,32,.25,-.25);core.apply(LooperAction::Play);CHECK(ready(core));process(core,32,0,0);
    core.apply(LooperAction::Overdub);process(core,32,.125,-.125);core.apply(LooperAction::Play);CHECK(ready(core));core.apply(LooperAction::Undo);
    for(int hold=0;hold<10;++hold){core.apply(LooperAction::Overdub);process(core,7,.5,-.5);core.apply(LooperAction::CancelOverdub);}
    core.apply(LooperAction::Redo);std::vector<double> l;process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.375,0);
    core.apply(LooperAction::Undo);core.apply(LooperAction::Overdub);process(core,32,.0625,-.0625);core.apply(LooperAction::Play);CHECK(ready(core));core.apply(LooperAction::Redo);
    process(core,32,0,0,&l);for(double x:l)CHECK_NEAR(x,.3125,0);
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));auto malformed=graph;malformed.retiredCount=1;CHECK(!core.restore(malformed));
    malformed={};malformed.frames=1;CHECK(!core.restore(malformed));
}

LUTHIER_TEST(RefPedalLooperQueries, eligibilityAndProvisionalCancelMatchBothRealCores)
{
    const auto exercise=[&](auto& core,auto settle)
    {
        std::array<double,512> l{},r{};
        const auto tick=[&](int n,double input){l.fill(input);r.fill(-input);core.process(l.data(),r.data(),n);};
        const auto available=[&](bool undo,bool redo)
        {
            const auto state=core.state();const auto length=core.length();const auto depth=core.undoDepth();const auto position=core.samplePosition();
            const auto allocations=luthier::tests::allocationsOnThisThread();bool correct=true;
            for(int i=0;i<128;++i)correct &= core.canUndo()==undo && core.canRedo()==redo;
            CHECK(luthier::tests::allocationsOnThisThread()==allocations);CHECK(correct);
            CHECK(core.state()==state);CHECK(core.length()==length);CHECK(core.undoDepth()==depth);CHECK(core.samplePosition()==position);
        };
        const auto audio=[&](double expected)
        {
            CHECK(settle());tick(256,0);tick(32,0);
            for(int i=0;i<32;++i){CHECK_NEAR(l[i],expected,0);CHECK_NEAR(r[i],-expected,0);}
        };
        available(false,false);core.apply(LooperAction::Record);available(false,false);tick(32,.25);
        core.apply(LooperAction::Play);audio(.25);available(false,false);
        core.apply(LooperAction::Overdub);available(false,false);tick(32,.125);
        core.apply(LooperAction::Play);audio(.375);available(true,false);
        core.apply(LooperAction::Undo);audio(.25);available(false,true);
        core.apply(LooperAction::Overdub);available(false,false);tick(7,.5);
        core.apply(LooperAction::CancelOverdub);audio(.25);available(false,true);
        core.apply(LooperAction::Redo);audio(.375);available(true,false);
        core.apply(LooperAction::Undo);available(false,true);
        core.apply(LooperAction::Overdub);tick(32,.0625);core.apply(LooperAction::Play);
        audio(.3125);available(true,false); // Accepting this pass invalidates old redo.
        core.apply(LooperAction::Stop);available(false,false);tick(512,0);
        core.apply(LooperAction::Play);audio(.3125);available(true,false);
        core.apply(LooperAction::Clear);available(false,false);core.reset();available(false,false);
    };
    LooperCore ram;CHECK(!ram.canUndo());CHECK(!ram.canRedo());
    CHECK(ram.prepare(48000,.01,1,2000000));ram.setSeamMilliseconds(0);
    exercise(ram,[]{return true;});
    PagedFixture f;PagedLooperCore paged;CHECK(!paged.canUndo());CHECK(!paged.canRedo());
    auto config=tiny();config.sampleRate=48000;config.seconds=.01;config.undoDepth=1;
    CHECK(paged.prepare(f.store,config));paged.setSeamMilliseconds(0);
    exercise(paged,[&]{return ready(paged);});
}

LUTHIER_TEST(RefPedalLooperQueries, rapidStopPlayRestartsAtLoopOriginInBothCores)
{
    PagedFixture fixture;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(int block:{1,64,2048})
        for(int stoppedFrames:{0,1,int(rate*.002)})
    {
        LooperCore ram;PagedLooperCore paged;
        CHECK(ram.prepare(rate,.01,1,2000000));ram.setSeamMilliseconds(0);
        auto config=tiny();config.sampleRate=rate;config.seconds=.01;
        CHECK(paged.prepare(fixture.store,config));paged.setSeamMilliseconds(0);
        std::array<double,2048> a{},ar{},b{},br{};
        const auto zeros=[&](int count)
        {
            while(count>0)
            {
                const int n=std::min(count,block);a.fill(0);ar.fill(0);b.fill(0);br.fill(0);
                ram.process(a.data(),ar.data(),n);paged.process(b.data(),br.data(),n);count-=n;
            }
        };
        ram.apply(LooperAction::Record);paged.apply(LooperAction::Record);
        for(int frame=0;frame<32;)
        {
            const int n=std::min(block,32-frame);
            for(int i=0;i<n;++i){a[i]=b[i]=.125+(frame+i)/1024.;ar[i]=br[i]=-a[i];}
            ram.process(a.data(),ar.data(),n);paged.process(b.data(),br.data(),n);frame+=n;
        }
        ram.apply(LooperAction::Play);paged.apply(LooperAction::Play);CHECK(ready(paged));
        zeros(int(std::ceil(rate*.01/32))*32);zeros(11);CHECK(paged.playhead()==11);
        const auto allocations=luthier::tests::allocationsOnThisThread();
        ram.apply(LooperAction::Stop);paged.apply(LooperAction::Stop);zeros(stoppedFrames);
        ram.apply(LooperAction::Play);paged.apply(LooperAction::Play);
        const bool rewound=paged.playhead()==0;
        double error=0;bool finite=true;
        for(int frame=0;frame<2048;)
        {
            const int n=std::min(block,2048-frame);a.fill(0);ar.fill(0);b.fill(0);br.fill(0);
            ram.process(a.data(),ar.data(),n);paged.process(b.data(),br.data(),n);
            for(int i=0;i<n;++i)
            {error=std::max({error,std::abs(a[i]-b[i]),std::abs(ar[i]-br[i])});finite &= std::isfinite(b[i]) && std::isfinite(br[i]);}
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        const auto label=juce::String(rate)+"/block"+juce::String(block)+"/stopped"+juce::String(stoppedFrames);
        CHECK_MSG(rewound,label+" paged Play did not rewind immediately");
        CHECK_MSG(error<1.e-15,label+" RAM/paged restart mismatch "+juce::String(error,15));CHECK(finite);
    }
}

LUTHIER_TEST(RefPedalPagedLooper, graphRestorePreservesAvailableRedoOrderAcrossPreparedDepthChange)
{
    PagedFixture fixture;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(int depth:{1,3,8})
    {
        auto config=tiny();config.sampleRate=rate;config.seconds=.01;config.undoDepth=depth;
        PagedLooperCore original;CHECK(original.prepare(fixture.store,config));original.setSeamMilliseconds(0);
        original.apply(LooperAction::Record);process(original,32,.125,-.125);
        original.apply(LooperAction::Play);CHECK(ready(original));
        const int warm=int(std::ceil(rate*.01/32))*32;process(original,warm,0,0);
        double expected=.125;
        for(int layer=1;layer<=depth;++layer)
        {
            original.apply(LooperAction::Overdub);process(original,32,layer/256.,-layer/256.);
            original.apply(LooperAction::Play);CHECK(ready(original));expected+=layer/256.;
        }
        const int undone=depth==1?1:depth-1;
        for(int layer=depth;layer>depth-undone;--layer)
        {original.apply(LooperAction::Undo);expected-=layer/256.;}
        CHECK(original.canRedo());CHECK(ready(original));
        PagedLooperCore::Graph graph;CHECK(original.capture(graph));
        CHECK(graph.redoCount==undone);CHECK(graph.layerCount==depth-undone);
        const auto position=original.samplePosition();
        auto reject=[&](const auto& bad)
        {
            CHECK(!original.restore(bad));CHECK(original.canRedo());
            CHECK(original.state()==PagedLooperCore::State::Playing);CHECK(original.samplePosition()==position);
            PagedLooperCore::Graph after;CHECK(original.capture(after));
            CHECK(after.base==graph.base);CHECK(after.layers==graph.layers);CHECK(after.redo==graph.redo);
            CHECK(after.redoCount==graph.redoCount);CHECK(after.generation==graph.generation);
        };
        for(int count:{-1,9,std::numeric_limits<int>::max()})
        {auto bad=graph;bad.redoCount=count;reject(bad);}
        {auto bad=graph;bad.redoCount=depth+1-bad.layerCount;reject(bad);}
        {auto bad=graph;bad.redo[size_t(undone-1)]={};reject(bad);}
        {auto bad=graph;bad.status=PagedLooperCore::CaptureStatus::Empty;bad.frames=0;bad.base={};bad.layerCount=bad.retiredCount=0;reject(bad);}
        auto expanded=config;expanded.undoDepth=8;
        LooperStateCodec::Stored stored;juce::var encoded;
        CHECK(LooperStateCodec::encode(stored,graph,config,encoded));
        CHECK(LooperStateCodec::decode(juce::JSON::parse(juce::JSON::toString(encoded,true,17)),stored));
        CHECK(stored.graph.redo==graph.redo);CHECK(stored.graph.redoCount==graph.redoCount);
        PagedLooperCore restored;CHECK(restored.prepare(fixture.store,expanded));CHECK(restored.restore(stored.graph));
        restored.setSeamMilliseconds(0);CHECK(ready(restored));restored.apply(LooperAction::Play);
        process(restored,warm,0,0);
        CHECK_MSG(restored.canRedo(),juce::String(rate)+" depth"+juce::String(depth)+" restored redo lost");
        for(int layer=depth-undone+1;layer<=depth;++layer)
        {
            const auto allocations=luthier::tests::allocationsOnThisThread();
            restored.apply(LooperAction::Redo);
            CHECK(luthier::tests::allocationsOnThisThread()==allocations);
            CHECK(ready(restored));expected+=layer/256.;
            std::array<double,32> left{},right{};restored.process(left.data(),right.data(),32,1,0);
            bool exact=true;for(int i=0;i<32;++i)exact &= left[i]==expected && right[i]==-expected;
            CHECK_MSG(exact,juce::String(rate)+" depth"+juce::String(depth)+" redo layer"+juce::String(layer)+" changed");
        }
        CHECK(!restored.canRedo());CHECK(restored.undoDepth()==depth);
    }
}

LUTHIER_TEST(RefPedalPagedLooper, redoAssetWorkerCompletionAndFailureNeverPublishPartialReadyGraphs)
{
    for(double rate:{44100.,192000.})for(bool fail:{false,true})
    {
        PagedFixture fixture;GatedPagedStore store(fixture.store);PagedLooperCore core;
        auto config=tiny();config.sampleRate=rate;config.seconds=.01;
        CHECK(core.prepare(store,config));core.setSeamMilliseconds(0);
        core.apply(LooperAction::Record);process(core,32,.125,-.125);
        core.apply(LooperAction::Play);CHECK(ready(core));
        {
            const std::lock_guard<std::mutex> lock(store.mutex);
            store.writes=0;store.block=true;store.fail=fail;
        }
        core.apply(LooperAction::Overdub);process(core,32,.0625,-.0625);CHECK(store.waitWrite());
        core.apply(LooperAction::Play);core.apply(LooperAction::Undo);CHECK(core.canRedo());
        PagedLooperCore::Graph graph;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        const bool captured=core.capture(graph);
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        CHECK(captured);CHECK(graph.status==PagedLooperCore::CaptureStatus::Pending);
        CHECK(graph.base.isNull());CHECK(graph.layerCount==0);CHECK(graph.redoCount==0);
        store.release();
        const auto expected=fail?PagedLooperCore::CaptureStatus::Failed:PagedLooperCore::CaptureStatus::Ready;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        bool terminal=false,invalidSnapshot=false;
        do
        {
            // Worker completion must resolve capture without another audio callback.
            if(core.capture(graph))
            {
                if(fail && graph.status==PagedLooperCore::CaptureStatus::Ready)invalidSnapshot=true;
                if(graph.status!=PagedLooperCore::CaptureStatus::Ready
                    && (!graph.base.isNull() || graph.layerCount || graph.redoCount))invalidSnapshot=true;
                if(graph.status==expected){terminal=true;break;}
            }
            std::this_thread::yield();
        }while(std::chrono::steady_clock::now()<until);
        CHECK(!invalidSnapshot);CHECK(terminal);CHECK(graph.status==expected);
        if(fail)
        {
            CHECK((graph.faults&LooperStorage::ioFailure)!=0);
            CHECK(graph.base.isNull());CHECK(graph.redoCount==0);CHECK(graph.layerCount==0);
        }
        else
        {
            CHECK(!graph.base.isNull());CHECK(graph.redoCount==1);CHECK(graph.layerCount==0);
            CHECK(!graph.redo[0].isNull());CHECK(fixture.store.hasBlob(graph.redo[0]));
        }
        LooperStateCodec::Stored stored;juce::var encoded;
        CHECK(LooperStateCodec::encode(stored,graph,config,encoded));
        CHECK(LooperStateCodec::decode(encoded,stored));CHECK(stored.graph.status==expected);
        CHECK(stored.graph.redoCount==graph.redoCount);CHECK(stored.graph.redo==graph.redo);
    }
}

// Regression (2026-10-28): at real-time pace the worker finished page 0's write
// and its slot was reused for page 1, so REC->PLAY underran at the loop origin
// until a disk read-back (5-11 ms silence, readUnderrun). The origin page now
// stays resident while recording. RC-5 geometry: 8192-frame pages, 4 write slots.
LUTHIER_TEST(RefPedalPagedLooper, realtimeRecordThenImmediatePlayStartsAtTheOriginWithoutUnderrun)
{
    for(int blocks:{56,190,800})
    {
        PagedFixture f;PagedLooperCore core;PagedLooperCore::Config c;c.sampleRate=48000;c.seconds=5400;c.undoDepth=1;
        c.pageFrames=8192;c.writePages=4;c.readPages=14;c.readAhead=13;c.ramBudget=256u<<20;c.storageBudget=32ull<<30;
        CHECK(core.prepare(f.store,c));core.setSeamMilliseconds(0);
        core.apply(LooperAction::Record);
        for(int b=0;b<blocks;++b){process(core,256,.25,-.25);std::this_thread::sleep_for(std::chrono::microseconds(5333));}
        CHECK(core.faults()==0);
        core.apply(LooperAction::Play);CHECK(core.state()==PagedLooperCore::State::Playing);
        std::vector<double> l;int silent=0;
        for(int b=0;b<8;++b){process(core,256,0,0,&l);for(size_t i=b==0?1:0;i<l.size();++i)silent+=l[i]==0;}
        CHECK(silent==0); // only frame 0 sits at the 5 ms fade-in origin
        CHECK((core.faults()&LooperStorage::readUnderrun)==0);
    }
}
