// OWNER: WP12
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/DualGainLegStage.h"
#include "../Common/NewtonClipper.h"
#include "../Common/NoiseGen.h"
#include "../Common/OnePole.h"
namespace luthier::reference::models
{
class ProcoRat final : public common::CircuitPedal<11>
{
  public:
    ProcoRat();
    static const PedalDesc &descriptor() noexcept;
    static double audioPot(double x) noexcept
    {
        return std::expm1(std::log(100.) * std::clamp(x, 0., 1.)) / 99;
    }
    static double toneCutoff(double p) noexcept
    {
        return 1 / (2 * common::pi * (1500 + 1e5 * audioPot(p)) * 3.3e-9);
    }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed; }
    void setParam(int, float) noexcept override;
    void onMidi(const MidiEvent &) noexcept override;
    void setExpression(int, float) noexcept override;

  private:
    void update() noexcept;
    struct Voice
    {
        common::DualGainLegStage amplifier;
        common::OnePole gbw, coupling, tone;
        double slew = 0;
    };
    std::array<std::array<Voice, 3>, 2> voices;
    std::array<common::OnePole, 2> inputHp;
    std::array<common::NewtonClipper, 2> diode;
    std::array<double, 3> weights{1, 0, 0}, step{};
    common::NoiseGen noise;
    uint32_t seed = 1;
    int variant = 0, remaining = 0;
    double drive = .5, filter = .5, volume = .5, pressure = 0, pressureTarget = 0, expression = 0,
           controlCoefficient = 0;
    bool expressionActive = false;
};
} // namespace luthier::reference::models
