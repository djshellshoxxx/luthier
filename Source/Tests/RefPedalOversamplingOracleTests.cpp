#include "../ReferencePedals/Common/OversamplerAdapter.h"
#include "TestFramework.h"
using namespace luthier::reference::common;
namespace
{
struct FullHalfbandOracle
{
    std::array<double, 257> taps{};
    std::array<double, 512> upHistory{}, downHistory{};
    unsigned upHead = 0, downHead = 0;
    int length = 0;
    void prepare(bool first)
    {
        length = first ? 257 : 33;
        int m = (length - 1) / 2;
        double sum = 0;
        for (int k = 1; k < m; k += 2)
        {
            double distance = k - m;
            double w = first ? std::cyl_bessel_i(0, 6.5 * std::sqrt(1 - distance * distance / (m * m))) /
                                   std::cyl_bessel_i(0, 6.5)
                             : .35875 - .48829 * std::cos(pi * k / m) + .14128 * std::cos(2 * pi * k / m) -
                                   .01168 * std::cos(3 * pi * k / m);
            taps[k] = taps[2 * m - k] = std::sin(.5 * pi * distance) / (pi * distance) * w;
            sum += 2 * taps[k];
        }
        for (auto &t : taps)
            t *= .5 / sum;
        taps[m] = .5;
    }
    double filter(std::array<double, 512> &state, unsigned &head, double x)
    {
        state[head] = x;
        double y = 0;
        for (int k = 0; k < length; ++k)
            y += taps[k] * state[(head - k) & 511];
        head = (head + 1) & 511;
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
LUTHIER_TEST(RefPedalOversamplingOracle, shortUpperStagesMatchFullConvolution)
{
    for (int factor : {2, 4, 8})
    {
        OversamplerAdapter os;
        os.prepareFactor(48000, factor);
        FullHalfbandOracle stages[3];
        for (int i = 0; i < 3; ++i)
            stages[i].prepare(i == 0);
        double worst = 0;
        for (int i = 0; i < 4096; ++i)
        {
            double x = .7 * std::sin(i * .713) + .2 * std::cos(i * .043);
            double expected[8]{x}, next[8]{};
            int count = 1, level = 0;
            while (count < factor)
            {
                for (int j = 0; j < count; ++j)
                    stages[level].up(expected[j], next[j * 2], next[j * 2 + 1]);
                std::copy_n(next, count * 2, expected);
                count *= 2;
                ++level;
            }
            for (int j = 0; j < count; ++j)
                expected[j] = std::tanh(17 * expected[j]);
            while (count > 1)
            {
                --level;
                for (int j = 0; j < count / 2; ++j)
                    next[j] = stages[level].down(expected[j * 2], expected[j * 2 + 1]);
                count /= 2;
                std::copy_n(next, count, expected);
            }
            worst = std::max(
                worst, std::abs(expected[0] - os.process(x, [](double v) { return std::tanh(17 * v); })));
        }
        CHECK(worst < 1e-13);
        CHECK(os.latencySamples() == (factor == 2 ? 128 : factor == 4 ? 136 : 140));
    }
}
