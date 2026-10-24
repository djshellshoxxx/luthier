#include "../ReferencePedals/Common/LowLatencyOversampler.h"
#include "TestFramework.h"
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalLowLatencyOversampler, impulseLatencyAndWetPassband)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        LowLatencyOversampler os;
        os.prepareFactor(sr, 2);
        CHECK(os.factor() == 2);
        CHECK(os.latencySamples() == 15);
        double largest = 0;
        int peak = 0;
        for (int i = 0; i < 64; ++i)
        {
            double y = os.process(i == 0 ? 1 : 0, [](double x) { return x; });
            if (std::abs(y) > largest)
            {
                largest = std::abs(y);
                peak = i;
            }
        }
        CHECK(peak == 15);
        for (double hz : {100., 1000., 6500.})
        {
            os.reset();
            double power = 0;
            int count = 0;
            for (int i = 0; i < (int)sr; ++i)
            {
                double y = os.process(std::sin(2 * pi * hz * i / sr), [](double x) { return x; });
                if (i > (int)sr / 2)
                {
                    power += y * y;
                    ++count;
                }
            }
            CHECK(std::abs(10 * std::log10(2 * power / count)) < .3);
        }
        os.prepareFactor(sr, 1);
        CHECK(os.latencySamples() == 0);
        CHECK(os.process(.123, [](double x) { return x; }) == .123);
    }
}
LUTHIER_TEST(RefPedalLowLatencyOversampler, fourTimesHasIntegerLatency)
{
    LowLatencyOversampler os;
    os.prepareFactor(48000, 4);
    CHECK(os.factor() == 4);
    CHECK(os.latencySamples() == 23);
    double largest = 0;
    int peak = 0;
    for (int i = 0; i < 128; ++i)
    {
        double y = os.process(i == 0 ? 1 : 0, [](double x) { return x; });
        if (std::abs(y) > largest)
        {
            largest = std::abs(y);
            peak = i;
        }
    }
    CHECK(peak == 23);
}
