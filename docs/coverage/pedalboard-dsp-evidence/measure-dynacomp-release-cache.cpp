// Actual-processor before/after oracle for the exact endpoint coefficient cache.
#include "../../../Source/ReferencePedals/Models/mxr_dynacomp.h"
#include <array>
#include <cstdio>
#include <limits>
#include <time.h>
using namespace luthier::reference;
static double cpu()
{
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    auto *output = std::fopen(argv[1], "wb");
    if (!output)
        return 3;
    uint64_t count = 0;
    double maximum = 0;
    for (double rate : {44100., 48000., 96000., 192000.})
        for (float quality : {0.f, .5f, 1.f})
        {
            models::MxrDynaComp p;
            p.setParam(6, quality);
            p.prepare(rate, 4096, 1);
            for (int section = 0; section < 128; ++section)
            {
                const auto &d = p.desc();
                p.setParam(0, (section % 17) / 16.f);
                p.setParam(1, (section % 7) / 6.f);
                p.setParam(2, (section % 3) * .5f);
                p.setParam(8, (section % 5) * .25f);
                p.setParam(5, (section % 2) ? 0.f : 1.f);
                std::array<double, 257> l{}, r{};
                for (int i = 0; i < 257; ++i)
                {
                    const auto n = section * 257 + i;
                    l[i] = .8 * std::sin(n * .17);
                    r[i] = .4 * std::cos(n * .23);
                }
                if (section == 30)
                {
                    l[0] = std::numeric_limits<double>::infinity();
                    r[1] = std::numeric_limits<double>::quiet_NaN();
                }
                if (section == 70)
                    p.reset();
                p.process(l.data(), r.data(), 257);
                for (int i = 0; i < 257; ++i)
                {
                    if (!std::isfinite(l[i]) || !std::isfinite(r[i]))
                        return 4;
                    maximum = std::max({maximum, std::abs(l[i]), std::abs(r[i])});
                }
                std::fwrite(l.data(), sizeof(double), 257, output);
                std::fwrite(r.data(), sizeof(double), 257, output);
                count += 514;
            }
        }
    std::fclose(output);
    std::printf("values=%llu peak=%.17g\n", (unsigned long long)count, maximum);
    for (int trial = 0; trial < 3; ++trial)
    {
        models::MxrDynaComp p;
        p.setParam(5, 0);
        p.prepare(48000, 128, 1);
        std::array<double, 128> l{}, r{};
        double elapsed = 0, checksum = 0;
        for (int block = 0; block < 37500; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const double n = block * 128 + i;
                l[i] = .1 * std::sin(n * .1308996939);
                r[i] = .13 * std::sin(n * .102);
            }
            const double start = cpu();
            p.process(l.data(), r.data(), 128);
            elapsed += cpu() - start;
            checksum += l[0] + r[0];
        }
        std::printf("trial=%d stereo_cpu_percent=%.9g checksum=%.17g\n", trial, elapsed, checksum);
    }
}
