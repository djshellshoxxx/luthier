// OWNER: WP14
#pragma once
#include "../PedalBase.h"
#include "../Common/BbdCore.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "../Common/Lfo.h"
namespace luthier::reference::models
{
class BossCe2w final : public PedalBase<9>
{
public:
    BossCe2w();static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setEnabled(bool e) noexcept override {enabled=e;}
    void setServices(const PedalServices& s) override {seed=s.instanceSeed;}
    void setTime(const TimeInfo&) noexcept override;
    void onTransport(TransportEvent,int) noexcept override;
    void setExpression(int,float) noexcept override;
    double modulationPhase() const noexcept {return lfo.phase();}
private:
    common::BbdCore wet[3];common::Biquad emphasis[3],deemphasis[3];common::OnePole inputHp;
    common::Lfo lfo;TimeInfo time;uint32_t seed=1;bool enabled=true;
    double sr=48000,engage=1,modeBlend=1,stereoBlend=1;
    int mode=0,oldMode=0;
};
}
