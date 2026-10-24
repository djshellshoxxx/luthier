// OWNER: WP11
#include "../ReferencePedals/Models/boss_ds1.h"
#include "TestFramework.h"
#include <array>
#include <cstring>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalDs1, descriptorAndPartitioning)
{
    models::BossDs1 descriptor;
    CHECK(std::strcmp(descriptor.desc().modelId, "boss-ds1") == 0);
    CHECK(descriptor.desc().numParams == 8);
    int lanes = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto &p = descriptor.desc().params[i];
        CHECK(std::strncmp(p.id, "boss_ds1_", 9) == 0);
        lanes += !p.isLaneExcluded();
    }
    CHECK(lanes == 3);
    CHECK(!descriptor.controlAvailable(6));
    for (double sr : {44100., 48000., 96000., 192000.})
        for (int block : {1, 37, 2048})
        {
            models::BossDs1 a, b;
            a.setParam(7, 0);
            b.setParam(7, 0);
            a.prepare(sr, 2048, 1);
            b.prepare(sr, 2048, 1);
            std::array<double, 4096> l{}, r{}, x{}, y{};
            for (int i = 0; i < 4096; ++i)
            {
                l[i] = x[i] = .1 * std::sin(i * .173);
                r[i] = y[i] = .03 * std::cos(i * .071);
            }
            for (int i = 0; i < 4096; i += block)
                a.process(l.data() + i, r.data() + i, std::min(block, 4096 - i));
            for (int i = 0; i < 4096; ++i)
                b.process(x.data() + i, y.data() + i, 1);
            for (int i = 0; i < 4096; ++i)
            {
                CHECK(l[i] == x[i]);
                CHECK(r[i] == y[i]);
                CHECK(std::isfinite(l[i]));
                CHECK(std::isfinite(r[i]));
            }
            a.reset();
            l.fill(0);
            r.fill(0);
            a.process(l.data(), r.data(), 2048);
            for (int i = 0; i < 2048; ++i)
            {
                CHECK(l[i] == 0);
                CHECK(r[i] == 0);
            }
        }
}
