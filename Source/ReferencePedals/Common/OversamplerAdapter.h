// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
namespace luthier::reference::common
{
// E37: reference-only linear phase path. The legacy engine Oversampler is
// unchanged. The first stage uses a 257-tap Kaiser halfband (beta6.5);
// upper stages use33-tap Blackman-Harris. Exact zero even taps, symmetry
// and polyphase splitting reduce arithmetic. Coefficients and
// storage are prepared off the audio thread; processing never allocates.
class HalfbandFir
{
  public:
    void prepare(bool firstStage = false) noexcept
    {
        half = firstStage ? 128 : 16;
        coef.fill(0);
        double sum = 0;
        for (unsigned k = 1; k < half; k += 2)
        {
            const double m = (double)k - half;
            const double w = firstStage ? bessel(6.5 * std::sqrt(1 - m * m / (half * half))) / bessel(6.5)
                                        : .35875 - .48829 * std::cos(pi * k / half) +
                                              .14128 * std::cos(2 * pi * k / half) -
                                              .01168 * std::cos(3 * pi * k / half);
            coef[(size_t)(k / 2)] = std::sin(.5 * pi * m) / (pi * m) * w;
            sum += 2 * coef[(size_t)(k / 2)];
        }
        for (auto &c : coef)
            c *= .5 / sum;
        reset();
    }
    void reset() noexcept
    {
        inputHistory.fill(0);
        evenHistory.fill(0);
        oddHistory.fill(0);
        upHead = downHead = 0;
    }
    void up(double x, double &even, double &odd) noexcept
    {
        inputHistory[upHead] = inputHistory[upHead + 256] = safe(x);
        const auto *history = inputHistory.data() + upHead;
        even = history[half / 2];
        odd = safe(2 * symmetricSum(history, 0));
        upHead = (upHead + 255) & 255;
    }
    double down(double even, double odd) noexcept
    {
        evenHistory[downHead] = evenHistory[downHead + 256] = safe(even);
        oddHistory[downHead] = oddHistory[downHead + 256] = safe(odd);
        const double y =
            .5 * evenHistory[downHead + half / 2] + symmetricSum(oddHistory.data() + downHead, 1);
        downHead = (downHead + 255) & 255;
        return safe(y);
    }

  private:
    double symmetricSum(const double *history, unsigned offset) const noexcept
    {
#if defined(__SSE2__) || defined(_M_X64)
        __m128d a = _mm_setzero_pd(), b = _mm_setzero_pd();
        for (unsigned j = 0; j < half / 2; j += 4)
        {
            auto reversed = _mm_loadu_pd(history + offset + half - 2 - j);
            auto pair = _mm_add_pd(_mm_loadu_pd(history + offset + j), _mm_shuffle_pd(reversed, reversed, 1));
            a = _mm_add_pd(a, _mm_mul_pd(_mm_loadu_pd(coef.data() + j), pair));
            reversed = _mm_loadu_pd(history + offset + half - 4 - j);
            pair = _mm_add_pd(_mm_loadu_pd(history + offset + j + 2), _mm_shuffle_pd(reversed, reversed, 1));
            b = _mm_add_pd(b, _mm_mul_pd(_mm_loadu_pd(coef.data() + j + 2), pair));
        }
        double lanes[4];
        _mm_storeu_pd(lanes, a);
        _mm_storeu_pd(lanes + 2, b);
        return (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
#else
        double lanes[4]{};
        for (unsigned j = 0; j < half / 2; j += 4)
            for (unsigned lane = 0; lane < 4; ++lane)
            {
                const auto tap = j + lane;
                lanes[lane] += coef[tap] * (history[offset + tap] + history[offset + half - 1 - tap]);
            }
        return (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
#endif
    }

    static double bessel(double x) noexcept
    {
        double sum = 1, term = 1;
        for (int k = 1; k <= 24; ++k)
        {
            term *= x * x / (4 * k * k);
            sum += term;
        }
        return sum;
    }
    std::array<double, 64> coef{};
    std::array<double, 512> inputHistory{}, evenHistory{}, oddHistory{};
    unsigned upHead = 0, downHead = 0, half = 64;
};
class OversamplerAdapter
{
  public:
    void prepare(double sr, int policy) noexcept { prepareFactor(sr, policy <= 0 ? 2 : policy == 1 ? 4 : 8); }
    void prepareFactor(double sr, int f) noexcept
    {
        baseRate = sampleRate(sr);
        multiplier = f >= 8 ? 8 : f >= 4 ? 4 : f >= 2 ? 2 : 1;
        for (size_t i = 0; i < stages.size(); ++i)
            stages[i].prepare(i == 0);
    }
    void reset() noexcept
    {
        for (auto &s : stages)
            s.reset();
    }
    int factor() const noexcept { return multiplier; }
    int latencySamples() const noexcept
    {
        return multiplier == 8 ? 140 : multiplier == 4 ? 136 : multiplier == 2 ? 128 : 0;
    }
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
        if (multiplier == 4)
        {
            for (auto &v : c)
                v = safe(shaper(v));
            a = stages[1].down(c[0], c[1]);
            b = stages[1].down(c[2], c[3]);
            return stages[0].down(a, b);
        }
        double d[8];
        for (int i = 0; i < 4; ++i)
            stages[2].up(c[i], d[2 * i], d[2 * i + 1]);
        for (auto &v : d)
            v = safe(shaper(v));
        for (int i = 0; i < 4; ++i)
            c[i] = stages[2].down(d[2 * i], d[2 * i + 1]);
        a = stages[1].down(c[0], c[1]);
        b = stages[1].down(c[2], c[3]);
        return stages[0].down(a, b);
    }

  private:
    std::array<HalfbandFir, 3> stages;
    double baseRate = 48000;
    int multiplier = 4;
};
} // namespace luthier::reference::common
