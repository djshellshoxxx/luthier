// OWNER: WP9
#pragma once
#include "../PedalBase.h"
#include "ModulationSupport.h"
#include <array>
namespace luthier::reference::generic
{
class Phaser final : public PedalBase<8>
{
public:
    Phaser();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setTime(const TimeInfo&) noexcept override;
private:
    detail::ModulationClock clock;
    TimeInfo time;
    std::array<std::array<double,12>,2> state {};
    std::array<double,2> feedback {};
    double sr=48000, stageBlend=1;
    int stages=4, previousStages=4;
};
}
