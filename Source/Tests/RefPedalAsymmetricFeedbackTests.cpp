// OWNER: WP7
#include "../ReferencePedals/Common/AsymmetricFeedbackDiodeStage.h"
#include "TestFramework.h"
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalAsymmetricFeedback, independentTrapezoidalKclOracle)
{
    for (double sr : {88200., 192000., 768000.})
        for (double is : {2.52e-9, 2e-7, 1e-12})
        {
            AsymmetricFeedbackDiodeStage stage;
            stage.prepare(sr, 4700, 47e-9, 100e-12);
            stage.configure(47e-9, 1027000, is, 1.752 * .02585);
            long double previousV = 0, previousI = 0, cgV = 0, cgI = 0;
            for (int i = 0; i < 2048; ++i)
            {
                const long double x = .5 * std::sin(2 * pi * 1100 * i / sr), k = 2 * sr;
                const long double nextCg = (x / 4700 + k * 47e-9L * cgV + cgI) / (1.L / 4700 + k * 47e-9L);
                const long double current = (x - nextCg) / 4700;
                const long double rhs = current + k * 100e-12L * previousV + previousI;
                auto f = [&](long double v)
                {
                    return v / 1027000 + k * 100e-12L * v +
                           is * (std::expm1(v / (1.752L * .02585L)) -
                                 std::expm1(-v / (2 * 1.752L * .02585L))) -
                           rhs;
                };
                long double lo = -4, hi = 4;
                for (int j = 0; j < 80; ++j)
                {
                    auto mid = (lo + hi) / 2;
                    if (f(mid) > 0)
                        hi = mid;
                    else
                        lo = mid;
                }
                const long double vf = (lo + hi) / 2;
                const double result = stage.process((double)x);
                CHECK_NEAR(result, (double)(x + vf), 2e-10);
                CHECK(std::abs(stage.lastResidualAmps()) < 1e-12);
                CHECK(stage.lastIterations() <= 44);
                cgI = current;
                cgV = nextCg;
                previousI = k * 100e-12L * (vf - previousV) - previousI;
                previousV = vf;
            }
            stage.reset();
            for (int i = 0; i < 100; ++i)
                CHECK(stage.process(0) == 0);
        }
}
