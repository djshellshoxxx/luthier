#include "../../../Source/ReferencePedals/Models/boss_ds1.h"
#include "../../../Source/ReferencePedals/Models/boss_sd1.h"
#include "../../../Source/ReferencePedals/Models/ehx_deluxe_muff.h"
#include "../../../Source/ReferencePedals/Models/ibanez_ts808.h"
#include "../../../Source/ReferencePedals/Models/mxr_distortion_plus.h"
#include "../../../Source/ReferencePedals/Models/proco_rat.h"
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
using namespace luthier::reference;
void fixture(IPedal &p, int driveIndex, double drive, double amplitude, double minimum, double maximum,
             bool thdn, const char *levelMeaning)
{
    p.setParam(driveIndex, (float)drive);
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        p.prepare(sr, 1, 1);
        std::array<std::complex<double>, 10> bins{}, rotations{}, phases{};
        for (int h = 0; h < 10; ++h)
        {
            rotations[h] = std::polar(1., -2 * common::pi * 1000 * (h + 1) / sr);
            phases[h] = 1;
        }
        double energy = 0, dc = 0;
        for (int i = 0; i < (int)(2 * sr); ++i)
        {
            double l = amplitude * std::sin(2 * common::pi * 1000 * i / sr), r = l;
            p.process(&l, &r, 1);
            if (i >= (int)sr)
            {
                energy += l * l;
                dc += l;
                for (int h = 0; h < 10; ++h)
                {
                    bins[h] += l * phases[h];
                    phases[h] *= rotations[h];
                }
            }
        }
        double harmonics = 0;
        for (int h = 1; h < 10; ++h)
            harmonics += std::norm(bins[h]);
        const double fundamentalPower = 2 * std::norm(bins[0]) / (sr * sr),
                     thd = 100 * std::sqrt(harmonics / std::norm(bins[0])),
                     noise =
                         100 * std::sqrt(std::max(0., energy / sr - dc * dc / (sr * sr) - fundamentalPower) /
                                         fundamentalPower),
                     value = thdn ? noise : thd;
        std::printf("%s sr%.0f drive%.2f %s amplitudePeak%.12g THD2to10 %.9f%% THD+N %.9f%% "
                    "original[%.3f,%.3f] %s H2/H3 %.6fdB H2/fund %.6fdBc gain %.9fdB mean %.9g\n",
                    p.desc().modelId, sr, drive, levelMeaning, amplitude, thd, noise, minimum, maximum,
                    value >= minimum && value <= maximum ? "PASS" : "FAIL",
                    20 * std::log10(std::abs(bins[1]) / std::abs(bins[2])),
                    20 * std::log10(std::abs(bins[1]) / std::abs(bins[0])),
                    10 * std::log10(energy / sr / (.5 * amplitude * amplitude)), dc / sr);
        std::fflush(stdout);
    }
}
int main()
{
    for (bool rms : {false, true})
    {
        const double scale = rms ? std::sqrt(2.) : 1;
        const char *meaning = rms ? "statedLevel=RMS" : "statedLevel=PEAK";
        models::BossDs1 ds;
        ds.setParam(7, 0);
        fixture(ds, 0, .2, .1 * scale, 1, 6, false, meaning);
        fixture(ds, 0, 1, .1 * scale, 25, 40, false, meaning);
        models::IbanezTs808 ts;
        ts.setParam(7, 0);
        fixture(ts, 0, .2, .1 * scale, .5, 5, false, meaning);
        fixture(ts, 0, 1, .1 * scale, 12, 35, false, meaning);
        models::BossSd1 sd;
        const double lo[]{.5, 3, 8, 15, 20}, hi[]{2, 8, 20, 30, 40};
        for (int index = 0; index < 5; ++index)
            fixture(sd, 0, index * .25, std::pow(10., -24. / 20) * scale, lo[index], hi[index], true,
                    meaning);
        models::MxrDistortionPlus dp;
        dp.setParam(2, 1);
        dp.setParam(6, 0);
        fixture(dp, 1, .25, .100 / .5 * scale, 5, 15, false, meaning);
        fixture(dp, 1, .5, .100 / .5 * scale, 10, 35, false, meaning);
        fixture(dp, 1, 1, .100 / .5 * scale, 25, 60, false, meaning);
        models::ProcoRat rat;
        rat.setParam(1, 0);
        rat.setParam(6, 0);
        fixture(rat, 0, 0, .010 / 1.228345603 * scale, 0, 1, false, meaning);
        fixture(rat, 0, .5, .100 / 1.228345603 * scale, 5, 30, false, meaning);
        fixture(rat, 0, 1, .100 / 1.228345603 * scale, 25, std::numeric_limits<double>::infinity(), false,
                meaning);
        models::EhxDeluxeMuff deluxe;
        fixture(deluxe, 3, .1, .1 * scale, 5, 20, false, meaning);
        fixture(deluxe, 3, 1, .1 * scale, 25, 60, false, meaning);
    }
}
