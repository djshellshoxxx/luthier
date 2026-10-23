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
