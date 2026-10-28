// OWNER: WP16
// Suite names start with RefPedal so 'LuthierTests refpedal' selects them (plan sec 1, E24).
#include "TestFramework.h"
#include "../ReferencePedals/Common/Rc5PressCycle.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <chrono>
#include <thread>

using namespace luthier::reference;
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

LUTHIER_TEST(RefPedalBossRc5, immediateMainPressCycleUsesActualRamAndPagedCores)
{
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("rc5-press-"+juce::Uuid().toString());
    {
        AssetStore store(directory);
        for(double rate:{44100.,48000.,88200.,96000.,192000.})for(int block:{1,16,512})
        {
            LooperCore ram;PagedLooperCore paged;
            CHECK(ram.prepare(rate,.01,2,2000000));ram.setSeamMilliseconds(0);
            PagedLooperCore::Config config;config.sampleRate=rate;config.seconds=.01;
            config.pageFrames=16;config.writePages=16;config.readPages=8;config.readAhead=3;
            config.undoDepth=2;config.ramBudget=8*1024*1024;config.storageBudget=1024*1024;
            CHECK(paged.prepare(store,config));paged.setSeamMilliseconds(0);
            auto exercise=[&](auto& core,auto wait)
            {
                using State=LooperCore::State;
                Rc5PressCycle pedal;
                const auto down=[&]{if(auto action=pedal.down(core.state()))core.apply(*action);};
                std::array<double,512> left{},right{};
                auto render=[&](int frames,double input)
                {
                    while(frames>0)
                    {
                        const int n=std::min(frames,block);left.fill(input);right.fill(-input);
                        core.process(left.data(),right.data(),n);frames-=n;
                    }
                };
                const auto edge=[&](State expected)
                {
                    const auto clock=core.samplePosition();
                    const auto allocations=luthier::tests::allocationsOnThisThread();
                    down();down(); // held duplicate cannot advance twice
                    const bool correct=core.state()==expected;
                    pedal.up();pedal.up();
                    const bool releaseUnchanged=core.state()==expected;
                    CHECK(luthier::tests::allocationsOnThisThread()==allocations);
                    CHECK(correct);CHECK(releaseUnchanged);CHECK(core.samplePosition()==clock);
                };
                // A press may close a zero-frame recording; use the core's
                // resulting Empty state rather than an imagined cycle index.
                pedal.up();pedal.up();
                const auto emptyAllocations=luthier::tests::allocationsOnThisThread();
                down();pedal.up();down();down();
                const bool closedEmpty=core.state()==State::Empty && core.length()==0;
                pedal.reset();
                CHECK(luthier::tests::allocationsOnThisThread()==emptyAllocations);
                CHECK(closedEmpty);CHECK(core.state()==State::Empty);CHECK(core.samplePosition()==0);
                edge(State::Recording);render(32,.125);edge(State::Playing);
                CHECK(core.length()==32);CHECK(wait());
                render(int(std::ceil(rate*.01/32))*32,0);
                edge(State::Overdubbing);render(32,.0625);edge(State::Playing);
                CHECK(wait());CHECK(core.undoDepth()==1);CHECK(core.canUndo());
                left.fill(0);right.fill(0);core.process(left.data(),right.data(),32,1,0);
                bool exact=true;
                for(int i=0;i<32;++i)exact &= left[i]==.1875 && right[i]==-.1875;
                CHECK(exact);
                core.apply(LooperAction::Stop);edge(State::Playing); // no release/window delay
                edge(State::Overdubbing);
                const auto clock=core.samplePosition();const auto depth=core.undoDepth();
                down(); // physical down remains held across an external transport action
                core.apply(LooperAction::Stop);down();CHECK(core.state()==State::Stopped);
                const auto beforeReset=core.state();pedal.reset();
                CHECK(core.state()==beforeReset);CHECK(core.samplePosition()==clock);
                CHECK(core.undoDepth()==depth+1);
                down();CHECK(core.state()==State::Playing);pedal.up();
                core.apply(LooperAction::Clear);edge(State::Recording);
                CHECK(core.length()==0);CHECK(core.undoDepth()==0);
                core.apply(LooperAction::Clear);
            };
            exercise(ram,[]{return true;});
            exercise(paged,[&]
            {
                const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                do
                {
                    paged.process(nullptr,nullptr,0);PagedLooperCore::Graph graph;
                    if(paged.capture(graph) && graph.status==PagedLooperCore::CaptureStatus::Ready
                        && paged.playbackReady() && graph.retiredCount==0)return true;
                    std::this_thread::yield();
                }while(std::chrono::steady_clock::now()<until);
                return false;
            });
        }
    }
    CHECK(directory.deleteRecursively());
}

LUTHIER_TEST(RefPedalBossRc5, immediatePressOnUnpreparedCoresRemainsSafe)
{
    LooperCore ram;PagedLooperCore paged;Rc5PressCycle pedal;
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int i=0;i<2;++i)if(auto action=pedal.down(ram.state()))ram.apply(*action);
    pedal.up();pedal.reset();
    for(int i=0;i<2;++i)if(auto action=pedal.down(paged.state()))paged.apply(*action);
    pedal.up();pedal.reset();
    CHECK(luthier::tests::allocationsOnThisThread()==allocations);
    CHECK(ram.state()==LooperCore::State::Empty);CHECK(paged.state()==LooperCore::State::Empty);
}

// ---------------------------------------------------------------------------
// BossRc5 model wrapper (Models/boss_rc5.*), spec sec 2.2, 4.3, 8 and 9.
#include "../ReferencePedals/Models/boss_rc5.h"
#include <memory>
#include <vector>
namespace
{
using luthier::reference::models::BossRc5;
struct Rc5Fixture
{
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rc5-wrapper-"+juce::Uuid().toString());
    std::unique_ptr<AssetStore> store=std::make_unique<AssetStore>(directory);
    ~Rc5Fixture(){store.reset();directory.deleteRecursively();}
    std::unique_ptr<BossRc5> make(double rate,bool withStore=true,int block=512)
    {
        auto pedal=std::make_unique<BossRc5>();PedalServices services;services.assets=withStore?store.get():nullptr;
        services.instanceSeed=7;services.maxBlock=block;services.sampleRate=rate;pedal->setServices(services);pedal->prepare(rate,block,1);return pedal;
    }
};
float norm(int index,double value){return float(BossRc5::descriptor().params[index].toNormalised(value));}
// Drives completed worker I/O without rendering audio.
bool waitReady(BossRc5& pedal)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do
    {
        pedal.process(nullptr,nullptr,0);PagedLooperCore::Graph graph;
        if(pedal.captureLoop(graph) && graph.status==PagedLooperCore::CaptureStatus::Ready && pedal.playbackReady())return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }while(std::chrono::steady_clock::now()<until);
    return false;
}
// Renders n frames of a generator in blocks; returns the left output.
template<class F> std::vector<double> render(BossRc5& pedal,int n,F input,int block=64,std::vector<double>* rightOut=nullptr)
{
    std::vector<double> l(static_cast<size_t>(n)),r(static_cast<size_t>(n));
    for(int i=0;i<n;++i){const auto x=input(i);l[size_t(i)]=x.first;r[size_t(i)]=x.second;}
    for(int i=0;i<n;i+=block)pedal.process(l.data()+i,r.data()+i,std::min(block,n-i));
    if(rightOut)*rightOut=r;
    return l;
}
auto silence=[](int){return std::pair<double,double>{0,0};};
void tap(BossRc5& pedal){pedal.onFoot({true});pedal.onFoot({false});}
// Records `frames` of the generator then closes the loop into PLAY.
template<class F> void recordLoop(BossRc5& pedal,int frames,F input)
{
    tap(pedal);render(pedal,frames,input);tap(pedal);
}
}

LUTHIER_TEST(RefPedalBossRc5, wrapperDescriptorMatchesReconciledParameterTableAndLanes)
{
    const auto& d=BossRc5::descriptor();
    CHECK(juce::String(d.modelId)=="boss-rc5");CHECK(juce::String(d.paramPrefix)=="boss_rc5_");CHECK(d.numParams==23);
    const char* lanes[]{"loop_level","rhythm_level","rhythm_on","tempo","pattern","variation","kit","memory","reverse","hold_ms","dtap_ms","dry_mute","half_speed"};
    int lane=0;
    for(int i=0;i<d.numParams;++i)
    {
        const auto& p=d.params[i];CHECK(juce::String(p.id).startsWith(d.paramPrefix));
        if(!p.isLaneExcluded()){CHECK(lane<13);if(lane<13)CHECK(juce::String(p.id)=="boss_rc5_"+juce::String(lanes[lane]));++lane;}
    }
    CHECK(lane==13);
    for(int i:{BossRc5::Pedal1,BossRc5::Pedal2,BossRc5::Pedal3})CHECK(d.params[i].kind==ParamKind::Momentary);
    for(int i:{BossRc5::Lofi,BossRc5::RamCapMb})CHECK(d.params[i].has(PF_Config));
    for(int i:{BossRc5::SyncClock,BossRc5::Quantize,BossRc5::ExpTarget,BossRc5::Exp,BossRc5::ClockOut})CHECK(d.params[i].has(PF_Ext));
    CHECK_NEAR(d.params[BossRc5::LoopLevel].def,100,0);CHECK_NEAR(d.params[BossRc5::Tempo].min,40,0);CHECK_NEAR(d.params[BossRc5::Tempo].max,300,0);
    CHECK_NEAR(d.params[BossRc5::HoldMs].def,2000,0);CHECK_NEAR(d.params[BossRc5::DtapMs].def,400,0);
    CHECK(BossRc5::prepareMemory(48000,512,1,nullptr).ownedBytes==sizeof(BossRc5));
    CHECK(BossRc5::prepareMemory(0,512,1,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperRecordsExactLengthAndPlaysBackRecordedAudio)
{
    for(double rate:{44100.,48000.,96000.})
    {
        Rc5Fixture f;auto pedal=f.make(rate);CHECK(pedal->looperAvailable());CHECK(pedal->transportState()==BossRc5::State::Empty);
        const int frames=int(rate*.1);
        auto signal=[](int i){return std::pair<double,double>{.3*std::sin(i*.01),-.2*std::cos(i*.013)};};
        tap(*pedal);CHECK(pedal->transportState()==BossRc5::State::Recording);
        const auto dry=render(*pedal,frames,signal);
        for(int i=0;i<frames;++i)CHECK_NEAR(dry[size_t(i)],signal(i).first,0); // dry unchanged while recording
        tap(*pedal);CHECK(pedal->transportState()==BossRc5::State::Playing);CHECK(pedal->loopLength()==uint64_t(frames));
        CHECK(waitReady(*pedal));
        std::vector<double> right;const auto out=render(*pedal,frames,silence,64,&right);
        const int edge=int(rate*.006); // 5 ms playback fade + 2 ms seam
        double error=0;for(int i=edge;i<frames-edge;++i)error=std::max({error,std::abs(out[size_t(i)]-double(float(signal(i).first))),std::abs(right[size_t(i)]-double(float(signal(i).second)))});
        CHECK_NEAR(error,0,1e-12);
    }
}

LUTHIER_TEST(RefPedalBossRc5, wrapperLoopLevelIsLinearToTwoAndDryIsUnchanged)
{
    Rc5Fixture f;auto pedal=f.make(48000);
    recordLoop(*pedal,4800,[](int){return std::pair<double,double>{.25,-.25};});CHECK(waitReady(*pedal));
    for(double level:{0.,50.,100.,150.,200.})
    {
        pedal->setParam(BossRc5::LoopLevel,norm(BossRc5::LoopLevel,level));
        render(*pedal,4800,silence); // past the 20 ms smoothing
        const auto out=render(*pedal,4800,silence);
        const double expected=.25*level/100;
        if(level==0)CHECK_NEAR(out[2400],0,1e-12);
        else CHECK_NEAR(20*std::log10(out[2400]/expected),0,.1);
    }
    pedal->setParam(BossRc5::Pedal2,1);pedal->setParam(BossRc5::Pedal2,0); // CTL1 stops
    CHECK(pedal->transportState()==BossRc5::State::Stopped);render(*pedal,4800,silence);
    const auto dry=render(*pedal,480,[](int i){return std::pair<double,double>{.1*std::sin(i*.1),.2};});
    for(int i=0;i<480;++i)CHECK_NEAR(dry[size_t(i)],.1*std::sin(i*.1),0);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperDoublePressStopsAndSlowSecondPressClosesOverdub)
{
    Rc5Fixture f;auto pedal=f.make(48000);
    recordLoop(*pedal,9600,[](int){return std::pair<double,double>{.25,.25};});CHECK(waitReady(*pedal));
    tap(*pedal);CHECK(pedal->transportState()==BossRc5::State::Overdubbing); // provisional
    render(*pedal,int(48000*.3),silence);tap(*pedal);              // 300 ms < 400 ms window
    CHECK(pedal->transportState()==BossRc5::State::Stopped);
    const auto stopped=render(*pedal,4800,silence);CHECK_NEAR(stopped.back(),0,0);
    tap(*pedal);CHECK(pedal->transportState()==BossRc5::State::Playing);CHECK(waitReady(*pedal));
    tap(*pedal);render(*pedal,int(48000*.5),silence);tap(*pedal);  // 500 ms: ordinary second press
    CHECK(pedal->transportState()==BossRc5::State::Playing);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperHoldUndoesRedoesAndClearsOnlyFromStop)
{
    Rc5Fixture f;auto pedal=f.make(48000);const int loop=24000,hold=48000*2; // 0.5 s loop, 2 s hold
    recordLoop(*pedal,loop,[](int){return std::pair<double,double>{.25,.25};});CHECK(waitReady(*pedal));
    tap(*pedal);render(*pedal,loop-480,[](int){return std::pair<double,double>{.125,.125};});tap(*pedal); // one overdub pass
    CHECK(pedal->transportState()==BossRc5::State::Playing);CHECK(waitReady(*pedal));
    auto level=[&]{CHECK(waitReady(*pedal));render(*pedal,loop,silence);return render(*pedal,loop,silence)[size_t(loop/2)];};
    CHECK_NEAR(level(),.375,1e-12);
    pedal->onFoot({true});render(*pedal,hold+64,[](int){return std::pair<double,double>{.5,.5};});pedal->onFoot({false}); // hold: undo
    CHECK(pedal->transportState()==BossRc5::State::Playing);CHECK_NEAR(level(),.25,1e-12);
    pedal->onFoot({true});render(*pedal,hold+64,[](int){return std::pair<double,double>{.5,.5};});pedal->onFoot({false}); // hold again: redo
    CHECK_NEAR(level(),.375,1e-12);
    pedal->setParam(BossRc5::Pedal3,1);pedal->setParam(BossRc5::Pedal3,0);CHECK_NEAR(level(),.25,1e-12); // CTL2: undo
    CHECK((pedal->readUiMeters().faults&(LooperStorage::writeOverrun|LooperStorage::readUnderrun|LooperStorage::ioFailure))==0);
    pedal->setParam(BossRc5::Pedal2,1);pedal->setParam(BossRc5::Pedal2,0);CHECK(pedal->transportState()==BossRc5::State::Stopped);
    render(*pedal,int(48000*.5),silence);
    pedal->onFoot({true});render(*pedal,hold-1,silence,1);pedal->onFoot({false});    // one sample short: no clear
    CHECK(pedal->transportState()==BossRc5::State::Playing);CHECK(pedal->loopLength()==uint64_t(loop));
    pedal->setParam(BossRc5::Pedal2,1);pedal->setParam(BossRc5::Pedal2,0);render(*pedal,int(48000*.5),silence);
    pedal->onFoot({true});render(*pedal,hold,silence,1);pedal->onFoot({false});
    CHECK(pedal->transportState()==BossRc5::State::Empty);CHECK(pedal->loopLength()==0);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperOverdubContinuesAcrossPassesAndUndoRemovesTheLastPass)
{
    Rc5Fixture f;auto pedal=f.make(48000);const int loop=12000;
    recordLoop(*pedal,loop,[](int){return std::pair<double,double>{.25,.25};});CHECK(waitReady(*pedal));
    // The worker folds each retired pass while the next plays; render at real-time pace.
    auto realtime=[&](int frames,double value){for(int i=0;i<frames;i+=512){const int n=std::min(512,frames-i);
        render(*pedal,n,[value](int){return std::pair<double,double>{value,value};},512);std::this_thread::sleep_for(std::chrono::microseconds(int(1e6*n/48000)));}};
    tap(*pedal);realtime(3*loop,.125);
    CHECK(pedal->transportState()==BossRc5::State::Overdubbing); // still overdubbing after three wraps
    realtime(loop/2,.0625);tap(*pedal);
    CHECK((pedal->readUiMeters().faults&(LooperStorage::writeOverrun|LooperStorage::readUnderrun))==0);
    CHECK(waitReady(*pedal));render(*pedal,loop,silence);const auto both=render(*pedal,loop,silence);
    // Playback resumes at loop/2 (where the closing press landed): output i is loop frame (i+loop/2)%loop.
    CHECK_NEAR(both[size_t(3*loop/4)],.25+3*.125+.0625,1e-12);CHECK_NEAR(both[size_t(loop/4)],.25+3*.125,1e-12);
    pedal->setParam(BossRc5::Pedal3,1);pedal->setParam(BossRc5::Pedal3,0);CHECK(waitReady(*pedal));
    render(*pedal,loop,silence);const auto undone=render(*pedal,loop,silence);
    CHECK_NEAR(undone[size_t(loop/4)],.25+3*.125,1e-12);CHECK_NEAR(undone[size_t(3*loop/4)],.25+3*.125,1e-12);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperWithoutAssetStorePassesDryRunsRhythmAndReportsFault)
{
    Rc5Fixture f;auto pedal=f.make(48000,false);CHECK(!pedal->looperAvailable());
    tap(*pedal);CHECK(pedal->transportState()==BossRc5::State::Empty);
    const auto out=render(*pedal,4800,[](int i){return std::pair<double,double>{.1*std::sin(i*.05),0};});
    for(int i=0;i<4800;++i)CHECK_NEAR(out[size_t(i)],.1*std::sin(i*.05),0);
    CHECK(pedal->readUiMeters().faults==BossRc5::noAssetStore);
    pedal->setParam(BossRc5::RhythmOn,1);const auto beat=render(*pedal,48000,silence);
    double energy=0;for(double x:beat)energy+=x*x;CHECK(energy>1e-3);
}

LUTHIER_TEST(RefPedalBossRc5, wrapperTempoFollowsHostOnlyInAutoSync)
{
    Rc5Fixture f;auto pedal=f.make(48000,false);
    TimeInfo host;host.valid=host.playing=true;host.bpm=90;pedal->setTime(host);render(*pedal,64,silence);CHECK_NEAR(pedal->rhythmTempo(),90,0);
    pedal->setParam(BossRc5::SyncClock,1);render(*pedal,64,silence);CHECK_NEAR(pedal->rhythmTempo(),120,1e-4);
    pedal->setParam(BossRc5::Tempo,norm(BossRc5::Tempo,300));host.bpm=500;pedal->setTime(host);render(*pedal,64,silence);CHECK_NEAR(pedal->rhythmTempo(),300,1e-3);
    pedal->setParam(BossRc5::SyncClock,0);render(*pedal,64,silence);CHECK_NEAR(pedal->rhythmTempo(),300,0); // clamped host tempo
}

LUTHIER_TEST(RefPedalBossRc5, wrapperStateRoundTripRestoresTheLoopInEitherLoadOrder)
{
    Rc5Fixture f;juce::var extra;const int frames=4800;
    auto signal=[](int i){return std::pair<double,double>{.3*std::sin(i*.02),.1};};
    {
        auto pedal=f.make(48000);recordLoop(*pedal,frames,signal);CHECK(waitReady(*pedal));pedal->saveExtra(extra);
        CHECK(extra.hasProperty("looper"));
    }
    for(bool loadFirst:{true,false})
    {
        auto pedal=std::make_unique<BossRc5>();PedalServices s;s.assets=f.store.get();s.sampleRate=48000;s.maxBlock=512;pedal->setServices(s);
        if(loadFirst)pedal->loadExtra(extra);
        pedal->prepare(48000,512,1);
        if(!loadFirst)pedal->loadExtra(extra);
        CHECK(pedal->transportState()==BossRc5::State::Stopped);CHECK(pedal->loopLength()==uint64_t(frames));
        pedal->setParam(BossRc5::Pedal2,1);pedal->setParam(BossRc5::Pedal2,0);CHECK(waitReady(*pedal));
        const auto out=render(*pedal,frames,silence);double error=0;
        for(int i=300;i<frames-300;++i)error=std::max(error,std::abs(out[size_t(i)]-double(float(signal(i).first))));
        CHECK_NEAR(error,0,1e-12);
        juce::var again;pedal->saveExtra(again);CHECK(again.hasProperty("looper"));
    }
}

LUTHIER_TEST(RefPedalBossRc5, wrapperResetAndReprepareKeepTheUsersLoop)
{
    Rc5Fixture f;auto pedal=f.make(48000);
    recordLoop(*pedal,4800,[](int){return std::pair<double,double>{.25,.25};});CHECK(waitReady(*pedal));
    pedal->reset();CHECK(pedal->transportState()==BossRc5::State::Stopped);CHECK(pedal->loopLength()==4800);
    pedal->prepare(48000,256,1);CHECK(pedal->transportState()==BossRc5::State::Stopped);CHECK(pedal->loopLength()==4800);
    juce::var extra;pedal->prepare(44100,256,1); // rate change: not restorable here, but still saved unchanged
    CHECK(pedal->transportState()==BossRc5::State::Empty);pedal->saveExtra(extra);CHECK(extra.hasProperty("looper"));
    CHECK(extra["looper"]["frames"].toString()=="4800");
}

LUTHIER_TEST(RefPedalBossRc5, wrapperCallbacksAndGesturesAllocateNothing)
{
    Rc5Fixture f;auto pedal=f.make(48000);pedal->setParam(BossRc5::RhythmOn,1);
    std::vector<double> l(64,.1),r(64,-.1);pedal->process(l.data(),r.data(),64);
    const auto before=luthier::tests::allocationsOnThisThread();
    for(int block=0;block<1500;++block)
    {
        if(block==10||block==400||block==700||block==1000)pedal->onFoot({true});
        if(block==11||block==401||block==701||block==1001)pedal->onFoot({false});
        if(block==1200){pedal->setParam(BossRc5::Pedal3,1);pedal->setParam(BossRc5::Pedal3,0);}
        std::fill(l.begin(),l.end(),.1);std::fill(r.begin(),r.end(),-.1);pedal->process(l.data(),r.data(),64);
        for(double x:l)if(!std::isfinite(x))CHECK(false);
    }
    CHECK(luthier::tests::allocationsOnThisThread()==before);
}
