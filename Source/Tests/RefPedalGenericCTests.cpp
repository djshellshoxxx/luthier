#include "TestFramework.h"
#include "../ReferencePedals/Generic/Delay.h"
#include <array>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalGenericC, delayImpulseTimingAndReset)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        generic::Delay p; p.setParam(0,float(p.desc().params[0].toNormalised(100))); p.setParam(1,0); p.setParam(2,1); p.prepare(sr,1,1);
        int peakIndex=0; double peak=0;
        for(int i=0;i<int(sr*.12);++i) { double l=i==0?1:0,r=0; p.process(&l,&r,1); if(std::abs(l)>peak) peak=std::abs(l),peakIndex=i; CHECK_NEAR(r,0,1e-15); }
        CHECK_NEAR(peakIndex,sr*.1,.5); CHECK(peak>.999);
        p.reset(); for(int i=0;i<int(sr*.12);++i) { double l=0,r=0; p.process(&l,&r,1); CHECK_NEAR(l,0,1e-15); }
    }
}
LUTHIER_TEST(RefPedalGenericC, delaySyncAndTapPersistence)
{
    generic::Delay p; p.setParam(2,1); p.setParam(1,0); p.setParam(7,.5f); p.setParam(6,1.f/6);
    TimeInfo t; t.valid=t.playing=true; t.bpm=120; p.setTime(t); p.prepare(48000,1,1);
    double peak=0; int at=0;
    for(int i=0;i<14000;++i) { double l=i==0?1:0,r=0; p.process(&l,&r,1); if(std::abs(l)>peak) peak=std::abs(l),at=i; }
    CHECK(at==12000);
    p.setParam(13,1); p.setParam(13,0);
    for(int i=0;i<24000;++i) { double l=0,r=0; p.process(&l,&r,1); }
    p.setParam(13,1); juce::var extra; p.saveExtra(extra); CHECK_NEAR(double(extra["tapMs"]),500,.001);
    generic::Delay restored; restored.loadExtra(extra); juce::var saved; restored.saveExtra(saved); CHECK_NEAR(double(saved["tapMs"]),500,.001);
}
LUTHIER_TEST(RefPedalGenericC, delayPingPongAndTrails)
{
    generic::Delay p; p.setParam(0,0); p.setParam(1,.5f/1.1f); p.setParam(2,1); p.setParam(8,.5f); p.prepare(48000,1,1);
    double firstL=0,firstR=0,secondL=0,secondR=0;
    for(int i=0;i<3000;++i)
    {
        double l=i==0?1:0,r=l; p.process(&l,&r,1);
        if(i==960) firstL=l,firstR=r;
        if(i==1920) secondL=l,secondR=r;
        if(i==1000) p.setEnabled(false);
    }
    CHECK(firstL>.99); CHECK_NEAR(firstR,0,1e-12); CHECK(std::abs(secondR)>.01); CHECK_NEAR(secondL,0,1e-12);
    CHECK(p.getTailEnergyDb()>-90);
}
LUTHIER_TEST(RefPedalGenericC, delayPartitionDeterminismAndBoundedFeedback)
{
    for(double sr:{44100.,48000.,96000.,192000.}) for(int mode=0;mode<3;++mode)
    {
        generic::Delay a,b; a.setParam(4,mode*.5f); b.setParam(4,mode*.5f);
        a.setParam(0,0); b.setParam(0,0); a.setParam(1,1); b.setParam(1,1); a.prepare(sr,2048,1); b.prepare(sr,2048,1);
        std::array<double,2048> l{},r{},x{},y{};
        for(int block=0;block<16;++block)
        {
            for(int i=0;i<2048;++i) l[i]=x[i]=.2*std::sin((block*2048+i)*.1),r[i]=y[i]=-l[i];
            a.process(l.data(),r.data(),2048);
            for(int i=0;i<2048;++i) b.process(x.data()+i,y.data()+i,1);
            CHECK(l==x); CHECK(r==y);
            for(int i=0;i<2048;++i) { CHECK(std::isfinite(l[i])); CHECK(std::abs(l[i])<=16); }
        }
    }
}
#include "../ReferencePedals/Generic/Reverb.h"
LUTHIER_TEST(RefPedalGenericC, reverbAllTypesPartitionAndReset)
{
    for(double sr:{44100.,48000.,96000.,192000.}) for(int type=0;type<5;++type)
    {
        generic::Reverb a,b; a.setParam(5,type*.25f);b.setParam(5,type*.25f);
        a.setParam(2,0);b.setParam(2,0);a.setParam(1,1);b.setParam(1,1);a.prepare(sr,2048,1);b.prepare(sr,2048,1);
        std::array<double,2048> l{},r{},x{},y{};double total=0;
        for(int block=0;block<int(sr*.5/2048)+1;++block)
        {
            l.fill(0);r.fill(0);x.fill(0);y.fill(0);if(block==0)l[0]=r[0]=x[0]=y[0]=1;
            a.process(l.data(),r.data(),2048);
            for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);
            CHECK(l==x);CHECK(r==y);
            for(int i=0;i<2048;++i) { CHECK(std::isfinite(l[i]));CHECK(std::isfinite(r[i]));total+=l[i]*l[i]+r[i]*r[i]; }
        }
        CHECK(total>1e-5); a.reset();l.fill(0);r.fill(0);a.process(l.data(),r.data(),2048);
        for(int i=0;i<2048;++i) { CHECK_NEAR(l[i],0,1e-15);CHECK_NEAR(r[i],0,1e-15); }
    }
}
LUTHIER_TEST(RefPedalGenericC, hallDecayAndFreeze)
{
    constexpr double sr=48000;
    for(double decay:{1.,2.,4.,8.})
    {
        generic::ReverbFdn tank;tank.prepare(sr);tank.set(decay,1,1,0,1,0);
        std::vector<double> energy(size_t(sr*decay*1.5));
        for(size_t i=0;i<energy.size();++i) {auto y=tank.process(i==0?1:0);energy[i]=y[0]*y[0]+y[1]*y[1];}
        for(size_t i=energy.size()-1;i>0;--i)energy[i-1]+=energy[i];
        size_t t5=0,t35=0;double referenceEnergy=energy[0];
        for(size_t i=0;i<energy.size();++i)
        {
            const double db=10*std::log10(std::max(1e-30,energy[i]/referenceEnergy));
            if(!t5 && db<=-5)t5=i;
            if(db<=-35){t35=i;break;}
        }
        const double rt60=2*double(t35-t5)/sr;
        CHECK_NEAR(rt60,decay,decay*.1);
    }
    generic::ReverbFdn tank;tank.prepare(sr);tank.set(4,1,1,0,1,0);
    for(int i=0;i<24000;++i)tank.process(i==0?1:0);
    tank.set(4,1,1,0,1,1);
    double start=0,end=0;
    for(int i=0;i<11*int(sr);++i) {auto y=tank.process(0);double e=y[0]*y[0]+y[1]*y[1];if(i<int(sr))start+=e;if(i>=10*int(sr))end+=e;}
    CHECK(std::abs(10*std::log10(end/start))<1);
}
LUTHIER_TEST(RefPedalGenericC, reverbTypeReturnNeverRevivesInactiveHistory)
{
    for(double sr:{44100.,48000.,96000.,192000.})for(int type=0;type<5;++type)
    {
        generic::Reverb p;p.setParam(5,type*.25f);p.setParam(1,1);p.setParam(2,0);p.prepare(sr,1,1);
        for(int i=0;i<int(sr*.2);++i){double l=.1*std::sin(i*.11),r=l;p.process(&l,&r,1);}
        p.setParam(5,((type+1)%5)*.25f);
        // Old input lowcut/filter ringing has decayed before the return; each
        // tank must begin with fresh history even if it was used previously.
        for(int i=0;i<int(sr*2);++i){double l=0,r=0;p.process(&l,&r,1);}
        p.setParam(5,type*.25f);double energy=0;
        for(int i=0;i<int(sr*.4);++i){double l=0,r=0;p.process(&l,&r,1);if(i>int(sr*.06))energy+=l*l+r*r;}
        CHECK(energy<1e-18);
    }
}
LUTHIER_TEST(RefPedalGenericC, reverbRapidTypeChangesRemainPartitionIndependent)
{
    generic::Reverb a,b;a.prepare(48000,64,1);b.prepare(48000,64,1);
    std::array<double,64> l{},r{},x{},y{};
    for(int block=0;block<600;++block)
    {
        if(block%7==0){float type=float((block/7)%5)*.25f;a.setParam(5,type);b.setParam(5,type);}
        for(int i=0;i<64;++i)l[i]=r[i]=x[i]=y[i]=.1*std::sin((block*64+i)*.1);
        a.process(l.data(),r.data(),64);for(int i=0;i<64;++i)b.process(x.data()+i,y.data()+i,1);
        CHECK(l==x);CHECK(r==y);
    }
}
