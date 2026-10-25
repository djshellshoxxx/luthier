// OWNER: WP15
#include "TestFramework.h"
#include "../ReferencePedals/Models/tc_ditto.h"
#include "../ReferencePedals/Services/AssetStore.h"
#include "../ReferencePedals/Common/LooperStateCodec.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <atomic>
#include <chrono>
#include <thread>
using namespace luthier::reference;
namespace
{
struct DittoFixture
{
    juce::File path=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ditto-model-"+juce::Uuid().toString());
    AssetStore assets{path};
    ~DittoFixture(){path.deleteRecursively();}
    std::unique_ptr<IPedal> create(double rate=1000,int depth=1)
    {
        auto pedal=models::makeTcDitto();PedalServices services;services.assets=&assets;services.sampleRate=rate;services.maxBlock=64;pedal->setServices(services);
        pedal->setParam(4,float(depth-1)/7);pedal->prepare(rate,64,0);return pedal;
    }
};
void press(IPedal& p){FootEvent e;e.press=true;p.onFoot(e);}
void release(IPedal& p){FootEvent e;e.press=false;p.onFoot(e);}
void render(IPedal& p,int n,double left=0,double right=0,std::vector<double>* out=nullptr)
{
    std::vector<double> l(size_t(n),left),r(size_t(n),right);for(int at=0;at<n;at+=64)p.process(l.data()+at,r.data()+at,std::min(64,n-at));if(out)*out=std::move(l);
}
void record(IPedal& p,double l=.25,double r=.25,int frames=1000)
{
    press(p);render(p,10,l,r);release(p);render(p,frames-10,l,r);press(p);release(p);
}
void overdub(IPedal& p,double l,double r,int frames=1000)
{
    render(p,400);press(p);render(p,10,l,r);release(p);render(p,frames-10,l,r);press(p);release(p);
}
bool saved(IPedal& p,juce::var& out,bool settled=false)
{
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do
    {
        p.process(nullptr,nullptr,0);p.saveExtra(out);
        if(out["looper"]["status"].toString()=="ready" && (!settled || out["looper"]["retired"].size()==0))return true;
        std::this_thread::yield();
    }while(std::chrono::steady_clock::now()<until);
    return false;
}
double gain(){return std::pow(10.,(-60.+66.*double(models::tcDittoDescription().params[0].defaultNormalised()))/20.);}
}
LUTHIER_TEST(RefPedalDitto, descriptorFactoryDefaultDepthAndLaneMappingAreReal)
{
    PedalRegistry::TestScope scope;CHECK(PedalRegistry::add(models::tcDittoDescription(),models::makeTcDitto,models::tcDittoPreparedMemory));const auto* desc=PedalRegistry::find("tc-ditto");CHECK(desc!=nullptr);CHECK(PedalRegistry::validate(*desc));CHECK(desc->numParams==12);CHECK(!desc->wantsRawMidi);CHECK(desc->maxLatencySamples==0);CHECK(desc->cpuUnits==300);CHECK(desc->cpuUnitsPerPercent==1000);
    const auto& lanes=PedalRegistry::laneMap("tc-ditto");const int expected[]{0,1,3,5,7};CHECK(lanes.numLanes==5);for(int i=0;i<5;++i)CHECK(lanes.laneToParam[i]==expected[i]);
    CHECK(desc->params[4].has(PF_Config));CHECK(desc->params[4].has(PF_Ext));CHECK(desc->params[4].defaultNormalised()==0);CHECK(desc->params[2].kind==ParamKind::Momentary);CHECK(!desc->params[2].has(PF_Engage));
    auto pedal=PedalRegistry::make("tc-ditto");CHECK(pedal!=nullptr);CHECK(!pedal->footControlsEngagement());CHECK(pedal->getParam(4)==0);
    const auto estimate=PedalRegistry::memoryRequirement("tc-ditto",48000,512,0,nullptr);CHECK(estimate.ownedBytes!=kUnknownPreparedMemoryBytes);CHECK(estimate.temporaryBytes==0);CHECK(models::tcDittoPreparedMemory(0,512,0,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}
LUTHIER_TEST(RefPedalDitto, nativeRecordIsImmediateMonoWetAndDryStaysBitExactStereo)
{
    DittoFixture f;auto p=f.create();CHECK(p->getPreparedMemoryBytes()==models::tcDittoPreparedMemory(1000,64,0,nullptr).ownedBytes);press(*p);std::array<double,10> l{},r{};l.fill(.25);r.fill(-.5);p->process(l.data(),r.data(),10);release(*p);
    for(size_t i=0;i<10;++i){CHECK(l[i]==.25);CHECK(r[i]==-.5);}render(*p,990,.25,-.5);press(*p);release(*p);
    CHECK(p->readUiMeters().loopLength==1000);CHECK(p->readUiMeters().transport==2);CHECK(p->getLatencySamples()==0);CHECK(!p->outputsDualMono());
    juce::var state;CHECK(saved(*p,state));render(*p,400);l.fill(0);r.fill(0);p->process(l.data(),r.data(),10);for(size_t i=0;i<10;++i){CHECK_NEAR(l[i],-.125*gain(),1e-12);CHECK_NEAR(r[i],l[i],0);}
    p->setParam(0,0);render(*p,1000);l.fill(.125);r.fill(-.25);p->process(l.data(),r.data(),10);for(size_t i=0;i<10;++i){CHECK_NEAR(l[i],.125,1e-12);CHECK_NEAR(r[i],-.25,1e-12);}
}
LUTHIER_TEST(RefPedalDitto, doubleTapCancelsSpeculativeAudioAndDoubleHoldErases)
{
    DittoFixture f;auto p=f.create();record(*p);juce::var state;CHECK(saved(*p,state));render(*p,400);
    press(*p);render(*p,100,.5,.5);release(*p);render(*p,100);press(*p);release(*p);CHECK(p->readUiMeters().transport==4);render(*p,400);
    press(*p);release(*p);std::vector<double> l;render(*p,1000,0,0,&l);for(size_t i=5;i<l.size();++i)CHECK_NEAR(l[i],.25*gain(),1e-12);
    render(*p,400);press(*p);render(*p,10);release(*p);render(*p,50);press(*p);render(*p,1001);CHECK(p->readUiMeters().transport==0);CHECK(p->readUiMeters().loopLength==0);
    p->saveExtra(state);CHECK(state["looper"]["status"].toString()=="empty");CHECK(state["looper"]["base"].isVoid());
}
LUTHIER_TEST(RefPedalDitto, physicalHoldUndoRedoSurvivesAssetSaveAndConfigDepthRebuild)
{
    DittoFixture f;auto p=f.create();record(*p);juce::var state;CHECK(saved(*p,state));overdub(*p,.125,.125);CHECK(saved(*p,state));render(*p,400);
    press(*p);render(*p,1500,.5,.5);release(*p);std::vector<double> l;render(*p,1000,0,0,&l);for(double x:l)CHECK_NEAR(x,.25*gain(),1e-12);
    CHECK(saved(*p,state));CHECK(state["looper"]["redo"].size()==1);auto vendor=new juce::DynamicObject();vendor->setProperty("tiny",1e-200);state["looper"].getDynamicObject()->setProperty("vendor",juce::var(vendor));
    auto wider=f.create(1000,8);wider->loadExtra(state);CHECK(wider->readUiMeters().transport==2);press(*wider);render(*wider,1500);release(*wider);render(*wider,1000,0,0,&l);for(double x:l)CHECK_NEAR(x,.375*gain(),1e-12);
    for(int pass=0;pass<7;++pass){overdub(*wider,double(pass+1)*.00390625,double(pass+1)*.00390625);CHECK(saved(*wider,state));}
    render(*wider,400);press(*wider);render(*wider,1500);release(*wider);CHECK(saved(*wider,state));CHECK(state["looper"]["redo"].size()==1);
    auto narrow=f.create(1000,1);narrow->loadExtra(state);CHECK(saved(*narrow,state,true));CHECK(state["looper"]["layers"].size()==0);CHECK(state["looper"]["redo"].size()==1);CHECK(double(state["looper"]["vendor"]["tiny"])==1e-200);
    render(*narrow,1000,0,0,&l);for(double x:l)CHECK_NEAR(x,.45703125*gain(),1e-12);
    press(*narrow);render(*narrow,1500);release(*narrow);render(*narrow,1000,0,0,&l);for(double x:l)CHECK_NEAR(x,.484375*gain(),1e-12);
}
LUTHIER_TEST(RefPedalDitto, quantizedBarLengthAndWetConverterDelayKeepZeroLatencyDry)
{
    DittoFixture f;auto p=f.create();p->setParam(8,1);TimeInfo time;time.valid=true;time.bpm=120;time.ppq=.25;p->setTime(time);
    press(*p);release(*p);render(*p,1875,.25,.25);CHECK(p->readUiMeters().transport==0);render(*p,1125,.25,.25);press(*p);release(*p);render(*p,876,.25,.25);
    CHECK(p->readUiMeters().loopLength==2000);CHECK(p->readUiMeters().transport==2);
    auto delayed=models::makeTcDitto();PedalServices services;services.assets=&f.assets;delayed->setServices(services);delayed->setParam(10,3.f/256);delayed->prepare(1000,64,0);
    record(*delayed);juce::var state;CHECK(saved(*delayed,state));std::vector<double> initial;render(*delayed,8,0,0,&initial);CHECK(initial[0]==0);CHECK(initial[1]==0);CHECK(initial[2]==0);CHECK(initial[3]>0);render(*delayed,400);std::array<double,8> l{},r{};l.fill(.125);r.fill(-.5);delayed->process(l.data(),r.data(),8);CHECK_NEAR(l[0],.125+.25*gain(),1e-12);CHECK_NEAR(r[0],-.5+.25*gain(),1e-12);l.fill(.125);r.fill(-.5);delayed->setEnabled(false);delayed->process(l.data(),r.data(),8);for(size_t i=0;i<8;++i){CHECK(l[i]==.125);CHECK(r[i]==-.5);}CHECK(delayed->getLatencySamples()==0);
}
LUTHIER_TEST(RefPedalDitto, forwardLooperStateStaysOpaqueAndMissingAssetsRejectRestore)
{
    DittoFixture f;auto p=f.create();record(*p);juce::var state;CHECK(saved(*p,state));auto future=state.clone();future["looper"].getDynamicObject()->setProperty("schema",2);future["looper"].getDynamicObject()->setProperty("base",juce::var(new juce::DynamicObject()));
    auto opaque=f.create();opaque->loadExtra(future);std::array<double,8> l{},r{};l.fill(.125);r.fill(-.25);opaque->process(l.data(),r.data(),8);for(size_t i=0;i<8;++i){CHECK(l[i]==.125);CHECK(r[i]==-.25);}CHECK((opaque->readUiMeters().faults&common::LooperStorage::invalidAsset)!=0);
    juce::var after;opaque->saveExtra(after);CHECK(after["looper"]["schema"]==juce::var(2));CHECK(after["looper"]["base"].getDynamicObject()!=nullptr);CHECK(after.getDynamicObject()->getProperties().size()==1);
    auto missing=state.clone();missing["looper"].getDynamicObject()->setProperty("base","sha256:"+juce::String::repeatedString("1",64));auto fresh=f.create();bool rejected=false;try{fresh->loadExtra(missing);}catch(const std::runtime_error&){rejected=true;}CHECK(rejected);
}
#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests{long allocationsOnThisThread() noexcept;}
LUTHIER_TEST(RefPedalDitto, callbackFootAndMetersDoNotAllocate)
{
    DittoFixture f;auto p=f.create();std::array<double,64> l{},r{};
    const auto before=luthier::tests::allocationsOnThisThread();press(*p);p->process(l.data(),r.data(),64);release(*p);p->process(l.data(),r.data(),64);p->readUiMeters();p->setParam(0,.5f);p->setParam(3,.5f);p->setEnabled(false);p->process(l.data(),r.data(),64);p->setEnabled(true);p->reset();
    CHECK(luthier::tests::allocationsOnThisThread()-before==0);
}
#endif

LUTHIER_TEST(RefPedalDitto, unavailableReturnTapNeverSubstitutesOrdinaryInputForRecording)
{
    DittoFixture f;auto p=f.create();p->setParam(11,1);press(*p);render(*p,10,.25,-.5);release(*p);render(*p,990,.25,-.5);
    CHECK(p->readUiMeters().transport==0);CHECK(p->readUiMeters().loopLength==0);CHECK((p->readUiMeters().faults&(1u<<20))!=0);
    p->setParam(11,0);render(*p,400);record(*p);juce::var state;CHECK(saved(*p,state));render(*p,400);p->setParam(11,1);press(*p);release(*p);CHECK(p->readUiMeters().transport==2);CHECK(p->readUiMeters().loopLength==1000);
}

LUTHIER_TEST(RefPedalDitto, thousandAcceptedOverdubsKeepAnalyticalSumAndUndoRedoBytes)
{
    DittoFixture f;auto pedal=f.create();record(*pedal,.125,.125);juce::var state;CHECK(saved(*pedal,state,true));
    constexpr double layer=1./4096.;
    for(int cycle=0;cycle<1000;++cycle)
    {
        overdub(*pedal,layer,layer);CHECK(saved(*pedal,state,true));
        CHECK(pedal->readUiMeters().faults==0);CHECK(pedal->readUiMeters().loopLength==1000);
    }
    std::vector<double> audio;render(*pedal,1000,0,0,&audio);const double expected=(.125+1000*layer)*gain();
    for(double sample:audio)CHECK_NEAR(sample,expected,1e-12);
    render(*pedal,400);press(*pedal);render(*pedal,1500);release(*pedal);render(*pedal,1000,0,0,&audio);
    const double beforeLast=(.125+999*layer)*gain();for(double sample:audio)CHECK_NEAR(sample,beforeLast,1e-12);
    press(*pedal);render(*pedal,1500);release(*pedal);render(*pedal,1000,0,0,&audio);for(double sample:audio)CHECK_NEAR(sample,expected,1e-12);
    CHECK(pedal->readUiMeters().faults==0);
}
