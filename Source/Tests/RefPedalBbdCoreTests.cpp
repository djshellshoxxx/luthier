#include "TestFramework.h"
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
