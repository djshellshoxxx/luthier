// OWNER: WP11
#pragma once
#include "../Common/AsymmetricFeedbackDiodeStage.h"
#include "../Common/CircuitPedal.h"
#include "../Common/OnePole.h"
#include "boss_sd1_params.h"
namespace luthier::reference::models
{
// Unregistered candidate: component feedback network plus explicitly estimated
// tone/rail/calibration. Lowcpu/eco algorithms and coloured board bypass remain
// unavailable; callers can query them rather than silently choosing a fallback.
class BossSd1 final : public common::CircuitPedal<15>
{
  public:
    BossSd1();
    static const PedalDesc &descriptor() noexcept;
    bool controlAvailable(int i) const noexcept { return i >= 0 && i < 15; }
    bool qualityAvailable(int quality) const noexcept { return quality == 1; }
    void setParam(int, float) noexcept override;
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void processBypass(double *, double *, int) noexcept override;
    void onFoot(const FootEvent &) noexcept override;
    void onMidi(const MidiEvent &) noexcept override;
    void setExpression(int, float) noexcept override;

  private:
    std::array<common::OnePole, 2> bypassHp, bypassLp;
    void controls() noexcept;
    void update() noexcept;
    std::array<std::array<common::AsymmetricFeedbackDiodeStage, 3>, 2> clip;
    std::array<common::OnePole, 2> inputHp, gbw, postClip, outputHp;
    std::array<common::FirstOrderShelf, 2> shelf;
    std::array<double, 3> weights{1, 0, 0}, weightStep{};
    int diodeTarget = 0, fadeRemaining = 0;
    double rf = 327000, tone = .5, levelGain = 0, velocity = 0, heldVelocity = 0, pressure = 0,
           pressureTarget = 0, expression = 0;
    double coefficient20 = 0, coefficient10 = 0, coefficient15 = 0, coefficient80 = 0;
    bool expressionActive = false;
};
} // namespace luthier::reference::models
