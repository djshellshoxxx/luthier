// OWNER: WP14
#include "BbdCore.h"
namespace luthier::reference::common
{
void BbdCore::prepare(double sampleRate,double maximumDelayMs,double preHz,double postHz)
{
    sr=common::sampleRate(sampleRate);maxDelay=std::clamp(common::safe(maximumDelayMs),1.,100.);
    delay.prepare(int(sr*maxDelay*.001)+8);saturation.prepareFactor(sr,sr<=48000 ? 2:1);
    // 15 Hz, the CE2 input buffer's ESTIMATED coupling corner, at the rate the
    // shaper really runs at.
    curvatureMeanCoefficient=1-std::exp(-2*pi*15/saturation.rate());
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
    noise.reset(seed);clockPhase=held=previousWet=curvatureMean=0;
}
double BbdCore::clockHz(double delayMs,int stages) noexcept
{
    return double(std::max(1,stages))/(2*.001*std::clamp(common::safe(delayMs),.01,1000.));
}
double BbdCore::process(double input,double delayMs,double noiseAmount,bool clocked) noexcept
{
    double x=common::safe(input);for(auto& f:pre)x=f.process(x);
    const auto headroom=[this](double v)
    {
        if(clipOnset<=0)return clipLevel*common::mildTanh(v/clipLevel);
        // E51 estimated CE2 headroom: unity small-signal gain up to -6 dBFS,
        // C2-continuous compression thereafter, preserving the original ceiling.
        const double magnitude=std::abs(v),span=clipLevel-clipOnset;
        return magnitude<=clipOnset?v:std::copysign(clipOnset+span*common::mildTanh((magnitude-clipOnset)/span),v);
    };
    if(clocked && transferCurvature>0)
    {
        // A bucket-brigade line is a single-ended charge-transfer chain, so its
        // low-level distortion is even order: the stored value is v+k*v^2 and the
        // headroom law limits that. Squaring the limited value keeps the curved
        // characteristic monotonic and the ceiling exactly where the headroom law
        // puts it, so the even harmonics fade as the line saturates. Only the
        // deviation from the uncurved law is AC-coupled (the device output is
        // capacitor-coupled): no DC is added and the linear path keeps its
        // response. It shares the headroom law's oversampled shaper, so its
        // products are band-limited the same way.
        x=saturation.process(x,[this,&headroom](double v)
        {
            const double limited=headroom(v),deviation=headroom(v+transferCurvature*limited*limited)-limited;
            curvatureMean=common::safe(curvatureMean+curvatureMeanCoefficient*(deviation-curvatureMean));
            return limited+(deviation-curvatureMean);
        });
    }
    else x=saturation.process(x,headroom);
    delay.push(x);
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
