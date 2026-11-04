// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <chrono>
#include <thread>
#include <limits>
#include <atomic>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
struct ResumeFixture
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("looper-resume-"+juce::Uuid().toString());
    AssetStore store{path};
    ~ResumeFixture(){path.deleteRecursively();}
};
PagedLooperCore::Config resumeConfig(double rate)
{
    PagedLooperCore::Config c;c.sampleRate=rate;c.seconds=.01;c.undoDepth=2;
    c.pageFrames=16;c.writePages=16;c.readPages=8;c.readAhead=3;
    c.ramBudget=8*1024*1024;c.storageBudget=1024*1024;return c;
}
bool waitResume(PagedLooperCore& core,bool service=true)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do
    {
        if(service)core.process(nullptr,nullptr,0);
        PagedLooperCore::Graph graph;
        if(core.playbackReady() && core.capture(graph)
            && graph.status==PagedLooperCore::CaptureStatus::Ready)return true;
        std::this_thread::yield();
    }while(std::chrono::steady_clock::now()<until);
    return false;
}
void recordResumeLoop(PagedLooperCore& core)
{
    core.setSeamMilliseconds(0);core.apply(LooperAction::Record);
    std::array<double,192> left{},right{};
    for(int i=0;i<192;++i){left[i]=.125+i/4096.;right[i]=-.25+i/8192.;}
    core.process(left.data(),right.data(),192);core.apply(LooperAction::Play);
}
}

LUTHIER_TEST(RefPedalLooperResume, optInRestoresNonzeroHeadBothFadesAndExactCommandClock)
{
    ResumeFixture fixture;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(int mode:{0,1,2})
    {
        PagedLooperCore original;auto config=resumeConfig(rate);CHECK(original.prepare(fixture.store,config));
        recordResumeLoop(original);CHECK(waitResume(original));
        auto advance=[&](int frames)
        {
            while(frames>0)
            {
                CHECK(waitResume(original));const int n=std::min(frames,16);
                std::array<double,16> left{},right{};original.process(left.data(),right.data(),n);frames-=n;
            }
        };
        if(mode!=1)advance(int(std::ceil(rate*.01/192))*192);
        advance(mode==2?130:137);
        if(mode==2){original.apply(LooperAction::Stop);advance(7);}
        CHECK(original.playhead()==137);
        PagedLooperCore::Graph graph;CHECK(original.capture(graph));CHECK(graph.runtime.available);
        CHECK(graph.runtime.head==137);CHECK(graph.runtime.sample==original.samplePosition());
        CHECK(graph.runtime.state==original.state());
        if(mode==0)CHECK(graph.runtime.gain==1);
        else {CHECK(graph.runtime.gain>0);CHECK(graph.runtime.gain<1);}
        auto expanded=config;expanded.undoDepth=3;
        PagedLooperCore resumed;CHECK(resumed.prepare(fixture.store,expanded));
        resumed.setSeamMilliseconds(0);
        CHECK(resumed.restore(graph,PagedLooperCore::RestorePolicy::ResumeCaptured));
        CHECK(resumed.state()==original.state());CHECK(resumed.playhead()==original.playhead());
        CHECK(resumed.samplePosition()==original.samplePosition());
        CHECK(waitResume(resumed,false)); // restore itself primes head137, not only page0
        const auto commandAt=original.samplePosition()+17;
        CHECK(original.enqueue({LooperAction::Stop,commandAt}));CHECK(resumed.enqueue({LooperAction::Stop,commandAt}));
        bool exact=true;double maxError=0;
        for(int frame=0;frame<256;frame+=16)
        {
            CHECK(waitResume(original));CHECK(waitResume(resumed));
            std::array<double,16> a{},ar{},b{},br{};
            a.fill(.03125);b=a;ar.fill(-.0625);br=ar;
            const auto allocations=luthier::tests::allocationsOnThisThread();
            original.process(a.data(),ar.data(),16);resumed.process(b.data(),br.data(),16);
            const bool allocated=luthier::tests::allocationsOnThisThread()!=allocations;
            CHECK(!allocated);
            if(frame==0)
            {
                const double step=1./std::max(1.,rate*.005);
                const double gain=std::clamp(graph.runtime.gain+(mode==2?-step:step),0.,1.);
                CHECK_NEAR(a[0],.03125+(.125+137./4096)*gain,1e-15);
                CHECK_NEAR(ar[0],-.0625+(-.25+137./8192)*gain,1e-15);
            }
            for(int i=0;i<16;++i)
            {exact &= a[i]==b[i] && ar[i]==br[i];maxError=std::max({maxError,std::abs(a[i]-b[i]),std::abs(ar[i]-br[i])});}
        }
        CHECK_MSG(exact,juce::String(rate)+" mode"+juce::String(mode)+" max error "+juce::String(maxError,17));
        CHECK(resumed.state()==original.state());CHECK(resumed.playhead()==original.playhead());
        CHECK(resumed.samplePosition()==original.samplePosition());
        PagedLooperCore ordinary;CHECK(ordinary.prepare(fixture.store,config));CHECK(ordinary.restore(graph));
        CHECK(ordinary.state()==PagedLooperCore::State::Stopped);CHECK(ordinary.playhead()==0);
        CHECK(ordinary.samplePosition()==0);
        LooperStateCodec::Stored stored;juce::var encoded;
        CHECK(LooperStateCodec::encode(stored,graph,config,encoded));CHECK(LooperStateCodec::decode(encoded,stored));
        CHECK(!stored.graph.runtime.available);CHECK(!encoded.hasProperty("runtime"));
        CHECK(!ordinary.restore(stored.graph,PagedLooperCore::RestorePolicy::ResumeCaptured));
        CHECK(ordinary.state()==PagedLooperCore::State::Stopped);
    }
}

LUTHIER_TEST(RefPedalLooperResume, invalidResumeRejectsBeforeMutationAndEmptyClockAndLimitAreSafe)
{
    ResumeFixture fixture;auto config=resumeConfig(48000);PagedLooperCore core;
    CHECK(core.prepare(fixture.store,config));recordResumeLoop(core);CHECK(waitResume(core));
    PagedLooperCore::Graph graph;CHECK(core.capture(graph));
    const auto position=core.samplePosition();
    auto reject=[&](auto bad)
    {
        CHECK(!core.restore(bad,PagedLooperCore::RestorePolicy::ResumeCaptured));
        CHECK(core.state()==PagedLooperCore::State::Playing);CHECK(core.samplePosition()==position);
        PagedLooperCore::Graph after;CHECK(core.capture(after));CHECK(after.generation==graph.generation);
        CHECK(after.base==graph.base);CHECK(after.runtime.head==graph.runtime.head);
    };
    {auto bad=graph;bad.runtime.available=false;reject(bad);}
    for(auto state:{PagedLooperCore::State::Empty,PagedLooperCore::State::Recording,
            PagedLooperCore::State::Overdubbing,static_cast<PagedLooperCore::State>(99)})
    {auto bad=graph;bad.runtime.state=state;reject(bad);}
    {auto bad=graph;bad.runtime.head=graph.frames;reject(bad);}
    for(double gain:{-1.,1.1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {auto bad=graph;bad.runtime.gain=gain;reject(bad);}
    {auto bad=graph;bad.runtime.sample=-1;reject(bad);}
    auto blankRuntime=graph;blankRuntime.status=PagedLooperCore::CaptureStatus::Empty;
    blankRuntime.frames=0;blankRuntime.base={};blankRuntime.layerCount=blankRuntime.redoCount=blankRuntime.retiredCount=0;
    blankRuntime.runtime.state=PagedLooperCore::State::Empty;blankRuntime.runtime.head=0;blankRuntime.runtime.gain=0;
    {auto bad=blankRuntime;bad.runtime.state=PagedLooperCore::State::Stopped;reject(bad);}
    {auto bad=blankRuntime;bad.runtime.head=1;reject(bad);}
    {auto bad=blankRuntime;bad.runtime.gain=.1;reject(bad);}
    {auto bad=blankRuntime;bad.runtime.available=false;reject(bad);}
    {auto bad=blankRuntime;bad.runtime.sample=-1;reject(bad);}
    CHECK(!core.restore(graph,static_cast<PagedLooperCore::RestorePolicy>(99)));CHECK(core.samplePosition()==position);
    for(auto action:{LooperAction::Record,LooperAction::Overdub})
    {
        PagedLooperCore pending;CHECK(pending.prepare(fixture.store,config));
        if(action==LooperAction::Overdub){recordResumeLoop(pending);CHECK(waitResume(pending));}
        pending.apply(action);PagedLooperCore::Graph unavailable;CHECK(pending.capture(unavailable));
        CHECK(!unavailable.runtime.available);CHECK(!core.restore(unavailable,PagedLooperCore::RestorePolicy::ResumeCaptured));
    }
    auto limit=graph;limit.runtime.sample=std::numeric_limits<int64_t>::max()-1;
    PagedLooperCore boundary;CHECK(boundary.prepare(fixture.store,config));
    CHECK(boundary.restore(limit,PagedLooperCore::RestorePolicy::ResumeCaptured));CHECK(waitResume(boundary));
    double left[3]{},right[3]{};boundary.process(left,right,3);
    CHECK(boundary.samplePosition()==std::numeric_limits<int64_t>::max());
    PagedLooperCore empty;CHECK(empty.prepare(fixture.store,config));double dry[13]{},dryRight[13]{};
    empty.process(dry,dryRight,13);PagedLooperCore::Graph blank;CHECK(empty.capture(blank));
    CHECK(blank.runtime.available);CHECK(boundary.restore(blank,PagedLooperCore::RestorePolicy::ResumeCaptured));
    CHECK(boundary.state()==PagedLooperCore::State::Empty);CHECK(boundary.samplePosition()==13);
}

LUTHIER_TEST(RefPedalLooperResume, concurrentCaptureKeepsHeadClockAndGainInOnePublishedTuple)
{
    ResumeFixture fixture;PagedLooperCore core;CHECK(core.prepare(fixture.store,resumeConfig(48000)));
    recordResumeLoop(core);CHECK(waitResume(core));
    for(int frame=0;frame<384;frame+=16)
    {CHECK(waitResume(core));double left[16]{},right[16]{};core.process(left,right,16);}
    PagedLooperCore::Graph original;CHECK(core.capture(original));CHECK(original.runtime.gain==1);
    std::atomic<bool> stop{false},valid{true};std::atomic<int> captures{0};
    std::thread reader([&]
    {
        while(!stop.load())
        {
            PagedLooperCore::Graph graph;if(!core.capture(graph))continue;
            if(graph.status!=PagedLooperCore::CaptureStatus::Ready || !graph.runtime.available
                || graph.runtime.state!=PagedLooperCore::State::Playing || graph.runtime.gain!=1
                || graph.frames!=192 || graph.base!=original.base || graph.runtime.sample<576
                || graph.runtime.head!=uint64_t(graph.runtime.sample-192)%192)valid=false;
            ++captures;
        }
    });
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(captures.load()==0 && std::chrono::steady_clock::now()<until)std::this_thread::yield();
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int sample=0;sample<5000;++sample){double left=0,right=0;core.process(&left,&right,1);}
    const bool allocated=luthier::tests::allocationsOnThisThread()!=allocations;
    stop=true;reader.join();CHECK(!allocated);CHECK(captures.load()>0);CHECK(valid.load());
}
