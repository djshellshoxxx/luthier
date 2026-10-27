#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include <array>
using namespace luthier::reference;
using models::BossDd3t;
namespace {void silence(BossDd3t& p,int count){for(int i=0;i<count;++i){double l=0,r=0;p.process(&l,&r,1);}}}
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
void tapVia(BossDd3t& pedal,bool foot) noexcept
{
    if(foot){pedal.onFoot({true,.5f,0,0});pedal.onFoot({false,.5f,0,0});}
    else {pedal.setParam(6,1);pedal.setParam(6,0);}
}
bool advanceTapFrames(BossDd3t& pedal,int count,int block) noexcept
{
    std::array<double,2048> l{},r{};bool finite=true;
    while(count>0)
    {
        const int n=std::min(count,block);std::fill_n(l.data(),n,0.);std::fill_n(r.data(),n,0.);
        pedal.process(l.data(),r.data(),n);
        for(int i=0;i<n;++i)finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
        count-=n;
    }
    return finite;
}
}
LUTHIER_TEST(RefPedalDd3t, tapIntervalBoundariesUseRepresentableSampleTimes)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
        for(int block:{1,64,2048}) for(bool foot:{false,true}) for(int edge=0;edge<4;++edge)
    {
        BossDd3t pedal;pedal.setParam(3,2.f/3);pedal.setParam(7,1);pedal.prepare(rate,2048,1);
        const int firstAccepted=int(std::ceil(133*rate/1000)),lastAccepted=int(std::floor(800*rate/1000));
        const int frames[]{firstAccepted-1,firstAccepted,lastAccepted,lastAccepted+1};
        const bool accepted=edge==1 || edge==2;
        const double interval=frames[edge]*1000./rate,previousDelay=pedal.delayMilliseconds();
        const auto allocations=luthier::tests::allocationsOnThisThread();
        tapVia(pedal,foot);bool finite=advanceTapFrames(pedal,frames[edge],block);tapVia(pedal,foot);
        finite &= advanceTapFrames(pedal,int(std::ceil(rate*.8)),block);
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);
        const auto label=juce::String(rate)+"/"+juce::String(block)+(foot?"/foot":"/trigger")+"/edge"+juce::String(edge);
        juce::var saved;pedal.saveExtra(saved);
        CHECK_MSG(bool(saved["hasTapped"])==accepted,label+" accepted/rejected wrong sample interval");
        CHECK_NEAR(double(saved["tapIntervalMs"]),accepted?interval:500.,1.e-9);
        CHECK_NEAR(pedal.readUiMeters().bpm,60000/(accepted?interval:500.),1.e-9);
        CHECK_NEAR(pedal.delayMilliseconds(),accepted?interval:previousDelay,2.); // Original tap tolerance.
        CHECK(finite);CHECK(!pedal.consumeEngageToggle());
        BossDd3t restored;restored.loadExtra(saved);juce::var again;restored.saveExtra(again);
        CHECK(bool(again["hasTapped"])==accepted);
        CHECK_NEAR(double(again["tapIntervalMs"]),accepted?interval:500.,1.e-9);
    }
}
LUTHIER_TEST(RefPedalDd3t, tapAverageRejectsInvalidGapsAndResetsAfterTwoPointOneSeconds)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
        for(int block:{1,64,2048}) for(bool foot:{false,true})
    {
        BossDd3t pedal;pedal.setParam(3,2.f/3);pedal.setParam(7,1);pedal.prepare(rate,2048,1);
        tapVia(pedal,foot);std::array<double,4> intervals{};int index=0;
        for(double ms:{300.,500.,700.,400.})
        {
            const int frames=int(std::llround(ms*rate/1000));intervals[index]=frames*1000./rate;
            const auto allocations=luthier::tests::allocationsOnThisThread();
            const bool finite=advanceTapFrames(pedal,frames,block);tapVia(pedal,foot);
            CHECK(luthier::tests::allocationsOnThisThread()==allocations);CHECK(finite);
            const int begin=std::max(0,index-2);double expected=0;
            for(int i=begin;i<=index;++i)expected+=intervals[i];expected/=index-begin+1;
            juce::var state;pedal.saveExtra(state);CHECK(bool(state["hasTapped"]));
            CHECK_NEAR(double(state["tapIntervalMs"]),expected,1.e-9);++index;
        }
        const double average=(intervals[1]+intervals[2]+intervals[3])/3;
        for(int rejectedFrames:{int(std::ceil(133*rate/1000))-1,int(std::floor(800*rate/1000))+1})
        {
            const auto allocations=luthier::tests::allocationsOnThisThread();
            const bool finite=advanceTapFrames(pedal,rejectedFrames,block);tapVia(pedal,foot);
            CHECK(luthier::tests::allocationsOnThisThread()==allocations);CHECK(finite);
            juce::var state;pedal.saveExtra(state);CHECK(bool(state["hasTapped"]));
            CHECK_NEAR(double(state["tapIntervalMs"]),average,1.e-9);
        }
        const auto allocations=luthier::tests::allocationsOnThisThread();
        bool finite=advanceTapFrames(pedal,int(std::llround(2.1*rate)),block);tapVia(pedal,foot);
        const int newGap=int(std::llround(.6*rate));
        finite &= advanceTapFrames(pedal,newGap,block);tapVia(pedal,foot);
        CHECK(luthier::tests::allocationsOnThisThread()==allocations);CHECK(finite);
        juce::var state;pedal.saveExtra(state);CHECK(bool(state["hasTapped"]));
        CHECK_NEAR(double(state["tapIntervalMs"]),newGap*1000./rate,1.e-9);
        CHECK(!pedal.consumeEngageToggle());
    }
}
LUTHIER_TEST(RefPedalDd3t, impulseRangesAndDryEndpoints)
{
    for(double sr:{44100.,48000.,96000.,192000.})for(int mode=0;mode<3;++mode)for(int end=0;end<2;++end)
    {
        BossDd3t p;p.setParam(0,1);p.setParam(1,0);p.setParam(2,float(end));p.setParam(3,mode/3.f);p.setParam(10,1);p.prepare(sr,1,1);
        const double expected=(mode==0?12.5:mode==1?50:200)*(end?4:1);int at=0;double peak=0;
        for(int i=0;i<int(sr*(expected+10)*.001);++i){double l=i==0?.01:0,r=l;p.process(&l,&r,1);if(i>int(sr*.001) && std::abs(l)>peak)peak=std::abs(l),at=i;}
        CHECK_NEAR(at,sr*expected*.001,sr*.0003);CHECK(peak>1e-5);
        p.setEnabled(false);silence(p,int(sr*.02));double l=.125,r=-.25;p.process(&l,&r,1);CHECK_NEAR(l,.125,1e-15);CHECK_NEAR(r,-.25,1e-15);
    }
}
LUTHIER_TEST(RefPedalDd3t, tapSubdivisionsAndSlowClockHalving)
{
    for(int mode=0;mode<3;++mode)
    {
        BossDd3t p;p.setParam(3,mode/3.f);p.setParam(7,1);p.prepare(48000,1,1);
        for(int tap=0;tap<4;++tap){p.setParam(6,1);p.setParam(6,0);silence(p,24000);}
        silence(p,48000);CHECK_NEAR(p.delayMilliseconds(),mode==0?1000./3:mode==1?375:500,.01);
        juce::var saved;p.saveExtra(saved);CHECK_NEAR(double(saved["tempoBpm"]),120,1e-8);
        BossDd3t restored;restored.loadExtra(saved);juce::var again;restored.saveExtra(again);CHECK_NEAR(double(again["tapIntervalMs"]),500,1e-8);
    }
    BossDd3t p;p.setParam(12,1);TimeInfo t;t.valid=t.playing=true;t.bpm=30;t.ppq=0;p.setTime(t);p.prepare(48000,1,1);
    CHECK_NEAR(p.delayMilliseconds(),500,1e-8);
}
LUTHIER_TEST(RefPedalDd3t, shortLoopRepeatsAndRelease)
{
    BossDd3t p;p.setParam(0,1);p.setParam(1,0);p.setParam(2,2.f/3);p.setParam(3,1);p.setParam(10,1);p.prepare(48000,1,1);
    for(int i=0;i<48000;++i){double l=.1*std::sin(i*.07),r=l;p.process(&l,&r,1);}
    p.onFoot({true,.5f,0,0});CHECK(p.shortLoopActive());silence(p,19200);
    std::array<double,19200> first{},second{},r{};for(int i=0;i<19200;++i)p.process(first.data()+i,r.data()+i,1);
    r.fill(0);for(int i=0;i<19200;++i)p.process(second.data()+i,r.data()+i,1);CHECK(first==second);
    p.onFoot({false,.5f,0,0});CHECK(!p.shortLoopActive());silence(p,48000);CHECK(p.getTailEnergyDb()<-90);
}
LUTHIER_TEST(RefPedalDd3t, partitionQuantizationAndLongHold)
{
    BossDd3t a,b;a.prepare(48000,2048,1);b.prepare(48000,2048,1);
    std::array<double,2048> l{},r{},x{},y{};
    for(int block=0;block<30;++block)
    {
        for(int i=0;i<2048;++i)l[i]=r[i]=x[i]=y[i]=.1*std::sin((block*2048+i)*.1);
        a.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);CHECK(l==x);CHECK(r==y);
    }
    a.onFoot({true,.5f,0,0});silence(a,96001);CHECK_NEAR(a.getParam(7),1,1e-8);a.onFoot({false,.5f,0,0});
    a.setParam(17,1);CHECK_NEAR(a.getParam(17),0,1e-8);
}
LUTHIER_TEST(RefPedalDd3t, fullFeedbackSixtySecondStability)
{
    BossDd3t p;p.setParam(0,1);p.setParam(1,1);p.setParam(2,0);p.setParam(3,0);p.prepare(48000,2048,1);
    std::array<double,2048> l{},r{};l[0]=r[0]=1;bool stable=true;
    for(int block=0;block<1407;++block){p.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)stable=stable && std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<=2 && std::abs(r[i])<=2;l.fill(0);r.fill(0);}
    CHECK(stable);
}

LUTHIER_TEST(RefPedalDd3t, audioThreadUiMetersExposeRealTempoAndLoop)
{
    BossDd3t p;p.prepare(48000,1,1);auto meter=p.readUiMeters();
    CHECK(meter.available&UiTempo);CHECK_NEAR(meter.bpm,120,1e-12);
    TimeInfo t;t.valid=true;t.bpm=90;t.ppq=0;p.setParam(12,1);p.setTime(t);
    CHECK_NEAR(p.readUiMeters().bpm,90,1e-12);
    p.setParam(3,1);silence(p,48000);p.onFoot({true,1,1,0});silence(p,1000);
    meter=p.readUiMeters();CHECK(meter.available&UiLoop);CHECK(meter.available&UiProgress);
    CHECK(meter.loopLength>0);CHECK(meter.playhead<meter.loopLength);CHECK(meter.progress>=0 && meter.progress<1);
    p.onFoot({false,1,1,0});CHECK((p.readUiMeters().available&UiLoop)==0);
}

LUTHIER_TEST(RefPedalDd3t, shortReleaseOwnsEngagementAndLongHoldDoesNot)
{
    BossDd3t p;p.prepare(48000,1,1);CHECK(!p.footControlsEngagement());
    p.onFoot({true,1,1,0});silence(p,4800);p.onFoot({false,1,1,0});
    CHECK(p.consumeEngageToggle());CHECK(!p.consumeEngageToggle());
    p.onFoot({true,1,1,0});silence(p,100800);CHECK_NEAR(p.getParam(7),1,1e-12);
    CHECK(!p.consumeEngageToggle());p.onFoot({false,1,1,0});CHECK(!p.consumeEngageToggle());
    p.onFoot({true,1,1,0});silence(p,4800);p.onFoot({false,1,1,0});CHECK(!p.consumeEngageToggle());
    p.onFoot({true,1,1,0});silence(p,100800);CHECK_NEAR(p.getParam(7),0,1e-12);
    p.onFoot({false,1,1,0});CHECK(!p.consumeEngageToggle());
    p.setParam(3,1);p.onFoot({true,1,1,0});silence(p,100);p.onFoot({false,1,1,0});CHECK(!p.consumeEngageToggle());
}
