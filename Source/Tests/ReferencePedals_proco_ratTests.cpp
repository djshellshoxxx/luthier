// OWNER: WP12
#include "../ReferencePedals/Models/proco_rat.h"
#include "TestFramework.h"
#include <array>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalRat, circuitContractAndPartitioning)
{
    models::ProcoRat p;
    CHECK(p.desc().numParams == 11);
    int lanes = 0;
    for (int i = 0; i < 11; ++i)
        lanes += !p.desc().params[i].isLaneExcluded();
    CHECK(lanes == 3);
    CHECK_NEAR(models::ProcoRat::toneCutoff(0), 32152.51375593845, 1e-6);
    CHECK_NEAR(models::ProcoRat::toneCutoff(1), 475.1603018118983, 1e-6);
    for (double sr : {44100., 48000., 96000., 192000.})
        for (int block : {1, 37, 2048})
        {
            models::ProcoRat a, b;
            a.prepare(sr, 2048, 1);
            b.prepare(sr, 2048, 1);
            a.setParam(4, 1);
            b.setParam(4, 1);
            a.setParam(0, .9f);
            b.setParam(0, .9f);
            std::array<double, 4096> l{}, r{}, x{}, y{};
            for (int i = 0; i < 4096; ++i)
            {
                l[i] = x[i] = .1 * std::sin(i * .17);
                r[i] = y[i] = .07 * std::cos(i * .11);
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
