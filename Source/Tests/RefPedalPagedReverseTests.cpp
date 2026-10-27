#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace luthier::tests {long allocationsOnThisThread() noexcept;}
namespace
{
using Direction=PagedLooperCore::Direction;
using State=PagedLooperCore::State;
using Status=PagedLooperCore::CaptureStatus;
class ReverseStore final : public IAssetStore
{
public:
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("paged-reverse-"+juce::Uuid().toString());
    AssetStore real{path};const std::thread::id owner=std::this_thread::get_id();
    ~ReverseStore() override {release();path.deleteRecursively();}
    bool putBlob(const AssetHash& h,const uint8_t* data,size_t size) override
    {
        {std::unique_lock<std::mutex> lock(mutex);++writes;cv.notify_all();
         cv.wait(lock,[&]{return !blockWrites || permits>0;});if(blockWrites)--permits;}
        return real.putBlob(h,data,size);
    }
    bool hasBlob(const AssetHash& h) const override {return real.hasBlob(h);}
    juce::File getBlobPath(const AssetHash& h) const override
    {
        if(std::this_thread::get_id()!=owner)
        {
            std::unique_lock<std::mutex> lock(mutex);
            if(blockReads){readEntered=true;cv.notify_all();cv.wait(lock,[&]{return !blockReads;});}
        }
        return real.getBlobPath(h);
    }
    void putBlobAsync(const AssetHash& h,AssetBlob blob,std::function<void(bool)> done) override
    {real.putBlobAsync(h,std::move(blob),std::move(done));}
    void getBlobAsync(const AssetHash& h,std::function<void(AssetBlob)> done) override
    {real.getBlobAsync(h,std::move(done));}
    std::vector<AssetHash> findMissing(const std::vector<AssetHash>& h) const override {return real.findMissing(h);}
    void holdReads(){const std::lock_guard<std::mutex> lock(mutex);blockReads=true;readEntered=false;}
    void holdWrites(){const std::lock_guard<std::mutex> lock(mutex);blockWrites=true;writes=permits=0;}
    void allowWrite(){const std::lock_guard<std::mutex> lock(mutex);++permits;cv.notify_all();}
    bool waitRead(){std::unique_lock<std::mutex> lock(mutex);return cv.wait_for(lock,std::chrono::seconds(3),[&]{return readEntered;});}
    bool waitWrites(int count){std::unique_lock<std::mutex> lock(mutex);return cv.wait_for(lock,std::chrono::seconds(3),[&]{return writes>=count;});}
    void release(){const std::lock_guard<std::mutex> lock(mutex);blockReads=blockWrites=false;cv.notify_all();}
private:
    mutable std::mutex mutex;mutable std::condition_variable cv;
    mutable bool blockReads=false,readEntered=false;bool blockWrites=false;int writes=0,permits=0;
};
template<typename F> bool reverseEventually(F predicate)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do{if(predicate())return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<until);
    return false;
}
PagedLooperCore::Config reverseConfig(double rate=1000)
{
    PagedLooperCore::Config c;c.sampleRate=rate;c.seconds=.25;c.undoDepth=2;
    c.pageFrames=16;c.writePages=16;c.readPages=8;c.readAhead=3;
    c.ramBudget=8*1024*1024;c.storageBudget=8*1024*1024;return c;
}
double sampleLeft(uint64_t frame){return double(frame+1)/512.;}
double sampleRight(uint64_t frame){return -double(frame+1)/1024.;}
AssetHash reverseAsset(ReverseStore& store,const PagedLooperCore::Config& c,uint64_t frames,
                       luthier::tests::TestContext& ctx,double constant=0)
{
    LooperStorage::Config s;s.sampleRate=c.sampleRate;s.maxFrames=frames;s.pageFrames=c.pageFrames;
    s.writePages=16;s.readPages=c.readPages;s.readAhead=c.readAhead;
    LooperStorage recorded;CHECK(recorded.prepare(store,s));recorded.beginRecord();
    for(uint64_t i=0;i<frames;++i)CHECK(recorded.append(constant?constant:sampleLeft(i),constant?-constant:sampleRight(i)));
    CHECK(recorded.finishRecord());AssetHash hash;CHECK(recorded.manifest(hash));return hash;
}
PagedLooperCore::Graph reverseGraph(const AssetHash& hash,uint64_t frames)
{
    PagedLooperCore::Graph graph;graph.status=Status::Ready;graph.base=hash;graph.frames=frames;
    graph.runtime={true,State::Playing,frames-1,123,1,Direction::Reverse};return graph;
}
bool reverseReady(PagedLooperCore& core,bool service=true,bool compact=false)
{
    return reverseEventually([&]
    {
        if(service)core.process(nullptr,nullptr,0);
        PagedLooperCore::Graph graph;
        return core.playbackReady() && core.capture(graph) && graph.status==Status::Ready
            && (!compact || graph.retiredCount==0);
    });
}
void sameGraph(const PagedLooperCore::Graph& a,const PagedLooperCore::Graph& b,luthier::tests::TestContext& ctx)
{
    CHECK(a.generation==b.generation);CHECK(a.frames==b.frames);CHECK(a.status==b.status);CHECK(a.faults==b.faults);
    CHECK(a.base==b.base);CHECK(a.layers==b.layers);CHECK(a.redo==b.redo);CHECK(a.retired==b.retired);
    CHECK(a.layerCount==b.layerCount);CHECK(a.redoCount==b.redoCount);CHECK(a.retiredCount==b.retiredCount);
    CHECK(a.runtime.available==b.runtime.available);CHECK(a.runtime.state==b.runtime.state);
    CHECK(a.runtime.head==b.runtime.head);CHECK(a.runtime.sample==b.runtime.sample);
    CHECK(a.runtime.gain==b.runtime.gain);CHECK(a.runtime.direction==b.runtime.direction);
}
void rejectDirection(PagedLooperCore& core,Direction direction,luthier::tests::TestContext& ctx)
{
    PagedLooperCore::Graph before,after;CHECK(core.capture(before));
    const auto allocations=luthier::tests::allocationsOnThisThread();const bool accepted=core.setPlaybackDirection(direction);
    const auto allocated=luthier::tests::allocationsOnThisThread()-allocations;CHECK(!accepted);CHECK(allocated==0);
    CHECK(core.capture(after));sameGraph(before,after,ctx);
}
void reverseRender(PagedLooperCore& core,int count,double rate,luthier::tests::TestContext& ctx,double delta=0)
{
    PagedLooperCore::Graph before;CHECK(core.capture(before));
    std::vector<double> left(size_t(count),.03125),right(size_t(count),-.0625);
    const auto allocations=luthier::tests::allocationsOnThisThread();core.process(left.data(),right.data(),count);
    const auto allocated=luthier::tests::allocationsOnThisThread()-allocations;CHECK(allocated==0);
    double gain=before.runtime.gain;
    for(int i=0;i<count;++i)
    {
        const auto frame=(before.runtime.head+before.frames-uint64_t(i)%before.frames)%before.frames;
        gain=std::clamp(gain+1./std::max(1.,rate*.005),0.,1.);
        CHECK_NEAR(left[size_t(i)],.03125+(sampleLeft(frame)+delta)*gain,1e-15);
        CHECK_NEAR(right[size_t(i)],-.0625+(sampleRight(frame)-delta)*gain,1e-15);
    }
    CHECK(core.playhead()==(before.runtime.head+before.frames-uint64_t(count)%before.frames)%before.frames);
    CHECK(core.samplePosition()==before.runtime.sample+count);
}
}

LUTHIER_TEST(RefPedalPagedReverse, restoredExactStereoAtFiveRatesAndBufferExtremes)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        ReverseStore store;const auto config=reverseConfig(rate);const auto hash=reverseAsset(store,config,33,ctx);
        for(int block:{1,4,16,64,512})
        {
            PagedLooperCore core;CHECK(core.prepare(store,config));core.setSeamMilliseconds(0);
            CHECK(core.restore(reverseGraph(hash,33),PagedLooperCore::RestorePolicy::ResumeCaptured));
            const bool ready=reverseReady(core,false);CHECK(ready);if(!ready)return;
            CHECK(core.playbackDirection()==Direction::Reverse);CHECK(core.playhead()==32);
            reverseRender(core,block,rate,ctx);CHECK(core.faults()==0);
        }
    }
}

LUTHIER_TEST(RefPedalPagedReverse, stoppedSelectionPrimesFreshPartialTailAndWholeWindowAcrossLongWraps)
{
    ReverseStore store;auto config=reverseConfig();config.readAhead=7;
    const auto hash=reverseAsset(store,config,133,ctx);PagedLooperCore core;CHECK(core.prepare(store,config));
    core.setSeamMilliseconds(0);store.holdReads();const bool restored=core.restore(reverseGraph(hash,133));
    const bool blocked=restored && store.waitRead();
    const auto allocations=luthier::tests::allocationsOnThisThread();const bool selected=core.setPlaybackDirection(Direction::Reverse);
    const auto allocated=luthier::tests::allocationsOnThisThread()-allocations;
    const bool readyWhileBlocked=core.playbackReady();store.release();
    CHECK(restored);CHECK(blocked);CHECK(selected);CHECK(allocated==0);CHECK(!readyWhileBlocked);
    CHECK(core.state()==State::Stopped);CHECK(core.playhead()==132);
    const bool ready=reverseReady(core,false);CHECK(ready);if(!ready)return;
    core.apply(LooperAction::Play);CHECK(core.playhead()==132);
    for(int page=0;page<27;++page)
    {
        const bool window=reverseReady(core);CHECK(window);if(!window)return;
        const int count=int(core.playhead()%16)+1;
        reverseRender(core,count,1000,ctx);
    }
    CHECK(core.faults()==0);
}

LUTHIER_TEST(RefPedalPagedReverse, setterRejectsActiveInvalidAndAudibleStopFadeWithoutMutation)
{
    PagedLooperCore unprepared;CHECK(!unprepared.setPlaybackDirection(Direction::Reverse));
    CHECK(unprepared.playbackDirection()==Direction::Forward);
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        ReverseStore store;const auto config=reverseConfig(rate);const auto hash=reverseAsset(store,config,33,ctx);
        PagedLooperCore core;CHECK(core.prepare(store,config));
        rejectDirection(core,static_cast<Direction>(99),ctx);
        CHECK(core.setPlaybackDirection(Direction::Reverse));CHECK(core.playhead()==0);
        auto graph=reverseGraph(hash,33);graph.runtime.direction=Direction::Forward;graph.runtime.head=7;
        CHECK(core.restore(graph,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(reverseReady(core));
        rejectDirection(core,Direction::Reverse,ctx);rejectDirection(core,Direction::Forward,ctx);
        core.apply(LooperAction::Stop);rejectDirection(core,Direction::Reverse,ctx);
        std::vector<double> l(size_t(int(rate*.005)-1)),r(l.size());core.process(l.data(),r.data(),int(l.size()));
        rejectDirection(core,Direction::Reverse,ctx);
        double tail[4]{},tailR[4]{};core.process(tail,tailR,4);
        CHECK(core.state()==State::Stopped);CHECK(core.setPlaybackDirection(Direction::Reverse));CHECK(core.playhead()==32);
        CHECK(core.setPlaybackDirection(Direction::Forward));CHECK(core.playhead()==0);
        core.apply(LooperAction::Play);core.apply(LooperAction::Overdub);CHECK(core.state()==State::Overdubbing);
        rejectDirection(core,Direction::Reverse,ctx);core.apply(LooperAction::CancelOverdub);
        core.apply(LooperAction::Record);CHECK(core.state()==State::Recording);rejectDirection(core,Direction::Reverse,ctx);
    }
}

LUTHIER_TEST(RefPedalPagedReverse, selectionSurvivesForwardRecordingClearResetAndRejectsReverseOverdub)
{
    ReverseStore store;const auto config=reverseConfig();PagedLooperCore core;CHECK(core.prepare(store,config));
    CHECK(core.setPlaybackDirection(Direction::Reverse));core.apply(LooperAction::Overdub);CHECK(core.state()==State::Empty);
    core.apply(LooperAction::Record);CHECK(core.playbackDirection()==Direction::Reverse);
    std::array<double,133> l{},r{};for(size_t i=0;i<l.size();++i){l[i]=sampleLeft(i);r[i]=sampleRight(i);}
    core.process(l.data(),r.data(),int(l.size()));rejectDirection(core,Direction::Forward,ctx);
    PagedLooperCore::Graph before,after;CHECK(core.capture(before));core.apply(LooperAction::Overdub);
    CHECK(core.capture(after));sameGraph(before,after,ctx);CHECK(core.state()==State::Recording);
    core.apply(LooperAction::Play);CHECK(core.playhead()==132);CHECK(core.playbackDirection()==Direction::Reverse);
    const bool ready=reverseReady(core);CHECK(ready);if(!ready)return;
    CHECK(core.capture(before));core.apply(LooperAction::Overdub);CHECK(core.capture(after));sameGraph(before,after,ctx);
    core.apply(LooperAction::Stop);CHECK(core.capture(before));core.apply(LooperAction::Overdub);
    CHECK(core.capture(after));sameGraph(before,after,ctx);CHECK(core.state()==State::Stopped);
    // The stored PCM remains in recording order despite the reverse playback selection.
    LooperStorage::Config c;c.sampleRate=1000;c.maxFrames=133;c.pageFrames=16;c.writePages=16;
    LooperStorage stored;CHECK(stored.prepare(store,c));CHECK(stored.restore(before.base));
    for(uint64_t frame=0;frame<133;++frame)
    {
        stored.prefetch(frame);const bool resident=reverseEventually([&]{return stored.available(frame);});
        CHECK(resident);if(!resident)return;double a=0,b=0;CHECK(stored.read(frame,a,b));
        CHECK_NEAR(a,sampleLeft(frame),0);CHECK_NEAR(b,sampleRight(frame),0);
    }
    core.apply(LooperAction::Clear);CHECK(core.state()==State::Empty);CHECK(core.playbackDirection()==Direction::Reverse);
    core.apply(LooperAction::Record);core.process(l.data(),r.data(),1);core.reset();
    CHECK(core.state()==State::Empty);CHECK(core.samplePosition()==0);CHECK(core.playbackDirection()==Direction::Reverse);
    CHECK(core.setPlaybackDirection(Direction::Forward));
}

LUTHIER_TEST(RefPedalPagedReverse, reverseLayersUndoRedoAndGatedRevisionHandoffRemainExact)
{
    ReverseStore store;auto config=reverseConfig();config.undoDepth=1;
    auto graph=reverseGraph(reverseAsset(store,config,33,ctx),33);
    graph.layerCount=1;graph.layers[0]=reverseAsset(store,config,33,ctx,.0625);
    graph.retiredCount=1;graph.retired[0]=reverseAsset(store,config,33,ctx,.125);
    PagedLooperCore core;CHECK(core.prepare(store,config));core.setSeamMilliseconds(0);CHECK(core.restore(graph));
    CHECK(core.setPlaybackDirection(Direction::Reverse));CHECK(reverseReady(core,false));
    store.holdWrites();core.apply(LooperAction::Play);const bool merging=store.waitWrites(1);CHECK(merging);
    reverseRender(core,33,1000,ctx,.1875);
    core.apply(LooperAction::Undo);CHECK(core.canRedo());reverseRender(core,33,1000,ctx,.125);
    core.apply(LooperAction::Redo);CHECK(core.canUndo());reverseRender(core,33,1000,ctx,.1875);
    store.allowWrite();const bool pageOne=store.waitWrites(2);CHECK(pageOne);
    core.process(nullptr,nullptr,0); // Queue the first replacement revision behind the held write.
    store.allowWrite();const bool pageTwo=store.waitWrites(3);CHECK(pageTwo);
    reverseRender(core,33,1000,ctx,.1875); // Mixed old/new base revisions must include the retired delta exactly once.
    PagedLooperCore::Graph frozen;CHECK(core.capture(frozen));CHECK(frozen.retiredCount==1);CHECK(frozen.base==graph.base);
    store.release();const bool compacted=reverseReady(core,true,true);CHECK(compacted);if(!compacted)return;
    reverseRender(core,33,1000,ctx,.1875);core.apply(LooperAction::Undo);reverseRender(core,33,1000,ctx,.125);
    core.apply(LooperAction::Redo);reverseRender(core,33,1000,ctx,.1875);
    PagedLooperCore::Graph complete;CHECK(core.capture(complete));CHECK(complete.retiredCount==0);
    CHECK(complete.layerCount==1);CHECK(complete.layers[0]==graph.layers[0]);CHECK(complete.base!=graph.base);
    PagedLooperCore restored;CHECK(restored.prepare(store,config));restored.setSeamMilliseconds(0);
    CHECK(restored.restore(complete,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(reverseReady(restored,false));
    reverseRender(restored,33,1000,ctx,.1875);CHECK(core.faults()==0);CHECK(restored.faults()==0);
}

LUTHIER_TEST(RefPedalPagedReverse, runtimeResumePreservesDirectionArbitraryHeadFadeAndJsonOmission)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(auto state:{State::Playing,State::Stopped})
    {
        ReverseStore store;const auto config=reverseConfig(rate);auto graph=reverseGraph(reverseAsset(store,config,33,ctx),33);
        graph.runtime.head=7;graph.runtime.sample=499;graph.runtime.gain=.375;graph.runtime.state=state;
        PagedLooperCore source,resumed;CHECK(source.prepare(store,config));CHECK(resumed.prepare(store,config));
        CHECK(source.restore(graph,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(reverseReady(source,false));
        PagedLooperCore::Graph boundary;CHECK(source.capture(boundary));CHECK(boundary.runtime.direction==Direction::Reverse);
        CHECK(resumed.restore(boundary,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(reverseReady(resumed,false));
        PagedLooperCore::Graph captured;CHECK(resumed.capture(captured));sameGraph(boundary,captured,ctx);
        if(state==State::Playing){CHECK(source.enqueue({LooperAction::Stop,502}));CHECK(resumed.enqueue({LooperAction::Stop,502}));}
        std::array<double,512> a{},ar{},b{},br{};a.fill(.03125);b=a;ar.fill(-.0625);br=ar;
        source.process(a.data(),ar.data(),512);resumed.process(b.data(),br.data(),512);
        CHECK(a==b);CHECK(ar==br);CHECK(source.playhead()==resumed.playhead());CHECK(source.samplePosition()==resumed.samplePosition());
        CHECK(resumed.playbackDirection()==Direction::Reverse);CHECK(resumed.state()==State::Stopped);CHECK(resumed.playhead()==32);
        const double nextGain=.375+(state==State::Playing?1.:-1.)/std::max(1.,rate*.005);
        CHECK_NEAR(a[0],.03125+sampleLeft(7)*nextGain,1e-15);
        CHECK_NEAR(ar[0],-.0625+sampleRight(7)*nextGain,1e-15);
        PagedLooperCore::Graph prior,after;CHECK(resumed.capture(prior));auto bad=boundary;bad.runtime.direction=static_cast<Direction>(99);
        CHECK(!resumed.restore(bad,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(resumed.capture(after));sameGraph(prior,after,ctx);
        LooperStateCodec::Stored stored,decoded;juce::var encoded;
        CHECK(LooperStateCodec::encode(stored,boundary,config,encoded));CHECK(!encoded.hasProperty("runtime"));CHECK(!encoded.hasProperty("direction"));
        CHECK(LooperStateCodec::decode(encoded,decoded));CHECK(!decoded.graph.runtime.available);
        CHECK(decoded.graph.runtime.direction==Direction::Forward);
        CHECK(!resumed.restore(decoded.graph,PagedLooperCore::RestorePolicy::ResumeCaptured));
        CHECK(resumed.restore(boundary));CHECK(resumed.state()==State::Stopped);CHECK(resumed.playhead()==0);
        CHECK(resumed.playbackDirection()==Direction::Forward);
    }
    ReverseStore store;PagedLooperCore source,target;CHECK(source.prepare(store,reverseConfig()));CHECK(target.prepare(store,reverseConfig()));
    CHECK(source.setPlaybackDirection(Direction::Reverse));double l[7]{},r[7]{};source.process(l,r,7);
    PagedLooperCore::Graph empty;CHECK(source.capture(empty));CHECK(empty.runtime.direction==Direction::Reverse);
    CHECK(target.restore(empty,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(target.playbackDirection()==Direction::Reverse);
    CHECK(target.state()==State::Empty);CHECK(target.samplePosition()==7);
    PagedLooperCore::Graph before,after;CHECK(target.capture(before));
    auto bad=empty;bad.runtime.direction=static_cast<Direction>(99);CHECK(!target.restore(bad,PagedLooperCore::RestorePolicy::ResumeCaptured));
    CHECK(target.capture(after));sameGraph(before,after,ctx);
    CHECK(target.playbackDirection()==Direction::Reverse);CHECK(target.restore(empty));CHECK(target.playbackDirection()==Direction::Forward);
}

LUTHIER_TEST(RefPedalPagedReverse, atomicCaptureKeepsStoppedDirectionAndOriginInOneTuple)
{
    ReverseStore store;const auto config=reverseConfig();const auto graph=reverseGraph(reverseAsset(store,config,33,ctx),33);
    PagedLooperCore core;CHECK(core.prepare(store,config));CHECK(core.restore(graph));CHECK(reverseReady(core));
    std::atomic<bool> started{false},stop{false},valid{true};std::atomic<int> captures{0};
    std::thread reader([&]
    {
        started=true;
        while(!stop.load())
        {
            PagedLooperCore::Graph sample;if(!core.capture(sample))continue;++captures;
            if(sample.runtime.state!=State::Stopped || sample.runtime.gain!=0 || sample.base!=graph.base
                || (sample.runtime.direction==Direction::Forward?sample.runtime.head!=0:sample.runtime.head!=32))valid=false;
        }
    });
    while(!started.load())std::this_thread::yield();
    bool selected=true;const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int i=0;i<2000;++i)selected=core.setPlaybackDirection(i%2?Direction::Forward:Direction::Reverse)&&selected;
    const auto allocated=luthier::tests::allocationsOnThisThread()-allocations;stop=true;reader.join();
    CHECK(selected);CHECK(allocated==0);CHECK(captures.load()>0);CHECK(valid.load());
}

LUTHIER_TEST(RefPedalPagedReverse, unreadyReverseKeepsDryAndExplicitFaultUntilWorkerRecovery)
{
    ReverseStore store;const auto config=reverseConfig();const auto graph=reverseGraph(reverseAsset(store,config,133,ctx),133);
    PagedLooperCore core;CHECK(core.prepare(store,config));store.holdReads();const bool restored=core.restore(graph);
    const bool blocked=restored && store.waitRead();const bool selected=core.setPlaybackDirection(Direction::Reverse);
    const bool ready=core.playbackReady();core.apply(LooperAction::Play);double l=.03125,r=-.0625;core.process(&l,&r,1);
    PagedLooperCore::Graph missed;const bool captured=core.capture(missed);store.release();
    CHECK(restored);CHECK(blocked);CHECK(selected);CHECK(!ready);CHECK(l==.03125);CHECK(r==-.0625);
    CHECK(captured);CHECK(missed.status==Status::Ready);CHECK(missed.base==graph.base);
    CHECK((core.faults()&LooperStorage::readUnderrun)!=0);CHECK((core.faults()&LooperStorage::ioFailure)==0);
    const bool recovered=reverseReady(core);CHECK(recovered);if(!recovered)return;
    reverseRender(core,1,1000,ctx);CHECK((core.faults()&LooperStorage::readUnderrun)!=0);
}

LUTHIER_TEST(RefPedalPagedReverse, fiveRateMaximumDurationReservationsKeepDecimalCapAndCoverage)
{
    ReverseStore store;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        auto c=reverseConfig(rate);c.seconds=5400;c.undoDepth=1;c.writePages=4;c.readPages=14;c.readAhead=13;
        c.pageFrames=rate<=48000?8192:rate<=96000?16384:32768;c.ramBudget=64000000;c.storageBudget=32ull*1024*1024*1024;
        PagedLooperCore core;CHECK(core.prepare(store,c));CHECK(core.reservedBytes()<=64000000);
        CHECK(core.capacity()==uint64_t(rate*5400));CHECK(core.setPlaybackDirection(Direction::Reverse));
        CHECK(PagedLooperCore::minimumWindowFrames(core.capacity(),c.pageFrames,c.readAhead)>=uint64_t(rate*2));
        CHECK(PagedLooperCore::minimumWindowFrames(uint64_t(c.pageFrames)*20+1,c.pageFrames,c.readAhead)>=uint64_t(rate*2));
    }
}
