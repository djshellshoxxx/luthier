// OWNER: WP2
#pragma once
#include <array>
#include <cmath>
#include "BoardTypes.h"
namespace luthier::reference
{
class WetDryMixer
{
public:
    static constexpr double halfPi = 1.57079632679489661923;
    // Prepared outside the callback. A table interpolant avoids per-sample trig.
    WetDryMixer()
    {
        for (size_t i = 0; i < table.size(); ++i)
            table[i] = std::sin (halfPi * static_cast<double> (i) / 1024.0);
    }
    double render (double dry, double wet, double amount, MixLaw law) const noexcept
    {
        if (amount <= 0.0) return dry;
        if (amount >= 1.0) return wet;
        if (law == MixLaw::Linear) return dry * (1.0 - amount) + wet * amount;
        double a = lookup (1.0 - amount), b = lookup (amount);
        if (law == MixLaw::ThirdLaw) { a = std::sqrt ((1.0 - amount) * a); b = std::sqrt (amount * b); }
        return dry * a + wet * b;
    }
    // Analytic reference, also useful to clients outside audio processing.
    static double mix (double dry, double wet, double amount, MixLaw law) noexcept
    {
        if (amount <= 0.0) return dry;
        if (amount >= 1.0) return wet;
        if (law == MixLaw::Linear) return dry * (1.0 - amount) + wet * amount;
        double a = std::cos (halfPi * amount), b = std::sin (halfPi * amount);
        if (law == MixLaw::ThirdLaw) { a = std::sqrt ((1.0 - amount) * a); b = std::sqrt (amount * b); }
        return dry * a + wet * b;
    }
private:
    double lookup (double amount) const noexcept
    {
        const double position = amount * 1024.0;
        const int index = std::min (1023, static_cast<int> (position));
        return table[static_cast<size_t> (index)] + (position - index)
            * (table[static_cast<size_t> (index + 1)] - table[static_cast<size_t> (index)]);
    }
    std::array<double, 1025> table {};
};
}
