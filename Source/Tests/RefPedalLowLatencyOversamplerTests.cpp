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
namespace
{
// Independent full-tap convolution oracle, intentionally not polyphase/symmetric.
struct ShortFirOracle
{
    std::array<double, 33> taps{};
    std::array<double, 64> upHistory{}, downHistory{};
    unsigned upHead = 0, downHead = 0;
    void prepare(int midpoint)
    {
        const int parity = midpoint % 2 ? 0 : 1;
        double sum = 0;
        for (int k = parity; k < midpoint; k += 2)
        {
            const double m = k - midpoint,
                         w = .42 - .5 * std::cos(pi * k / midpoint) + .08 * std::cos(2 * pi * k / midpoint);
            taps[k] = taps[2 * midpoint - k] = std::sin(.5 * pi * m) / (pi * m) * w;
            sum += 2 * taps[k];
        }
        for (auto &t : taps)
            t *= .5 / sum;
        taps[midpoint] = .5;
    }
    double filter(std::array<double, 64> &history, unsigned &head, double x)
    {
        history[head] = x;
        double y = 0;
        for (unsigned k = 0; k < taps.size(); ++k)
            y += taps[k] * history[(head - k) & 63];
        head = (head + 1) & 63;
        return y;
    }
    void up(double x, double &a, double &b)
    {
        a = 2 * filter(upHistory, upHead, x);
        b = 2 * filter(upHistory, upHead, 0);
    }
    double down(double a, double b)
    {
        double y = filter(downHistory, downHead, a);
        filter(downHistory, downHead, b);
        return y;
    }
};
} // namespace
LUTHIER_TEST(RefPedalLowLatencyOversampler, agreesWithIndependentFullConvolution)
{
    for (int factor : {2, 4})
    {
        LowLatencyOversampler os;
        os.prepareFactor(48000, factor);
        ShortFirOracle first, second;
        first.prepare(15);
        second.prepare(16);
        for (int i = 0; i < 8192; ++i)
        {
            const double x = .7 * std::sin(i * .931) + .2 * std::cos(i * .071);
            const double actual = os.process(x, [](double v) { return std::tanh(v); });
            double a, b;
            first.up(x, a, b);
            if (factor == 2)
            {
                a = std::tanh(a);
                b = std::tanh(b);
            }
            else
            {
                double c[4];
                second.up(a, c[0], c[1]);
                second.up(b, c[2], c[3]);
                for (auto &v : c)
                    v = std::tanh(v);
                a = second.down(c[0], c[1]);
                b = second.down(c[2], c[3]);
            }
            CHECK_NEAR(actual, first.down(a, b), 1e-14);
        }
    }
}
