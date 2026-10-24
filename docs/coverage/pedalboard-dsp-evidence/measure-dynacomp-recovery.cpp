#include "../../../Source/ReferencePedals/Models/mxr_dynacomp.h"
#include <cstdio>
#include <vector>
using namespace luthier::reference;
double crossing(const std::vector<double> &values, double target, double previous, bool rising, double sr)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        const double now = values[i];
        if ((rising && now >= target) || (!rising && now <= target))
            return (i + std::clamp((target - previous) / (now - previous), 0., 1.)) * 1000 / sr;
        previous = now;
    }
    return -1;
}
int main()
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (float sensitivity : {.1f, .15f, .25f, .5f})
            for (int slow = 0; slow < 2; ++slow)
            {
                models::MxrDynaComp p;
                p.setParam(0, (float)p.desc().params[0].toNormalised(sensitivity));
                p.setParam(4, .5f);
                p.setParam(5, 0);
                p.setParam(2, (float)slow);
                p.prepare(sr, 1, 2);
                int clock = 0;
                auto render = [&](double db)
                {
                    double l = common::dbGain(db) * std::sin(2 * common::pi * 1000 * clock++ / sr), r = l;
                    p.process(&l, &r, 1);
                    return p.gainReductionDb();
                };
                double baseline = 0;
                for (int i = 0; i < (int)(sr * 5); ++i)
                    baseline = render(-30);
                std::vector<double> rise((size_t)(sr * 3)), fall((size_t)(sr * 5));
                for (auto &gr : rise)
                    gr = render(-10);
                for (auto &gr : fall)
                    gr = render(-30);
                const double high = rise.back(),
                             attackTarget = baseline + (1 - std::exp(-1.)) * (high - baseline),
                             releaseTarget = baseline + std::exp(-1.) * (high - baseline);
                const double attack = high - baseline > 1e-6
                                          ? crossing(rise, attackTarget, baseline, true, sr)
                                          : -1,
                             release =
                                 high - baseline > 1e-6 ? crossing(fall, releaseTarget, high, false, sr) : -1;
                const char *status = high - baseline <= 1e-6     ? "NO_GR_STEP"
                                     : attack < 0 || release < 0 ? "CENSORED"
                                                                 : "OBSERVED";
                std::printf("sr%.0f naturalSensitivity%.2f slow%d baselineGR%.9f highGR%.9f endGR%.9f "
                            "attack63ms%.9f release63ms%.9f observation5s %s\n",
                            sr, sensitivity, slow, baseline, high, fall.back(), attack, release, status);
                std::fflush(stdout);
            }
}
