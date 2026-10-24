// OWNER: WP11
#include "../ReferencePedals/Models/boss_ds1.h"
#include "../ReferencePedals/Models/boss_sd1.h"
#include "../ReferencePedals/Models/ibanez_ts808.h"
#include "TestFramework.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalNamedBypass, independentPreparedBufferStates)
{
    for (int model = 0; model < 3; ++model)
    {
        std::unique_ptr<IPedal> a, b;
        if (model == 0)
        {
            a = std::make_unique<models::BossDs1>();
            b = std::make_unique<models::BossDs1>();
        }
        if (model == 1)
        {
            a = std::make_unique<models::IbanezTs808>();
            b = std::make_unique<models::IbanezTs808>();
        }
        if (model == 2)
        {
            a = std::make_unique<models::BossSd1>();
            b = std::make_unique<models::BossSd1>();
        }
        if (model < 2)
        {
            a->setParam(7, 0);
            b->setParam(7, 0);
        }
        a->prepare(48000, 1, 1);
        b->prepare(48000, 1, 1);
        double changed = 0;
        for (int i = 0; i < 4096; ++i)
        {
            double x = .1 * std::sin(i * .013), r = x, original = x;
            a->processBypass(&x, &r, 1);
            changed += std::abs(x - original);
        }
        CHECK(changed > 1e-3);
        for (int i = 0; i < 4096; ++i)
        {
            double x = .1 * std::sin(i * .13), r = x, y = x, t = x;
            a->process(&x, &r, 1);
            b->process(&y, &t, 1);
            CHECK(x == y);
            CHECK(r == t);
        }
        if (model == 2)
        {
            a->setParam(7, 0);
            for (int i = 0; i < 100; ++i)
            {
                double x = .1 * std::sin(i * .31), r = x, original = x;
                a->processBypass(&x, &r, 1);
                CHECK(x == original);
                CHECK(r == original);
            }
        }
    }
}
