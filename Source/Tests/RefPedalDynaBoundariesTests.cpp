#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_dynacomp.h"
#include <array>
#include <cstring>
#include <limits>
using namespace luthier::reference;
using models::MxrDynaComp;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(RefPedalDynaBoundaries, unsupportedPreparationInputsCannotReserve)
{
    for(double rate:{-1.,0.,7999.,192001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        CHECK(MxrDynaComp::prepareMemory(rate,2048,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int block:{-1,0})CHECK(MxrDynaComp::prepareMemory(48000,block,2,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
    for(int policy:{-1,3,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
        CHECK(MxrDynaComp::prepareMemory(48000,2048,policy,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}
LUTHIER_TEST(RefPedalDynaBoundaries, qualityLatencyAndMemoryAgreeAtAllFiveRates)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(int policy:{0,1,2})for(int quality=0;quality<3;++quality)
    {
        MxrDynaComp pedal;pedal.setParam(6,quality*.5f);pedal.prepare(rate,2048,policy);
        const auto before=luthier::tests::allocationsOnThisThread();
        const auto bound=MxrDynaComp::prepareMemory(rate,2048,policy,nullptr),small=MxrDynaComp::prepareMemory(rate,1,policy,nullptr);
        const auto bytes=pedal.getPreparedMemoryBytes();
        CHECK(luthier::tests::allocationsOnThisThread()==before);
        const int latency=quality==2?0:quality==1 && policy>0?23:15;
        CHECK(pedal.getLatencySamples()==latency);CHECK(latency<=pedal.desc().maxLatencySamples);
        CHECK(bytes>=sizeof(MxrDynaComp)+2*39*sizeof(double));CHECK(bytes<=bound.ownedBytes);
        CHECK(small.ownedBytes==bound.ownedBytes);CHECK(bound.temporaryBytes==0);
    }
}
LUTHIER_TEST(RefPedalDynaBoundaries, thirtySecondsPerRateOfDeterministicHostileInputAndControlProcessing)
{
    // 150seconds simulated stereo audio total; this is not a30minute soak or
    // real-time host/hardware acceptance. Fixed seed and frame counts reproduce
    // every nonfinite/denormal/hot input, control change and reset.
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrDynaComp blocked,sample;PedalServices services;services.instanceSeed=73;
        for(auto* p:{&blocked,&sample}){p->setServices(services);p->prepare(rate,2048,2);}
        std::array<double,2048> l{},r{},x{},y{};
        uint32_t rng=73;const auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
        const int blocks[]{1,4,16,64,512,2048};const int end=int(rate*30);
        int frame=0,block=0,resets=0;bool identical=true,finite=true,boundedReduction=true;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        while(frame<end)
        {
            if(block%31==0)
            {
                const int index=draw()%blocked.desc().numParams;const float value=float(draw()>>8)/16777215;
                for(auto* p:{&blocked,&sample})p->setParam(index,value);
            }
            if(block%997==0){++resets;for(auto* p:{&blocked,&sample})p->reset();}
            const int n=std::min(blocks[block%6],end-frame);
            for(int i=0;i<n;++i)
            {
                const int at=frame+i;
                double input=.3*std::sin(at*.13);
                switch(at%257)
                {
                    case 0:input=0;break;
                    case 1:input=std::numeric_limits<double>::denorm_min();break;
                    case 2:input=std::numeric_limits<double>::min();break;
                    case 3:input=std::numeric_limits<double>::quiet_NaN();break;
                    case 4:input=std::numeric_limits<double>::infinity();break;
                    case 5:input=-std::numeric_limits<double>::infinity();break;
                    case 6:input=1.e6;break;
                    case 7:input=-1.e6;break;
                }
                l[i]=x[i]=input;r[i]=y[i]=-input*.7;
            }
            blocked.process(l.data(),r.data(),n);
            for(int i=0;i<n;++i)sample.process(x.data()+i,y.data()+i,1);
            identical &= std::memcmp(l.data(),x.data(),n*sizeof(double))==0 && std::memcmp(r.data(),y.data(),n*sizeof(double))==0;
            for(int i=0;i<n;++i)finite &= std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<=16 && std::abs(r[i])<=16;
            const double reduction=blocked.gainReductionDb();boundedReduction &= std::isfinite(reduction) && reduction>=0 && reduction<=36.0001;
            frame+=n;++block;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);CHECK(identical);CHECK(finite);CHECK(boundedReduction);
        CHECK(resets>1);
        std::cout<<"  Dyna safety rate="<<rate<<" frames="<<frame<<" blocks="<<block<<" resets="<<resets<<" final_rng="<<rng<<'\n';
    }
}
