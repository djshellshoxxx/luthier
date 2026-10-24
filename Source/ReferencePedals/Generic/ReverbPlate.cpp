// OWNER: WP10
#include "ReverbPlate.h"
#include <cmath>
#include <algorithm>
namespace luthier::reference::generic
{
void ReverbPlate::prepare(double sampleRate,uint32_t seed)
{
    sr=sampleRate; settingsValid=false;
    for(auto& d:inputDiffusion) d.prepare(int(sr*.04)+8);
    for(auto& d:modDiffusion) d.prepare(int(sr*.07)+8);
    for(auto& d:lateDiffusion) d.prepare(int(sr*.16)+8);
    for(auto& d:firstDelay) d.prepare(int(sr*.27)+8);
    for(auto& d:lastDelay) d.prepare(int(sr*.23)+8);
    lfo.prepare(sr,seed);reset();set(2.2,1,0,.25,0);
}
void ReverbPlate::reset() noexcept
{
    for(auto& d:inputDiffusion) d.reset(); for(auto& d:modDiffusion) d.reset();
    for(auto& d:lateDiffusion) d.reset(); for(auto& d:firstDelay) d.reset(); for(auto& d:lastDelay) d.reset();
    for(auto& f:damping) f.reset();feedback[0]=feedback[1]=0;lfo.reset();
}
void ReverbPlate::set(double decay,double size,double tone,double modulation,double frozen) noexcept
{
    const std::array<double,5> settings{decay,size,tone,modulation,frozen};
    if(settingsValid && settings==lastSettings)return;
    lastSettings=settings;settingsValid=true;
    scale=std::clamp(size,.3,1.7)*sr/29761;
    decayGain=std::pow(10,-3*((4453+1800+3720)*scale/sr)/std::clamp(decay,.2,12.));
    mod=std::clamp(modulation,0.,1.); freeze=std::clamp(frozen,0.,1.);
    for(auto& f:damping) f.setLowpass(sr,2000*std::pow(8,(std::clamp(tone,-1.,1.)+1)*.5));
}
std::array<double,2> ReverbPlate::process(double input) noexcept
{
    auto allpass=[](common::FractionalDelay& line,double x,double length,double gain) noexcept
    {
        const double delayed=line.read(std::max(1.,length-1)),y=delayed-gain*x;
        line.push(x+gain*y); return y;
    };
    constexpr int inputLengths[]{142,107,379,277};
    double x=input;
    for(int i=0;i<4;++i) x=allpass(inputDiffusion[i],x,std::round(inputLengths[i]*sr/29761),i<2 ? .75:.625);
    constexpr int modLength[]{672,908},firstLength[]{4453,4217},lateLength[]{1800,2656},lastLength[]{3720,3163};
    double next[2]{},out[2]{};const double movement=lfo.next();
    for(int c=0;c<2;++c)
    {
        const double gain=decayGain*(1-freeze)+freeze;
        double y=x+feedback[1-c]*gain;
        y=allpass(modDiffusion[c],y,std::round(modLength[c]*scale)+(c==0 ? 1:-1)*mod*(1-freeze)*sr*.001*movement,-.7);
        const double delayed=firstDelay[c].read(std::round(firstLength[c]*scale)-1); firstDelay[c].push(y);
        y=damping[c].process(delayed)*(1-freeze)+delayed*freeze;
        y=allpass(lateDiffusion[c],y,std::round(lateLength[c]*scale),.5);
        next[c]=lastDelay[c].read(std::round(lastLength[c]*scale)-1); lastDelay[c].push(y);
        out[c]=.5*(delayed+next[c]);
    }
    feedback[0]=next[0];feedback[1]=next[1];return {out[0],out[1]};
}
}
