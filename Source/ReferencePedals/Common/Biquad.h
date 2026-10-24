// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
#include <limits>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
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
    void setShelf(double sr, double hz, double db, bool high, double slope = 1.0) noexcept
    {
        // RBJ shelf slope S. Restrict to the monotone range; S=1 preserves
        // the existing four-argument coefficient arithmetic exactly.
        slope = std::clamp(safe(slope), .05, 1.0);
        const double A = std::sqrt(dbGain(db)), w = 2 * pi * frequency(sr, hz) / sampleRate(sr),
                     c = std::cos(w), s = std::sin(w),
                     t = slope == 1.0 ? std::sqrt(2 * A) * s
                                      : std::sqrt(A * ((A + 1 / A) * (1 / slope - 1) + 2)) * s;
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
    // Independent lanes preserve scalar operation association and safe() rules.
    // On targets permitting fused operations, use the compiler's scalar process
    // so this API cannot silently change rounding relative to that reference.
    static std::array<double, 2> processPair(Biquad& a, Biquad& b, double left, double right) noexcept
    {
        if (&a == &b)
        {
            const double first = a.process(left);
            return {first, b.process(right)};
        }
#if (defined(__SSE2__) || defined(_M_X64)) && !defined(__FMA__) && !defined(__AVX2__) && !defined(__FAST_MATH__)
        const auto clean = [](__m128d value) noexcept
        {
            const __m128d magnitude = _mm_andnot_pd(_mm_set1_pd(-0.0), value);
            const __m128d mask = _mm_and_pd(_mm_cmpge_pd(magnitude, _mm_set1_pd(1e-30)),
                                           _mm_cmple_pd(magnitude, _mm_set1_pd(std::numeric_limits<double>::max())));
            return _mm_and_pd(value, mask);
        };
        const __m128d x = clean(_mm_set_pd(right, left));
        const __m128d y = clean(_mm_add_pd(_mm_mul_pd(_mm_set_pd(b.b0, a.b0), x), _mm_set_pd(b.z1, a.z1)));
        const __m128d state1 = clean(_mm_add_pd(_mm_sub_pd(_mm_mul_pd(_mm_set_pd(b.b1, a.b1), x),
                                                         _mm_mul_pd(_mm_set_pd(b.a1, a.a1), y)),
                                               _mm_set_pd(b.z2, a.z2)));
        const __m128d state2 = clean(_mm_sub_pd(_mm_mul_pd(_mm_set_pd(b.b2, a.b2), x),
                                               _mm_mul_pd(_mm_set_pd(b.a2, a.a2), y)));
        std::array<double, 2> output, firstState, secondState;
        _mm_storeu_pd(output.data(), y);
        _mm_storeu_pd(firstState.data(), state1);
        _mm_storeu_pd(secondState.data(), state2);
        a.z1 = firstState[0]; b.z1 = firstState[1];
        a.z2 = secondState[0]; b.z2 = secondState[1];
        return output;
#else
        const double first = a.process(left);
        return {first, b.process(right)};
#endif
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
