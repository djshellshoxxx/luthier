// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
namespace luthier::reference::common
{
// WP14's31-tap linear-phase halfband for mild, already-bandlimited BBD wet
// saturation. This short filter is not the drive-pedal alias-control path.
class LowLatencyOversampler
{
public:
    void prepareFactor(double sr,int requestedFactor) noexcept
    {
        baseRate=sampleRate(sr);multiplier=requestedFactor>=2?2:1;
        double sum=0;
        for(int k=0;k<15;k+=2)
        {
            const double m=k-15.;
            const double window=.42-.5*std::cos(2*pi*k/30.)+.08*std::cos(4*pi*k/30.);
            coefficients[(size_t)k/2]=std::sin(.5*pi*m)/(pi*m)*window;
            sum+=2*coefficients[(size_t)k/2];
        }
        for(auto& c:coefficients)c*=.5/sum;
        reset();
    }
    void reset() noexcept {up.fill(0);down.fill(0);upHead=downHead=0;}
    int factor() const noexcept {return multiplier;}
    int latencySamples() const noexcept {return multiplier==2?15:0;}
    double rate() const noexcept {return baseRate*multiplier;}
    template<class F> double process(double x,F&& shaper) noexcept
    {
        x=std::clamp(safe(x),-1e6,1e6);
        if(multiplier==1)return safe(shaper(x));
        const double a=safe(shaper(2*filter(up,upHead,x)));
        const double b=safe(shaper(2*filter(up,upHead,0)));
        const double result=filter(down,downHead,a);
        down[downHead]=b;downHead=(downHead+1)&63;
        return result;
    }
private:
    double filter(std::array<double,64>& data,unsigned& head,double x) noexcept
    {
        data[head]=safe(x);double result=.5*data[(head-15)&63];
        for(unsigned k=0;k<15;k+=2)result+=coefficients[k/2]*(data[(head-k)&63]+data[(head-(30-k))&63]);
        head=(head+1)&63;return safe(result);
    }
    std::array<double,8> coefficients{};
    std::array<double,64> up{},down{};
    unsigned upHead=0,downHead=0;double baseRate=48000;int multiplier=1;
};
}
