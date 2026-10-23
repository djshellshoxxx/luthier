#pragma once
#include "../../DSP/Common/Oversampler.h"
#include "Smoothers.h"
namespace luthier::reference::common {
class OversamplerAdapter {
public:
 void prepare(double sr,int policy) noexcept {core.prepare(sampleRate(sr),policy<=0?2:policy==1?4:8);}
 void prepareFactor(double sr,int factor) noexcept {core.prepare(sampleRate(sr),factor);}
 void reset() noexcept {core.reset();}
 int factor() const noexcept {return core.getFactor();}
 // Existing allpass core has frequency-dependent group delay: nominal DC delay only.
 int latencySamples() const noexcept {return core.getLatencySamples();}
 double rate() const noexcept {return core.getOversampledRate();}
 template<class F> double process(double x,F&& shaper) noexcept {return safe(core.processSample(std::clamp(safe(x),-1e6,1e6),[&](double y){return safe(shaper(y));}));}
private: luthier::Oversampler core;
};
}
