// OWNER: WP8
#include "../ReferencePedals/Generic/Compressor.h"
#include "TestFramework.h"
#include <cstdio>
using namespace luthier::reference;
namespace
{
void natural(generic::Compressor &p, int index, double value)
{
    p.setParam(index, (float)p.desc().params[index].toNormalised(value));
}
void configure(generic::Compressor &p, double sr, double attack, double release)
{
    natural(p, 1, attack);
    natural(p, 2, release);
    natural(p, 3, 0);
    natural(p, 5, 1);
    natural(p, 6, -24);
    natural(p, 7, 4);
    natural(p, 8, 0);
    natural(p, 10, 1);
    p.prepare(sr, 1, 1);
}
double render(generic::Compressor &p, int &clock, double amplitude, double *rightGain = nullptr)
{
    // Alternating polarity gives an exactly constant steady-state peak at the
    // 80 Hz sidechain HP output. The main audio stays safely below the limiter.
    double side = clock++ % 2 ? amplitude : -amplitude, zero = 0;
    double left = .001, right = .0001;
    p.setSidechain(&side, &zero, 1);
    p.process(&left, &right, 1);
    if (rightGain)
        *rightGain = right / .0001;
    return left / .001;
}
} // namespace
LUTHIER_TEST(RefPedalCompressorTiming, actualStaticCurveAndStereoLink)
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (double inputDb : {-40., -30., -24., -18., -12., -6.})
        {
            generic::Compressor p;
            configure(p, sr, 10, 40);
            int clock = 0;
            double gain = 1, rightGain = 1;
            for (int i = 0; i < (int)(sr * .3); ++i)
                gain = render(p, clock, std::pow(10., inputDb / 20), &rightGain);
            const double expectedReduction = inputDb <= -24 ? 0 : -.75 * (inputDb + 24);
            CHECK_NEAR(20 * std::log10(gain), expectedReduction, .2);
            CHECK_NEAR(gain, rightGain, 1e-12);
            CHECK(p.sidechainAvailable());
        }
}
LUTHIER_TEST(RefPedalCompressorTiming, originalTimeConstantTolerance)
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (double attack : {.5, 10., 60.})
            for (double release : {40., 350., 1200.})
            {
                generic::Compressor p;
                configure(p, sr, attack, release);
                int clock = 0;
                for (int i = 0; i < (int)(sr * .05); ++i)
                    render(p, clock, .01);
                const double targetAttack = 9 * (1 - std::exp(-1.));
                double measuredAttack = -1, before = p.gainReductionMeterDb();
                for (int i = 0; i < (int)std::ceil(sr * attack * .003); ++i)
                {
                    render(p, clock, std::pow(10., -12. / 20));
                    const double now = p.gainReductionMeterDb();
                    if (measuredAttack < 0 && now >= targetAttack)
                        measuredAttack = (i + (targetAttack - before) / (now - before)) * 1000 / sr;
                    before = now;
                }
                for (int i = 0; i < (int)std::ceil(sr * attack * .008); ++i)
                    render(p, clock, std::pow(10., -12. / 20));
                const double initial = p.gainReductionMeterDb(), targetRelease = initial * std::exp(-1.);
                double measuredRelease = -1;
                before = initial;
                for (int i = 0; i < (int)std::ceil(sr * release * .0012); ++i)
                {
                    render(p, clock, .01);
                    const double now = p.gainReductionMeterDb();
                    if (measuredRelease < 0 && now <= targetRelease)
                        measuredRelease = (i + (before - targetRelease) / (before - now)) * 1000 / sr;
                    before = now;
                }
                std::printf("compressor sr%.0f attack requested%.3f measured%.6fms release requested%.3f "
                            "measured%.6fms\n",
                            sr, attack, measuredAttack, release, measuredRelease);
                CHECK(std::abs(measuredAttack / attack - 1) <= .1);
                CHECK(std::abs(measuredRelease / release - 1) <= .1);
            }
}
LUTHIER_TEST(RefPedalCompressorTiming, automaticMakeupMatchesSpecifiedLaw)
{
    generic::Compressor p;
    natural(p, 10, 1);
    p.prepare(48000, 1, 1);
    int clock = 0;
    double gain = 1;
    for (int i = 0; i < 24000; ++i)
        gain = render(p, clock, .001);
    const double threshold = -10 - 35 * .5, ratio = 2 + 10 * .5;
    const double expected = 6 - threshold * (1 - 1 / ratio) * .5;
    CHECK_NEAR(20 * std::log10(gain), expected, .2);
}
