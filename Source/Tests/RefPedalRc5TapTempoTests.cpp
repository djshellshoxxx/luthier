// OWNER: WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/Rc5TapTempo.h"
#include "../ReferencePedals/Common/Rc5Rhythm.h"
#include <limits>
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

LUTHIER_TEST(RefPedalRc5TapTempo, eight120BpmTapsAtFiveRates)
{
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        Rc5TapTempo tempo;CHECK(tempo.prepare(rate));CHECK(!tempo.tap(0));
        for(int i=1;i<=8;++i)
        {
            const auto bpm=tempo.tap(int64_t(rate*.5)*i);
            CHECK(bpm.has_value());CHECK_NEAR(bpm.value_or(0),120.,.5);
            CHECK_NEAR(bpm.value_or(0),120.,1e-12);
        }
    }
}
LUTHIER_TEST(RefPedalRc5TapTempo, arithmeticMeanUsesLastFourAcceptedIntervals)
{
    Rc5TapTempo tempo;CHECK(tempo.prepare(1000));CHECK(!tempo.tap(0));
    int64_t sample=0;
    const std::array<int64_t,6> intervals{200,300,500,1000,700,400};
    const std::array<double,6> expected{300.,240.,180.,120.,96.,60000.*4/2600};
    for(size_t i=0;i<intervals.size();++i)
    {
        sample+=intervals[i];const auto bpm=tempo.tap(sample);
        CHECK(bpm.has_value());CHECK_NEAR(bpm.value_or(0),expected[i],1e-12);
    }
}
LUTHIER_TEST(RefPedalRc5TapTempo, inclusiveIntegerIntervalBounds)
{
    for(double rate:{1.,3.,1000.,44100.25,48000.,192000.})
    {
        const auto minimum=int64_t(std::ceil(rate/5));
        const auto maximum=int64_t(std::floor(3*rate/2));
        for(auto frames:{minimum,maximum})
        {
            Rc5TapTempo tempo;CHECK(tempo.prepare(rate));CHECK(!tempo.tap(0));
            const auto bpm=tempo.tap(frames);CHECK(bpm.has_value());
            CHECK_NEAR(bpm.value_or(0),std::clamp(60*rate/double(frames),40.,300.),1e-12);
        }
        Rc5TapTempo below,above;CHECK(below.prepare(rate));CHECK(above.prepare(rate));
        CHECK(!below.tap(0));CHECK(!above.tap(0));
        CHECK(!below.tap(minimum-1));CHECK(!above.tap(maximum+1));
    }
}
LUTHIER_TEST(RefPedalRc5TapTempo, invalidIntervalsAreExcludedButUpdateAdjacentAnchor)
{
    Rc5TapTempo tempo;CHECK(tempo.prepare(1000));CHECK(!tempo.tap(0));
    CHECK_NEAR(tempo.tap(500).value_or(0),120.,1e-12);
    CHECK(!tempo.tap(600)); // rejected 100, but 600 is the next anchor
    CHECK_NEAR(tempo.tap(900).value_or(0),150.,1e-12);
    CHECK(!tempo.tap(2500)); // rejected 1600, below timeout
    CHECK_NEAR(tempo.tap(2900).value_or(0),150.,1e-12);
    CHECK(!tempo.tap(2900)); // duplicate is a rejected zero interval
    CHECK_NEAR(tempo.tap(3500).value_or(0),60000.*4/1800,1e-12);
}
LUTHIER_TEST(RefPedalRc5TapTempo, twoSecondGapClearsMeanAndReanchors)
{
    for(double rate:{1000.,1000.25,44100.,192000.})
    {
        const auto gap=int64_t(std::ceil(2*rate));
        const auto half=int64_t(std::ceil(rate/2));
        const auto quarter=int64_t(std::ceil(rate/4));
        Rc5TapTempo before,at;CHECK(before.prepare(rate));CHECK(at.prepare(rate));
        for(auto* tempo:{&before,&at}){CHECK(!tempo->tap(0));CHECK(tempo->tap(half).has_value());}
        CHECK(!before.tap(half+gap-1));CHECK(!at.tap(half+gap));
        CHECK_NEAR(before.tap(half+gap-1+quarter).value_or(0),120*rate/double(half+quarter),1e-12);
        CHECK_NEAR(at.tap(half+gap+quarter).value_or(0),60*rate/double(quarter),1e-12);
    }
    Rc5TapTempo tempo;CHECK(tempo.prepare(1000));CHECK(!tempo.tap(0));
    CHECK(tempo.tap(500).has_value());CHECK(!tempo.tap(10000));
    CHECK_NEAR(tempo.tap(10200).value_or(0),300.,1e-12);
}
LUTHIER_TEST(RefPedalRc5TapTempo, backwardNegativeAndResetClearBookkeeping)
{
    Rc5TapTempo tempo;CHECK(tempo.prepare(1000));CHECK(!tempo.tap(0));CHECK(tempo.tap(500).has_value());
    CHECK(!tempo.tap(499));CHECK(!tempo.tap(1000));
    CHECK_NEAR(tempo.tap(1250).value_or(0),240.,1e-12);
    CHECK(!tempo.tap(-1));CHECK(!tempo.tap(2000));
    CHECK_NEAR(tempo.tap(2500).value_or(0),120.,1e-12);
    CHECK(!tempo.tap(std::numeric_limits<int64_t>::min()));
    CHECK(!tempo.tap(0));CHECK(tempo.tap(500).has_value());tempo.reset();
    CHECK(!tempo.tap(0));CHECK_NEAR(tempo.tap(1000).value_or(0),60.,1e-12);
}
LUTHIER_TEST(RefPedalRc5TapTempo, prepareValidationPreservesPriorStateAndValidPrepareResets)
{
    Rc5TapTempo tempo;CHECK(!tempo.tap(0));CHECK(!tempo.tap(24000));
    for(double invalid:{0.,-1.,192000.1,std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        CHECK(tempo.prepare(1000));CHECK(!tempo.tap(0));CHECK(tempo.tap(500).has_value());
        CHECK(!tempo.prepare(invalid));CHECK_NEAR(tempo.tap(750).value_or(0),160.,1e-12);
    }
    CHECK(tempo.prepare(2000));CHECK(!tempo.tap(1000));
    CHECK_NEAR(tempo.tap(2000).value_or(0),120.,1e-12);
    CHECK(tempo.prepare(2000));CHECK(!tempo.tap(0));
    CHECK_NEAR(tempo.tap(500).value_or(0),240.,1e-12);
}
LUTHIER_TEST(RefPedalRc5TapTempo, sampleClockExtremesCannotOverflow)
{
    constexpr auto last=std::numeric_limits<int64_t>::max();
    Rc5TapTempo tempo;CHECK(tempo.prepare(192000));CHECK(!tempo.tap(0));
    CHECK(!tempo.tap(last-96000));CHECK_NEAR(tempo.tap(last).value_or(0),120.,1e-12);
    CHECK(!tempo.tap(last));CHECK(!tempo.tap(0));CHECK(!tempo.tap(0));
    CHECK_NEAR(tempo.tap(96000).value_or(0),120.,1e-12);
    CHECK(!tempo.tap(last));CHECK(!tempo.tap(std::numeric_limits<int64_t>::min()));
    CHECK(!tempo.tap(last-38400));CHECK_NEAR(tempo.tap(last).value_or(0),300.,1e-12);
}
LUTHIER_TEST(RefPedalRc5TapTempo, callbackMethodsAllocateNothing)
{
    Rc5TapTempo tempo;bool valid=true;double sum=0;
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int pass=0;pass<64;++pass)
    {
        valid &= tempo.prepare(48000);valid &= !tempo.tap(0).has_value();
        for(int64_t sample=24000;sample<=240000;sample+=24000)
            sum+=tempo.tap(sample).value_or(0);
        valid &= !tempo.prepare(std::numeric_limits<double>::quiet_NaN());
        valid &= !tempo.tap(-1).has_value();tempo.reset();
    }
    const auto after=luthier::tests::allocationsOnThisThread();
    CHECK(valid);CHECK_NEAR(sum,64*10*120,0);CHECK(after==allocations);
}
LUTHIER_TEST(RefPedalRc5TapTempo, acceptedTemposDriveActualRhythmWithoutPhaseResetOrPartitionDependence)
{
    constexpr int frames=100000;
    const std::array<int,6> events{0,10001,23004,39009,59016,83027};
    // Independently written manual sequence: final mean evicts the first interval.
    const std::array<double,6> manual{120.,2880000./10001,5760000./23004,
        8640000./39009,11520000./59016,11520000./73026};
    Rc5Rhythm expected,actual,partitioned,rounded;Rc5TapTempo tap,otherTap;
    for(auto* rhythm:{&expected,&actual,&partitioned,&rounded})
    {rhythm->prepare(48000,73);rhythm->setRunning(true);}
    CHECK(tap.prepare(48000));CHECK(otherTap.prepare(48000));
    std::vector<double> el(frames),er(frames),al(frames),ar(frames),pl(frames),pr(frames),rl(frames),rr(frames);
    bool phases=true,tempos=true;double analyticalPhase=0;
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(size_t i=0;i<events.size();++i)
    {
        const auto bpm=tap.tap(events[i]),other=otherTap.tap(events[i]);
        const double before=actual.stepPosition(),partitionBefore=partitioned.stepPosition();
        if(i==0)tempos &= !bpm && !other;
        else
        {
            tempos &= bpm && other && std::abs(*bpm-manual[i])<1e-12 && *bpm==*other;
            actual.setTempo(bpm.value_or(120));partitioned.setTempo(other.value_or(120));
        }
        phases &= actual.stepPosition()==before && partitioned.stepPosition()==partitionBefore;
        expected.setTempo(manual[i]);rounded.setTempo(std::round(manual[i]*10)/10);
        const int start=events[i],end=i+1<events.size()?events[i+1]:frames;
        expected.process(el.data()+start,er.data()+start,end-start);
        rounded.process(rl.data()+start,rr.data()+start,end-start);
        for(int p=start;p<end;p+=37)actual.process(al.data()+p,ar.data()+p,std::min(37,end-p));
        for(int p=start;p<end;p+=257)partitioned.process(pl.data()+p,pr.data()+p,std::min(257,end-p));
        analyticalPhase=std::fmod(analyticalPhase+(end-start)*manual[i]/(15*48000),16.);
        phases &= std::abs(actual.stepPosition()-analyticalPhase)<1e-9;
    }
    const auto after=luthier::tests::allocationsOnThisThread();
    CHECK(tempos);CHECK(phases);CHECK(after==allocations);
    CHECK(el==al);CHECK(er==ar);CHECK(el==pl);CHECK(er==pr);
    CHECK(std::abs(actual.stepPosition()-rounded.stepPosition())>1e-5);
    CHECK(el!=rl);CHECK(actual.stepPosition()==expected.stepPosition());
}
