// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common
{
// RBJ cookbook coefficients, transposed direct form II, double state.
class Biquad
{
  public:
    void setLowpass(double sr, double hz, double q = 0.7071067811865476) noexcept
    {
        setPass(sr, hz, q, false);
    }
    void setHighpass(double sr, double hz, double q = 0.7071067811865476) noexcept
    {
        setPass(sr, hz, q, true);
    }
    void setPeak(double sr, double hz, double db, double q) noexcept
    {
        const double A = std::sqrt(dbGain(db)), w = 2 * pi * frequency(sr, hz) / sampleRate(sr),
                     alpha = std::sin(w) / (2 * std::clamp(safe(q), .05, 100.)), c = -2 * std::cos(w);
        coefficients(1 + alpha * A, c, 1 - alpha * A, 1 + alpha / A, c, 1 - alpha / A);
    }
    void setShelf(double sr, double hz, double db, bool high) noexcept
    {
        const double A = std::sqrt(dbGain(db)), w = 2 * pi * frequency(sr, hz) / sampleRate(sr),
                     c = std::cos(w), s = std::sin(w), t = std::sqrt(2 * A) * s;
        if (high)
            coefficients(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c),
                         A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t,
                         2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
        else
            coefficients(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c),
                         A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t,
                         -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    double process(double x) noexcept
    {
        x = safe(x);
        double y = safe(b0 * x + z1);
        z1 = safe(b1 * x - a1 * y + z2);
        z2 = safe(b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }

  private:
    void coefficients(double b, double c, double d, double a, double e, double f) noexcept
    {
        b0 = b / a;
        b1 = c / a;
        b2 = d / a;
        a1 = e / a;
        a2 = f / a;
    }
    void setPass(double sr, double hz, double q, bool hp) noexcept
    {
        double w = 2 * pi * frequency(sr, hz) / sampleRate(sr), c = std::cos(w),
               alpha = std::sin(w) / (2 * std::clamp(safe(q), 0.05, 100.0));
        double b = (1 + (hp ? c : -c)) / 2;
        coefficients(b, (hp ? -2 : 2) * b, b, 1 + alpha, -2 * c, 1 - alpha);
    }
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};
} // namespace luthier::reference::common
