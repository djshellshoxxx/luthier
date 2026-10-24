// OWNER: WP7
#include "../ReferencePedals/Common/PreparedDiodeRoot.h"
#include "TestFramework.h"
#include <limits>
using namespace luthier::reference::common;
#if defined(LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
#endif
namespace
{
long double oracle(long double rhs, long double g, long double a = 5.04e-9L,
                   long double vt = 1.752L * .02585L)
{
    const long double magnitude = std::abs(rhs);
    long double lo = 0, hi = std::min(magnitude / g, vt * std::asinh(magnitude / a));
    for (int i = 0; i < 90; ++i)
    {
        const long double v = (lo + hi) / 2;
        if (g * v + a * std::sinh(v / vt) > magnitude)
            hi = v;
        else
            lo = v;
    }
    return std::copysign((lo + hi) / 2, rhs);
}
LUTHIER_TEST(RefPedalPreparedDiodeRoot, diodeParametersAndRealtimeAllocationContract)
{
    for (double a : {6e-15, 5.04e-9, 2e-6})
        for (double vt : {.02585 * 1.3, .02585 * 1.752, .02585 * 1.95})
        {
            PreparedDiodeRoot root;
            root.prepare(a, vt, 2e-6, .002, .01);
#if defined(LUTHIER_ALLOCATION_COUNTER)
            const auto allocations = luthier::tests::allocationsOnThisThread();
#endif
            double worstError = 0, worstResidual = 0;
            for (int i = 0; i < 2048; ++i)
            {
                const double rhs = .01 * std::sin(i * .371), g = 2e-6 + .001998 * (i % 33) / 32.;
                const auto result = root.solve(rhs, g);
                worstError = std::max(worstError, std::abs(result.voltage - (double)oracle(rhs, g, a, vt)));
                worstResidual = std::max(worstResidual, std::abs(result.residual));
            }
#if defined(LUTHIER_ALLOCATION_COUNTER)
            CHECK(luthier::tests::allocationsOnThisThread() == allocations);
#endif
            CHECK(worstError < 1e-10);
            CHECK(worstResidual < 1e-11);
        }
}
} // namespace
LUTHIER_TEST(RefPedalPreparedDiodeRoot, independentMonotoneKclOracle)
{
    PreparedDiodeRoot root;
    root.prepare(5.04e-9, 1.752 * .02585, 1e-6, 1e-3, .01);
    double largestVoltageError = 0, largestCurrentError = 0;
    for (int i = 0; i < 20000; ++i)
    {
        // Log-distributed, decorrelated current and conductance values include
        // interpolation boundaries and out-of-table fallbacks in both axes.
        const double rhs = (i % 2 ? -1 : 1) * std::pow(10., -12 + 11 * (i % 1001) / 1000.);
        const double g = std::pow(10., -7 + 5 * ((i * 313) % 997) / 996.);
        const auto result = root.solve(rhs, g);
        largestVoltageError =
            std::max(largestVoltageError, std::abs(result.voltage - (double)oracle(rhs, g)));
        largestCurrentError = std::max(largestCurrentError, std::abs(result.residual));
        CHECK(std::isfinite(result.voltage));
        CHECK(result.iterations <= 8);
    }
    CHECK(largestVoltageError < 1e-10);
    CHECK(largestCurrentError < 1e-11);
    CHECK(root.solve(0, 1e-4).voltage == 0);
    CHECK(root.solve(std::numeric_limits<double>::quiet_NaN(), 1e-4).voltage == 0);
    CHECK(root.solve(std::numeric_limits<double>::infinity(), 1e-4).voltage == 0);
}
LUTHIER_TEST(RefPedalPreparedDiodeRoot, finiteGuardsAndFixedConductance)
{
    for (bool prepared : {false, true})
    {
        PreparedDiodeRoot root;
        if (prepared)
            root.prepare(5.04e-9, 1.752 * .02585, 1e-4, 1e-4, .01);
        for (double rhs : {-1e300, -.01, -1e-310, 0., .01, 1e300})
            for (double g : {0., -1., 1e-4, 1e300, std::numeric_limits<double>::quiet_NaN()})
            {
                const auto value = root.solve(rhs, g);
                CHECK(std::isfinite(value.voltage));
                CHECK(std::isfinite(value.residual));
                CHECK(value.iterations <= 8);
            }
        for (double rhs : {-.005, -1e-12, 0., 1e-12, .005})
            CHECK_NEAR(root.solve(rhs, 1e-4).voltage, (double)oracle(rhs, 1e-4), 1e-10);
    }
}
