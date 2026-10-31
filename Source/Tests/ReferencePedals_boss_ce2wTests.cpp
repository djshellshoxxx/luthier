#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include <array>
#include <complex>
using luthier::reference::models::BossCe2w;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(RefPedalCe2w, rapidModeReversalKeepsTheCurrentAudibleBlend)
{
    // The three fixed-mode models have identical prepared histories. Blend their
    // independently rendered outputs using the written10ms transition, starting
    // a new transition at the current blend rather than an abandoned endpoint.
    for(double rate:{44100.,48000.,88200.,96000.,192000.}) for(int hostBlock:{1,64,2048})
    {
        BossCe2w changing;
        std::array<BossCe2w,3> voices;
        changing.setParam(7,0);changing.setParam(1,0);changing.prepare(rate,2048,1);
        for(int mode=0;mode<3;++mode)
        {
            voices[mode].setParam(7,0);voices[mode].setParam(1,0);voices[mode].setParam(2,mode*.5f);
            voices[mode].prepare(rate,2048,1);
        }
        const int start=int(rate*.125),end=start+int(rate*.05);
        const int events[]{start,start+int(rate*.003),start+int(rate*.005),start+int(rate*.0065)};
        const int targets[]{2,1,0,2};
        std::array<double,3> weights{1,0,0},origin=weights;
        double blend=1,error=0,maxDelta=0,previousL=0,previousR=0;bool finite=true;
        int target=0,eventIndex=0,frame=0;
        const auto allocations=luthier::tests::allocationsOnThisThread();
        while(frame<end)
        {
            if(eventIndex<4 && frame==events[eventIndex])
            {
                target=targets[eventIndex++];origin=weights;blend=0;changing.setParam(2,target*.5f);
            }
            int n=std::min({2048,hostBlock-frame%hostBlock,end-frame});
            if(eventIndex<4)n=std::min(n,events[eventIndex]-frame);
            std::array<double,2048> l{},r{};
            std::array<std::array<double,2048>,3> x{},y{};
            for(int i=0;i<n;++i)
            {
                l[i]=.1*std::sin(6.283185307179586*100*(frame+i)/rate+.37);r[i]=l[i];
                for(int m=0;m<3;++m){x[m][i]=l[i];y[m][i]=r[i];}
            }
            changing.process(l.data(),r.data(),n);
            for(int m=0;m<3;++m)voices[m].process(x[m].data(),y[m].data(),n);
            for(int i=0;i<n;++i)
            {
                blend=std::min(1.,blend+1/(rate*.01));
                double expectedL=0,expectedR=0;
                for(int m=0;m<3;++m)
                {
                    weights[m]=origin[m]+((m==target?1.:0.)-origin[m])*blend;
                    expectedL+=weights[m]*x[m][i];expectedR+=weights[m]*y[m][i];
                }
                error=std::max({error,std::abs(l[i]-expectedL),std::abs(r[i]-expectedR)});
                if(frame+i>=start)maxDelta=std::max({maxDelta,std::abs(l[i]-previousL),std::abs(r[i]-previousR)});
                previousL=l[i];previousR=r[i];
                finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
            }
            frame+=n;
        }
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        const auto label=juce::String(rate)+"/"+juce::String(hostBlock);
        CHECK_MSG(error<1.e-12,label+" interrupted fade lost audible weights "+juce::String(error,15));
        CHECK_MSG(maxDelta<std::pow(10,-30./20),label+" original-30dB mode-step gate "+juce::String(maxDelta,15));
        CHECK(finite);
    }
}
LUTHIER_TEST(RefPedalCe2w, resetAndReprepareClearInterruptedModeWeights)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})for(bool reprepare:{false,true})
    {
        BossCe2w pedal,fresh;
        for(auto* p:{&pedal,&fresh}){p->setParam(7,0);p->prepare(rate,2048,1);}
        std::array<double,2048> l{},r{},x{},y{};
        pedal.setParam(2,1);pedal.process(l.data(),r.data(),int(rate*.003));
        pedal.setParam(2,.5f);pedal.process(l.data(),r.data(),int(rate*.002));
        const auto allocations=luthier::tests::allocationsOnThisThread();
        if(reprepare)pedal.prepare(rate,2048,1);else pedal.reset();
        if(!reprepare)CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        fresh.setParam(2,.5f);fresh.reset();
        for(int i=0;i<2048;++i){l[i]=x[i]=.1*std::sin(i*.13);r[i]=y[i]=.12*std::cos(i*.17);}
        const auto renderAllocations=luthier::tests::allocationsOnThisThread();
        pedal.process(l.data(),r.data(),2048);fresh.process(x.data(),y.data(),2048);
        CHECK(luthier::tests::allocationsOnThisThread()==renderAllocations);
        CHECK(l==x);CHECK(r==y);
    }
}
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
        p.setEnabled(false);for(int i=0;i<int(sr);++i){double l=.1,r=-.2;p.processBypass(&l,&r,1);}
        double l=.125,r=-.25;p.processBypass(&l,&r,1);const double gain=1/(1+std::tan(3.14159265358979323846*10/sr));
        CHECK_NEAR(l,.025*gain,1e-12);CHECK_NEAR(r,-.05*gain,1e-12);
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

// Original provisional section 9 gates, with modulation/noise disabled so the
// coherent harmonic measurement isolates transfer distortion, not sidebands.
LUTHIER_TEST(RefPedalCe2w, originalStandardAndCe1ThdGates)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int mode=0;mode<3;++mode)
    {
        BossCe2w pedal;pedal.setParam(1,0);pedal.setParam(2,mode*.5f);pedal.setParam(7,0);pedal.prepare(sr,1,1);
        const double amplitude=mode==0?std::pow(10.,-10./20):1.;
        std::array<std::complex<double>,10> harmonics{};bool finite=true;
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*pi*1000*i/sr;double l=amplitude*std::sin(angle),r=l;pedal.process(&l,&r,1);
            finite &= std::isfinite(l) && std::isfinite(r);
            if(i>=int(sr))
            {
                const std::complex<double> rotation(std::cos(angle),-std::sin(angle));auto phase=rotation;
                for(int h=1;h<10;++h){harmonics[h]+=l*phase;phase*=rotation;}
            }
        }
        double energy=0;for(int h=2;h<10;++h)energy+=std::norm(harmonics[h]);
        const double fundamental=std::abs(harmonics[1]);
        CHECK(finite);CHECK(fundamental>0);
        if(!finite || fundamental<=0)continue;
        const double thd=std::sqrt(energy)/fundamental;
        const auto detail=juce::String(sr)+" Hz mode "+juce::String(mode)+" THD ratio "+juce::String(thd,6);
        if(mode==0){CHECK_MSG(thd<.01,detail);}
        else {CHECK_MSG(thd>=.01,detail);CHECK_MSG(thd<=.03,detail);}
    }
}
