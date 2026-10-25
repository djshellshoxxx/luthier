// OWNER: WP9
#pragma once
#include "../PedalBase.h"
#include "../Common/FractionalDelay.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "ModulationSupport.h"
namespace luthier::reference::generic
{
class Chorus final : public PedalBase<9>
{
public:
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    Chorus();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setTime(const TimeInfo&) noexcept override;
private:
    common::FractionalDelay delay;
    common::OnePole highpass;
    common::Biquad tone[2];
    detail::ModulationClock clock;
    TimeInfo time;
    double sr=48000, modeBlend=1;
    int mode=0, previousMode=0;
};
}
