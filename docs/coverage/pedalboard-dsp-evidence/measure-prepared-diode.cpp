#include "../../../Source/ReferencePedals/Common/PreparedDiodeRoot.h"
#include <cstdio>
#include <ctime>
#include <vector>
using namespace luthier::reference::common;
namespace
{
double cpu()
{
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
PreparedDiodeRoot::Result direct(double rhs, double g)
{
    constexpr double a = 5.04e-9, vt = 1.752 * .02585;
    const double magnitude = std::abs(rhs);
    double v = std::min(magnitude / g, vt * std::asinh(magnitude / a));
    PreparedDiodeRoot::Result result;
    for (int i = 0; i < 8; ++i)
    {
        ++result.iterations;
        const double f = g * v + a * std::sinh(v / vt) - magnitude;
        if (std::abs(f) < 1e-18)
            break;
        v -= f / (g + a / vt * std::cosh(v / vt));
    }
    result.voltage = std::copysign(v, rhs);
    result.residual = g * result.voltage + a * std::sinh(result.voltage / vt) - rhs;
    return result;
}
} // namespace
int main()
{
    constexpr int count = 1000000;
    std::vector<double> currents(count), conductances(count);
    for (int i = 0; i < count; ++i)
    {
        currents[i] = .003 * std::sin(i * .137) * (.5 + .5 * std::cos(i * .00173));
        conductances[i] = 1e-6 + (1e-3 - 1e-6) * (.5 + .5 * std::sin(i * .00037));
    }
    PreparedDiodeRoot root;
    const double start = cpu();
    root.prepare(5.04e-9, 1.752 * .02585, 1e-6, 1e-3, .01);
    std::printf("prepare threadCPU %.9fs; root table bytes %zu; upper-bound array bytes %zu\n",
                cpu() - start, size_t(33 * 513 * 8), size_t(513 * 8));
    double error = 0, residual = 0, iterations = 0;
    int maximumIterations = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto result = root.solve(currents[i], conductances[i]);
        error = std::max(error, std::abs(result.voltage - direct(currents[i], conductances[i]).voltage));
        residual = std::max(residual, std::abs(result.residual));
        iterations += result.iterations;
        maximumIterations = std::max(maximumIterations, result.iterations);
    }
    std::printf("max difference vs direct %.17gV; max residual %.17gA; mean iterations %.6f max %d\n", error,
                residual, iterations / count, maximumIterations);
    for (int trial = 0; trial < 3; ++trial)
        for (bool prepared : {false, true})
        {
            double sum = 0;
            const double start = cpu();
            for (int i = 0; i < count; ++i)
            {
                const auto result = prepared ? root.solve(currents[i], conductances[i])
                                             : direct(currents[i], conductances[i]);
                sum += result.voltage + result.residual;
            }
            std::printf("trial%d %s %.9fs checksum %.17g\n", trial, prepared ? "prepared" : "direct",
                        cpu() - start, sum);
        }
}
