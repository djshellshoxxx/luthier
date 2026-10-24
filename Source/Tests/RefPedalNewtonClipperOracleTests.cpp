// OWNER: WP7
#include "../ReferencePedals/Common/NewtonClipper.h"
#include "TestFramework.h"
#include <limits>
using namespace luthier::reference::common;
namespace
{
double original(double x, double a, double vt)
{
    x = std::clamp(safe(x), -16., 16.);
    const double mag = std::abs(x);
    double v = std::min(mag, vt * std::asinh(mag / a));
    for (int i = 0; i < 8; ++i)
    {
        const double f = v + a * std::sinh(v / vt) - mag;
        if (std::abs(f) < 1e-12)
            break;
        v -= f / (1 + a / vt * std::cosh(v / vt));
    }
    return std::copysign(v, x);
}
long double oracle(long double x, long double a, long double vt)
{
    const auto mag = std::abs(x);
    long double lo = 0, hi = std::min(mag, vt * std::asinh(mag / a));
    for (int i = 0; i < 90; ++i)
    {
        const auto v = (lo + hi) / 2;
        if (v + a * std::sinh(v / vt) > mag)
            hi = v;
        else
            lo = v;
    }
    return std::copysign((lo + hi) / 2, x);
}
} // namespace
LUTHIER_TEST(RefPedalNewtonClipperOracle, guardedDomainIndependentKclAndOriginalAlgorithm)
{
    double worstOriginal = 0, worstOracle = 0, worstResidual = 0;
    for (double a : {1e-24, 6e-15, 5.04e-6, .0000504, .02, 1., 1000.})
        for (double vt : {.001, .02585 * 1.3, .02585 * 1.752, .02585 * 1.95, 1.})
        {
            NewtonClipper c;
            c.configure(a, vt);
            for (int i = 0; i < 1024; ++i)
            {
                const double x =
                    (i % 2 ? -1 : 1) * std::pow(10., -14 + (std::log10(16.) + 14) * (i % 513) / 512.);
                const double y = c.process(x);
                CHECK(std::isfinite(y));
                CHECK(y * x >= 0);
                CHECK(std::abs(y) <= std::abs(x) + 1e-12);
                CHECK(c.lastIterations() <= 8);
                worstOriginal = std::max(worstOriginal, std::abs(y - original(x, a, vt)));
                worstOracle = std::max(worstOracle, std::abs(y - (double)oracle(x, a, vt)));
                worstResidual = std::max(worstResidual, std::abs(c.residual(y, x)));
            }
        }
    CHECK(worstOriginal < 2e-12);
    CHECK(worstOracle < 2e-12);
    CHECK(worstResidual < 2e-11);
}
LUTHIER_TEST(RefPedalNewtonClipperOracle, hostileParametersAndInputRecovery)
{
    NewtonClipper c;
    for (double a :
         {-1., 0., 1e300, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        for (double vt : {-1., 0., 1e300, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()})
        {
            c.configure(a, vt);
            for (double x : {-1e300, -1e-310, 0., 1e-310, 1e300, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()})
            {
                CHECK(std::isfinite(c.process(x)));
                CHECK(c.lastIterations() <= 8);
            }
            CHECK(std::isfinite(c.process(.1)));
            c.reset();
            CHECK(c.lastIterations() == 0);
        }
}
