// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common
{
// Non-inverting amplifier H(s)=1+(Rf||Cf)/(Rg+1/sCg). The complete
// second-order transfer is bilinear transformed; optional prewarp targets
// the feedback pole only when that analog pole is representable below Nyquist.
class LinearGainStage
{
  public:
    void prepare(double rate, double rg, double cg, double rf, double cf, bool prewarp = false) noexcept
    {
        sr = sampleRate(rate);
        configure(rg, cg, rf, cf, prewarp);
        reset();
    }
    void configure(double rg, double cg, double rf, double cf, bool prewarp = false) noexcept
    {
        rg = std::clamp(safe(rg), 1., 1e9);
        rf = std::clamp(safe(rf), 1., 1e9);
        cg = std::clamp(safe(cg), 1e-15, 1.);
        cf = std::clamp(safe(cf), 1e-15, 1.);
        const double pole = 1 / (rf * cf),
                     k = prewarp && pole < .9 * pi * sr ? pole / std::tan(pole / (2 * sr)) : 2 * sr;
        const double d1 = rg * cg + rf * cf, d2 = rg * cg * rf * cf, n1 = d1 + rf * cg;
        const double den = 1 + d1 * k + d2 * k * k;
        b0 = (1 + n1 * k + d2 * k * k) / den;
        b1 = 2 * (1 - d2 * k * k) / den;
        b2 = (1 - n1 * k + d2 * k * k) / den;
        a1 = 2 * (1 - d2 * k * k) / den;
        a2 = (1 - d1 * k + d2 * k * k) / den;
    }
    double process(double x) noexcept
    {
        x = std::clamp(safe(x), -16., 16.);
        const double y = safe(b0 * x + s1);
        s1 = safe(b1 * x - a1 * y + s2);
        s2 = safe(b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { s1 = s2 = 0; }

  private:
    double sr = 48000, b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
};
} // namespace luthier::reference::common
