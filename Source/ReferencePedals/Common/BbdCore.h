// OWNER: WP14
#pragma once
#include "FractionalDelay.h"
#include "Biquad.h"
#include "NoiseGen.h"
#include "LowLatencyOversampler.h"
#include "MildTanh.h"
#include "OnePole.h"
namespace luthier::reference::common
{
// Behavioral BBD wet path. Clocked mode is a host-rate sample/hold approximation,
// not a transistor-level switched-capacitor MN3007 simulation.
class BbdCore
{
public:
    void prepare(double sampleRate,double maximumDelayMs=40,double preHz=6500,double postHz=5500);
    void reset(uint32_t seed=1) noexcept;
    void setClipLevel(double level) noexcept {clipLevel=std::clamp(common::safe(level),.1,16.);clipOnset=std::min(clipOnset,clipLevel*.99); }
    void setClipOnset(double level) noexcept {clipOnset=std::clamp(common::safe(level),0.,clipLevel*.99);}
    void setFilters(double preHz,double postHz) noexcept;
    // Clocked-mode charge-transfer nonlinearity: even*(x^2-<x^2>) - odd*x^3, the
    // even term DC-free via a 5 Hz mean of x^2 (output coupling). Zero (default)
    // leaves output bit-identical to the linear core; values are per-model ESTIMATES.
    void setTransferNonlinearity(double even,double odd=0) noexcept
    {transferEven=std::clamp(common::safe(even),0.,1.);transferOdd=std::clamp(common::safe(odd),0.,.25);}
    double process(double input,double delayMs,double noiseAmount=0,bool clocked=false) noexcept;
    static double clockHz(double delayMs,int stages=1024) noexcept;
    // Heap storage only; filters/oversampler are inline in the owning model.
    uint64_t ownedMemoryBytes() const noexcept {return delay.ownedMemoryBytes();}
private:
    FractionalDelay delay;
    Biquad pre[2],post[2];
    LowLatencyOversampler saturation;
    NoiseGen noise;
    OnePole squareMean;
    double sr=48000,maxDelay=40,clockPhase=0,held=0,previousWet=0,clipLevel=1,clipOnset=0,transferEven=0,transferOdd=0;
};
}
