// OWNER: WP11
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/LinearGainStage.h"
#include "../Common/NewtonClipper.h"
#include "../Common/NoiseGen.h"
#include "../Common/OnePole.h"
namespace luthier::reference::models
{
// Unregistered modern-revision circuit candidate. Estimated transistor rail
// law and unloaded tone branches are explicitly not a verified hardware model.
class BossDs1 final : public common::CircuitPedal<8>
{
  public:
    BossDs1();
    static const PedalDesc &descriptor() noexcept;
    bool controlAvailable(int i) const noexcept { return i >= 0 && i < 8 && i != 6; }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void processBypass(double *, double *, int) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed; }

  private:
    std::array<common::OnePole, 2> bypassHp, bypassLp;
    void update() noexcept;
    std::array<common::LinearGainStage, 2> amplifier;
    std::array<common::NewtonClipper, 2> diode;
    std::array<common::OnePole, 2> inputHp, boosterHp, boosterOutputHp, ampHp, postClip, toneLp, toneHp,
        outputHp;
    common::NoiseGen noise;
    uint32_t seed = 1;
};
} // namespace luthier::reference::models
