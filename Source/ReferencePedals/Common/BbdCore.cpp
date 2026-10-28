// OWNER: WP14
#include "BbdCore.h"
namespace luthier::reference::common
{
void BbdCore::prepare(double sampleRate,double maximumDelayMs,double preHz,double postHz)
{
    sr=common::sampleRate(sampleRate);maxDelay=std::clamp(common::safe(maximumDelayMs),1.,100.);
    delay.prepare(int(sr*maxDelay*.001)+8);saturation.prepareFactor(sr,sr<=48000 ? 2:1);
    setFilters(preHz,postHz);reset();
}
void BbdCore::setFilters(double preHz,double postHz) noexcept
{
    // Fourth-order Butterworth pole pairs, estimated CE-family bandwidth.
    pre[0].setLowpass(sr,preHz,.541196100146197);pre[1].setLowpass(sr,preHz,1.30656296487638);
    post[0].setLowpass(sr,postHz,.541196100146197);post[1].setLowpass(sr,postHz,1.30656296487638);
}
void BbdCore::reset(uint32_t seed) noexcept
{
    delay.reset();saturation.reset();for(auto& f:pre)f.reset();for(auto& f:post)f.reset();
    noise.reset(seed);clockPhase=held=previousWet=0;
}
double BbdCore::clockHz(double delayMs,int stages) noexcept
{
    return double(std::max(1,stages))/(2*.001*std::clamp(common::safe(delayMs),.01,1000.));
}
double BbdCore::process(double input,double delayMs,double noiseAmount,bool clocked) noexcept
{
    double x=common::safe(input);for(auto& f:pre)x=f.process(x);
    x=saturation.process(x,[this](double v)
    {
        if(clipOnset<=0)return clipLevel*common::mildTanh(v/clipLevel);
        // E51 estimated CE2 headroom: unity small-signal gain up to -6 dBFS,
        // C2-continuous compression thereafter, preserving the original ceiling.
        const double magnitude=std::abs(v),span=clipLevel-clipOnset;
        return magnitude<=clipOnset?v:std::copysign(clipOnset+span*common::mildTanh((magnitude-clipOnset)/span),v);
    });delay.push(x);
    const double ms=std::clamp(common::safe(delayMs),.1,maxDelay);
    double wet=delay.read(std::max(1.,sr*.001*ms-saturation.latencySamples()));
    const double unheld=wet;
    if(clocked)
    {
        const double clocksPerSample=clockHz(ms)/sr;
        clockPhase+=clocksPerSample;
        if(clockPhase>=1)
        {
            clockPhase-=std::floor(clockPhase);
            // Reconstruct the most recent clock event between host samples.
            // Even above host rate, its ZOH age is generally nonzero.
            const double age=clockPhase/clocksPerSample;
            held=wet+(previousWet-wet)*age;
        }
        wet=held;
    }
    previousWet=unheld;
    if(noiseAmount>0)wet+=noise.next()*std::clamp(common::safe(noiseAmount),0.,1.)*.000316227766016838;
    for(auto& f:post)wet=f.process(wet);
    return common::safe(wet);
}
}
