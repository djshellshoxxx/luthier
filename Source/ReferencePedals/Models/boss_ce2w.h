// OWNER: WP14
#pragma once
#include "../PedalBase.h"
#include "../Common/BbdCore.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "../Common/Lfo.h"
#include <array>
namespace luthier::reference::models
{
class BossCe2w final : public PedalBase<9>
{
public:
    BossCe2w();static const PedalDesc& descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    void prepare(double,int,int) override;void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void processBypass(double*,double*,int) noexcept override;
    void setServices(const PedalServices& s) override {seed=s.instanceSeed;}
    void setTime(const TimeInfo&) noexcept override;
    void onTransport(TransportEvent,int) noexcept override;
    void setExpression(int,float) noexcept override;
    double modulationPhase() const noexcept {return lfo.phase();}
private:
    friend struct ChorusMemoryTestAccess;
    common::BbdCore wet[3];common::Biquad emphasis[3],deemphasis[3];common::OnePole inputHp,bypassHp[2];
    common::Lfo lfo;TimeInfo time;uint32_t seed=1;
    double lastRate=-1,cachedRate=1;
    double sr=48000,modeBlend=1,stereoBlend=1;
    std::array<double,3> modeWeights{1,0,0},modeOrigin{1,0,0};
    int mode=0;
};
}
