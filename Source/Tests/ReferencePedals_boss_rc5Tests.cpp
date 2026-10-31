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

// Black-box release acceptance: these tests exercise the registered runtime,
// unlike helper tests above. Missing registration is a real, early failure.
#include "../ReferencePedals/PedalRegistry.h"

LUTHIER_TEST(RefPedalBossRc5Runtime, maintainedRuntimeExposesNormativeControls)
{
    PedalRegistry::registerAll();
    auto pedal=PedalRegistry::make("boss-rc5");
    CHECK_MSG(pedal!=nullptr,"boss-rc5 needs an actual maintained IPedal runtime");
    if(!pedal)return;
    const auto& d=pedal->desc();
    CHECK(juce::String(d.paramPrefix)=="boss_rc5_");
    CHECK(d.wantsRawMidi);CHECK(!pedal->footControlsEngagement());
    const char* lanes[]{"loop_level","rhythm_level","rhythm_on","tempo","pattern","variation","kit","memory","reverse","hold_ms","dtap_ms","dry_mute","half_speed"};
    const auto map=PedalRegistry::buildLaneMap(d);CHECK(map.numLanes==13);
    for(int i=0;i<13 && i<map.numLanes;++i)
        CHECK(juce::String(d.params[map.laneToParam[i]].id)==juce::String("boss_rc5_")+lanes[i]);
    for(const auto* suffix:{"pedal1","pedal2","pedal3","sync_clock","quantize","exp_target","exp","lofi","ram_cap_mb"})
        CHECK(PedalRegistry::findParam(d,(juce::String("boss_rc5_")+suffix).toRawUTF8())>=0);
}

LUTHIER_TEST(RefPedalBossRc5Runtime, mainFootRecordsExactlyAndStopHoldClearsAtThreshold)
{
    PedalRegistry::registerAll();auto pedal=PedalRegistry::make("boss-rc5");
    CHECK_MSG(pedal!=nullptr,"missing boss-rc5 wrapper");if(!pedal)return;
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rc5-runtime-"+juce::Uuid().toString());
    {
        AssetStore store(directory);PedalServices services;services.assets=&store;services.sampleRate=48000;services.maxBlock=64;
        pedal->setServices(services);pedal->prepare(48000,64,0);
        CHECK(pedal->getPreparedMemoryBytes()!=kUnknownPreparedMemoryBytes);
        std::array<double,64> left{},right{};
        const auto render=[&](int count,double input)
        {while(count>0){const int n=std::min(64,count);left.fill(input);right.fill(-input);pedal->process(left.data(),right.data(),n);count-=n;}};
        const auto down=[&]{FootEvent e;e.press=true;pedal->onFoot(e);};
        const auto up=[&]{FootEvent e;e.press=false;pedal->onFoot(e);};
        down();CHECK(pedal->readUiMeters().transport==int(LooperCore::State::Recording));up();render(1024,.125);
        down();up();auto meters=pedal->readUiMeters();CHECK(meters.loopLength==1024);CHECK(meters.transport==int(LooperCore::State::Playing));
        // External stop disarms the provisional double-press gesture.
        pedal->onTransport(TransportEvent::Stop,0);render(1024,0);
        CHECK(pedal->readUiMeters().transport==int(LooperCore::State::Stopped));
        down();render(95999,0);CHECK(pedal->readUiMeters().loopLength==1024);
        render(1,0);CHECK(pedal->readUiMeters().loopLength==0);up();
        pedal.reset(); // retire storage workers before their asset service
    }
    CHECK(directory.deleteRecursively());
}

LUTHIER_TEST(RefPedalBossRc5Runtime, unassignedCcAndInvalidProgramLeaveEmptyDryAudioUntouched)
{
    PedalRegistry::registerAll();auto pedal=PedalRegistry::make("boss-rc5");
    CHECK_MSG(pedal!=nullptr,"missing boss-rc5 wrapper");if(!pedal)return;
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rc5-midi-"+juce::Uuid().toString());
    {
        AssetStore store(directory);PedalServices services;services.assets=&store;services.sampleRate=48000;services.maxBlock=64;
        pedal->setServices(services);pedal->prepare(48000,64,0);
        std::array<double,64> left{},right{};
        const auto allocations=luthier::tests::allocationsOnThisThread();
        for(int cc=80;cc<=87;++cc)
        {MidiEvent event;event.status=0xb0;event.d1=uint8_t(cc);event.d2=127;pedal->onMidi(event);event.d2=0;pedal->onMidi(event);}
        for(int pc=99;pc<128;++pc){MidiEvent event;event.status=0xc0;event.d1=uint8_t(pc);pedal->onMidi(event);}
        for(int cc:{0,7,11,32}){MidiEvent event;event.status=0xb0;event.d1=uint8_t(cc);event.d2=127;pedal->onMidi(event);}
        left.fill(.125);right.fill(-.0625);pedal->process(left.data(),right.data(),64);
        const bool allocated=luthier::tests::allocationsOnThisThread()!=allocations;
        bool exact=true;for(int i=0;i<64;++i)exact &= left[i]==.125 && right[i]==-.0625;
        CHECK(!allocated);CHECK(exact);CHECK(pedal->readUiMeters().transport==int(LooperCore::State::Empty));
        CHECK(pedal->readUiMeters().loopLength==0);
        pedal.reset(); // retire storage workers before their asset service
    }
    CHECK(directory.deleteRecursively());
}
