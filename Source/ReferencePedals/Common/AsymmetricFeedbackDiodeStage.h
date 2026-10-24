// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common
{
// Dynamic non-inverting amplifier with one forward diode and a reverse pair.
// Separate from the reviewed symmetric stage: its API and arithmetic stay intact.
// Bounded safeguarded Newton followed by bisection solves the monotone KCL.
class AsymmetricFeedbackDiodeStage
{
  public:
    void prepare(double rate, double rg = 4700, double cg = 47e-9, double cf = 100e-12) noexcept
    {
        sr = sampleRate(rate);
        legR = std::clamp(safe(rg), 1., 1e9);
        capG = 2 * sr * std::clamp(safe(cf), 0., 1e-3);
        configure(cg, 327000, 2.52e-9, 1.752 * .02585);
        reset();
    }
    void configure(double cg, double rf, double diodeIs, double nVt) noexcept
    {
        const double g = 1 / (2 * sr * legR * std::clamp(safe(cg), 1e-15, 1.));
        legCoefficient = g / (1 + g);
        feedbackG = 1 / std::clamp(safe(rf), 1., 1e9);
        is = std::clamp(safe(diodeIs), 1e-15, 1e-3);
        vt = std::clamp(safe(nVt), .001, 1.);
    }
    void reset() noexcept
    {
        legState = history = vf = residual = 0;
        iterations = 0;
    }
    double process(double input) noexcept
    {
        const double x = std::clamp(safe(input), -16., 16.), v = (x - legState) * legCoefficient,
                     vc = legState + v;
        legState = safe(vc + v);
        const double rhs = (x - vc) / legR + history, g = feedbackG + capG;
        const double direction = rhs < 0 ? -1 : 1, magnitude = std::abs(rhs),
                     vp = direction > 0 ? vt : 2 * vt, vn = direction > 0 ? 2 * vt : vt;
        double lo = 0, hi = std::min(magnitude / g, vp * std::log1p(magnitude / is)), value = hi;
        iterations = 0;
        for (int i = 0; i < 44; ++i)
        {
            ++iterations;
            const double ep = std::exp(value / vp), en = std::exp(-value / vn),
                         f = g * value + is * (ep - en) - magnitude;
            if (std::abs(f) < 1e-18)
                break;
            if (f > 0)
                hi = value;
            else
                lo = value;
            const double next = value - f / (g + is * (ep / vp + en / vn));
            value = i < 12 && next > lo && next < hi ? next : .5 * (lo + hi);
        }
        vf = direction * value;
        residual = g * vf + is * (std::exp(vf / vt) - std::exp(-vf / (2 * vt))) - rhs;
        history = safe(2 * capG * vf - history);
        return safe(x + vf);
    }
    double lastResidualAmps() const noexcept { return residual; }
    int lastIterations() const noexcept { return iterations; }

  private:
    double sr = 48000, legR = 4700, legCoefficient = 0, capG = 0, feedbackG = 1. / 327000, is = 2.52e-9,
           vt = 1.752 * .02585;
    double legState = 0, history = 0, vf = 0, residual = 0;
    int iterations = 0;
};
} // namespace luthier::reference::common
