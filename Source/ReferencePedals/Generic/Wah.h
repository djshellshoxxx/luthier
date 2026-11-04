// OWNER: WP9
#pragma once
#include "../PedalBase.h"
#include "../Common/TptSvf.h"
#include "../Common/EnvelopeFollower.h"
#include "ModulationSupport.h"
namespace luthier::reference::generic
{
class Wah final : public PedalBase<10>
{
public:
    static PreparedMemoryRequirement prepareMemory(double sr,int block,int policy,const float* params) noexcept {return fixedPreparedMemory<Wah>(sr,block,policy,params);}
    uint64_t getPreparedMemoryBytes() const noexcept override {return sizeof(*this);}
    Wah();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setExpression(int,float) noexcept override;
    // E41: virtual on current IPedal, compatible with the earlier frozen header.
    bool consumeEngageToggle() noexcept {const bool result=toeRequest;toeRequest=false;return result;}
private:
    common::TptSvf filter[2];
    common::EnvelopeFollower envelope;
    detail::ModulationClock clock;
    double sr=48000,position=.5;
    bool toeArmed=true,toeRequest=false;
};
}
