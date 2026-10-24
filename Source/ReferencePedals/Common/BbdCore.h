// OWNER: WP14
#pragma once
#include "FractionalDelay.h"
#include "Biquad.h"
#include "NoiseGen.h"
#include "LowLatencyOversampler.h"
namespace luthier::reference::common
{
// Behavioral BBD wet path. Clocked mode is a host-rate sample/hold approximation,
// not a transistor-level switched-capacitor MN3007 simulation.
class BbdCore
{
public:
    void prepare(double sampleRate,double maximumDelayMs=40,double preHz=6500,double postHz=5500);
    void reset(uint32_t seed=1) noexcept;
    void setClipLevel(double level) noexcept {clipLevel=std::clamp(common::safe(level),.1,16.); }
    void setFilters(double preHz,double postHz) noexcept;
    double process(double input,double delayMs,double noiseAmount=0,bool clocked=false) noexcept;
    static double clockHz(double delayMs,int stages=1024) noexcept;
private:
    FractionalDelay delay;
    Biquad pre[2],post[2];
    LowLatencyOversampler saturation;
    NoiseGen noise;
    double sr=48000,maxDelay=40,clockPhase=0,held=0,clipLevel=1;
};
}
