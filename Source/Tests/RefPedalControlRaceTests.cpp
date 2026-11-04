// OWNER: WP2
#include "TestFramework.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../Support/ThreadProbe.h"
#include <atomic>
#include <thread>
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
using namespace luthier::reference;
LUTHIER_TEST(RefPedalControlRace, publisherReclaimerAndAudioHaveDistinctThreads)
{
    BoardModel model;BoardEngine engine(model);CHECK(engine.setAssetStore(nullptr));
    CHECK(model.apply(cmd::Add{"generic.boost"}));CHECK(model.apply(cmd::SetBoardEnabled{true}));
    std::atomic<bool> start{false},done{false};std::atomic<int> invalid{0},audioLocks{0},audioBlocks{0};std::atomic<long> audioAllocations{0};
    std::thread audio([&]
    {
        double l[64]{},r[64]{};while(!start.load())std::this_thread::yield();
        luthier::ThreadProbe::markAsAudioThread();
        const auto allocationsBefore=luthier::tests::allocationsOnThisThread();
        const auto locksBefore=luthier::ThreadProbe::audioThreadLocks.load();
        while(!done.load())
        {
            engine.process(l,r,64,Placement::beforeExistingPre);
            ++audioBlocks;
            for(int i=0;i<64;++i)if(!std::isfinite(l[i])||!std::isfinite(r[i]))++invalid;
        }
        audioAllocations.store(luthier::tests::allocationsOnThisThread()-allocationsBefore);
        audioLocks.store(luthier::ThreadProbe::audioThreadLocks.load()-locksBefore);
        luthier::ThreadProbe::markAsAudioThread(false);
    });
    std::thread reclaimer([&]{while(!start.load())std::this_thread::yield();while(!done.load())engine.reclaim();});
    std::thread capture([&]{while(!start.load())std::this_thread::yield();while(!done.load()){std::vector<PedalInstance> instances;BoardSettings settings;model.snapshot(instances,settings);}});
    start.store(true);
    for(int i=0;i<1000;++i){juce::String error;const bool accepted=model.apply(cmd::SetMix{float(i%100)/100,0},&error);CHECK_MSG(accepted||error.containsIgnoreCase("saturated"),error);}
    done.store(true);audio.join();reclaimer.join();capture.join();CHECK(invalid.load()==0);
    CHECK(audioBlocks.load()>0);
    CHECK_MSG(audioAllocations.load()==0,juce::String("audio allocations: ")+juce::String((int)audioAllocations.load()));
    CHECK_MSG(audioLocks.load()==0,juce::String("audio blocking locks: ")+juce::String(audioLocks.load()));
    engine.reclaim();CHECK(engine.retiredPlanHighWater()<=8);
}

#include "../ReferencePedals/PreparedMemory.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <functional>
#include <stdexcept>
namespace
{
std::function<void()> duringExtraSave;
int extraPinDestroyed=0,extraPinSaved=0;
bool repeatExtraSaveCallback=false;
const ParamDesc extraPinParams[]{{"test_controlpin_mode","Mode","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Config,0,-1}};
const PedalDesc extraPinDesc{"test.controlpin","test_controlpin_",1,"Control pin","Test",nullptr,
    ChannelMode::Stereo,BypassMode::True,1,extraPinParams,0,false,false,false,0};
class ExtraPinPedal final:public IPedal
{
public:
    ~ExtraPinPedal()override{++extraPinDestroyed;}
    const PedalDesc& desc()const noexcept override{return extraPinDesc;}
    void prepare(double,int,int)override{}void reset()noexcept override{}
    void setParam(int,float)noexcept override{}float getParam(int)const noexcept override{return 0;}
    void process(double*,double*,int)noexcept override{}
    int getLatencySamples()const noexcept override{return 0;}
    uint64_t getPreparedMemoryBytes()const noexcept override{return sizeof(*this);}
    void saveExtra(juce::var& output)const override
    {
        auto callback=repeatExtraSaveCallback?duringExtraSave:std::move(duringExtraSave);if(callback)callback();
        ++extraPinSaved;auto* object=new juce::DynamicObject;object->setProperty("marker",marker);output=juce::var(object);
    }
private:int marker=73;
};
std::unique_ptr<IPedal> makeExtraPin(){return std::make_unique<ExtraPinPedal>();}
}
LUTHIER_TEST(RefPedalControlRace, runtimePinSurvivesReentrantRemovalAndReclaimOutsideControlGuard)
{
    PedalRegistry::TestScope scope;extraPinDestroyed=extraPinSaved=0;duringExtraSave={};repeatExtraSaveCallback=false;
    juce::String registrationReason;CHECK_MSG(PedalRegistry::validate(extraPinDesc,&registrationReason),registrationReason);
    CHECK(PedalRegistry::add(extraPinDesc,makeExtraPin,&fixedPreparedMemory<ExtraPinPedal>));
    BoardModel model;BoardEngine engine(model);CHECK(engine.setAssetStore(nullptr));
    CHECK(model.apply(cmd::Add{extraPinDesc.modelId}));const auto order=model.getOrder();CHECK(order.size()==1);if(order.empty())return;
    duringExtraSave=[&]{CHECK(model.apply(cmd::Remove{order.front()}));engine.reclaim();CHECK(extraPinDestroyed==0);};
    std::vector<PedalInstance> captured;BoardSettings settings;model.snapshot(captured,settings);
    CHECK(captured.size()==1);if(!captured.empty())CHECK(static_cast<int>(captured.front().extras["marker"])==73);
    CHECK(extraPinSaved==1);CHECK(extraPinDestroyed==1);CHECK(model.getOrder().empty());duringExtraSave={};
}

LUTHIER_TEST(RefPedalControlRace, configRebuildAllowsSaveExtraToReclaimEngine)
{
    PedalRegistry::TestScope scope;extraPinDestroyed=extraPinSaved=0;duringExtraSave={};
    juce::String registrationReason;CHECK_MSG(PedalRegistry::validate(extraPinDesc,&registrationReason),registrationReason);
    CHECK(PedalRegistry::add(extraPinDesc,makeExtraPin,&fixedPreparedMemory<ExtraPinPedal>));
    BoardModel model;BoardEngine engine(model);CHECK(engine.setAssetStore(nullptr));
    CHECK(model.apply(cmd::Add{extraPinDesc.modelId}));const auto order=model.getOrder();CHECK(order.size()==1);if(order.empty())return;
    duringExtraSave=[&]{(void)engine.retiredPlanCount();engine.reclaim();};
    CHECK(model.apply(cmd::SetParam{order.front(),0,1.f}));
    CHECK(extraPinSaved==1);CHECK(engine.retiredPlanCount()<=8);duringExtraSave={};
}

LUTHIER_TEST(RefPedalControlRace, snapshotRestoreAllowsSaveExtraToReclaimEngine)
{
    PedalRegistry::TestScope scope;extraPinDestroyed=extraPinSaved=0;duringExtraSave={};repeatExtraSaveCallback=false;
    CHECK(PedalRegistry::add(extraPinDesc,makeExtraPin,&fixedPreparedMemory<ExtraPinPedal>));
    BoardModel model;BoardEngine engine(model);CHECK(engine.setAssetStore(nullptr));
    CHECK(model.apply(cmd::Add{extraPinDesc.modelId}));const auto order=model.getOrder();CHECK(order.size()==1);if(order.empty())return;
    CHECK(model.apply(cmd::SetScene{0,{}}));CHECK(model.apply(cmd::SetParam{order.front(),0,1.f}));
    extraPinSaved=0;int callbackCalls=0;
    duringExtraSave=[&]{if(++callbackCalls==2){(void)engine.retiredPlanCount();engine.reclaim();}};
    repeatExtraSaveCallback=true;
    CHECK(model.apply(cmd::ApplySnapshot{0,{}}));
    CHECK_MSG(extraPinSaved==2,juce::String("saveExtra calls during capture+restore: ")+juce::String(extraPinSaved));
    CHECK(callbackCalls==2);CHECK(engine.retiredPlanCount()<=8);duringExtraSave={};repeatExtraSaveCallback=false;
}

LUTHIER_TEST(RefPedalControlRace, saveExtraExceptionReacquiresGuardAndAllowsRetry)
{
    PedalRegistry::TestScope scope;extraPinDestroyed=extraPinSaved=0;duringExtraSave={};repeatExtraSaveCallback=false;
    CHECK(PedalRegistry::add(extraPinDesc,makeExtraPin,&fixedPreparedMemory<ExtraPinPedal>));
    BoardModel model;BoardEngine engine(model);CHECK(engine.setAssetStore(nullptr));
    CHECK(model.apply(cmd::Add{extraPinDesc.modelId}));const auto order=model.getOrder();CHECK(order.size()==1);if(order.empty())return;
    duringExtraSave=[]{throw std::runtime_error("intentional saveExtra failure");};
    CHECK(!model.apply(cmd::SetParam{order.front(),0,1.f}));
    CHECK(model.apply(cmd::SetParam{order.front(),0,1.f}));CHECK(engine.retiredPlanCount()<=8);duringExtraSave={};
}
