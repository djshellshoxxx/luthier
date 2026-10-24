// OWNER: WP10
#pragma once
#include "../PedalBase.h"
#include "../Common/FractionalDelay.h"
#include "../Common/Biquad.h"
#include "ReverbFdn.h"
#include "ReverbPlate.h"
#include "ReverbSpring.h"
namespace luthier::reference::generic
{
class Reverb final : public PedalBase<11>
{
public:
    Reverb();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setEnabled(bool e) noexcept override { enabled=e; }
    float getTailEnergyDb() const noexcept override;
private:
    ReverbFdn fdn[3]; ReverbPlate plate; ReverbSpring spring;
    common::FractionalDelay predelay;
    common::Biquad lowcut;
    double sr=48000,inputGain=1,tailGain=1,freeze=0,typeBlend=1,energy=0;
    double lastCut=-1,energyCoefficient=0,typeStep=0;
    int type=1;
    bool enabled=true;
};
}
