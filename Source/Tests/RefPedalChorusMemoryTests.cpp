#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include "../ReferencePedals/PedalPool.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <limits>
using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace luthier::reference::models
{
struct ChorusMemoryTestAccess
{
    template<class P> static uint64_t actual(const P& pedal) noexcept
    {
        uint64_t bytes=sizeof(pedal);
        for(const auto& voice:pedal.wet)bytes+=voice.ownedMemoryBytes();
        return bytes;
    }
};
}
namespace
{
template<class P> void knownActualCapacity(luthier::tests::TestContext& ctx,int voices,double delayMs)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        P pedal;pedal.prepare(rate,2048,2);
        const auto before=luthier::tests::allocationsOnThisThread();
        const auto bytes=pedal.getPreparedMemoryBytes();
        CHECK(luthier::tests::allocationsOnThisThread()==before);
        CHECK_MSG(bytes!=kUnknownPreparedMemoryBytes,"Prepared chorus memory unknown at "+juce::String(rate));
        const uint64_t payload=voices*sizeof(double)*(uint64_t(rate*delayMs*.001)+24);
        if(bytes!=kUnknownPreparedMemoryBytes)CHECK(bytes>=sizeof(P)+payload);
    }
}
namespace
{
template<class P> void freshAndRetained(luthier::tests::TestContext& ctx)
{
    for(double rate:{8000.,44100.,44100.1,48000.,88200.,96000.,192000.,192000.125,1000000.})
        for(int policy:{0,1,2}) for(bool maximums:{false,true})
    {
        const auto before=luthier::tests::allocationsOnThisThread();
        const auto bound=P::prepareMemory(rate,2048,policy,nullptr);
        CHECK(luthier::tests::allocationsOnThisThread()==before);
        CHECK(bound.ownedBytes!=kUnknownPreparedMemoryBytes);CHECK(bound.temporaryBytes==0);
        P pedal;
        if(maximums)for(int i=0;i<pedal.desc().numParams;++i)pedal.setParam(i,1);
        pedal.prepare(rate,2048,policy);
        const auto report=pedal.getPreparedMemoryBytes();
        CHECK(report==models::ChorusMemoryTestAccess::actual(pedal));
        CHECK(report>=sizeof(P) && report<=bound.ownedBytes);
        CHECK(P::prepareMemory(rate,1,policy,nullptr).ownedBytes==bound.ownedBytes);
        pedal.reset();CHECK(pedal.getPreparedMemoryBytes()==report);
    }
    P pedal;pedal.prepare(192000,2048,2);
    const auto high=pedal.getPreparedMemoryBytes();
    pedal.prepare(48000,1,0);
    const auto low=pedal.getPreparedMemoryBytes();
    CHECK(low==models::ChorusMemoryTestAccess::actual(pedal));
    CHECK(low>=sizeof(P));pedal.reset();CHECK(pedal.getPreparedMemoryBytes()==low);
    std::cout<<"  "<<P::descriptor().modelId<<" owned bytes high="<<high<<" lower="<<low<<'\n';
}
template<class P> void invalidRequirements(luthier::tests::TestContext& ctx)
{
    for(double rate:{-1.,0.,7999.,1000001.,std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::max()})
        CHECK(P::prepareMemory(rate,512,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int block:{-1,0})CHECK(P::prepareMemory(48000,block,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int policy:{-1,3})CHECK(P::prepareMemory(48000,512,policy,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}
int memoryFactories=0;
template<class P> std::unique_ptr<IPedal> makeChorus()
{++memoryFactories;return std::make_unique<P>();}
template<class P> void poolAdmission(luthier::tests::TestContext& ctx)
{
    PedalRegistry::TestScope scope;
    CHECK(PedalRegistry::add(P::descriptor(),makeChorus<P>,&P::prepareMemory));
    PedalPool pool;CHECK(pool.setAssetStore(nullptr));pool.prepare(1000000,2048);
    const auto required=sizeof(PedalRuntime)+P::prepareMemory(1000000,2048,2,nullptr).ownedBytes+2*sizeof(double);
    const int fitting=int(kPreparedPedalMemoryLimit/required);
    std::vector<PedalInstance> instances;
    for(int i=0;i<=fitting;++i)
    {
        PedalInstance instance;instance.modelId=P::descriptor().modelId;instance.slot=i%kMaxSlots;
        for(int p=0;p<P::descriptor().numParams;++p)instance.params[p]=float(P::descriptor().params[p].defaultNormalised());
        instances.push_back(std::move(instance));
    }
    memoryFactories=0;juce::String error;
    CHECK(kPreparedPedalMemoryLimit==64000000);
    CHECK(!pool.preflightFresh(instances,error));CHECK(error.contains("memory"));
    CHECK(memoryFactories==0);CHECK(pool.preparedBytes()==0);
    const auto extra=instances.back();instances.pop_back();
    CHECK(pool.preflightFresh(instances,error));CHECK(memoryFactories==0);
    std::vector<std::shared_ptr<PedalRuntime>> retained;
    uint64_t actual=0;
    for(const auto& instance:instances)
    {
        auto runtime=pool.acquire(instance,error);CHECK_MSG(runtime!=nullptr,error);if(!runtime)return;
        actual+=sizeof(PedalRuntime)+runtime->pedal->getPreparedMemoryBytes()
            +runtime->dry[0].preparedMemoryBytes()+runtime->dry[1].preparedMemoryBytes();
        retained.push_back(std::move(runtime));
    }
    CHECK(memoryFactories==fitting);CHECK(actual==pool.preparedBytes());CHECK(actual<=kPreparedPedalMemoryLimit);
    auto shared=pool.acquire(instances.front(),error,retained.front().get());
    CHECK(shared==retained.front());CHECK(memoryFactories==fitting);CHECK(actual==pool.preparedBytes());
    CHECK(!pool.acquire(extra,error));CHECK(memoryFactories==fitting);CHECK(actual==pool.preparedBytes());
    const uint64_t pinned=sizeof(PedalRuntime)+shared->pedal->getPreparedMemoryBytes()
        +shared->dry[0].preparedMemoryBytes()+shared->dry[1].preparedMemoryBytes();
    retained.clear();CHECK(pool.preparedBytes()==pinned);shared.reset();CHECK(pool.preparedBytes()==0);
    pool.prepare(7999,512);CHECK(!pool.acquire(extra,error));CHECK(memoryFactories==fitting);
    std::cout<<"  "<<P::descriptor().modelId<<" admitted="<<fitting<<" retained bytes="<<actual<<'\n';
}
}
LUTHIER_TEST(RefPedalChorusMemory, freshBoundsAndReprepareCoverAllModeHistories)
{
    freshAndRetained<models::BossCe2>(ctx);freshAndRetained<models::BossCe2w>(ctx);
}
LUTHIER_TEST(RefPedalChorusMemory, invalidPreparationCannotReserve)
{
    invalidRequirements<models::BossCe2>(ctx);invalidRequirements<models::BossCe2w>(ctx);
}
LUTHIER_TEST(RefPedalChorusMemory, poolRejectsBeforeFactoryAndPreservesSharedPins)
{
    // Pool-only retained-runtime test across generations, not an overfull board.
    // Named models are registered only inside this isolated test scope.
    poolAdmission<models::BossCe2>(ctx);poolAdmission<models::BossCe2w>(ctx);
}
}
LUTHIER_TEST(RefPedalChorusMemory, preparedModelsHaveKnownCapacityReports)
{
    knownActualCapacity<models::BossCe2>(ctx,2,12);
    knownActualCapacity<models::BossCe2w>(ctx,3,40);
}
