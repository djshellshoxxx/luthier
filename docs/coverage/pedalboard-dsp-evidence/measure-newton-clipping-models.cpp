// Actual model corpus/CPU probe. Run unchanged before and after helper revision.
#include "../../../Source/ReferencePedals/Models/boss_ds1.h"
#include "../../../Source/ReferencePedals/Models/ehx_deluxe_muff.h"
#include "../../../Source/ReferencePedals/Models/mxr_distortion_plus.h"
#include "../../../Source/ReferencePedals/Models/proco_rat.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <time.h>
using namespace luthier::reference;
static double cpu()
{
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
template <class T> int run(FILE *output)
{
    uint64_t count = 0;
    for (double sr : {44100., 48000., 96000., 192000.})
        for (float variant : {0.f, 1.f})
        {
            T p;
            const auto &desc = p.desc();
            for (int i = 0; i < desc.numParams; ++i)
            {
                if (std::strstr(desc.params[i].id, "noise"))
                    p.setParam(i, 0);
                if (std::strstr(desc.params[i].id, "variant"))
                    p.setParam(i, variant);
            }
            p.prepare(sr, 2048, 1);
            for (int block = 0; block < 64; ++block)
            {
                for (int i = 0; i < desc.numParams; ++i)
                    if (desc.params[i].kind == ParamKind::Continuous && !desc.params[i].has(PF_Config))
                        p.setParam(i, float((block + i * 7) % 33) / 32);
                std::array<double, 128> l{}, r{};
                for (int i = 0; i < 128; ++i)
                {
                    const int n = block * 128 + i;
                    l[i] = .7 * std::sin(n * .173);
                    r[i] = .3 * std::cos(n * .097);
                }
                if (block == 17)
                {
                    l[0] = std::numeric_limits<double>::infinity();
                    r[1] = std::numeric_limits<double>::quiet_NaN();
                }
                if (block == 37)
                    p.reset();
                p.process(l.data(), r.data(), 128);
                for (int i = 0; i < 128; ++i)
                    if (!std::isfinite(l[i]) || !std::isfinite(r[i]))
                        return 4;
                std::fwrite(l.data(), 8, 128, output);
                std::fwrite(r.data(), 8, 128, output);
                count += 256;
            }
        }
    std::printf("model=%s corpus_values=%llu\n", T::descriptor().modelId, (unsigned long long)count);
    for (int trial = 0; trial < 3; ++trial)
    {
        T p;
        for (int i = 0; i < p.desc().numParams; ++i)
            if (std::strstr(p.desc().params[i].id, "noise"))
                p.setParam(i, 0);
        p.prepare(48000, 128, 1);
        std::array<double, 128> l{}, r{};
        double elapsed = 0, checksum = 0;
        for (int block = 0; block < 7500; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const int n = block * 128 + i;
                l[i] = .1 * std::sin(n * .1308996939);
                r[i] = .13 * std::sin(n * .102);
            }
            const double start = cpu();
            p.process(l.data(), r.data(), 128);
            elapsed += cpu() - start;
            checksum += l[0] + r[0];
        }
        std::printf("trial=%d cpu_percent=%.9g checksum=%.17g\n", trial, 5 * elapsed, checksum);
    }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    auto *f = std::fopen(argv[1], "wb");
    if (!f)
        return 3;
    int result = run<models::BossDs1>(f) | run<models::MxrDistortionPlus>(f) | run<models::ProcoRat>(f) |
                 run<models::EhxDeluxeMuff>(f);
    std::fclose(f);
    return result;
}
