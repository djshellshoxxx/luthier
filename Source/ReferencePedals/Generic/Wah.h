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
    Wah();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setExpression(int,float) noexcept override;
private:
    common::TptSvf filter[2];
    common::EnvelopeFollower envelope;
    detail::ModulationClock clock;
    double sr=48000,position=.5;
};
}
