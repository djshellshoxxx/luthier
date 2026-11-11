// OWNER: WP16/WP24
#include "Rc5Rhythm.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::common
{
namespace
{
constexpr double rhythmPi=3.1415926535897932384626433832795;
struct Pattern {const char* name;int bars;Rc5Rhythm::Grid a,b;};
constexpr Pattern patterns[]{
#include "../Models/rc5_patterns.inc"
};
static_assert(std::size(patterns)==Rc5Rhythm::patternCount);
double finite(double x) noexcept{return std::isfinite(x)&&std::abs(x)>=1e-30?x:0;}
}
const Rc5Rhythm::Grid& Rc5Rhythm::grid(int p,int v) noexcept
{const auto& item=patterns[std::clamp(p,1,57)-1];return std::clamp(v,0,1)?item.b:item.a;}
const char* Rc5Rhythm::name(int p) noexcept{return patterns[std::clamp(p,1,57)-1].name;}
int Rc5Rhythm::bars(int p) noexcept{return patterns[std::clamp(p,1,57)-1].bars;}
void Rc5Rhythm::prepare(double sr,uint32_t seed) noexcept
{
    rate=std::isfinite(sr)&&sr>=8000&&sr<=768000?sr:48000;
    initialSeed=seed?seed:0x51a7u;
    // Synthesis-only1Hz DC removal; dry remains outside this filter. Keep it
    // before the existing level/stop smoother so it adds no independent tail.
    dc.prepare(rate,1);
    levelCoefficient=std::exp(-1/(.020*rate));
    constexpr double duration[]{.12,.14,.04,.20,.10,.035,.18,.55};
    constexpr double frequency[]{150,180,0,0,1500,1600,260,0};
    constexpr double floors[]{50,180,0,0,1500,1600,90,0};
    constexpr double noiseMix[]{0,.72,1,1,1,.2,.05,1};
    constexpr double pitches[]{1,.82,1.13,1.2,.69,1.05,.91};
    constexpr double lengths[]{1,1.22,.82,.58,.75,1.1,1.35};
    for(int k=0;k<7;++k)for(int i=0;i<8;++i)
    {
        auto& c=coefficients[k][i];double seconds=duration[i]*lengths[k];
        c.decay=std::exp(-7/(seconds*rate));c.frequency=frequency[i]*pitches[k];c.frequencyFloor=floors[i]*pitches[k];
        c.frequencyDecay=std::exp(-1/(.025*rate));c.noiseLow=std::exp(-2*rhythmPi*(i==1?4000:7000)/rate);
        c.noiseMix=noiseMix[i];c.bodyMix=1-noiseMix[i];c.duration=(int)std::ceil(seconds*rate);
    }
    reset();setTempo(120);
}
void Rc5Rhythm::reset() noexcept
{voices={};dc.reset();random=initialSeed;phase=level=0;step=bar=0;running=false;needsTrigger=true;applySelection();}
void Rc5Rhythm::applySelection() noexcept
{pattern=pendingPattern;variation=pendingVariation;kit=pendingKit;bar%=bars(pattern);}
void Rc5Rhythm::select(int p,int v,int k) noexcept
{pendingPattern=std::clamp(p,1,57);pendingVariation=std::clamp(v,0,1);pendingKit=std::clamp(k,1,7);if(!running)applySelection();}
void Rc5Rhythm::setTempo(double bpm) noexcept
{bpm=std::isfinite(bpm)?std::clamp(bpm,40.,300.):120.;increment=bpm/(15*rate);}
void Rc5Rhythm::setLevel(double percent) noexcept
{percent=std::clamp(finite(percent),0.,100.);targetLevel=(percent*.01)*(percent*.01);}
void Rc5Rhythm::locate(double quarterNotes) noexcept
{
    quarterNotes=std::clamp(finite(quarterNotes),-1e12,1e12);
    const double sixteenths=quarterNotes*4,whole=std::floor(sixteenths);
    phase=sixteenths-whole;step=(int)std::fmod(whole,16.);if(step<0)step+=16;
    needsTrigger=phase<1e-12;if(step==0&&needsTrigger)applySelection();
    bar=(int)std::fmod(std::floor(whole/16),bars(pattern));if(bar<0)bar+=bars(pattern);
}
void Rc5Rhythm::setRunning(bool on) noexcept
{if(on&&!running){phase=0;step=bar=0;needsTrigger=true;applySelection();}running=on;}
double Rc5Rhythm::noise() noexcept
{random^=random<<13;random^=random>>17;random^=random<<5;return (double)random*(2./4294967295.)-1.;}
void Rc5Rhythm::trigger() noexcept
{
    const auto& hits=grid(pattern,bar==bars(pattern)-1&&step>=12?1-variation:variation);
    for(int i=0;i<8;++i)if(hits[i]&(uint16_t)(1u<<step))
    {
        const auto& c=coefficients[kit-1][i];auto& v=voices[i];
        v.amplitude=(step%4==0?.23:.17);v.phase=0;v.frequency=c.frequency;v.noiseLow=v.body=0;v.samples=c.duration;v.kit=kit-1;
    }
}
double Rc5Rhythm::renderVoice(int i) noexcept
{
    auto& v=voices[i];if(v.samples<=0)return 0;
    const auto& c=coefficients[v.kit][i];double tonal=0,n=0;
    if(c.noiseMix>0)
    {
        n=noise();v.noiseLow=(1-c.noiseLow)*n+c.noiseLow*v.noiseLow;
        n=i==1?v.noiseLow:n-v.noiseLow;
    }
    if(c.bodyMix>0)
    {
        tonal=std::sin(v.phase);if(i==5)tonal=.5*(tonal+std::sin(v.phase*1.43));
        v.phase+=2*rhythmPi*v.frequency/rate;if(v.phase>=2*rhythmPi)v.phase-=2*rhythmPi;
        v.frequency=c.frequencyFloor+c.frequencyDecay*(v.frequency-c.frequencyFloor);
    }
    const double out=v.amplitude*(c.bodyMix*tonal+c.noiseMix*n);v.amplitude*=c.decay;
    if(--v.samples==0||v.amplitude<1e-15)v={};
    return finite(out);
}
void Rc5Rhythm::process(double* left,double* right,int n) noexcept
{
    if(!left||!right||n<=0)return;
    for(int sample=0;sample<n;++sample)
    {
        if(running&&needsTrigger){trigger();needsTrigger=false;}
        double sum=0;for(int i=0;i<8;++i)sum+=renderVoice(i);
        const double target=running?targetLevel:0;level=target+levelCoefficient*(level-target);if(std::abs(level)<1e-15)level=0;
        const double out=dc.process(sum)*level;left[sample]=std::clamp(finite(left[sample])+out,-16.,16.);right[sample]=std::clamp(finite(right[sample])+out,-16.,16.);
        if(running)
        {
            phase+=increment;
            if(phase>=1-1e-12){phase=std::max(0.,phase-1);step=(step+1)%16;needsTrigger=true;if(step==0){bar=(bar+1)%bars(pattern);applySelection();}}
        }
    }
}
}
