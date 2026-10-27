// OWNER: WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/Rc5PressCycle.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include <chrono>
#include <thread>
#include <limits>
using namespace luthier::reference;
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
struct DoubleFixture
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("rc5-double-"+juce::Uuid().toString());
    AssetStore store{path};
    ~DoubleFixture(){path.deleteRecursively();}
};
bool doubleReady(PagedLooperCore& core)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do
    {
        core.process(nullptr,nullptr,0);PagedLooperCore::Graph graph;
        if(core.capture(graph) && graph.status==PagedLooperCore::CaptureStatus::Ready && core.playbackReady())return true;
        std::this_thread::yield();
    }while(std::chrono::steady_clock::now()<until);
    return false;
}
}

LUTHIER_TEST(RefPedalRc5Double, inclusiveDownWindowCancelsOnlyOwnedProvisionalPassAndPreservesRedo)
{
    DoubleFixture fixture;
    for(int rate:{44100,48000,88200,96000,192000})for(int block:{1,64,2048})
        for(int boundary:{-1,0,1})for(bool fromOverdub:{false,true})
    {
        LooperCore ram;PagedLooperCore paged;
        CHECK(ram.prepare(rate,.01,2,2000000));ram.setSeamMilliseconds(0);
        PagedLooperCore::Config config;config.sampleRate=rate;config.seconds=.01;config.undoDepth=2;
        config.pageFrames=64;config.writePages=16;config.readPages=8;config.readAhead=3;
        config.ramBudget=8*1024*1024;config.storageBudget=1024*1024;
        CHECK(paged.prepare(fixture.store,config));paged.setSeamMilliseconds(0);
        auto exercise=[&](auto& core,auto wait)
        {
            Rc5PressCycle pedal;
            const auto apply=[&](const auto& actions)
            {for(int i=0;i<std::clamp(actions.count,0,2);++i)core.apply(actions.actions[size_t(i)]);};
            const auto render=[&](int frames,double input)
            {
                std::array<double,2048> left{},right{};
                while(frames>0)
                {const int n=std::min(frames,block);left.fill(input);right.fill(-input);core.process(left.data(),right.data(),n);frames-=n;}
            };
            core.apply(LooperAction::Record);render(32,.125);core.apply(LooperAction::Play);CHECK(wait());
            render(int(std::ceil(rate*.01/32))*32,0);
            core.apply(LooperAction::Overdub);render(32,.0625);core.apply(LooperAction::Play);CHECK(wait());
            core.apply(LooperAction::Undo);CHECK(core.canRedo());
            if(fromOverdub){core.apply(LooperAction::Overdub);render(32,.03125);}
            const auto firstAt=core.samplePosition();
            const auto allocations=luthier::tests::allocationsOnThisThread();
            const auto first=pedal.down(core.state(),firstAt,rate,150.125);apply(first);
            const auto duplicate=pedal.down(core.state(),firstAt,rate,150.125);apply(duplicate);pedal.up();
            const bool firstAllocated=luthier::tests::allocationsOnThisThread()!=allocations;
            CHECK(!firstAllocated);CHECK(first.count==1);CHECK(duplicate.count==0);
            CHECK(first.actions[0]==(fromOverdub?LooperAction::Play:LooperAction::Overdub));
            // Independent rational conversion:150.125ms =1201/8ms.
            const int elapsed=int(int64_t(rate)*1201/8000)+boundary;
            render(elapsed,1./1048576);
            const auto secondAt=core.samplePosition();CHECK(secondAt-firstAt==elapsed);
            const auto before=luthier::tests::allocationsOnThisThread();
            const auto second=pedal.down(core.state(),secondAt,rate,150.125);apply(second);
            const bool secondAllocated=luthier::tests::allocationsOnThisThread()!=before;
            CHECK(!secondAllocated);
            if(boundary<=0)
            {
                CHECK(second.count==(fromOverdub?1:2));
                CHECK(second.actions[size_t(std::clamp(second.count-1,0,1))]==LooperAction::Stop);
                if(!fromOverdub)CHECK(second.actions[0]==LooperAction::CancelOverdub);
                CHECK(core.state()==LooperCore::State::Stopped);
            }
            else
            {
                CHECK(second.count==1);CHECK(second.actions[0]==(fromOverdub?LooperAction::Overdub:LooperAction::Play));
                CHECK(core.state()==(fromOverdub?LooperCore::State::Overdubbing:LooperCore::State::Playing));
            }
            // All following transport actions are outside this gesture's ownership.
            pedal.invalidateCandidate();pedal.up();
            if(core.state()==LooperCore::State::Overdubbing)core.apply(LooperAction::CancelOverdub);
            CHECK(wait());core.apply(LooperAction::Stop);core.apply(LooperAction::Play);
            render(int(std::ceil(rate*.01/32))*32,0);
            std::array<double,32> left{},right{};core.process(left.data(),right.data(),32,1,0);
            bool exact=true;
            for(int i=0;i<32;++i)
            {
                const double expected=fromOverdub?.15625:boundary<=0?.125
                    :.125+double(elapsed/32+(i<elapsed%32?1:0))/1048576;
                exact &= left[i]==expected && right[i]==-expected;
            }
            CHECK_MSG(exact,juce::String(rate)+" block"+juce::String(block)+" boundary"+juce::String(boundary)+" audio/history changed");
            CHECK(core.canRedo()==(!fromOverdub && boundary<=0));
            if(!fromOverdub && boundary<=0)
            {
                core.apply(LooperAction::Redo);CHECK(wait());left.fill(0);right.fill(0);core.process(left.data(),right.data(),32,1,0);
                bool restored=true;for(int i=0;i<32;++i)restored &= left[i]==.1875 && right[i]==-.1875;
                CHECK(restored);
            }
        };
        exercise(ram,[]{return true;});exercise(paged,[&]{return doubleReady(paged);});
    }
}

LUTHIER_TEST(RefPedalRc5Double, zeroIntervalExpiryExternalInvalidationAndClockBoundsAreExplicit)
{
    using State=LooperCore::State;Rc5PressCycle pedal;
    CHECK(pedal.down(State::Playing,0,48000,400).actions[0]==LooperAction::Overdub);pedal.up();
    auto second=pedal.down(State::Overdubbing,0,48000,400);CHECK(second.count==2);
    CHECK(second.actions[0]==LooperAction::CancelOverdub);CHECK(second.actions[1]==LooperAction::Stop);pedal.up();
    CHECK(pedal.down(State::Stopped,1,48000,400).actions[0]==LooperAction::Play);pedal.up();
    CHECK(pedal.down(State::Playing,2,48000,400).actions[0]==LooperAction::Overdub);
    pedal.invalidateCandidate(); // preserves held through external media/config action
    CHECK(pedal.down(State::Overdubbing,3,48000,400).count==0);pedal.up();
    CHECK(pedal.down(State::Overdubbing,4,48000,400).actions[0]==LooperAction::Play);pedal.up();
    // Failed first Overdub is stillPlaying: Stop must not cancel some other take.
    pedal.reset();pedal.down(State::Playing,0,48000,400);pedal.up();
    auto failed=pedal.down(State::Playing,1,48000,400);CHECK(failed.count==1);CHECK(failed.actions[0]==LooperAction::Stop);
    // Configuration change cannot extend the old deadline.
    pedal.reset();pedal.down(State::Playing,0,48000,400);pedal.up();
    auto changed=pedal.down(State::Overdubbing,19201,48000,600);CHECK(changed.count==1);CHECK(changed.actions[0]==LooperAction::Play);
    // A threshold change during a duplicate held-down notification disarms
    // the old pair without generating another action or releasing the guard.
    pedal.reset();pedal.down(State::Playing,0,48000,400);
    CHECK(pedal.down(State::Overdubbing,1,48000,600).count==0);pedal.up();
    auto heldChange=pedal.down(State::Overdubbing,2,48000,600);
    CHECK(heldChange.count==1);CHECK(heldChange.actions[0]==LooperAction::Play);
    // A backwards clock rejects/disarms; intentional clock resets use reset().
    pedal.reset();pedal.down(State::Playing,100,48000,400);pedal.up();
    CHECK(pedal.down(State::Overdubbing,99,48000,400).count==0);
    CHECK(pedal.down(State::Overdubbing,101,48000,400).actions[0]==LooperAction::Play);
    for(auto invalid: {std::pair<double,double>{0,400},{48000,149},{48000,601},
            {std::numeric_limits<double>::quiet_NaN(),400},{48000,std::numeric_limits<double>::infinity()}})
    {pedal.reset();CHECK(pedal.down(State::Playing,0,invalid.first,invalid.second).count==0);}
    pedal.reset();CHECK(pedal.down(State::Playing,-1,48000,400).count==0);
    const auto maximum=std::numeric_limits<int64_t>::max();pedal.reset();
    pedal.down(State::Playing,maximum-19200,48000,400);pedal.up();
    auto last=pedal.down(State::Overdubbing,maximum,48000,400);CHECK(last.count==2);CHECK(last.actions[1]==LooperAction::Stop);
}
