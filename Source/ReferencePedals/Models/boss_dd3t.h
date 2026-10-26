// OWNER: WP14
#pragma once
#include "../PedalBase.h"
#include "../Common/FractionalDelay.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "../Common/LowLatencyOversampler.h"
#include "../Common/NoiseGen.h"
#include "../Common/MildTanh.h"
#include <vector>
namespace luthier::reference::models
{
class BossDd3t final : public PedalBase<19>
{
public:
    BossDd3t();static const PedalDesc& descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    void prepare(double,int,int) override;void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    bool processWithWetResidual(double*,double*,double*,double*,int) noexcept override;
    void setParam(int,float) noexcept override;
    void setEnabled(bool e) noexcept override {enabled=e;}
    void setServices(const PedalServices& s) override {seed=s.instanceSeed;}
    void setTime(const TimeInfo&) noexcept override;
    void onFoot(const FootEvent&) noexcept override;
    void setExpression(int,float) noexcept override;
    float getTailEnergyDb() const noexcept override;
    PedalUiMeters readUiMeters() const noexcept override;
    void saveExtra(juce::var&) const override;void loadExtra(const juce::var&) override;
    double delayMilliseconds() const noexcept {return currentMs;}
    bool shortLoopActive() const noexcept {return holding;}
    bool footControlsEngagement() const noexcept override {return false;}
    bool consumeEngageToggle() noexcept override {const bool request=engageRequest;engageRequest=false;return request;}
private:
    friend struct BossDd3tMemoryTestAccess; // Observe owned capacities without prescribing an allocation policy.
    void render(double*,double*,double*,double*,int) noexcept;
    void onParamChanged(int,float) noexcept override;void onTrigger(int) noexcept override;
    void tap() noexcept;void beginHold() noexcept;double targetTime() const noexcept;
    common::FractionalDelay line[2];common::OnePole highpass[2];common::Biquad pre[2][2],post[2][2];
    common::LowLatencyOversampler saturation[2];common::NoiseGen noise[2];
    std::vector<double> history[2],loop[2];size_t historyHead=0,loopLength=0;double loopPosition=0,referenceMs=0;
    // E55: one coherent lock-free snapshot. Magnitude is interval (always >0),
    // sign encodes hasTapped without quantizing any interval mantissa bits.
    static_assert(std::atomic<double>::is_always_lock_free,"RT tap snapshots require lock-free double atomics");
    std::atomic<double> publishedTap{500};
    void publishTap() noexcept {publishedTap.store(hasTapped?-tapInterval:tapInterval,std::memory_order_release);}
    double hostBpm=120,cachedTargetMs=320;
    double fadeSeconds=.005,quantScale=2048,inverseQuantScale=1./2048;int lastBits=-1;
    double timeCoefficient=0,energyCoefficient=0,lastLevel=-1,lastFeedback=-1,cachedLevel=0,cachedFeedback=0;
    double sr=48000,currentMs=320,tapInterval=500,syncMs=500,inputGain=1,wetGain=1,holdMix=0,energy=0;
    double feedback[2]{},heldQuant[2]{},previousQuant[2]{};
    double tapIntervals[3]{},converterPhase=0,force=0;
    int tapCount=0,tapIndex=0,filterCountdown=0;double lastCutoff=0,lastBeat=-1;
    uint32_t seed=1;int64_t samples=0,lastTap=-1,downAt=-1;
    bool engageRequest=false,downWasTap=false;
    bool enabled=true,holding=false,holdActionFired=false,hasTapped=false,hasSync=false;
};
}
