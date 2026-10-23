// OWNER: WP2
#pragma once
#include <algorithm>
#include <cmath>
namespace luthier::reference
{
struct BoardLimiter
{
    static double process (double x, bool enabled, double ceiling) noexcept
    {
        if (! std::isfinite (x)) return 0.0;
        if (! enabled) return std::clamp (x, -16.0, 16.0);
        const double magnitude = std::abs (x), knee = 0.9 * ceiling;
        if (magnitude > knee)
            x = std::copysign (knee + 0.1 * ceiling * std::tanh ((magnitude - knee) / (0.1 * ceiling)), x);
        return std::clamp (x, -16.0, 16.0);
    }
};
}
