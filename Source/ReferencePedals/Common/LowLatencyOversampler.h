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
            evenHistory.fill(0);
            oddHistory.fill(0);
            upHead = downHead = 0;
        }
        void up(double x, double &a, double &b) noexcept
        {
            upHistory[upHead] = safe(x);
            double sum = 0;
            for (unsigned j = 0; j < 8; ++j)
                sum += coefficients[j] * (upHistory[(upHead - j) & 31] + upHistory[(upHead - (15 - j)) & 31]);
            const double interpolated = safe(2 * sum);
            const double delayed = upHistory[(upHead - (unsigned)(centre / 2)) & 31];
            if (parity == 0)
            {
                a = interpolated;
                b = delayed;
            }
            else
            {
                a = delayed;
                b = interpolated;
            }
            upHead = (upHead + 1) & 31;
        }
        double down(double a, double b) noexcept
        {
            evenHistory[downHead] = safe(a);
            oddHistory[downHead] = safe(b);
            double result;
            if (parity == 0)
            {
                result = .5 * oddHistory[(downHead - 8) & 31];
                for (unsigned j = 0; j < 8; ++j)
                    result += coefficients[j] *
                              (evenHistory[(downHead - j) & 31] + evenHistory[(downHead - (15 - j)) & 31]);
            }
            else
            {
                result = .5 * evenHistory[(downHead - 8) & 31];
                for (unsigned j = 0; j < 8; ++j)
                    result += coefficients[j] * (oddHistory[(downHead - (j + 1)) & 31] +
                                                 oddHistory[(downHead - (16 - j)) & 31]);
            }
            downHead = (downHead + 1) & 31;
            return safe(result);
        }

      private:
        std::array<double, 8> coefficients{};
        std::array<double, 32> upHistory{}, evenHistory{}, oddHistory{};
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
