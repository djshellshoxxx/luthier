// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
namespace luthier::reference::common
{
// Short linear-phase halfbands for mild, already-bandlimited saturation.
// Stage one31taps; optional stage two33taps makes4x total delay integer23.
// This is not the high-gain drive-pedal alias-control path.
class LowLatencyOversampler
{
    class Stage
    {
      public:
        void prepare(int midpoint) noexcept
        {
            centre = midpoint;
            parity = centre % 2 ? 0 : 1;
            double sum = 0;
            for (int j = 0; j < 8; ++j)
            {
                const int k = 2 * j + parity;
                const double m = k - (double)centre;
                const double window =
                    .42 - .5 * std::cos(pi * k / centre) + .08 * std::cos(2 * pi * k / centre);
                coefficients[(size_t)j] = std::sin(.5 * pi * m) / (pi * m) * window;
                sum += 2 * coefficients[(size_t)j];
            }
            for (auto &c : coefficients)
                c *= .5 / sum;
            reset();
        }
        void reset() noexcept
        {
            upHistory.fill(0);
            downHistory.fill(0);
            upHead = downHead = 0;
        }
        void up(double x, double &a, double &b) noexcept
        {
            a = 2 * filter(upHistory, upHead, x);
            b = 2 * filter(upHistory, upHead, 0);
        }
        double down(double a, double b) noexcept
        {
            double y = filter(downHistory, downHead, a);
            downHistory[downHead] = safe(b);
            downHead = (downHead + 1) & 63;
            return y;
        }

      private:
        double filter(std::array<double, 64> &data, unsigned &head, double x) noexcept
        {
            data[head] = safe(x);
            double result = .5 * data[(head - (unsigned)centre) & 63];
            for (unsigned j = 0; j < 8; ++j)
            {
                const unsigned k = 2 * j + (unsigned)parity;
                result += coefficients[j] *
                          (data[(head - k) & 63] + data[(head - ((unsigned)(2 * centre) - k)) & 63]);
            }
            head = (head + 1) & 63;
            return safe(result);
        }
        std::array<double, 8> coefficients{};
        std::array<double, 64> upHistory{}, downHistory{};
        unsigned upHead = 0, downHead = 0;
        int centre = 15, parity = 0;
    };

  public:
    void prepareFactor(double sr, int requestedFactor) noexcept
    {
        baseRate = sampleRate(sr);
        multiplier = requestedFactor >= 4 ? 4 : requestedFactor >= 2 ? 2 : 1;
        stages[0].prepare(15);
        stages[1].prepare(16);
    }
    void reset() noexcept
    {
        for (auto &s : stages)
            s.reset();
    }
    int factor() const noexcept { return multiplier; }
    int latencySamples() const noexcept { return multiplier == 4 ? 23 : multiplier == 2 ? 15 : 0; }
    double rate() const noexcept { return baseRate * multiplier; }
    template <class F> double process(double x, F &&shaper) noexcept
    {
        x = std::clamp(safe(x), -1e6, 1e6);
        if (multiplier == 1)
            return safe(shaper(x));
        double a, b;
        stages[0].up(x, a, b);
        if (multiplier == 2)
        {
            a = safe(shaper(a));
            b = safe(shaper(b));
            return stages[0].down(a, b);
        }
        double c[4];
        stages[1].up(a, c[0], c[1]);
        stages[1].up(b, c[2], c[3]);
        for (auto &v : c)
            v = safe(shaper(v));
        a = stages[1].down(c[0], c[1]);
        b = stages[1].down(c[2], c[3]);
        return stages[0].down(a, b);
    }

  private:
    std::array<Stage, 2> stages;
    double baseRate = 48000;
    int multiplier = 1;
};
} // namespace luthier::reference::common
