// OWNER: WP11
#include "../ReferencePedals/Models/ibanez_ts808.h"
#include "TestFramework.h"
#include <array>
#include <cstring>
#include <limits>
using namespace luthier::reference;
LUTHIER_TEST(RefPedalTs808, descriptorAndCircuitContract)
{
    models::IbanezTs808 p;
    CHECK(std::strcmp(p.desc().modelId, "ibanez-ts808") == 0);
    CHECK(p.desc().numParams == 9);
    int lanes = 0;
    for (int i = 0; i < p.desc().numParams; ++i)
    {
        const auto &d = p.desc().params[i];
        CHECK(std::strncmp(d.id, "ibanez_ts808_", 12) == 0);
        lanes += !d.isLaneExcluded();
        p.setParam(i, .37f);
        CHECK_NEAR(p.getParam(i), .37f, 1e-7);
    }
    CHECK(lanes == 4);
    CHECK_NEAR(models::IbanezTs808::feedbackResistance(0), 51000, 1e-9);
    CHECK_NEAR(models::IbanezTs808::feedbackResistance(1), 551000, 1e-9);
    CHECK_NEAR(models::IbanezTs808::feedbackResistance(.5), 139388.34764831845, 1e-6);
    CHECK(!p.controlAvailable(6)); // Board-owned variable bypass ramp not wired.
    CHECK(!p.controlAvailable(8)); // Phase-two chip variants deliberately unavailable.
}
LUTHIER_TEST(RefPedalTs808, deterministicCircuitAcrossRatesAndPartitions)
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (int block : {1, 37, 2048})
        {
            models::IbanezTs808 a, b;
            a.setParam(7, 0);
            b.setParam(7, 0);
            a.prepare(sr, 2048, 1);
            b.prepare(sr, 2048, 1);
            CHECK(a.getLatencySamples() == (sr >= 96000 ? 128 : 136));
            std::array<double, 4096> l{}, r{}, x{}, y{};
            for (int i = 0; i < 4096; ++i)
            {
                l[i] = x[i] = .1 * std::sin(i * .137);
                r[i] = y[i] = .07 * std::cos(i * .073);
            }
            l[11] = x[11] = std::numeric_limits<double>::quiet_NaN();
            r[23] = y[23] = std::numeric_limits<double>::infinity();
            l[25] = x[25] = 1e6;
            for (int i = 0; i < 4096; i += block)
                a.process(l.data() + i, r.data() + i, std::min(block, 4096 - i));
            for (int i = 0; i < 4096; ++i)
                b.process(x.data() + i, y.data() + i, 1);
            double difference = 0;
            for (int i = 0; i < 4096; ++i)
            {
                CHECK(l[i] == x[i]);
                CHECK(r[i] == y[i]);
                CHECK(std::isfinite(l[i]));
                CHECK(std::isfinite(r[i]));
                difference += std::abs(l[i] - r[i]);
            }
            CHECK(difference > 1);
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
