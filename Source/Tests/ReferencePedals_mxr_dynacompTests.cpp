#include "../ReferencePedals/Models/mxr_dynacomp.h"
#include "TestFramework.h"
#include <array>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalDynaComp, finiteBlockDeterminismAndSilence)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        models::MxrDynaComp a, b;
        a.setParam(5, 0);
        b.setParam(5, 0);
        a.prepare(sr, 2048, 2);
        b.prepare(sr, 2048, 2);
        std::array<double, 2048> l{}, r{}, x{}, y{};
        a.process(l.data(), r.data(), 2048);
        for (double v : l)
            CHECK(v == 0);
        a.reset();
        for (int i = 0; i < 2048; ++i)
        {
            l[i] = r[i] = x[i] = y[i] = .3 * std::sin(i * .17);
        }
        a.process(l.data(), r.data(), 2048);
        for (int i = 0; i < 2048; ++i)
            b.process(x.data() + i, y.data() + i, 1);
        for (int i = 0; i < 2048; ++i)
        {
            CHECK(l[i] == x[i]);
            CHECK(l[i] == r[i]);
            CHECK(std::isfinite(l[i]));
        }
    }
}
LUTHIER_TEST(RefPedalDynaComp, feedbackDetectorAndQualityLatency)
{
    models::MxrDynaComp p;
    p.setParam(5, 0);
    p.setParam(0, 1);
    p.prepare(48000, 1, 2);
    CHECK(p.getLatencySamples() == 15);
    for (int i = 0; i < 96000; ++i)
    {
        double x = .5 * std::sin(i * common::pi / 24), r = x;
        p.process(&x, &r, 1);
    }
    CHECK(p.gainReductionDb() > 0);
    CHECK(p.gainReductionDb() <= 36.0001);
    p.setParam(6, 1);
    p.prepare(48000, 1, 2);
    CHECK(p.getLatencySamples() == 0);
}
#ifdef LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
LUTHIER_TEST(RefPedalDynaComp, realtimeLifecycleDoesNotAllocate)
{
    models::MxrDynaComp p;
    p.prepare(192000, 2048, 2);
    std::array<double, 2048> l{}, r{};
    const auto before = luthier::tests::allocationsOnThisThread();
    for (int k = 0; k < 16; ++k)
    {
        for (int i = 0; i < p.desc().numParams; ++i)
            p.setParam(i, (k % 5) * .25f);
        p.process(l.data(), r.data(), 2048);
        p.setExpression(0, .5f);
        p.onMidi({});
        p.onFoot({});
        p.reset();
    }
    CHECK(before == luthier::tests::allocationsOnThisThread());
}
#endif
LUTHIER_TEST(RefPedalDynaComp, releaseCoefficientsFollowReprepareRateAndQuality)
{
    models::MxrDynaComp reused;
    for (double sr : {192000., 44100., 96000., 48000.})
        for (float quality : {0.f, .5f, 1.f})
        {
            models::MxrDynaComp fresh;
            for (auto *p : {&reused, &fresh})
            {
                p->setParam(5, 0);
                p->setParam(0, .8f);
                p->setParam(6, quality);
                p->prepare(sr, 2048, 1);
            }
            std::array<float, kMaxParams> params{};
            for (int i = 0; i < fresh.desc().numParams; ++i)
                params[i] = fresh.getParam(i);
            CHECK(fresh.getPreparedMemoryBytes() ==
                  models::MxrDynaComp::prepareMemory(sr, 2048, 1, params.data()).ownedBytes);
            for (int block = 0; block < 8; ++block)
            {
                std::array<double, 256> l{}, r{}, x{}, y{};
                for (int i = 0; i < 256; ++i)
                {
                    const int n = block * 256 + i;
                    l[i] = x[i] = (block % 2 ? .1 : .8) * std::sin(n * .17);
                    r[i] = y[i] = .7 * std::cos(n * .13);
                }
                reused.process(l.data(), r.data(), 256);
                fresh.process(x.data(), y.data(), 256);
                for (int i = 0; i < 256; ++i)
                {
                    CHECK(l[i] == x[i]);
                    CHECK(r[i] == y[i]);
                }
            }
        }
}
