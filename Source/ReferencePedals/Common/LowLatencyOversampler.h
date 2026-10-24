// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <array>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
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

#if (defined(__SSE2__) || defined(_M_X64)) && !defined(__FMA__) && !defined(__AVX2__) && !defined(__FAST_MATH__)
        static void upPair(Stage& left, Stage& right, const std::array<double,2>& input,
                           std::array<double,2>& a, std::array<double,2>& b) noexcept
        {
            left.upHistory[left.upHead] = safe(input[0]);
            right.upHistory[right.upHead] = safe(input[1]);
            __m128d sum = _mm_setzero_pd();
            for(unsigned j=0;j<8;++j)
            {
                const __m128d first = _mm_set_pd(right.upHistory[(right.upHead-j)&31],left.upHistory[(left.upHead-j)&31]);
                const __m128d second = _mm_set_pd(right.upHistory[(right.upHead-(15-j))&31],left.upHistory[(left.upHead-(15-j))&31]);
                sum = _mm_add_pd(sum,_mm_mul_pd(_mm_set_pd(right.coefficients[j],left.coefficients[j]),_mm_add_pd(first,second)));
            }
            std::array<double,2> interpolated;
            _mm_storeu_pd(interpolated.data(),_mm_mul_pd(_mm_set1_pd(2),sum));
            Stage* lanes[]{&left,&right};
            for(unsigned c=0;c<2;++c)
            {
                auto& lane=*lanes[c];const double filtered=safe(interpolated[c]);
                const double delayed=lane.upHistory[(lane.upHead-(unsigned)(lane.centre/2))&31];
                a[c]=lane.parity==0?filtered:delayed;b[c]=lane.parity==0?delayed:filtered;
                lane.upHead=(lane.upHead+1)&31;
            }
        }
        static std::array<double,2> downPair(Stage& left, Stage& right,
            const std::array<double,2>& a,const std::array<double,2>& b) noexcept
        {
            Stage* lanes[]{&left,&right};
            std::array<double,2> initial;
            for(unsigned c=0;c<2;++c)
            {
                auto& lane=*lanes[c];lane.evenHistory[lane.downHead]=safe(a[c]);lane.oddHistory[lane.downHead]=safe(b[c]);
                initial[c]=.5*(lane.parity==0?lane.oddHistory:lane.evenHistory)[(lane.downHead-8)&31];
            }
            __m128d sum=_mm_loadu_pd(initial.data());
            const auto& lh=left.parity==0?left.evenHistory:left.oddHistory;
            const auto& rh=right.parity==0?right.evenHistory:right.oddHistory;
            const unsigned lp=left.parity==0?0:1,rp=right.parity==0?0:1;
            for(unsigned j=0;j<8;++j)
            {
                const __m128d first=_mm_set_pd(rh[(right.downHead-j-rp)&31],lh[(left.downHead-j-lp)&31]);
                const __m128d second=_mm_set_pd(rh[(right.downHead-(15-j)-rp)&31],lh[(left.downHead-(15-j)-lp)&31]);
                sum=_mm_add_pd(sum,_mm_mul_pd(_mm_set_pd(right.coefficients[j],left.coefficients[j]),_mm_add_pd(first,second)));
            }
            std::array<double,2> output;_mm_storeu_pd(output.data(),sum);
            for(unsigned c=0;c<2;++c){output[c]=safe(output[c]);lanes[c]->downHead=(lanes[c]->downHead+1)&31;}
            return output;
        }
#endif

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

    // Each callback is scalar and invoked for all left high-rate samples before
    // all right samples, matching two sequential process() calls. Callbacks may
    // maintain separate detector state, but must not call process/prepare/reset
    // or otherwise mutate either oversampler from inside a callback.
    template<class LeftShape,class RightShape>
    static std::array<double,2> processPair(LowLatencyOversampler& left,LowLatencyOversampler& right,
        double x,double y,LeftShape&& leftShape,RightShape&& rightShape) noexcept
    {
#if (defined(__SSE2__) || defined(_M_X64)) && !defined(__FMA__) && !defined(__AVX2__) && !defined(__FAST_MATH__)
        if(&left!=&right && left.multiplier==right.multiplier && left.multiplier>1)
        {
            std::array<double,2> input{std::clamp(safe(x),-1e6,1e6),std::clamp(safe(y),-1e6,1e6)},a,b;
            Stage::upPair(left.stages[0],right.stages[0],input,a,b);
            if(left.multiplier==2)
            {
                a[0]=safe(leftShape(a[0]));b[0]=safe(leftShape(b[0]));
                a[1]=safe(rightShape(a[1]));b[1]=safe(rightShape(b[1]));
                return Stage::downPair(left.stages[0],right.stages[0],a,b);
            }
            std::array<std::array<double,2>,4> c;
            Stage::upPair(left.stages[1],right.stages[1],a,c[0],c[1]);
            Stage::upPair(left.stages[1],right.stages[1],b,c[2],c[3]);
            for(auto& v:c)v[0]=safe(leftShape(v[0]));
            for(auto& v:c)v[1]=safe(rightShape(v[1]));
            a=Stage::downPair(left.stages[1],right.stages[1],c[0],c[1]);
            b=Stage::downPair(left.stages[1],right.stages[1],c[2],c[3]);
            return Stage::downPair(left.stages[0],right.stages[0],a,b);
        }
#endif
        const double first=left.process(x,leftShape);
        return {first,right.process(y,rightShape)};
    }

  private:
    std::array<Stage, 2> stages;
    double baseRate = 48000;
    int multiplier = 1;
};
} // namespace luthier::reference::common
