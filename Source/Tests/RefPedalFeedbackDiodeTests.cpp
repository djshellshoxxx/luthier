#include "../ReferencePedals/Common/FeedbackDiodeStage.h"
#include "TestFramework.h"
#include <complex>
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalFeedbackDiode, dynamicKclAndSymmetry)
{
    for (double sr : {88200., 192000., 768000.})
        for (double rf : {51000., 139388.3476, 551000.})
        {
            FeedbackDiodeStage a, b;
            a.prepare(sr);
            b.prepare(sr);
            a.setFeedbackResistance(rf);
            b.setFeedbackResistance(rf);
            for (int i = 0; i < 16384; ++i)
            {
                const double x = .5 * std::sin(2 * pi * 5000 * i / sr);
                const double y = a.process(x), z = b.process(-x);
                CHECK(std::isfinite(y));
                CHECK_NEAR(y, -z, 1e-12);
                CHECK(std::abs(a.lastResidualAmps()) < 1e-11);
                CHECK(a.lastIterations() <= 8);
            }
            a.reset();
            for (int i = 0; i < 64; ++i)
                CHECK(a.process(0) == 0);
        }
}
LUTHIER_TEST(RefPedalFeedbackDiode, lowLevelCircuitTransfer)
{
    // Independent analog transfer H=1+(Rf||Cf)/(Rg+1/sCg), bilinear frequency.
    for (double hz : {80., 720., 1000., 5000.})
    {
        constexpr double sr = 192000, rf = 551000, rg = 4700, cg = 47e-9, cf = 51e-12;
        FeedbackDiodeStage stage;
        stage.prepare(sr);
        stage.setFeedbackResistance(rf);
        double re = 0, im = 0;
        for (int i = 0; i < 96000; ++i)
        {
            double phase = 2 * pi * hz * i / sr, y = stage.process(1e-7 * std::cos(phase));
            if (i >= 48000)
            {
                re += y * std::cos(phase);
                im -= y * std::sin(phase);
            }
        }
        const double omega = 2 * sr * std::tan(pi * hz / sr);
        // Include the small-signal diode conductance 2Is/nVt in parallel with Rf.
        const std::complex<double> j(0, 1),
            zf = 1.0 / (1 / rf + 2 * 2.52e-9 / (1.752 * .02585) + j * omega * cf),
            zg = rg + 1.0 / (j * omega * cg), h = 1.0 + zf / zg;
        CHECK_NEAR(re * 2 / 48000 / 1e-7, h.real(), 1e-5);
        CHECK_NEAR(im * 2 / 48000 / 1e-7, h.imag(), 1e-5);
    }
}
