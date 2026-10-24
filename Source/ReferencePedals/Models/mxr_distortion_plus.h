// OWNER: WP12
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/LinearGainStage.h"
#include "../Common/NewtonClipper.h"
#include "../Common/NoiseGen.h"
#include "../Common/OnePole.h"
namespace luthier::reference::models
{
class MxrDistortionPlus final : public common::CircuitPedal<7>
{
  public:
    MxrDistortionPlus();
    static const PedalDesc &descriptor() noexcept;
    static double driveResistance(double d) noexcept
    {
        return 10000 + 1e6 * std::expm1(5 * std::clamp(d, 0., 1.)) / std::expm1(5.);
    }
    static double outputGain(double p) noexcept
    {
        p = std::clamp(p, 0., 1.);
        return p <= .5 ? .2 * p : 1.8 * p - .8;
    }
    bool qualityAvailable(int q) const noexcept { return q == 1; }
    void setParam(int, float) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed; }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;

  private:
    void update() noexcept;
    std::array<common::LinearGainStage, 2> amplifier;
    std::array<common::OnePole, 2> inputHp, gbw, postClip;
    std::array<common::NewtonClipper, 2> diode;
    common::LinearSmoother variant;
    common::NoiseGen noise;
    uint32_t seed = 1;
    double noiseScale = 0;
};
} // namespace luthier::reference::models
