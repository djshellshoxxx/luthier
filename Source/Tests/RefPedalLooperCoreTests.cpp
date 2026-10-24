#include "TestFramework.h"
#include "../ReferencePedals/Common/LooperCore.h"
#include <array>
using luthier::reference::common::LooperCore;
LUTHIER_TEST(RefPedalLooperCore, timestampedRecordAndDryThrough)
{
    LooperCore core;CHECK(core.prepare(48000,1,2,2000000));core.setSeamMilliseconds(0);
    CHECK(core.enqueue({LooperCore::Action::Record,3}));CHECK(core.enqueue({LooperCore::Action::Play,1003}));
    std::array<double,1010> l{},r{};l.fill(.25);r.fill(-.5);core.process(l.data(),r.data(),1010);
    CHECK(core.length()==1000);CHECK(core.state()==LooperCore::State::Playing);
    for(int i=0;i<1003;++i){CHECK_NEAR(l[i],.25,1e-15);CHECK_NEAR(r[i],-.5,1e-15);}
    core.apply(LooperCore::Action::Clear);double x=.1,y=-.2;core.process(&x,&y,1);CHECK_NEAR(x,.1,1e-15);CHECK_NEAR(y,-.2,1e-15);CHECK(core.length()==0);
}
LUTHIER_TEST(RefPedalLooperCore, layerUndoRedoAndCancelAreExact)
{
    LooperCore core;CHECK(core.prepare(48000,1,2,2000000));core.setSeamMilliseconds(0);
    core.apply(LooperCore::Action::Record);
    for(int i=0;i<1000;++i){double l=std::sin(i*.03)*.1,r=-l;core.process(&l,&r,1);}
    core.apply(LooperCore::Action::Play);
    for(int i=0;i<1000;++i){double l=0,r=0;core.process(&l,&r,1);}
    std::array<double,1000> baselineL{},baselineR{},dubL{},dubR{},undoL{},undoR{},redoL{},redoR{};
    core.process(baselineL.data(),baselineR.data(),1000);
    core.apply(LooperCore::Action::Overdub);
    for(int i=0;i<1000;++i){double l=.125,r=.25;core.process(&l,&r,1);}
    core.apply(LooperCore::Action::Play);core.process(dubL.data(),dubR.data(),1000);
    core.apply(LooperCore::Action::Undo);core.process(undoL.data(),undoR.data(),1000);
    CHECK(baselineL==undoL);CHECK(baselineR==undoR);
    core.apply(LooperCore::Action::Redo);core.process(redoL.data(),redoR.data(),1000);
    CHECK(dubL==redoL);CHECK(dubR==redoR);CHECK(dubL!=baselineL);
    core.apply(LooperCore::Action::Overdub);
    for(int i=0;i<1000;++i){double l=.5,r=-.5;core.process(&l,&r,1);}
    core.apply(LooperCore::Action::CancelOverdub);redoL.fill(0);redoR.fill(0);core.process(redoL.data(),redoR.data(),1000);
    CHECK(dubL==redoL);CHECK(dubR==redoR);
}
LUTHIER_TEST(RefPedalLooperCore, capacityBudgetAndQueueOverflow)
{
    LooperCore core;CHECK(core.prepare(192000,5400,8,240000));CHECK(core.memoryBytes()<=240000);
    core.apply(LooperCore::Action::Record);const size_t capacity=core.capacity();
    for(size_t i=0;i<capacity;++i){double l=.1,r=.1;core.process(&l,&r,1);}
    CHECK(core.length()==capacity);CHECK(core.state()==LooperCore::State::Playing);
    for(int i=0;i<64;++i)CHECK(core.enqueue({LooperCore::Action::Stop,100000}));
    CHECK(!core.enqueue({LooperCore::Action::Stop,100000}));CHECK(core.droppedCommands()==1);
}
LUTHIER_TEST(RefPedalLooperCore, unlimitedPartialOverdubsAndCancels)
{
    LooperCore core;CHECK(core.prepare(48000,.01,2,2000000));core.setSeamMilliseconds(0);
    core.apply(LooperCore::Action::Record);
    std::array<double,32> zero{};auto right=zero;
    core.process(zero.data(),right.data(),32);core.apply(LooperCore::Action::Play);
    for(int i=0;i<256;++i){double l=0,r=0;core.process(&l,&r,1,1,0);}
    std::array<double,32> expected{};size_t head=0;
    const auto bytes=core.memoryBytes();
    for(int cycle=0;cycle<300;++cycle)
    {
        core.apply(LooperCore::Action::Overdub);
        CHECK(core.state()==LooperCore::State::Overdubbing);
        auto delta=expected;delta.fill(0);
        const int n=1+(cycle*17)%65;
        for(int i=0;i<n;++i)
        {
            double l=.001,r=-.001;core.process(&l,&r,1,1,0);
            delta[head]+=.001;head=(head+1)%32;
        }
        if(cycle%3==0)core.apply(LooperCore::Action::CancelOverdub);
        else {core.apply(LooperCore::Action::Play);for(size_t i=0;i<32;++i)expected[i]+=delta[i];}
        for(int i=0;i<32;++i)
        {
            double l=0,r=0;core.process(&l,&r,1,1,0);
            CHECK_NEAR(l,expected[head],1e-12);CHECK_NEAR(r,-expected[head],1e-12);head=(head+1)%32;
        }
        CHECK(core.memoryBytes()==bytes);
    }
}
LUTHIER_TEST(RefPedalLooperCore, cancelAtCapacityRetainsUndoAndResetDoesNotResurrectAudio)
{
    LooperCore core;CHECK(core.prepare(48000,.01,1,2000000));core.setSeamMilliseconds(0);
    auto render=[&](int n,double input)
    {for(int i=0;i<n;++i){double l=input,r=input;core.process(&l,&r,1,1,0);}};
    core.apply(LooperCore::Action::Record);render(32,.125);core.apply(LooperCore::Action::Play);render(256,0);
    for(int pass=0;pass<50;++pass)
    {
        core.apply(LooperCore::Action::Overdub);render(32,.001);core.apply(LooperCore::Action::Play);
        CHECK(core.undoDepth()==1);
        core.apply(LooperCore::Action::Overdub);render(7,.5);core.apply(LooperCore::Action::CancelOverdub);
        CHECK(core.undoDepth()==1);
        double before=0,r=0;core.process(&before,&r,1,1,0);
        core.apply(LooperCore::Action::Undo);double undone=0;core.process(&undone,&r,1,1,0);
        CHECK_NEAR(before-undone,.001,1e-12);
        core.apply(LooperCore::Action::Redo);render(32,0);
    }
    for(int i=0;i<1000;++i)
    {
        core.apply(LooperCore::Action::Overdub);core.apply(LooperCore::Action::CancelOverdub);
        CHECK(!core.layerLimitReached());
    }
    core.reset();core.apply(LooperCore::Action::Record);render(32,0);core.apply(LooperCore::Action::Play);render(256,0);
    for(int i=0;i<10;++i){core.apply(LooperCore::Action::Overdub);render(1,0);core.apply(LooperCore::Action::Play);}
    double silent=0,r=0;core.process(&silent,&r,1);CHECK(silent==0);CHECK(r==0);
    CHECK(!core.prepare(48000,1,8,0));CHECK(core.memoryBytes()==0);
}
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(RefPedalLooperCore, unlimitedCompactionNeverAllocatesInAudio)
{
    LooperCore core;CHECK(core.prepare(48000,.01,2,2000000));core.setSeamMilliseconds(0);
    std::array<double,64> l{},r{};
    const auto before=luthier::tests::allocationsOnThisThread();
    core.apply(LooperCore::Action::Record);core.process(l.data(),r.data(),64);core.apply(LooperCore::Action::Play);
    for(int i=0;i<100;++i)
    {
        core.apply(LooperCore::Action::Overdub);core.process(l.data(),r.data(),37);
        core.apply(i%3 ? LooperCore::Action::Play : LooperCore::Action::CancelOverdub);
        core.apply(LooperCore::Action::Undo);core.apply(LooperCore::Action::Redo);
    }
    core.apply(LooperCore::Action::Clear);core.reset();
    const auto count=luthier::tests::allocationsOnThisThread()-before;CHECK(count==0);
}
#endif
