// OWNER: WP9
#pragma once
#include "../PedalBase.h"
#include "../Common/Biquad.h"
#include "ModulationSupport.h"
#include <array>
namespace luthier::reference::generic
{
class Tremolo final : public PedalBase<10>
{
public:
    Tremolo();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setTime(const TimeInfo&) noexcept override;
private:
    void onTrigger(int) noexcept override;
    detail::ModulationClock clock;
    TimeInfo time;
    common::Biquad low[2],high[2];
    double sr=48000,opto[2]{},waveBlend=1;
    int wave=0,oldWave=0;
    int64_t samples=0,lastTap=-1;
    std::array<double,3> intervals{};
    int intervalCount=0,intervalIndex=0;
};
}
