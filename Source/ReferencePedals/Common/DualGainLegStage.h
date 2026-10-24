// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
namespace luthier::reference::common
{
// RAT topology: parallel series-RC gain legs, common Rf||Cf feedback.
// Linear trapezoidal network with one fixed 1kHz prewarp for all components.
class DualGainLegStage
{
  public:
    void prepare(double rate) noexcept
    {
        const double sr = sampleRate(rate), k = 2 * pi * 1000 / std::tan(pi * 1000 / sr);
        capG = k * 100e-12;
        const double capacitance[] = {4.7e-6, 2.2e-6};
        for (int i = 0; i < 2; ++i)
        {
            const double g = 1 / (k * resistance[i] * capacitance[i]);
            coefficient[i] = g / (1 + g);
        }
        reset();
    }
    void setFeedbackResistance(double rf) noexcept { feedbackG = 1 / std::clamp(safe(rf), 1e-3, 1e9); }
    void reset() noexcept
    {
        state.fill(0);
        history = 0;
    }
    double process(double input) noexcept
    {
        const double x = std::clamp(safe(input), -16., 16.);
        double current = 0;
        for (int i = 0; i < 2; ++i)
        {
            const double v = (x - state[i]) * coefficient[i], vc = state[i] + v;
            state[i] = safe(vc + v);
            current += (x - vc) / resistance[i];
        }
        const double vf = (current + history) / (feedbackG + capG);
        history = safe(2 * capG * vf - history);
        return safe(x + vf);
    }

  private:
    static constexpr double resistance[2] = {560, 47};
    std::array<double, 2> coefficient{}, state{};
    double capG = 0, feedbackG = 1. / 1000, history = 0;
};
} // namespace luthier::reference::common
