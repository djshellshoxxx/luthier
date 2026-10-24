// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common
{
// Trapezoidal symmetric feedback diode network. Component defaults follow the
// TS808 specification; this primitive is not a complete or calibrated pedal.
// Unlike a memoryless shaper, the feedback capacitor participates in its KCL.
class FeedbackDiodeStage
{
  public:
    void prepare(double rate, double rg = 4700, double cg = 47e-9, double cf = 51e-12,
                 double diodeIs = 2.52e-9, double nVt = 1.752 * .02585) noexcept
    {
        sr = sampleRate(rate);
        legR = std::clamp(safe(rg), 1.0, 1e9);
        const double g = 1 / (2 * sr * legR * std::clamp(safe(cg), 1e-15, 1.0));
        legCoefficient = g / (1 + g);
        capG = 2 * sr * std::clamp(safe(cf), 0.0, 1e-3);
        is2 = 2 * std::clamp(safe(diodeIs), 1e-15, 1e-3);
        vt = std::clamp(safe(nVt), .001, 1.0);
        reset();
    }
    void setFeedbackResistance(double ohms) noexcept { feedbackG = 1 / std::clamp(safe(ohms), 1.0, 1e9); }
    void reset() noexcept
    {
        legState = history = vf = residual = 0;
        iterations = 0;
    }
    double process(double input) noexcept
    {
        const double x = std::clamp(safe(input), -16.0, 16.0);
        const double v = (x - legState) * legCoefficient, vc = legState + v;
        legState = safe(vc + v);
        const double current = (x - vc) / legR, rhs = current + history, g = feedbackG + capG;
        const double magnitude = std::abs(rhs);
        double value = std::min(magnitude / g, vt * std::asinh(magnitude / is2));
        iterations = 0;
        for (int i = 0; i < 8; ++i)
        {
            ++iterations;
            const double f = g * value + is2 * std::sinh(value / vt) - magnitude;
            if (std::abs(f) < 1e-20)
                break;
            value -= f / (g + is2 / vt * std::cosh(value / vt));
        }
        vf = std::copysign(value, rhs);
        residual = g * vf + is2 * std::sinh(vf / vt) - rhs;
        history = safe(2 * capG * vf - history);
        return safe(x + vf);
    }
    double lastResidualAmps() const noexcept { return residual; }
    int lastIterations() const noexcept { return iterations; }
    double feedbackVoltage() const noexcept { return vf; }

  private:
    double sr = 48000, legR = 4700, legCoefficient = 0, capG = 0, feedbackG = 1.0 / 51000;
    double is2 = 5.04e-9, vt = 1.752 * .02585, legState = 0, history = 0, vf = 0, residual = 0;
    int iterations = 0;
};
} // namespace luthier::reference::common
