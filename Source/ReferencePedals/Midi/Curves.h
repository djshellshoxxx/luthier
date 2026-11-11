// OWNER: WP5
#pragma once
#include "MidiTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace luthier::reference::midi
{
struct CurvePoint { float x = 0, y = 0; };
using CustomPoints = std::array<CurvePoint, 5>;
inline CustomPoints linearPoints() noexcept
{ CustomPoints p {}; for (int i = 0; i < 5; ++i) p[(size_t) i] = { i * .25f, i * .25f }; return p; }
inline bool validPoints (const CustomPoints& p, bool allowNonMonotone = false) noexcept
{
    if (p.front().x != 0 || p.back().x != 1) return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (! std::isfinite (p[i].x) || ! std::isfinite (p[i].y) || p[i].x < 0 || p[i].x > 1 || p[i].y < 0 || p[i].y > 1
            || (i && (p[i].x - p[i-1].x < .009999f || (! allowNonMonotone && p[i].y < p[i-1].y)))) return false;
    return true;
}
inline std::array<float, 5> pchipSlopes (const CustomPoints& p) noexcept
{
    std::array<float, 4> h {}, d {}; std::array<float, 5> m {};
    for (int i = 0; i < 4; ++i) { h[(size_t) i] = p[(size_t) i+1].x - p[(size_t) i].x; d[(size_t) i] = (p[(size_t) i+1].y - p[(size_t) i].y) / h[(size_t) i]; }
    for (int i = 1; i < 4; ++i)
        if (d[(size_t) i-1] * d[(size_t) i] > 0)
        {
            const float a = 2 * h[(size_t) i] + h[(size_t) i-1], b = h[(size_t) i] + 2 * h[(size_t) i-1];
            m[(size_t) i] = (a+b) / (a/d[(size_t) i-1] + b/d[(size_t) i]);
        }
    auto edge = [] (float h0, float h1, float d0, float d1)
    {
        float v = ((2*h0+h1)*d0-h0*d1)/(h0+h1);
        if (v*d0 <= 0) return 0.f;
        if (d0*d1 < 0 && std::abs (v) > std::abs (3*d0)) v = 3*d0;
        return v;
    };
    m[0] = edge(h[0],h[1],d[0],d[1]); m[4] = edge(h[3],h[2],d[3],d[2]); return m;
}
inline float pchipValue (const CustomPoints& p, const std::array<float, 5>& slopes, float x) noexcept
{
    x = std::clamp (x, 0.f, 1.f); int i = 0;
    while (i < 3 && x > p[(size_t) i+1].x) ++i;
    const float h = p[(size_t) i+1].x - p[(size_t) i].x, t = (x-p[(size_t) i].x)/h;
    const float t2 = t*t, t3 = t2*t;
    return std::clamp ((2*t3-3*t2+1)*p[(size_t) i].y + (t3-2*t2+t)*h*slopes[(size_t) i]
        + (-2*t3+3*t2)*p[(size_t) i+1].y + (t3-t2)*h*slopes[(size_t) i+1], 0.f, 1.f);
}
inline float curveValue (CurveId id, float x, float p) noexcept
{
    x = std::clamp (x, 0.f, 1.f);
    if (x == 0.f) return id == CurveId::Inverse ? 1.f : 0.f;
    if (x == 1.f) return id == CurveId::Inverse ? 0.f : 1.f;
    if (id == CurveId::SCurve) { const float k = std::isfinite (p) ? std::clamp (p, 0.f, 1.f) : 1.f; return x + k * (x*x*(3-2*x)-x); }
    if (id == CurveId::Step) { const float n = std::isfinite (p) ? std::clamp (std::round (p), 2.f, 128.f) : 2.f; return std::min (1.f, std::floor (x*n)/(n-1)); }
    p = std::isfinite (p) && p > 0 ? std::clamp (p, .5f, 4.f) : 2.f;
    switch (id)
    {
        case CurveId::Log: return std::log10 (1.f + (std::pow (10.f, p)-1)*x)/p;
        case CurveId::Exp: return std::expm1 (std::log (10.f)*p*x)/std::expm1 (std::log (10.f)*p);
        case CurveId::Inverse: return 1-x;
        default: return x;
    }
}
// Adaptive 256-entry LUT: fixed uniform nodes cannot meet the stated 1e-4
// error at log(p=4)'s steep heel. Nodes/values build only on the message thread.
struct CurveLut
{
    std::array<float, 256> x {}, y {};
    uint16_t count = 0;
    uint8_t steps = 0;
};
inline void buildCurve (CurveLut& lut, CurveId id, float p, const CustomPoints* custom = nullptr) noexcept
{
    CustomPoints points = custom && validPoints (*custom, true) ? *custom : linearPoints();
    const auto slopes = pchipSlopes (points);
    auto value = [&] (float x) { return id == CurveId::Custom5 ? pchipValue (points, slopes, x) : curveValue (id, x, p); };
    lut.steps = id == CurveId::Step ? (uint8_t) (std::isfinite (p) ? std::clamp (std::round (p), 2.f, 128.f) : 2.f) : 0;
    lut.count = 2; lut.x[0] = 0; lut.x[1] = 1; lut.y[0] = value (0); lut.y[1] = value (1);
    if (lut.steps || id == CurveId::Linear || id == CurveId::Inverse) return;
    std::array<float, 256> errors {};
    auto error = [&] (int i)
    {
        float e = 0;
        for (float t : { .25f, .5f, .75f }) e = std::max (e, std::abs (value (lut.x[(size_t) i] + t*(lut.x[(size_t) i+1]-lut.x[(size_t) i]))
            - (lut.y[(size_t) i] + t*(lut.y[(size_t) i+1]-lut.y[(size_t) i]))));
        return e;
    };
    if (id == CurveId::Custom5)
    { lut.count = 5; for (size_t i = 0; i < 5; ++i) { lut.x[i] = points[i].x; lut.y[i] = points[i].y; } }
    for (int i = 0; i < lut.count-1; ++i) errors[(size_t) i] = error (i);
    while (lut.count < 256)
    {
        int worst = 0; for (int i = 1; i < lut.count-1; ++i) if (errors[(size_t) i] > errors[(size_t) worst]) worst = i;
        if (errors[(size_t) worst] <= 1e-6f) break;
        const float middle = .5f*(lut.x[(size_t) worst]+lut.x[(size_t) worst+1]);
        if (middle <= lut.x[(size_t) worst] || middle >= lut.x[(size_t) worst+1]) break;
        for (int i = lut.count; i > worst+1; --i) { lut.x[(size_t) i] = lut.x[(size_t) i-1]; lut.y[(size_t) i] = lut.y[(size_t) i-1]; errors[(size_t) i] = errors[(size_t) i-1]; }
        ++lut.count; lut.x[(size_t) worst+1] = middle; lut.y[(size_t) worst+1] = value (middle);
        errors[(size_t) worst] = error (worst); errors[(size_t) worst+1] = error (worst+1);
    }
}
inline float evalCurve (const CurveLut& lut, float x) noexcept
{
    x = std::clamp (x, 0.f, 1.f);
    if (lut.steps) return std::min (1.f, std::floor (x*lut.steps)/(lut.steps-1));
    if (lut.count < 2) return x;
    int lo = 0, hi = lut.count-1;
    while (hi-lo > 1) { const int mid = (lo+hi)/2; if (x < lut.x[(size_t) mid]) hi = mid; else lo = mid; }
    return lut.y[(size_t) lo] + (x-lut.x[(size_t) lo])/(lut.x[(size_t) hi]-lut.x[(size_t) lo])*(lut.y[(size_t) hi]-lut.y[(size_t) lo]);
}
}
