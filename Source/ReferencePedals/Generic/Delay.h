// OWNER: WP10
#pragma once
#include "../PedalBase.h"
#include "../Common/FractionalDelay.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "../Common/NoiseGen.h"
#include "../Common/Lfo.h"
namespace luthier::reference::generic
{
class Delay final : public PedalBase<14>
{
public:
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    Delay();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    bool processWithWetResidual(double*,double*,double*,double*,int) noexcept override;
    void setEnabled(bool) noexcept override;
    void setServices(const PedalServices& s) override { seed=s.instanceSeed; }
    void setTime(const TimeInfo&) noexcept override;
    float getTailEnergyDb() const noexcept override;
    void saveExtra(juce::var&) const override;
    void loadExtra(const juce::var&) override;
private:
    void render(double*,double*,double*,double*,int) noexcept;
    void onTrigger(int) noexcept override;
    double desiredTime() const noexcept;
    common::FractionalDelay lines[2];
    common::Biquad lowpass[2];
    common::OnePole highpass[2];
    common::Lfo wow,flutter;
    common::NoiseGen noise;
    uint32_t seed=1;
    static_assert(std::atomic<double>::is_always_lock_free,"RT tap snapshots require lock-free double atomics");
    std::atomic<double> publishedTapMs{500};
    double drift=0,driftTarget=0,driftPhase=0;
    double timeCoefficient=0,driftCoefficient=0,energyCoefficient=0,lastCutoff=-1;
    int lastMode=-1;
    TimeInfo time;
    double sr=48000,currentMs=380,oldMs=380,targetMs=380,tapMs=500,tapBlend=1,inputGain=1,tailGain=1,holdBlend=0,energy=0;
    bool enabled=true;
    int64_t samples=0,lastTap=-1;
};
}
