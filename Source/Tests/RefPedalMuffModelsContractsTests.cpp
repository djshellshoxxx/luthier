// OWNER: WP13
#include "../ReferencePedals/Models/ehx_big_muff_nyc.h"
#include "../ReferencePedals/Models/ehx_deluxe_muff.h"
#include "../ReferencePedals/Models/ehx_green_russian.h"
#include "../ReferencePedals/Models/ehx_muff_wicker.h"
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
namespace
{
template <class Model> void exercise(luthier::tests::TestContext &ctx)
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (int block : {1, 37, 2048})
        {
            Model a, b;
            a.prepare(sr, 2048, 1);
            b.prepare(sr, 2048, 1);
            std::array<double, 4096> l{}, r{}, x{}, y{};
            for (int i = 0; i < 4096; ++i)
            {
                l[i] = x[i] = .1 * std::sin(i * .173);
                r[i] = y[i] = .07 * std::cos(i * .119);
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
            for (int event = 0; event < 16; ++event)
            {
                l.fill(.03);
                r.fill(-.07);
                l[0] = std::numeric_limits<double>::quiet_NaN();
                r[1] = std::numeric_limits<double>::infinity();
                l[2] = 1e6;
                r[3] = -1e6;
                l[4] = 1e-310;
#if defined(LUTHIER_ALLOCATION_COUNTER)
                const auto before = luthier::tests::allocationsOnThisThread();
#endif
                for (int k = 0; k < a.desc().numParams; ++k)
                    a.setParam(k, float((event * 17 + k * 13) % 101) / 100);
                a.onMidi({0xd0, (uint8_t)(event * 8), 0, 0});
                a.setExpression(0, event / 15.f);
                a.processBypass(l.data(), r.data(), 2048);
                a.process(l.data(), r.data(), 2048);
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
} // namespace
LUTHIER_TEST(RefPedalMuffContracts, nyc) { exercise<models::EhxBigMuffNyc>(ctx); }
LUTHIER_TEST(RefPedalMuffContracts, wicker) { exercise<models::EhxMuffWicker>(ctx); }
LUTHIER_TEST(RefPedalMuffContracts, green) { exercise<models::EhxGreenRussian>(ctx); }
LUTHIER_TEST(RefPedalMuffContracts, deluxe) { exercise<models::EhxDeluxeMuff>(ctx); }
