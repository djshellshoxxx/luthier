// OWNER: WP14
#pragma once
#include "../PedalBase.h"
#include "../Common/BbdCore.h"
#include "../Common/Lfo.h"
#include "../Common/OnePole.h"
#include <array>
namespace luthier::reference::models
{
class BossCe2 final : public PedalBase<14>
{
public:
    BossCe2();static const PedalDesc& descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    void prepare(double,int,int) override;void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void processBypass(double*,double*,int) noexcept override;
    void setServices(const PedalServices& s) override {seed=s.instanceSeed;}
    void setTime(const TimeInfo&) noexcept override;
    void onTransport(TransportEvent,int) noexcept override;
    void onFoot(const FootEvent&) noexcept override;
    void setExpression(int,float) noexcept override;
    double modulationPhase() const noexcept {return lfo.phase();}
private:
    friend struct ChorusMemoryTestAccess;
    common::BbdCore wet[2];common::OnePole inputHp[2],bypassHp[2];common::Lfo lfo;
    TimeInfo time;uint32_t seed=1;
    double lastRate=-1,cachedRate=1,triangleCoefficient=0;
    double sr=48000,triangle=0,force=0;
    int variant=0;int64_t samples=0,lastTap=-1;
    std::array<double,3> taps{};int tapCount=0,tapIndex=0;
};
}
