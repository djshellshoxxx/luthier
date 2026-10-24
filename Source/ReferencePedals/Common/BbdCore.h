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
    uint64_t ownedMemoryBytes() const noexcept {return delay.ownedMemoryBytes();}
    void prepare(double sampleRate,double maximumDelayMs=40,double preHz=6500,double postHz=5500);
    void reset(uint32_t seed=1) noexcept;
    void setClipLevel(double level) noexcept {clipLevel=std::clamp(common::safe(level),.1,16.);clipOnset=std::min(clipOnset,clipLevel*.99); }
    void setClipOnset(double level) noexcept {clipOnset=std::clamp(common::safe(level),0.,clipLevel*.99);}
    void setFilters(double preHz,double postHz) noexcept;
    double process(double input,double delayMs,double noiseAmount=0,bool clocked=false) noexcept;
    // A model may share this prefix only between voices with identical input,
    // coefficients and reset history. Each voice still advances its own suffix.
    double processInput(double input) noexcept;
    double processPreparedInput(double filteredInput,double delayMs,double noiseAmount=0,bool clocked=false) noexcept;
    static double clockHz(double delayMs,int stages=1024) noexcept;
private:
    FractionalDelay delay;
    Biquad pre[2],post[2];
    LowLatencyOversampler saturation;
    NoiseGen noise;
    double sr=48000,maxDelay=40,clockPhase=0,held=0,previousWet=0,clipLevel=1,clipOnset=0;
};
}
