// OWNER: WP13
#include "../ReferencePedals/Models/ehx_deluxe_muff.h"
#include "TestFramework.h"
LUTHIER_TEST(RefPedalDeluxeMuff, descriptorAndFunctionalMids)
{
    using namespace luthier::reference::models;
    EhxDeluxeMuff a, b;
    CHECK(a.desc().numParams == 18);
    int lanes = 0;
    for (int i = 0; i < 18; ++i)
        lanes += !a.desc().params[i].isLaneExcluded();
    CHECK(lanes == 11);
    a.setParam(6, 1);
    a.setParam(7, 1);
    a.prepare(48000, 1, 1);
    b.prepare(48000, 1, 1);
    double difference = 0;
    for (int i = 0; i < 12000; ++i)
    {
        double x = .1 * std::sin(i * .163), r = x, y = x, t = x;
        a.process(&x, &r, 1);
        b.process(&y, &t, 1);
        if (i > 10000)
            difference += std::abs(x - y);
        CHECK(std::isfinite(x));
    }
    CHECK(difference > 1);
    CHECK(a.controlAvailable(14));
    a.setParam(14, 0);
    CHECK(a.getParam(14) == 0);
    a.setExpression(0, 1);
    CHECK(a.getParam(12) == 1);
    CHECK(a.getParam(11) == 1);
}
