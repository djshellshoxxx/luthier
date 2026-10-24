// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common
{
// Bilinear transform of the analog RC lowpass, with prewarped cutoff.
class OnePole
{
  public:
    void setLowpass(double sr, double hz) noexcept
    {
        double g = std::tan(pi * frequency(sr, hz) / sampleRate(sr));
        b = g / (1 + g);
        a = (g - 1) / (g + 1);
    }
    double process(double x) noexcept
    {
        x = safe(x);
        double y = safe(b * (x + x1) - a * y1);
        x1 = x;
        y1 = y;
        return y;
    }
    double highpass(double x) noexcept
    {
        x = safe(x);
        return safe(x - process(x));
    }
    void reset() noexcept { x1 = y1 = 0; }

  private:
    double a = 0, b = 1, x1 = 0, y1 = 0;
};
} // namespace luthier::reference::common

namespace luthier::reference::common
{
// First-order analog shelving transfer, bilinear with prewarped midpoint.
class FirstOrderShelf
{
  public:
    void set(double sr, double hz, double db, bool high) noexcept
    {
        const double gain = dbGain(db), root = std::sqrt(gain),
                     g = std::tan(pi * frequency(sr, hz) / sampleRate(sr));
        if (high)
        {
            const double denominator = g + 1 / root;
            b0 = (g + root) / denominator;
            b1 = (g - root) / denominator;
            a1 = (g - 1 / root) / denominator;
        }
        else
        {
            const double denominator = g + root;
            b0 = gain * (g + 1 / root) / denominator;
            b1 = gain * (g - 1 / root) / denominator;
            a1 = (g - root) / denominator;
        }
    }
    double process(double x) noexcept
    {
        x = safe(x);
        const double y = safe(b0 * x + state);
        state = safe(b1 * x - a1 * y);
        return y;
    }
    void reset() noexcept { state = 0; }

  private:
    double b0 = 1, b1 = 0, a1 = 0, state = 0;
};
} // namespace luthier::reference::common
