#include "../../../Source/ReferencePedals/Models/ehx_big_muff_nyc.h"
#include "../../../Source/ReferencePedals/Models/ehx_muff_wicker.h"
#include <cstdio>
#include <ctime>
#include <fstream>
#include <limits>
using namespace luthier::reference;
double threadSeconds()
{
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
void measure(IPedal &pedal, bool wicker, std::ofstream &trace)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        pedal.prepare(sr, 2048, 1);
        for (int i = 0; i < 16384; ++i)
        {
            if (i % 256 == 0)
            {
                pedal.setParam(1, float((i / 256 * 17) % 101) / 100);
                pedal.setParam(2, float((i / 256 * 31) % 101) / 100);
                pedal.setParam(3, float((i / 256 * 13) % 101) / 100);
                if (wicker)
                {
                    pedal.setParam(4, (i / 2048) % 2);
                    pedal.setParam(5, (i / 1024) % 2);
                    pedal.setParam(6, float((i / 256 * 47) % 101) / 100);
                }
            }
            double l = .3 * std::sin(i * .137), r = .2 * std::cos(i * .173);
            if (i % 4096 == 1)
                l = std::numeric_limits<double>::quiet_NaN();
            pedal.process(&l, &r, 1);
            trace.write((const char *)&l, sizeof l);
            trace.write((const char *)&r, sizeof r);
        }
    }
    for (int trial = 0; trial < 3; ++trial)
    {
        pedal.setParam(1, .5);
        pedal.setParam(2, .5);
        pedal.setParam(3, .5);
        if (wicker)
        {
            pedal.setParam(4, 1);
            pedal.setParam(5, 0);
            pedal.setParam(6, 0);
        }
        const double start = threadSeconds();
        pedal.prepare(48000, 128, 1);
        const double preparation = threadSeconds() - start;
        double seconds = 0, checksum = 0;
        for (int block = 0; block < 750; ++block)
        {
            double l[128], r[128];
            for (int i = 0; i < 128; ++i)
            {
                l[i] = .1 * std::sin(i * .137);
                r[i] = .07 * std::cos(i * .173);
            }
            const double before = threadSeconds();
            pedal.process(l, r, 128);
            seconds += threadSeconds() - before;
            checksum += l[127] + r[127];
        }
        std::printf("%s trial%d prepare %.9fs process-only stereo CPU %.9f%% checksum %.17g\n",
                    pedal.desc().modelId, trial, preparation, seconds * 50, checksum);
    }
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    std::ofstream trace(argv[1], std::ios::binary);
    if (!trace)
        return 3;
    models::EhxBigMuffNyc nyc;
    models::EhxMuffWicker wicker;
    measure(nyc, false, trace);
    measure(wicker, true, trace);
}
