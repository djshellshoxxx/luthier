// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
#include <limits>
#include <vector>
namespace luthier::reference::common
{
// Experimental prepared initial guesses for g*v + diodeScale*sinh(v/Vt)=rhs.
// The table is only an initial guess: bounded Newton refinement solves the
// original equation. Preparation allocates; solve/reset do not allocate.
class PreparedDiodeRoot
{
  public:
    struct Result
    {
        double voltage = 0, residual = 0;
        int iterations = 0;
    };
    void prepare(double diodeScale, double thermalVoltage, double minimumConductance,
                 double maximumConductance, double maximumCurrent)
    {
        a = std::clamp(safe(diodeScale), 1e-24, 1.);
        vt = std::clamp(safe(thermalVoltage), .001, 1.);
        gMin = std::clamp(safe(minimumConductance), 1e-12, 1e6);
        gMax = std::max(gMin, std::clamp(safe(maximumConductance), 1e-12, 1e6));
        rhsMax = std::clamp(safe(maximumCurrent), 1e-12, 1e6);
        currentScale = 1 / (a + gMin * vt);
        logMaximum = std::log1p(rhsMax * currentScale);
        roots.resize((size_t)(rows + 1) * (columns + 1));
        for (int column = 0; column <= columns; ++column)
        {
            const double current =
                column == columns ? rhsMax : std::expm1(logMaximum * column / columns) / currentScale;
            const double voltage = vt * std::asinh(current / a);
            diodeUpper[column] = voltage + 8 * std::numeric_limits<double>::epsilon() * (1 + voltage);
        }
        for (int row = 0; row <= rows; ++row)
        {
            const double g = gMin + (gMax - gMin) * row / rows;
            for (int column = 0; column <= columns; ++column)
            {
                const long double rhs = std::expm1(logMaximum * column / columns) / currentScale;
                long double lo = 0, hi = std::min(rhs / g, vt * std::asinh(rhs / a));
                for (int iteration = 0; iteration < 64; ++iteration)
                {
                    const long double v = (lo + hi) / 2;
                    if (g * v + a * std::sinh(v / vt) > rhs)
                        hi = v;
                    else
                        lo = v;
                }
                roots[(size_t)row * (columns + 1) + column] = (double)((lo + hi) / 2);
            }
        }
    }
    Result solve(double rhs, double conductance) const noexcept
    {
        rhs = std::clamp(safe(rhs), -1e6, 1e6);
        const double g = std::clamp(safe(conductance), 1e-12, 1e6), magnitude = std::abs(rhs);
        double v = 0, lo = 0, hi = magnitude / g;
        // Unprepared and out-of-domain queries use the original analytical
        // upper bound. They never extend/rebuild the table on the audio thread.
        if (!roots.empty() && magnitude <= rhsMax && g >= gMin && g <= gMax)
        {
            const double x = std::clamp(std::log1p(magnitude * currentScale) * columns / logMaximum, 0.,
                                        (double)columns),
                         y = gMax == gMin ? 0 : (g - gMin) * rows / (gMax - gMin);
            const int column = std::min(columns - 1, (int)x), row = std::min(rows - 1, (int)y);
            // The next grid current bounds this query's diode-only root. This
            // retains a physical bracket without evaluating asinh per query.
            hi = std::min(hi, diodeUpper[column + 1]);
            const double fx = x - column, fy = y - row;
            const auto index = (size_t)row * (columns + 1) + column;
            const double low = roots[index] + fx * (roots[index + 1] - roots[index]),
                         high = roots[index + columns + 1] +
                                fx * (roots[index + columns + 2] - roots[index + columns + 1]);
            v = low + fy * (high - low);
            v = std::clamp(v, lo, hi);
        }
        else
            v = hi = std::min(hi, vt * std::asinh(magnitude / a));
        Result result;
        for (int iteration = 0; iteration < 8; ++iteration)
        {
            ++result.iterations;
            // v is nonnegative. A single expm1 evaluates both hyperbolic
            // terms without cancellation near zero or two transcendental calls.
            const double em = std::expm1(v / vt), ratio = em / (1 + em), sinhValue = .5 * (em + ratio),
                         coshValue = 1 + .5 * em * ratio, f = g * v + a * sinhValue - magnitude;
            if (std::abs(f) < 1e-18)
                break;
            if (f > 0)
                hi = v;
            else
                lo = v;
            double next = v - f / (g + a / vt * coshValue);
            // At floating-point stagnation another identical iteration cannot
            // improve the solution; preserve the voltage rather than spin.
            if (next == v)
                break;
            if (!(next > lo && next < hi))
                next = .5 * (lo + hi);
            v = next;
        }
        result.voltage = std::copysign(v, rhs);
        result.residual = g * result.voltage + a * std::sinh(result.voltage / vt) - rhs;
        return result;
    }

  private:
    static constexpr int rows = 32, columns = 512;
    std::vector<double> roots;
    std::array<double, columns + 1> diodeUpper{};
    double a = 5.04e-9, vt = 1.752 * .02585, gMin = 1e-6, gMax = 1e-3, rhsMax = .01;
    double currentScale = 1, logMaximum = 1;
};
} // namespace luthier::reference::common
