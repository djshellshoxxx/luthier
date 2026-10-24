#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include <array>
using luthier::reference::models::BossCe2w;
LUTHIER_TEST(RefPedalCe2w, modesImpulseStereoAndBypass)
{
    for(double sr:{44100.,48000.,96000.,192000.})for(int mode=0;mode<3;++mode)
    {
        BossCe2w p;p.setParam(1,0);p.setParam(2,mode*.5f);p.setParam(7,0);p.prepare(sr,1,1);
        int at=0;double peak=0,initial=0,stereoCancellation=0;
        for(int i=0;i<int(sr*.02);++i)
        {
            double l=i==0?.01:0,r=l;p.process(&l,&r,1);
            if(i==0)initial=std::abs(l);
            if(i>int(sr*.001) && std::abs(l)>peak)peak=std::abs(l),at=i;
            if(mode==2)stereoCancellation+=std::abs(l+r);
        }
        const double centre=mode==0?6.5:mode==1?7:4.5;CHECK_NEAR(at,sr*centre*.001,sr*.0005);
        if(mode==2){CHECK(initial<1e-8);CHECK(stereoCancellation<1e-12);}
        p.setEnabled(false);for(int i=0;i<int(sr*.01);++i){double l=.1,r=-.2;p.process(&l,&r,1);}
        double l=.125,r=-.25;p.process(&l,&r,1);CHECK_NEAR(l,.125,1e-15);CHECK_NEAR(r,-.25,1e-15);
    }
}
LUTHIER_TEST(RefPedalCe2w, partitionAndNoiseOffReset)
{
    BossCe2w a,b;a.setParam(7,0);b.setParam(7,0);a.prepare(48000,2048,1);b.prepare(48000,2048,1);
    std::array<double,2048> l{},r{},x{},y{};for(int i=0;i<2048;++i)l[i]=r[i]=x[i]=y[i]=.1*std::sin(i*.1);
    a.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);CHECK(l==x);CHECK(r==y);
    a.reset();l.fill(0);r.fill(0);a.process(l.data(),r.data(),2048);double energy=0;for(int i=0;i<2048;++i)energy+=l[i]*l[i]+r[i]*r[i];CHECK_NEAR(energy,0,1e-25);
}
LUTHIER_TEST(RefPedalCe2w, lfoRatesAndModeTransition)
{
    for(float setting:{0.f,.5f,1.f})
    {
        BossCe2w p;p.setParam(0,setting);p.setParam(7,0);p.prepare(48000,1,1);double phase=0;int wraps=0;
        for(int i=0;i<48000*4;++i){double l=0,r=0;p.process(&l,&r,1);if(p.modulationPhase()<phase)++wraps;phase=p.modulationPhase();}
        CHECK_NEAR((wraps+phase)/4,.3*std::pow(4/.3,setting),1e-9);
    }
    BossCe2w p;p.setParam(7,0);p.prepare(48000,1,1);double previous=0,maxDelta=0;
    for(int i=0;i<48000;++i)
    {
        if(i==12000)p.setParam(2,.5f);if(i==24000)p.setParam(2,1);if(i==36000)p.setParam(2,0);
        double l=.1*std::sin(6.283185307179586*100*i/48000),r=l;p.process(&l,&r,1);
        maxDelta=std::max(maxDelta,std::abs(l-previous));previous=l;
    }
    CHECK(maxDelta<std::pow(10,-30./20));
}

LUTHIER_TEST(RefPedalCe2w, inactiveModesDoNotReplayStaleAudio)
{
    BossCe2w pedal;pedal.setParam(7,0);pedal.setParam(1,0);pedal.prepare(48000,1,1);
    for(int i=0;i<256;++i){double l=.2,r=.2;pedal.process(&l,&r,1);}
    pedal.setParam(2,.5f);
    for(int i=0;i<48000;++i){double l=0,r=0;pedal.process(&l,&r,1);}
    pedal.setParam(2,0);double energy=0;
    for(int i=0;i<4800;++i){double l=0,r=0;pedal.process(&l,&r,1);energy+=l*l+r*r;}
    CHECK(energy<1e-20);
}
