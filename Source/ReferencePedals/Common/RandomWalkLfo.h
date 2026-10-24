// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include "NoiseGen.h"
namespace luthier::reference::common
{
// Bounded random walk between cubic-smoothed targets. Segment rates are drawn
// from the configured Hz interval. All scheduling uses the sample clock; there
// is no block-dependent random draw, allocation, lock or lazy initialization.
class RandomWalkLfo
{
public:
    void prepare(double sampleRate,uint32_t instanceSeed=1) noexcept
    {
        sr=common::sampleRate(sampleRate);seed=instanceSeed;reset();
    }
    void reset() noexcept
    {
        random.reset(seed);position=0;previous=target=0;chooseTarget();
    }
    void setRateRange(double minimumHz,double maximumHz) noexcept
    {
        minimum=std::clamp(common::safe(minimumHz),.001,100.);
        maximum=std::clamp(common::safe(maximumHz),minimum,100.);
        currentRate=std::clamp(currentRate,minimum,maximum);
        increment=currentRate/sr;
    }
    double next() noexcept
    {
        const double curve=position*position*(3-2*position);
        const double value=previous+(target-previous)*curve;
        position+=increment;
        if(position>=1)
        {
            const double remainderSeconds=(position-1)/currentRate;
            previous=target;chooseTarget();position=remainderSeconds*currentRate;
        }
        return value;
    }
    double phase() const noexcept {return position;}
    double rate() const noexcept {return currentRate;}
private:
    void chooseTarget() noexcept
    {
        target=std::clamp(previous+.8*random.next(),-1.,1.);
        currentRate=minimum+(maximum-minimum)*(.5+.5*random.next());
        increment=currentRate/sr;
    }
    NoiseGen random;
    uint32_t seed=1;
    double sr=48000,minimum=.1,maximum=.9,currentRate=.5,increment=.5/48000;
    double position=0,previous=0,target=0;
};
}
