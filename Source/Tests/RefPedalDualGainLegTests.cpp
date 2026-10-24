// OWNER: WP7
#include "../ReferencePedals/Common/DualGainLegStage.h"
#include "../ReferencePedals/Common/NewtonClipper.h"
#include "TestFramework.h"
#include <complex>
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalDualGainLeg, ratIndependentComplexTransfer)
{
    constexpr double sr = 192000;
    for (double rf : {1000., 10000., 100000.})
        for (double hz : {100., 1000., 5000.})
        {
            DualGainLegStage stage;
            stage.prepare(sr);
            stage.setFeedbackResistance(rf);
            double re = 0, im = 0;
            for (int i = 0; i < 192000; ++i)
            {
                const double phase = 2 * pi * hz * i / sr, y = stage.process(1e-7 * std::cos(phase));
                if (i >= 144000)
                {
                    re += y * std::cos(phase);
                    im -= y * std::sin(phase);
                }
            }
            const double k = 2 * pi * 1000 / std::tan(pi * 1000 / sr);
            const std::complex<double> s(0, k * std::tan(pi * hz / sr));
            const auto expected = 1. + rf / (1. + s * rf * 100e-12) *
                                           (1. / (560. + 1. / (s * 4.7e-6)) + 1. / (47. + 1. / (s * 2.2e-6)));
            CHECK_NEAR(re * 2 / 48000 / 1e-7, expected.real(), 2e-7);
            CHECK_NEAR(im * 2 / 48000 / 1e-7, expected.imag(), 2e-7);
        }
}
LUTHIER_TEST(RefPedalDualGainLeg, tinyLedSaturationCurrentIsPreserved)
{
    NewtonClipper c;
    c.configure(2 * 1000 * 3e-18, 1.95 * .02585);
    for (double x : {.1, 1., 2., 3.5})
    {
        const double y = c.process(x), f = x - y - 2 * 1000 * 3e-18 * std::sinh(y / (1.95 * .02585));
        CHECK(std::abs(f) < 1e-10);
    }
}
