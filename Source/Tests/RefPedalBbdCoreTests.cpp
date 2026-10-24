#include "TestFramework.h"
#include <cstring>
#include <limits>
#include "../ReferencePedals/Common/BbdCore.h"
using luthier::reference::common::BbdCore;
LUTHIER_TEST(RefPedalBbdCore, clockLawImpulseAndReset)
{
    CHECK_NEAR(BbdCore::clockHz(3.4),150588.235294,1e-5);
    CHECK_NEAR(BbdCore::clockHz(2.2),232727.272727,1e-5);
    CHECK_NEAR(BbdCore::clockHz(4.5),113777.777778,1e-5);
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        BbdCore core;core.prepare(sr);double peak=0;int at=0;
        for(int i=0;i<int(sr*.012);++i){double y=core.process(i==0?.001:0,3.4);if(std::abs(y)>peak)peak=std::abs(y),at=i;}
        CHECK_NEAR(at,sr*.0034,sr*.0003);
        core.reset();double energy=0;for(int i=0;i<int(sr*.02);++i){double y=core.process(0,3.4);energy+=y*y;}
        CHECK_NEAR(energy,0,1e-25);
    }
}
LUTHIER_TEST(RefPedalBbdCore, seedDeterminismAndBandwidth)
{
    BbdCore a,b;a.prepare(48000);b.prepare(48000);a.reset(17);b.reset(17);
    bool identical=true;for(int i=0;i<24000;++i)identical=identical && a.process(0,3.4,1)==b.process(0,3.4,1);CHECK(identical);
    double rms[2]{};
    for(int k=0;k<2;++k)
    {
        a.reset();double hz=k==0?1000:10000;
        for(int i=0;i<48000;++i){const double x=.001*std::sin(6.283185307179586*hz*i/48000),y=a.process(x,3.4);if(i>=24000)rms[k]+=y*y;}
    }
    CHECK(10*std::log10(rms[1]/rms[0])<-30);
}

LUTHIER_TEST(RefPedalBbdCore, clockEventsRemainDistinctAboveHostRate)
{
    using luthier::reference::common::BbdCore;
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        BbdCore analog,clocked;analog.prepare(sr,12);clocked.prepare(sr,12);double difference=0,energy=0;
        for(int i=0;i<int(sr*.1);++i)
        {
            const double x=.1*std::sin(6.283185307179586*1000*i/sr);
            const double a=analog.process(x,3.4),b=clocked.process(x,3.4,0,true);
            difference+=(a-b)*(a-b);energy+=b*b;
        }
        CHECK(difference>1e-7);CHECK(energy>1);clocked.reset();double silence=0;
        for(int i=0;i<4096;++i)silence+=std::abs(clocked.process(0,3.4,0,true));
        CHECK_NEAR(silence,0,1e-20);
    }
}

LUTHIER_TEST(RefPedalBbdCore, sharedPrefixPreservesBothIndependentSuffixHistories)
{
    // Independent complete cores are the oracle for two voices sharing only
    // their identical prefix. Delays, clocks, noise and post filters differ.
    BbdCore wholeA,wholeB,prefixAndA,suffixB;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        for(auto* core:{&wholeA,&wholeB,&prefixAndA,&suffixB})
        {core->prepare(sr,40,6590,6930);core->setClipLevel(4.5);core->reset(73);}
        for(int i=0;i<32768;++i)
        {
            if(i==16384)for(auto* core:{&wholeA,&wholeB,&prefixAndA,&suffixB})core->reset(17);
            const double x=i%1021==0?std::numeric_limits<double>::quiet_NaN():.6*std::sin(i*.137);
            const double delayA=7+3.5*std::sin(i*.0007),delayB=4.5+2.5*std::sin(i*.0007);
            const double noise=(i/4000)%2?.4:0;const bool clocked=(i/3000)%2;
            const double prepared=prefixAndA.processInput(x);
            const double a=wholeA.process(x,delayA,noise,clocked),b=wholeB.process(x,delayB,noise,clocked);
            const double sharedA=prefixAndA.processPreparedInput(prepared,delayA,noise,clocked);
            const double sharedB=suffixB.processPreparedInput(prepared,delayB,noise,clocked);
            CHECK(std::memcmp(&a,&sharedA,sizeof(double))==0);
            CHECK(std::memcmp(&b,&sharedB,sizeof(double))==0);
        }
    }
}
