// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
namespace luthier::reference::common
{
// Experimental second-order antiderivative antialiasing for STATIC tanh only.
// Drive/offset automation belongs before this helper so all history samples
// remain coordinates of the same primitive. No cached parameter-dependent F2.
// Linear limit is (x[n]+x[n-1]+x[n-2])/3, not a pure delay: high-frequency
// droop must be accepted/corrected explicitly by a caller's measured design.
// prepareFactor pads by factor-1 high-rate samples, giving exactly one base
// sample of linear-phase delay. Fixed storage; no callback allocation/locks.
class TanhADAA2
{
    struct Node
    {
        double x = 0, f2 = 0, tail = 0;
    };

  public:
    void prepareFactor(int factor) noexcept
    {
        multiplier = factor >= 8 ? 8 : factor >= 4 ? 4 : factor >= 2 ? 2 : 1;
        reset();
    }
    void reset() noexcept
    {
        previous = older = {};
        padding.fill(0);
        head = 0;
    }
    int latencyHighRateSamples() const noexcept { return multiplier; }
    int latencyBaseSamples() const noexcept { return 1; }
    double process(double input) noexcept
    {
        const Node current = makeNode(std::clamp(safe(input), -1e6, 1e6));
        double y = std::clamp(safe(evaluate(current, previous, older)), -1.0, 1.0);
        older = previous;
        previous = current;
        if (multiplier == 1)
            return y;
        padding[head] = y;
        y = padding[(head + 8 - (unsigned)(multiplier - 1)) & 7];
        head = (head + 1) & 7;
        return y;
    }
    // Public for high-precision primitive verification, not required by callers.
    static double secondPrimitive(double input) noexcept
    {
        return makeNode(std::clamp(safe(input), -1e6, 1e6)).f2;
    }

  private:
    static Node makeNode(double x) noexcept
    {
        Node result;
        result.x = x;
        const double a = std::abs(x);
        if (a >= 1 && a < 40)
        {
            const double z = -std::exp(-2 * a);
            double power = z, sum = 0;
            for (int k = 1; k <= 16; ++k)
            {
                sum += power / (k * k);
                power *= z;
            }
            result.tail = .5 * sum;
        }
        if (a <= 1)
        {
            // Root's independent high-precision fit: Chebyshev F2(x)/x^3 in x^2.
            // Absolute primitive error alone is insufficient; divided differences
            // below use analytic repeated-node and small-span limits as well.
            constexpr double c[] = {.15933294965834852,     -.007028598546481005,   .00028960151323986446,
                                    -1.463767785022923e-5,  8.258641755667648e-7,   -5.000066056618013e-8,
                                    3.1806991124988824e-9,  -2.098973757668248e-10, 1.4249509090255247e-11,
                                    -9.894623481755805e-13, 6.996862583857914e-14,  -5.0386582920133766e-15,
                                    3.55699530819682e-16};
            const double t = 2 * x * x - 1;
            double b1 = 0, b2 = 0;
            for (int k = 12; k >= 1; --k)
            {
                const double b0 = 2 * t * b1 - b2 + c[k];
                b2 = b1;
                b1 = b0;
            }
            result.f2 = x * x * x * (t * b1 - b2 + c[0]);
        }
        else
            result.f2 = std::copysign(.5 * a * a - a * logTwo + result.tail + pi * pi / 24, x);
        return result;
    }
    static double firstPrimitive(double x, bool tail) noexcept
    {
        if (tail)
            return std::log1p(std::exp(-2 * x));
        const double a = std::abs(x);
        if (a < .5)
        {
            const double s = std::sinh(.5 * x);
            return std::log1p(2 * s * s);
        }
        return a + std::log1p(std::exp(-2 * a)) - logTwo;
    }
    static double secant(Node a, Node b, bool tail) noexcept
    {
        const double delta = b.x - a.x;
        if (delta >= .01)
            return ((tail ? b.tail : b.f2) - (tail ? a.tail : a.f2)) / delta;
        // Symmetric secant Taylor limit through delta^6. Includes repeated nodes.
        const double mid = (a.x + b.x) * .5, t = std::tanh(mid), t2 = t * t, d1 = 1 - t2;
        const double d3 = (-2 + 6 * t2) * d1, d5 = (16 - 120 * t2 + 120 * t2 * t2) * d1, d2 = delta * delta;
        return firstPrimitive(mid, tail) + d2 * (d1 / 24 + d2 * (d3 / 1920 + d2 * d5 / 322560));
    }
    static double smallSpan(double a, double b, double c) noexcept
    {
        const double centre = (a + b + c) / 3, x = a - centre, y = b - centre, z = c - centre;
        const double e1 = x + y + z, e2 = x * y + x * z + y * z, e3 = x * y * z, t = std::tanh(centre),
                     t2 = t * t, d1 = 1 - t2;
        const double derivative[] = {t,
                                     d1,
                                     -2 * t * d1,
                                     (-2 + 6 * t2) * d1,
                                     (16 * t - 24 * t * t2) * d1,
                                     (16 - 120 * t2 + 120 * t2 * t2) * d1,
                                     (-272 * t + 960 * t * t2 - 720 * t * t2 * t2) * d1};
        constexpr double scale[] = {1, 1.0 / 3, 1.0 / 12, 1.0 / 60, 1.0 / 360, 1.0 / 2520, 1.0 / 20160};
        double h[7]{1}, sum = t;
        for (int k = 1; k <= 6; ++k)
        {
            h[k] = e1 * h[k - 1] - (k >= 2 ? e2 * h[k - 2] : 0) + (k >= 3 ? e3 * h[k - 3] : 0);
            sum += scale[k] * derivative[k] * h[k];
        }
        return sum;
    }
    static double sorted(Node lo, Node mid, Node hi, bool tail) noexcept
    {
        if (lo.x >= 18)
            return 1;
        const double span = hi.x - lo.x;
        if (span < .01)
            return smallSpan(lo.x, mid.x, hi.x);
        // In a same-sign saturated region, subtract the known quadratic/linear
        // primitive analytically, avoiding cancellation between large F2 values.
        return (tail ? 1 : 0) + 2 * (secant(mid, hi, tail) - secant(lo, mid, tail)) / span;
    }
    static double evaluate(Node a, Node b, Node c) noexcept
    {
        if (a.x > b.x)
            std::swap(a, b);
        if (b.x > c.x)
            std::swap(b, c);
        if (a.x > b.x)
            std::swap(a, b);
        if (c.x <= -1)
        {
            Node lo{-c.x, -c.f2, c.tail}, mid{-b.x, -b.f2, b.tail}, hi{-a.x, -a.f2, a.tail};
            return -sorted(lo, mid, hi, true);
        }
        return sorted(a, b, c, a.x >= 1);
    }
    static constexpr double logTwo = .693147180559945309417232121458176568;
    Node previous, older;
    std::array<double, 8> padding{};
    unsigned head = 0;
    int multiplier = 1;
};
} // namespace luthier::reference::common
