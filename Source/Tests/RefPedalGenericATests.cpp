#include "../ReferencePedals/Generic/Boost.h"
#include "../ReferencePedals/Generic/Compressor.h"
#include "../ReferencePedals/Generic/Fuzz.h"
#include "../ReferencePedals/Generic/Overdrive.h"
#include "TestFramework.h"
#include <array>
#include <limits>
using namespace luthier::reference;
namespace
{
template <class P> void exerciseGenericA(luthier::tests::TestContext &ctx)
{
    for (double sr : {44100., 48000., 88200., 96000., 192000.})
        for (int size : {1, 16, 37, 64, 481, 2048})
        {
            P a, b;
            a.prepare(sr, 2048, 2);
            b.prepare(sr, 2048, 2);
            std::array<double, 4096> l{}, r{}, x{}, y{};
            for (size_t i = 0; i < l.size(); ++i)
            {
                l[i] = x[i] = std::sin(i * .19) * .2;
                r[i] = y[i] = std::cos(i * .13) * .1;
            }
            a.setParam(0, .9f);
            b.setParam(0, .9f);
            for (int i = 0; i < 4096; i += size)
                a.process(l.data() + i, r.data() + i, std::min(size, 4096 - i));
            for (int i = 0; i < 4096; ++i)
                b.process(x.data() + i, y.data() + i, 1);
            for (size_t i = 0; i < l.size(); ++i)
            {
                CHECK(l[i] == x[i]);
                CHECK(r[i] == y[i]);
                CHECK(std::isfinite(l[i]));
                CHECK(std::abs(l[i]) <= 16);
            }
            a.reset();
            double bad[] = {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            1e6,
                            -1e6,
                            1e-310,
                            0};
            double rr[6]{};
            a.process(bad, rr, 6);
            for (double v : bad)
            {
                CHECK(std::isfinite(v));
                CHECK(std::abs(v) <= 16);
            }
        }
}
} // namespace
LUTHIER_TEST(RefPedalGenericA, boostBlockAndFinite) { exerciseGenericA<generic::Boost>(ctx); }
LUTHIER_TEST(RefPedalGenericA, overdriveBlockAndFinite) { exerciseGenericA<generic::Overdrive>(ctx); }
LUTHIER_TEST(RefPedalGenericA, fuzzBlockAndFinite) { exerciseGenericA<generic::Fuzz>(ctx); }
LUTHIER_TEST(RefPedalGenericA, compressorBlockAndFinite) { exerciseGenericA<generic::Compressor>(ctx); }
LUTHIER_TEST(RefPedalGenericA, compressorStaticCurve)
{
    CHECK_NEAR(generic::Compressor::gainReductionDb(-12, -24, 4, 0), -9, 1e-12);
    CHECK_NEAR(generic::Compressor::gainReductionDb(-30, -24, 4, 0), 0, 1e-12);
    CHECK_NEAR(generic::Compressor::gainReductionDb(-24, -24, 4, 6), -.5625, 1e-12);
    generic::Compressor p;
    p.setParam(4, 0);
    p.prepare(48000, 64, 1);
    double l[] = {.1, .2, -.3, .4}, r[] = {.4, .3, .2, .1};
    p.process(l, r, 4);
    CHECK(l[0] == .1);
    CHECK(l[2] == -.3);
    CHECK(r[0] == .4);
}
LUTHIER_TEST(RefPedalGenericA, externalSidechainRequiresFreshBuffers)
{
    generic::Compressor p;
    p.setParam(10, 1);
    p.prepare(48000, 64, 1);
    double l[64], r[64], sc[64];
    std::fill_n(l, 64, .2);
    std::fill_n(r, 64, .1);
    std::fill_n(sc, 64, 1.0);
    p.process(l, r, 64);
    CHECK(l[63] == .2);
    CHECK(r[63] == .1);
    CHECK(!p.sidechainAvailable());
    p.setSidechain(sc, sc, 64);
    p.process(l, r, 64);
    CHECK(p.sidechainAvailable());
    CHECK(l[63] != .2);
    std::fill_n(l, 64, .2);
    std::fill_n(r, 64, .1);
    p.process(l, r, 64);
    CHECK(!p.sidechainAvailable());
    CHECK(l[63] == .2);
}
LUTHIER_TEST(RefPedalGenericA, latencyStableAcrossEnableAndDescriptorRoundTrips)
{
    std::array<std::unique_ptr<IPedal>, 4> pedals{
        std::make_unique<generic::Boost>(), std::make_unique<generic::Overdrive>(),
        std::make_unique<generic::Fuzz>(), std::make_unique<generic::Compressor>()};
    for (auto &p : pedals)
    {
        p->prepare(48000, 64, 2);
        const int delay = p->getLatencySamples();
        p->setEnabled(false);
        CHECK(p->getLatencySamples() == delay);
        p->setEnabled(true);
        CHECK(p->getLatencySamples() == delay);
        for (int j = 0; j < p->desc().numParams; ++j)
        {
            const auto &d = p->desc().params[j];
            if (d.kind == ParamKind::Continuous)
                for (double x : {0., .25, .5, .75, 1.})
                    CHECK_NEAR(d.toNormalised(d.fromNormalised(x)), x, 1e-6);
        }
    }
}
#ifdef LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
LUTHIER_TEST(RefPedalGenericA, realtimeCallsDoNotAllocate)
{
    generic::Boost a;
    generic::Overdrive b;
    generic::Fuzz c;
    generic::Compressor d;
    IPedal *pedals[] = {&a, &b, &c, &d};
    for (auto *p : pedals)
    {
        p->prepare(192000, 2048, 2);
        std::array<double, 2048> l{}, r{};
        const auto before = luthier::tests::allocationsOnThisThread();
        for (int i = 0; i < 32; ++i)
        {
            for (int j = 0; j < p->desc().numParams; ++j)
                p->setParam(j, (float)(i % 5) * .25f);
            p->setEnabled((i % 3) != 0);
            p->setSidechain(l.data(), r.data(), 2048);
            p->process(l.data(), r.data(), 2048);
            p->onMidi({});
            p->onFoot({});
            p->reset();
        }
        const auto after = luthier::tests::allocationsOnThisThread();
        CHECK(after == before);
    }
}
#endif
LUTHIER_TEST(RefPedalGenericA, referenceLevelAndCleanBoostBandwidth)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        double lowest = 100, highest = -100;
        for (float drive : {0.f, .25f, .4f, .5f, .75f, 1.f})
        {
            generic::Overdrive p;
            p.setParam(0, drive);
            p.setParam(5, 1);
            p.prepare(sr, 1, 2);
            double power = 0;
            int count = 0;
            for (int i = 0; i < (int)(sr * .2); ++i)
            {
                double x = common::dbGain(-18) * std::sin(2 * common::pi * 1000 * i / sr), r = x;
                p.process(&x, &r, 1);
                if (i > (int)(sr * .1))
                {
                    power += x * x;
                    ++count;
                }
            }
            double gain = 10 * std::log10(2 * power / count) + 18;
            CHECK(std::abs(gain) <= 3);
            lowest = std::min(lowest, gain);
            highest = std::max(highest, gain);
        }
        CHECK(highest - lowest <= 6);
        for (double hz : {20., 1000., 10000., 20000.})
        {
            generic::Boost p;
            p.setParam(0, 0);
            p.setParam(2, .8f);
            p.setParam(4, 1);
            p.prepare(sr, 1, 2);
            double power = 0;
            int count = 0;
            for (int i = 0; i < (int)sr; ++i)
            {
                double x = common::dbGain(-30) * std::sin(2 * common::pi * hz * i / sr), r = x;
                p.process(&x, &r, 1);
                if (i > (int)(sr * .5))
                {
                    power += x * x;
                    ++count;
                }
            }
            CHECK(std::abs(10 * std::log10(2 * power / count) + 30) <= .3);
        }
    }
}
namespace
{
template <class P> void monoThenStereo(luthier::tests::TestContext &ctx)
{
    P pair, left, right;
    pair.prepare(48000, 1, 2);
    left.prepare(48000, 1, 2);
    right.prepare(48000, 1, 2);
    for (int i = 0; i < 4096; ++i)
    {
        double a = .2 * std::sin(i * .17), b = i < 1023 ? a : .1 * std::cos(i * .13), l = a, r = b,
               unusedL = a, unusedR = b;
        pair.process(&a, &b, 1);
        left.process(&l, &unusedL, 1);
        right.process(&r, &unusedR, 1);
        CHECK(a == l);
        CHECK(b == r);
    }
}
} // namespace
LUTHIER_TEST(RefPedalGenericA, deferredStereoStateIsExact)
{
    monoThenStereo<generic::Boost>(ctx);
    monoThenStereo<generic::Overdrive>(ctx);
    monoThenStereo<generic::Fuzz>(ctx);
}

// E38 calibration is -18 dBFS peak. The original nominal level also admits
// -18 dBFS RMS relative to 1.0; retain both default-unity and drive-span gates.
LUTHIER_TEST(RefPedalGenericA, rmsReferenceRetainsOriginalUnityAndDriveSpan)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        double low = 100, high = -100;
        for (float drive : {0.f, .25f, .4f, .5f, .75f, 1.f})
        {
            generic::Overdrive p;
            p.setParam(0, drive);
            p.setParam(5, 1);
            p.prepare(sr, 1, 2);
            double power = 0;
            int count = 0;
            for (int i = 0; i < (int)(sr * .3); ++i)
            {
                double x = std::sqrt(2.0) * common::dbGain(-18) * std::sin(2 * common::pi * 1000 * i / sr),
                       r = x;
                p.process(&x, &r, 1);
                if (i > (int)(sr * .2))
                {
                    power += x * x;
                    ++count;
                }
            }
            const double gain = 10 * std::log10(power / count) + 18;
            if (drive == .4f)
                CHECK(std::abs(gain) <= 3);
            low = std::min(low, gain);
            high = std::max(high, gain);
        }
        CHECK(high - low <= 6);
    }
}
