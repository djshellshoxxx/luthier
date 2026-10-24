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
