// OWNER: WP13
#include "../ReferencePedals/Models/ehx_muff_wicker.h"
#include "TestFramework.h"
LUTHIER_TEST(RefPedalMuffWicker, descriptorAndToneOutInvariance)
{
    using namespace luthier::reference::models;
    EhxMuffWicker a, b;
    CHECK(a.desc().numParams == 15);
    int lanes = 0;
    for (int i = 0; i < 15; ++i)
        lanes += !a.desc().params[i].isLaneExcluded();
    CHECK(lanes == 8);
    a.setParam(4, 0);
    b.setParam(4, 0);
    a.setParam(2, 0);
    b.setParam(2, 1);
    a.prepare(48000, 1, 1);
    b.prepare(48000, 1, 1);
    for (int i = 0; i < 12000; ++i)
    {
        double x = .1 * std::sin(i * .131), r = x, y = x, t = x;
        a.process(&x, &r, 1);
        b.process(&y, &t, 1);
        CHECK(x == y);
        CHECK(r == t);
    }
    CHECK(!a.controlAvailable(10));
    CHECK(!a.controlAvailable(14));
}
LUTHIER_TEST(RefPedalMuffWicker, feedbackReadBeforeWriteHasFiveMillisecondAge)
{
    // The feedback loop reads the previous output history before storing the
    // current output. Its impulse centroid must retain the physical 5 ms age,
    // including the half-sample case at 44.1 kHz.
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        luthier::reference::common::FractionalDelay history;
        history.prepare((int)std::ceil(sr * .005) + 2);
        double sum = 0, moment = 0;
        for (int i = 0; i < 2000; ++i)
        {
            const double y = history.read(sr * .005 - 1);
            sum += y;
            moment += i * y;
            history.push(i == 0 ? 1. : 0.);
        }
        CHECK_NEAR(sum, 1., 1e-12);
        CHECK_NEAR(moment / sum / sr, .005, 1e-12);
    }
}
