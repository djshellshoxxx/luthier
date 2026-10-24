// OWNER: WP7
#include "../ReferencePedals/Common/Biquad.h"
#include "TestFramework.h"
#include <array>
#include <complex>
#include <limits>
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalShelfSlope, independentAnalogPrototypeTransfer)
{
    // Independently evaluate the analog shelving prototype at the bilinear
    // mapped frequency, then compare the actual impulse response transform.
    constexpr std::array<double, 8> probes{7.5, 30, 75, 150, 300, 750, 3000, 15000};
    for (double sr : {44100., 48000., 96000., 192000.})
        for (bool high : {false, true})
            for (double db : {-8., 8.})
                for (double slope : {.1, .7, 1.})
                {
                    Biquad filter;
                    filter.setShelf(sr, 150, db, high, slope);
                    std::array<std::complex<double>, 8> rotation{}, phase{}, response{};
                    for (size_t k = 0; k < probes.size(); ++k)
                    {
                        rotation[k] = std::polar(1., -2 * pi * probes[k] / sr);
                        phase[k] = 1;
                    }
                    for (int n = 0; n < 65536; ++n)
                    {
                        const double y = filter.process(n == 0 ? 1. : 0.);
                        for (size_t k = 0; k < probes.size(); ++k)
                        {
                            response[k] += y * phase[k];
                            phase[k] *= rotation[k];
                        }
                    }
                    const long double A = std::pow(10.L, db / 40.L);
                    const long double beta = std::sqrt((A + 1 / A) * (1 / slope - 1) + 2);
                    for (size_t k = 0; k < probes.size(); ++k)
                    {
                        const long double u =
                            std::tan((long double)pi * probes[k] / sr) / std::tan((long double)pi * 150 / sr);
                        const std::complex<long double> j(0, beta * std::sqrt(A) * u);
                        const auto numerator = A - u * u + j, denominator = 1 - A * u * u + j;
                        const auto expected =
                            high ? A * denominator / numerator : A * numerator / denominator;
                        CHECK_NEAR(response[k].real(), (double)expected.real(), 1e-8);
                        CHECK_NEAR(response[k].imag(), (double)expected.imag(), 1e-8);
                    }
                }
}
LUTHIER_TEST(RefPedalShelfSlope, legacyDefaultAndHostileSlope)
{
    for (bool high : {false, true})
    {
        Biquad legacy, explicitOne;
        legacy.setShelf(48000, 150, 8, high);
        explicitOne.setShelf(48000, 150, 8, high, 1);
        for (int n = 0; n < 4096; ++n)
        {
            const double x = std::sin(n * .173);
            CHECK(legacy.process(x) == explicitOne.process(x));
        }
        for (double slope : {0., -1., 100., std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()})
        {
            Biquad filter;
            filter.setShelf(48000, 150, 8, high, slope);
            for (int n = 0; n < 4096; ++n)
                CHECK(std::isfinite(filter.process(n == 0 ? 1 : 0)));
        }
    }
}
