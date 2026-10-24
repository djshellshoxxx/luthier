// OWNER: WP5
#pragma once
#include "MidiTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace luthier::reference::midi
{
inline float curveValue (CurveId id, float x, float p) noexcept
{
    x = std::clamp (x, 0.f, 1.f);
    p = std::isfinite (p) && p > 0 ? std::clamp (p, .5f, 4.f) : 2.f;
    switch (id)
    {
        case CurveId::Log: return std::log10 (1.f + (std::pow (10.f, p) - 1.f) * x) / p;
        case CurveId::Exp: return std::expm1 (std::log (10.f) * p * x) / std::expm1 (std::log (10.f) * p);
        case CurveId::SCurve: return x * x * (3.f - 2.f * x);
        case CurveId::Inverse: return 1.f - x;
        case CurveId::Step: { auto n = std::max (2.f, std::round (p)); return std::min (1.f, std::floor (x * n) / (n - 1.f)); }
        default: return x;
    }
}
using CurveLut = std::array<float, 256>;
inline float evalCurve (const CurveLut& lut, float x) noexcept
{
    const float pos = std::clamp (x, 0.f, 1.f) * 255.f;
    const int i = (int) pos;
    return i == 255 ? lut[255] : lut[(size_t) i] + (pos - (float) i) * (lut[(size_t) i + 1] - lut[(size_t) i]);
}
}
