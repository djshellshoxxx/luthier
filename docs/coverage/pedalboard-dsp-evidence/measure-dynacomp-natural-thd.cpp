#include "../../../Source/ReferencePedals/Models/mxr_dynacomp.h"
#include <array>
#include <complex>
#include <cstdio>
using namespace luthier::reference;
int main()
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (double sensitivity : {.25, .5, 1.})
        {
            models::MxrDynaComp p;
            p.setParam(5, 0);
            p.setParam(0, (float)p.desc().params[0].toNormalised(sensitivity));
            p.prepare(sr, 1, 2);
            std::array<std::complex<double>, 10> bins{}, rotation{}, phase{};
            for (int h = 0; h < 10; ++h)
            {
                rotation[h] = std::polar(1., -2 * common::pi * 1000 * (h + 1) / sr);
                phase[h] = 1;
            }
            for (int i = 0; i < (int)(3 * sr); ++i)
            {
                double l = .1 * std::sin(2 * common::pi * 1000 * i / sr), r = l;
                p.process(&l, &r, 1);
                if (i >= (int)(2 * sr))
                    for (int h = 0; h < 10; ++h)
                    {
                        bins[h] += l * phase[h];
                        phase[h] *= rotation[h];
                    }
            }
            double harmonicPower = 0;
            for (int h = 1; h < 10; ++h)
                harmonicPower += std::norm(bins[h]);
            std::printf(
                "sr%.0f naturalSensitivity%.2f normalized%.9f input-20dBFSpeak THD2to10 %.9f%% GR%.9fdB\n",
                sr, sensitivity, p.getParam(0), 100 * std::sqrt(harmonicPower / std::norm(bins[0])),
                p.gainReductionDb());
        }
}
