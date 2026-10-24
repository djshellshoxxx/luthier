// OWNER: WP11
#include "../ReferencePedals/Models/boss_ds1.h"
#include "../ReferencePedals/Models/boss_sd1.h"
#include "../ReferencePedals/Models/ibanez_ts808.h"
#include "TestFramework.h"
#include <array>
#include <limits>
using namespace luthier::reference;
#if defined(LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
#endif
LUTHIER_TEST(RefPedalNamedDrives, automationHostileInputsAndNoAllocations)
{
    models::BossDs1 ds;
    models::BossSd1 sd;
    models::IbanezTs808 ts;
    for (IPedal *p : {static_cast<IPedal *>(&ds), static_cast<IPedal *>(&sd), static_cast<IPedal *>(&ts)})
    {
        p->prepare(48000, 2048, 1);
        std::array<double, 2048> l{}, r{};
        for (int j = 0; j < 64; ++j)
        {
            for (int i = 0; i < 2048; ++i)
            {
                l[i] = .1 * std::sin(i * .13);
                r[i] = .07 * std::cos(i * .17);
            }
            l[0] = std::numeric_limits<double>::quiet_NaN();
            r[1] = std::numeric_limits<double>::infinity();
            l[2] = 1e6;
            r[3] = -1e6;
            l[4] = 1e-310;
#if defined(LUTHIER_ALLOCATION_COUNTER)
            const auto before = luthier::tests::allocationsOnThisThread();
#endif
            for (int k = 0; k < p->desc().numParams; ++k)
                p->setParam(k, float((j * 17 + k * 13) % 101) / 100);
            p->onFoot({j % 2 == 0, .8f, .4f, 0});
            p->onMidi({0xd0, (uint8_t)(j * 2), 0, 0});
            p->setExpression(0, j / 63.f);
            p->process(l.data(), r.data(), 2048);
#if defined(LUTHIER_ALLOCATION_COUNTER)
            CHECK(luthier::tests::allocationsOnThisThread() == before);
#endif
            for (int i = 0; i < 2048; ++i)
            {
                CHECK(std::isfinite(l[i]));
                CHECK(std::isfinite(r[i]));
                CHECK(std::abs(l[i]) <= 16);
                CHECK(std::abs(r[i]) <= 16);
            }
        }
    }
}
LUTHIER_TEST(RefPedalNamedDrives, sd1ExtensionControlsAreAudible)
{
    // Each virtual input has an independently rendered default reference.
    // Reversed expression bounds must also produce an audible, bounded result.
    for (int mode = 0; mode < 5; ++mode)
    {
        models::BossSd1 a, b;
        if (mode == 0)
            a.setParam(8, 1);
        if (mode == 1)
            a.setParam(9, 1);
        if (mode == 2)
        {
            a.setParam(10, 1.f / 3);
            a.setParam(11, 1);
        }
        if (mode == 3)
        {
            a.setParam(12, 1);
            a.setParam(13, 1);
            a.setParam(14, 0);
        }
        a.prepare(48000, 1, 1);
        b.prepare(48000, 1, 1);
        if (mode <= 1)
            a.onFoot({true, 1, 0, 0});
        if (mode == 2)
            a.onMidi({0xd0, 127, 0, 0});
        if (mode == 3)
            a.setExpression(0, 1);
        if (mode == 4)
            a.setParam(6, 0);
        double difference = 0;
        for (int i = 0; i < 12000; ++i)
        {
            double x = .02 * std::sin(i * .131), r = x, y = x, t = x;
            a.process(&x, &r, 1);
            b.process(&y, &t, 1);
            if (i > 10000)
                difference += std::abs(x - y);
        }
        CHECK(difference > 1e-3);
    }
}
