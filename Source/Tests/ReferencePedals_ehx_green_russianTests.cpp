// OWNER: WP13
#include "../ReferencePedals/Models/ehx_green_russian.h"
#include "TestFramework.h"
LUTHIER_TEST(RefPedalGreenRussian, descriptorAndPressure)
{
    using namespace luthier::reference::models;
    EhxGreenRussian a, b;
    CHECK(a.desc().numParams == 8);
    int lanes = 0;
    for (int i = 0; i < 8; ++i)
        lanes += !a.desc().params[i].isLaneExcluded();
    CHECK(lanes == 3);
    CHECK(!a.controlAvailable(7));
    a.setParam(6, 1);
    a.prepare(48000, 1, 1);
    b.prepare(48000, 1, 1);
    a.onMidi({0xd0, 127, 0, 0});
    double difference = 0;
    for (int i = 0; i < 12000; ++i)
    {
        double x = .03 * std::sin(i * .131), r = x, y = x, t = x;
        a.process(&x, &r, 1);
        b.process(&y, &t, 1);
        if (i > 10000)
            difference += std::abs(x - y);
        CHECK(std::isfinite(x));
    }
    CHECK(difference > 1e-3);
}
