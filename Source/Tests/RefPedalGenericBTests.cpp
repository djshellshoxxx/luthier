#include "TestFramework.h"
#include "../ReferencePedals/Generic/Chorus.h"
#include "../ReferencePedals/Generic/Phaser.h"
#include "../ReferencePedals/Generic/Tremolo.h"
#include "../ReferencePedals/Generic/Wah.h"
#include <array>
using namespace luthier::reference;
namespace {
template<class P> bool partitionInvariant(double sr)
{
    P a,b; a.prepare(sr,2048,1); b.prepare(sr,2048,1);
    std::array<double,4096> al{},ar{},bl{},br{};
    for(size_t i=0;i<al.size();++i) al[i]=bl[i]=.2*std::sin(i*.31),ar[i]=br[i]=.1*std::cos(i*.17);
    a.process(al.data(),ar.data(),2048); a.process(al.data()+2048,ar.data()+2048,2048);
    int at=0,step=1;
    while(at<4096) { const int n=std::min(step,4096-at); b.process(bl.data()+at,br.data()+at,n); at+=n; step=step*17%503+1; }
    return al==bl && ar==br;
}
template<class P> bool finiteExtremes(double sr)
{
    P p; p.prepare(sr,2048,1);
    std::array<double,2048> l{},r{};
    for(int pass=0;pass<8;++pass)
    {
        for(int k=0;k<p.desc().numParams;++k) p.setParam(k,float((pass+k)%2));
        for(size_t i=0;i<l.size();++i) l[i]=i%37==0 ? 16 : .3*std::sin(i*.3),r[i]=-l[i];
        p.process(l.data(),r.data(),2048);
        for(size_t i=0;i<l.size();++i) if(!std::isfinite(l[i]) || !std::isfinite(r[i]) || std::abs(l[i])>16 || std::abs(r[i])>16) return false;
    }
    return true;
}
}
LUTHIER_TEST(RefPedalGenericB, blockPartitionAndRates)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        CHECK(partitionInvariant<generic::Chorus>(sr)); CHECK(partitionInvariant<generic::Phaser>(sr));
        CHECK(partitionInvariant<generic::Tremolo>(sr)); CHECK(partitionInvariant<generic::Wah>(sr));
        CHECK(finiteExtremes<generic::Chorus>(sr)); CHECK(finiteExtremes<generic::Phaser>(sr));
        CHECK(finiteExtremes<generic::Tremolo>(sr)); CHECK(finiteExtremes<generic::Wah>(sr));
    }
}
LUTHIER_TEST(RefPedalGenericB, dryAndSilence)
{
    generic::Chorus chorus; chorus.setParam(2,0); chorus.prepare(48000,1,1);
    generic::Phaser phaser; phaser.setParam(4,0); phaser.prepare(48000,1,1);
    generic::Tremolo tremolo; tremolo.setParam(1,0); tremolo.prepare(48000,1,1);
    generic::Wah wah; wah.setParam(7,0); wah.prepare(48000,1,1);
    for(IPedal* pedal:std::array<IPedal*,4>{&chorus,&phaser,&tremolo,&wah})
        for(int i=0;i<1000;++i) { double l=.1*std::sin(i*.1),r=-l,expected=l; pedal->process(&l,&r,1); CHECK_NEAR(l,expected,1e-14); CHECK_NEAR(r,-expected,1e-14); }
}
LUTHIER_TEST(RefPedalGenericB, tremoloDepthRateAndPanPower)
{
    generic::Tremolo p; p.setParam(0,float(p.desc().params[0].toNormalised(4))); p.setParam(1,1); p.prepare(48000,1,1);
    double min=1,max=0; int rising=0; double previous=0;
    for(int i=0;i<48000;++i) { double l=1,r=1; p.process(&l,&r,1); min=std::min(min,l); max=std::max(max,l); if(previous<.5 && l>=.5) ++rising; previous=l; }
    CHECK_NEAR(min,0,1e-8); CHECK_NEAR(max,1,1e-8); CHECK(rising==4);
    p.setParam(4,.5f); p.prepare(48000,1,1);
    for(int i=0;i<48000;++i) { double l=1,r=1; p.process(&l,&r,1); CHECK_NEAR(l*l+r*r,1,1e-12); }
}
LUTHIER_TEST(RefPedalGenericB, tapAndExpression)
{
    generic::Tremolo trem; trem.prepare(48000,1,1); trem.setParam(8,1); trem.setParam(8,0);
    for(int i=0;i<24000;++i) { double l=0,r=0; trem.process(&l,&r,1); }
    trem.setParam(8,1); CHECK_NEAR(trem.desc().params[0].fromNormalised(trem.getParam(0)),2,1e-6);
    generic::Wah wah; wah.setExpression(0,.25f); CHECK_NEAR(wah.getParam(0),.25,1e-6);
    wah.setExpression(1,.75f); CHECK_NEAR(wah.getParam(0),.25,1e-6);
}
LUTHIER_TEST(RefPedalGenericB, syncClockConverges)
{
    generic::detail::ModulationClock clock; clock.prepare(48000);
    for(int block=0;block<400;++block)
    {
        TimeInfo t; t.valid=t.playing=true; t.bpm=120; t.ppq=block*120.0/48000*2;
        clock.time(t,1);
        for(int n=0;n<120;++n) clock.advance(2);
    }
    CHECK_NEAR(clock.phase(),0,1e-9);
}
LUTHIER_TEST(RefPedalGenericB, phaserAnalyticNotches)
{
    constexpr double sr=48000,pi=3.14159265358979323846;
    constexpr int counts[]{2,4,6,8,12};
    for(int selection=0;selection<5;++selection)
    {
        generic::Phaser pedal;pedal.setParam(1,0);pedal.setParam(2,.5f);pedal.setParam(3,selection*.25f);pedal.prepare(sr,2048,1);
        std::array<double,8192> left{},right{};left[0]=right[0]=1;
        for(int block=0;block<4;++block)pedal.process(left.data()+2048*block,right.data()+2048*block,2048);
        const double center=pedal.desc().params[5].fromNormalised(pedal.getParam(5));
        for(int k=0;k<counts[selection]/2;++k)
        {
            const double f=sr/pi*std::atan(std::tan(pi*center/sr)*std::tan((2*k+1)*pi/(2*counts[selection])));
            double real=0,imaginary=0;
            for(int i=0;i<8192;++i) {real+=left[i]*std::cos(2*pi*f*i/sr);imaginary-=left[i]*std::sin(2*pi*f*i/sr);}
            CHECK(std::hypot(real,imaginary)<1e-5);
        }
    }
}
LUTHIER_TEST(RefPedalGenericB, chorusStaticDelayAndReset)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        generic::Chorus p;p.setParam(1,0);p.setParam(2,1);p.prepare(sr,1,1);
        double peak=0;int at=0;
        for(int i=0;i<int(sr*.012);++i){double l=i==0?1:0,r=l;p.process(&l,&r,1);if(std::abs(l)>peak)peak=std::abs(l),at=i;}
        CHECK_NEAR(at,sr*.007,sr*.00015);
        p.reset();double energy=0;
        for(int i=0;i<int(sr*.025);++i){double l=0,r=0;p.process(&l,&r,1);energy+=l*l+r*r;}
        CHECK_NEAR(energy,0,1e-20);
    }
}

LUTHIER_TEST(RefPedalGenericB, toeSwitchHysteresisAndSingleConsumption)
{
    generic::Wah p;p.setParam(8,1);p.prepare(48000,1,1);
    auto settle=[&](float position){p.setExpression(0,position);for(int i=0;i<12000;++i){double l=0,r=0;p.process(&l,&r,1);}};
    settle(1);CHECK(p.consumeEngageToggle());CHECK(!p.consumeEngageToggle());
    settle(.95f);settle(1);CHECK(!p.consumeEngageToggle());
    settle(.8f);settle(1);CHECK(p.consumeEngageToggle());
    p.setParam(8,0);settle(.5f);settle(1);CHECK(!p.consumeEngageToggle());
    p.setParam(8,1);settle(.5f);p.setParam(4,1);
    for(int i=0;i<48000;++i){double l=1,r=1;p.process(&l,&r,1);}
    CHECK(!p.consumeEngageToggle());
}
