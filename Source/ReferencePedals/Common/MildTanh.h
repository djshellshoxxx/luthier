// OWNER: WP7
#pragma once
#include <cmath>
namespace luthier::reference::common
{
// Numerically equivalent tanh for the small-signal region, not a new shaper law.
// For |x|<=1/4, the alternating Taylor remainder is bounded by the first omitted
// term (929569/638512875)*|x|^15 <1.36e-12. Horner rounding adds <1e-16.
// The endpoint value step against libm is below1.4e-12; outside this narrow
// interval libm handles saturation without a polynomial excursion.
inline double mildTanh(double x) noexcept
{
    if(!std::isfinite(x))return 0;
    if(std::abs(x)>.25)return std::tanh(x);
    const double xx=x*x;
    return x*(1+xx*(-1.0/3+xx*(2.0/15+xx*(-17.0/315+xx*(62.0/2835+xx*(-1382.0/155925+xx*(21844.0/6081075)))))));
}
}
