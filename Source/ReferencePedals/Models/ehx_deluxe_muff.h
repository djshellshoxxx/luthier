// OWNER: WP13
#pragma once
#include "../Common/Biquad.h"
#include "../Common/CircuitPedal.h"
#include "../Common/EnvelopeFollower.h"
#include "../Common/FractionalDelay.h"
#include "../Common/NewtonClipper.h"
#include "ehx_muff_core.h"
namespace luthier::reference::models
{
// Separate estimated Deluxe topology and expressly behavioural attack/gate.
// Unregistered: no proprietary circuit trace or hardware calibration exists.
class EhxDeluxeMuff final : public common::CircuitPedal<18>
{
  public:
    EhxDeluxeMuff();
    static const PedalDesc &descriptor() noexcept;
    bool controlAvailable(int i) const noexcept { return i >= 0 && i < 18; }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void processBypass(double *, double *, int) noexcept override;
    void setExpression(int, float) noexcept override;

  private:
    struct Channel
    {
        common::OnePole inBuffer, boostHp, coupling1, coupling2, clipLp1, clipLp2, outHp, attackHp, bypassHp;
        common::Biquad bass, mids;
        MuffTone tone;
        common::EnvelopeFollower detector, fast, slow;
        common::FractionalDelay attackDelay, detectorDelay;
        bool open = true;
        int below = 0;
        double gateGain = 1;
    };
    void update() noexcept;
    std::array<Channel, 2> channels;
    common::NewtonClipper diode;
    common::LinearSmoother midsSwitch, bassSwitch;
    double midsAmount = 0, bassAmount = 0, boostGain = 1, gateAttack = 0, gateRelease = 0,
           expressionFrequency = 1245, expressionCoefficient = 0;
};
} // namespace luthier::reference::models
