// OWNER: WP14
#pragma once
#include "FractionalDelay.h"
#include "Biquad.h"
#include "NoiseGen.h"
#include "LowLatencyOversampler.h"
#include "MildTanh.h"
namespace luthier::reference::common
{
// Behavioral BBD wet path. Clocked mode is a host-rate sample/hold approximation,
// not a transistor-level switched-capacitor MN3007 simulation.
class BbdCore
{
public:
    void prepare(double sampleRate,double maximumDelayMs=40,double preHz=6500,double postHz=5500);
    void reset(uint32_t seed=1) noexcept;
    void setClipLevel(double level) noexcept {clipLevel=std::clamp(common::safe(level),.1,16.);clipOnset=std::min(clipOnset,clipLevel*.99);transferCurvature=std::min(transferCurvature,.25/clipLevel); }
    void setClipOnset(double level) noexcept {clipOnset=std::clamp(common::safe(level),0.,clipLevel*.99);}
    // Opt-in even-order curvature of the charge-transfer characteristic, per unit of
    // signal. It acts in clocked mode only (law in BbdCore.cpp). Zero, the default,
    // leaves every output bit-identical to a core without it. The bound keeps the
    // curved characteristic monotonic at any clip level.
    void setTransferCurvature(double perUnit) noexcept {transferCurvature=std::clamp(common::safe(perUnit),0.,.25/clipLevel);}
    void setFilters(double preHz,double postHz) noexcept;
    double process(double input,double delayMs,double noiseAmount=0,bool clocked=false) noexcept;
    static double clockHz(double delayMs,int stages=1024) noexcept;
    // Heap storage only; filters/oversampler are inline in the owning model.
    uint64_t ownedMemoryBytes() const noexcept {return delay.ownedMemoryBytes();}
private:
    FractionalDelay delay;
    Biquad pre[2],post[2];
    LowLatencyOversampler saturation;
    NoiseGen noise;
    double sr=48000,maxDelay=40,clockPhase=0,held=0,previousWet=0,clipLevel=1,clipOnset=0;
    double transferCurvature=0,curvatureMean=0,curvatureMeanCoefficient=0;
};
}
