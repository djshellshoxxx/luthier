// OWNER: WP11
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/FeedbackDiodeStage.h"
#include "../Common/NoiseGen.h"
#include "../Common/OnePole.h"
namespace luthier::reference::models
{
// Unregistered circuit candidate. Feedback topology is component-derived;
// the specified tone fit, calibration and supply limits remain estimates.
class IbanezTs808 final : public common::CircuitPedal<9>
{
  public:
    IbanezTs808();
    static const PedalDesc &descriptor() noexcept;
    static double feedbackResistance(double knob) noexcept
    {
        return 51000 + 500000 * std::pow(std::clamp(knob, 0.0, 1.0), 2.5);
    }
    bool controlAvailable(int index) const noexcept
    {
        return index >= 0 && index < 9 && index != 6 && index != 8;
    }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed; }

  private:
    void update() noexcept;
    std::array<common::FeedbackDiodeStage, 2> clip;
    std::array<common::OnePole, 2> inputHp, couplingHp, gbw, postClip, toneLp, outputHp;
    std::array<common::FirstOrderShelf, 2> shelf;
    uint32_t seed = 1, random = 1;
};
} // namespace luthier::reference::models
