// OWNER: WP11
#include "../ReferencePedals/Models/boss_sd1.h"
#include "TestFramework.h"
#include <array>
#include <cstring>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalSd1, parameterContractAndBoundedCircuit)
{
    models::BossSd1 p;
    CHECK(p.desc().numParams == 15);
    int lanes = 0;
    for (int i = 0; i < 15; ++i)
    {
        const auto &d = p.desc().params[i];
        CHECK(std::strncmp(d.id, "boss_sd1_", 9) == 0);
        lanes += !d.isLaneExcluded();
    }
    CHECK(lanes == 6);
    CHECK(p.controlAvailable(7));
    CHECK(!p.qualityAvailable(0));
    CHECK(!p.qualityAvailable(2));
    CHECK(p.qualityAvailable(1));
    p.setParam(4, 0);
    CHECK_NEAR(p.getParam(4), .5, 1e-7);
    for (double sr : {44100., 48000., 96000., 192000.})
        for (int block : {1, 37, 2048})
        {
            models::BossSd1 a, b;
            a.prepare(sr, 2048, 1);
            b.prepare(sr, 2048, 1);
            a.setParam(0, .9f);
            b.setParam(0, .9f);
            a.setParam(5, 1);
            b.setParam(5, 1);
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
