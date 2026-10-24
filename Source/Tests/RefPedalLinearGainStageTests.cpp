// OWNER: WP7
#include "../ReferencePedals/Common/LinearGainStage.h"
#include "../ReferencePedals/Common/NewtonClipper.h"
#include "TestFramework.h"
#include <complex>
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalLinearGainStage, ds1ComplexTransferAndClipperKcl)
{
    constexpr double sr = 192000, cg = 1e-6, cf = 250e-12;
    for (double position : {0., .5, 1.})
        for (double hz : {100., 1000., 5000.})
        {
            const double rf = 100 + 1e5 * position, rg = 4700 + 1e5 * (1 - position);
            LinearGainStage stage;
            stage.prepare(sr, rg, cg, rf, cf, true);
            double re = 0, im = 0;
            for (int i = 0; i < 1008000; ++i)
            {
                const double phase = 2 * pi * hz * i / sr, y = stage.process(1e-5 * std::cos(phase));
                if (i >= 960000)
                {
                    re += y * std::cos(phase);
                    im -= y * std::sin(phase);
                }
            }
            const double pole = 1 / (rf * cf),
                         k = pole < .9 * pi * sr ? pole / std::tan(pole / (2 * sr)) : 2 * sr;
            const std::complex<double> s(0, k * std::tan(pi * hz / sr));
            const auto expected = 1. + (rf / (1. + s * rf * cf)) / (rg + 1. / (s * cg));
            CHECK_NEAR(re * 2 / 48000 / 1e-5, expected.real(), 1e-8);
            CHECK_NEAR(im * 2 / 48000 / 1e-5, expected.imag(), 1e-8);
        }
    NewtonClipper clip;
    clip.configure(2 * 2.52e-9 * 2200 / (1 + 2200. / 15000), 1.752 * .02585);
    for (int i = -4000; i <= 4000; ++i)
    {
        const double vin = i * .001, v = clip.process(vin / (1 + 2200. / 15000));
        const double residual = (vin - v) / 2200 - 2 * 2.52e-9 * std::sinh(v / (1.752 * .02585)) - v / 15000;
        CHECK(std::abs(residual) < 1e-12);
        double lo = -1, hi = 1;
        for (int k = 0; k < 80; ++k)
        {
            double mid = (lo + hi) * .5;
            double f = (vin - mid) / 2200 - 2 * 2.52e-9 * std::sinh(mid / (1.752 * .02585)) - mid / 15000;
            if (f > 0)
                lo = mid;
            else
                hi = mid;
        }
        CHECK_NEAR(v, (lo + hi) * .5, 1e-11);
    }
}
