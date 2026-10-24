#include "TestFramework.h"
#include "../ReferencePedals/Common/RandomWalkLfo.h"
#include <array>
using luthier::reference::common::RandomWalkLfo;
LUTHIER_TEST(RefPedalRandomWalkLfo, boundedContinuousSeededSampleClock)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(uint32_t seed:{1u,73u,0xdeadbeefu})
    {
        RandomWalkLfo walk;walk.prepare(sr,seed);double previous=0,maxStep=0,minimum=1,maximum=-1;bool bounded=true,rates=true;
        int boundaries=0;double lastPhase=walk.phase();
        for(int i=0;i<int(sr*30);++i)
        {
            const double value=walk.next();bounded=bounded&&std::isfinite(value)&&std::abs(value)<=1;
            rates=rates&&walk.rate()>=.1&&walk.rate()<=.9;
            maxStep=std::max(maxStep,std::abs(value-previous));previous=value;
            minimum=std::min(minimum,value);maximum=std::max(maximum,value);
            if(walk.phase()<lastPhase)++boundaries;lastPhase=walk.phase();
        }
        CHECK(bounded);CHECK(rates);CHECK(maxStep<3/sr);CHECK(boundaries>=3);CHECK(maximum-minimum>.1);
    }
}
LUTHIER_TEST(RefPedalRandomWalkLfo, resetAndPartitionsAreBitExact)
{
    RandomWalkLfo a,b,c;a.prepare(48000,73);b.prepare(48000,73);c.prepare(48000,74);
    std::array<double,8192> reference{};for(auto& v:reference)v=a.next();
    int position=0;bool equal=true,different=false;
    for(int n:{1,4,16,64,512,2048,37,481,5029})for(int i=0;i<n;++i)
    {
        if(position==int(reference.size()))break;
        equal=equal&&b.next()==reference[size_t(position)];different=different||c.next()!=reference[size_t(position)];++position;
    }
    CHECK(position==int(reference.size()));CHECK(equal);CHECK(different);
    a.reset();for(double v:reference)CHECK(a.next()==v);
    // Changing the allowed speed does not reset position or current value.
    double phase=a.phase();a.setRateRange(.2,.4);CHECK(a.phase()==phase);CHECK(a.rate()>=.2&&a.rate()<=.4);
    double before=a.next();a.setRateRange(.1,.9);CHECK(std::abs(a.next()-before)<3./48000);
}
LUTHIER_TEST(RefPedalRandomWalkLfo, segmentRemainderPreservesPhysicalTime)
{
    // Independent elapsed-seconds scheduler with long-double accumulation.
    // Wide rates expose carrying normalized old-rate phase into a new segment.
    RandomWalkLfo walk;walk.setRateRange(20,100);walk.prepare(8000,73);
    luthier::reference::common::NoiseGen random(73);
    long double elapsed=0,previous=0,target=.8L*random.next(),rate=20+80*(.5L+.5L*random.next());
    double worst=0;
    for(int i=0;i<80000;++i)
    {
        const long double phase=elapsed*rate,curve=phase*phase*(3-2*phase);
        const double expected=double(previous+(target-previous)*curve);
        worst=std::max(worst,std::abs(walk.next()-expected));
        elapsed+=1.L/8000;
        if(elapsed>=1/rate){elapsed-=1/rate;previous=target;target=std::clamp(previous+.8L*random.next(),-1.L,1.L);rate=20+80*(.5L+.5L*random.next());}
    }
    CHECK(worst<1e-10);
}
