#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include "../ReferencePedals/PedalPool.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <limits>
using namespace luthier::reference;
using models::MxrPhase90Script;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(RefPedalPhase90Memory, inlineOnlyModelReportsItsPreparedFootprint)
{
    const auto allocations=luthier::tests::allocationsOnThisThread();
    MxrPhase90Script pedal;pedal.prepare(48000,2048,1);
    const auto report=pedal.getPreparedMemoryBytes();
    CHECK(luthier::tests::allocationsOnThisThread()==allocations);
    CHECK(report!=kUnknownPreparedMemoryBytes);CHECK(report==sizeof(pedal));
}
LUTHIER_TEST(RefPedalPhase90Memory, constructorFreeBoundMatchesEveryInlineConfiguration)
{
    for(double rate:{8000.,44100.,44100.1,48000.,88200.,96000.,192000.,192000.125,1000000.})
        for(int policy:{0,1,2})for(int controls:{0,1,2})
    {
        const auto allocations=luthier::tests::allocationsOnThisThread();
        const auto bound=MxrPhase90Script::prepareMemory(rate,2048,policy,nullptr);
        MxrPhase90Script pedal;
        if(controls)for(int p=0;p<pedal.desc().numParams;++p)pedal.setParam(p,float(controls-1));
        pedal.prepare(rate,2048,policy);pedal.reset();
        const auto report=pedal.getPreparedMemoryBytes();
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        CHECK(bound.ownedBytes==sizeof(pedal));CHECK(bound.temporaryBytes==0);CHECK(report==bound.ownedBytes);
        CHECK(MxrPhase90Script::prepareMemory(rate,1,policy,nullptr).ownedBytes==report);
    }
}
LUTHIER_TEST(RefPedalPhase90Memory, unsupportedInputsCannotReserve)
{
    for(double rate:{-1.,0.,7999.,1000001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        CHECK(MxrPhase90Script::prepareMemory(rate,2048,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int block:{-1,0})CHECK(MxrPhase90Script::prepareMemory(48000,block,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int policy:{-1,3})CHECK(MxrPhase90Script::prepareMemory(48000,2048,policy,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}
namespace
{
int phaseFactories=0;
std::unique_ptr<IPedal> memoryPhase90(){++phaseFactories;return std::make_unique<MxrPhase90Script>();}
}
LUTHIER_TEST(RefPedalPhase90Memory, testOnlyPoolAdmissionTracksInlineAndDryStorage)
{
    PedalRegistry::TestScope scope;
    CHECK(PedalRegistry::add(MxrPhase90Script::descriptor(),memoryPhase90,&MxrPhase90Script::prepareMemory));
    PedalPool pool;CHECK(pool.setAssetStore(nullptr));pool.prepare(192000,2048);
    PedalInstance instance;instance.modelId=MxrPhase90Script::descriptor().modelId;
    for(int p=0;p<MxrPhase90Script::descriptor().numParams;++p)instance.params[p]=float(MxrPhase90Script::descriptor().params[p].defaultNormalised());
    phaseFactories=0;juce::String error;CHECK(pool.preflightFresh({instance},error));CHECK(phaseFactories==0);
    auto runtime=pool.acquire(instance,error);CHECK_MSG(runtime!=nullptr,error);if(!runtime)return;
    const auto actual=sizeof(PedalRuntime)+sizeof(MxrPhase90Script)
        +runtime->dry[0].preparedMemoryBytes()+runtime->dry[1].preparedMemoryBytes();
    CHECK(pool.preparedBytes()==actual);CHECK(phaseFactories==1);
    auto shared=pool.acquire(instance,error,runtime.get());CHECK(shared==runtime);CHECK(phaseFactories==1);
    auto changed=instance;
    for(int p=0;p<MxrPhase90Script::descriptor().numParams;++p)if(MxrPhase90Script::descriptor().params[p].has(PF_Config))
    {changed.params[p]=changed.params[p]<.5f?1.f:0.f;break;}
    auto replacement=pool.acquire(changed,error,runtime.get());
    CHECK_MSG(replacement!=nullptr,error);if(!replacement)return;
    CHECK(replacement!=runtime);CHECK(phaseFactories==2);CHECK(pool.preparedBytes()==actual*2);
    replacement.reset();CHECK(pool.preparedBytes()==actual);
    runtime.reset();CHECK(pool.preparedBytes()==actual);shared.reset();CHECK(pool.preparedBytes()==0);
    pool.prepare(7999,2048);CHECK(!pool.acquire(instance,error));CHECK(phaseFactories==2);
}
