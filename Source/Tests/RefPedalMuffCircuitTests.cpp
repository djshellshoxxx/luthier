// OWNER: WP13
#include "../ReferencePedals/Models/ehx_muff_core.h"
#include "TestFramework.h"
#include <complex>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalMuffCircuit, toneGoldenTableAndIndependentTransfer)
{
    constexpr double rates[] = {88200, 192000, 768000},
                     frequencies[] = {80, 160, 320, 500, 800, 1000, 1250, 2000, 4000, 8000};
    constexpr double table[5][10] = {{-2.5, -2.8, -3.7, -5, -7.1, -8.3, -9.7, -12.7, -17.6, -23.2},
                                     {-4.5, -4.8, -5.9, -7.4, -10.1, -11.6, -13, -14.5, -13.3, -12.4},
                                     {-7.1, -7.5, -8.7, -10.6, -13.2, -13.8, -13.3, -10.5, -7.5, -6.4},
                                     {-10.8, -11.3, -12.7, -14.2, -13.7, -12, -10.1, -6.7, -3.9, -2.9},
                                     {-17.4, -17.6, -17.3, -15, -10.8, -8.8, -6.9, -3.8, -1.3, -.3}};
    for (double sr : rates)
        for (int t = 0; t < 5; ++t)
            for (int f = 0; f < 10; ++f)
            {
                models::MuffTone tone;
                tone.configure(sr, t * .25);
                const double hz = frequencies[f];
                double re = 0, im = 0;
                const int count = (int)sr / 4;
                for (int i = 0; i < 2 * count; ++i)
                {
                    const double phase = 2 * common::pi * hz * i / sr, y = tone.process(std::cos(phase));
                    if (i >= count)
                    {
                        re += y * std::cos(phase);
                        im -= y * std::sin(phase);
                    }
                }
                const std::complex<double> s(0, 2 * sr * std::tan(common::pi * hz / sr));
                const double gl = 1. / 39000, gh = 1. / 22000, gp = 1. / 100000, cl = 10e-9, ch = 4e-9;
                const auto den =
                    s * s * cl * ch + s * (cl * (gh + gp) + ch * (gl + gp)) + gl * gh + gl * gp + gp * gh;
                const auto na = s * ch * (gl + gp) + gl * (gh + gp),
                           nb = s * s * cl * ch + s * ch * (gl + gp) + gp * gl;
                const auto h = ((1 - t * .25) * na + t * .25 * nb) / den;
                CHECK_NEAR(re * 2 / count, h.real(), 1e-6);
                CHECK_NEAR(im * 2 / count, h.imag(), 1e-6);
                const double measured = 20 * std::log10(std::hypot(re, im) * 2 / count);
                CHECK(std::abs(measured - table[t][f]) <= 1.0);
            }
}
LUTHIER_TEST(RefPedalMuffCircuit, transistorFeedbackIndependentKcl)
{
    for (double sr : {88200., 192000., 768000.})
        for (double cf : {22e-12, 470e-12})
        {
            models::MuffClipStage stage;
            stage.prepare(sr);
            stage.configure(15000, cf);
            long double previousV = 0, previousI = 0;
            for (int i = 0; i < 4096; ++i)
            {
                const long double x = .5 * std::sin(2 * common::pi * 1000 * i / sr), k = 2 * sr;
                const long double g = (1.L / 15000 + 1.L / 25000) / 151 + 1.L / 470000 + k * cf;
                const long double rhs = x / 15000 + k * cf * previousV + previousI;
                auto f = [&](long double v)
                { return g * v + 2 * 2.52e-9L * std::sinh(v / (1.752L * .02585L)) - rhs; };
                long double lo = -1.3, hi = 1.3;
                for (int j = 0; j < 80; ++j)
                {
                    auto mid = (lo + hi) / 2;
                    if (f(mid) > 0)
                        hi = mid;
                    else
                        lo = mid;
                }
                const long double vd = (lo + hi) / 2;
                const double y = stage.process((double)x);
                CHECK_NEAR(y, (double)(-150 * vd / 151), 1e-10);
                CHECK(std::abs(stage.lastResidualAmps()) < 1e-12);
                previousI = k * cf * (vd - previousV) - previousI;
                previousV = vd;
            }
        }
}
