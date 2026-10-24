// OWNER: WP13
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/FractionalDelay.h"
#include "../Common/NoiseGen.h"
#include "ehx_muff_core.h"
namespace luthier::reference::models
{
class EhxMuffWicker final : public common::CircuitPedal<15>
{
  public:
    EhxMuffWicker();
    static const PedalDesc &descriptor() noexcept;
    bool controlAvailable(int i) const noexcept { return i >= 0 && i < 15 && i != 10 && i != 14; }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed; }

  private:
    void update() noexcept;
    std::array<MuffCore, 2> core;
    std::array<common::FractionalDelay, 2> howlDelay;
    std::array<common::OnePole, 2> howlHp, howlLp;
    common::LinearSmoother toneSwitch, wickerSwitch;
    common::NoiseGen noise;
    uint32_t seed = 1;
    double toneMix = 1, wicker = 0;
};
} // namespace luthier::reference::models
