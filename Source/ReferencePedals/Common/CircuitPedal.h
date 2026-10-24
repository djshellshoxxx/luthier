// OWNER: WP7
#pragma once
#include "PedalDsp.h"
namespace luthier::reference::common
{
// Experimental named-circuit base. Double exponential control smoothing and a
// sample-indexed coefficient clock are independent of host block partitioning.
// The board owns bypass. Structural quality changes take effect in prepare().
template <int N> class CircuitPedal : public PedalBase<N>
{
  public:
    explicit CircuitPedal(const PedalDesc &desc) : PedalBase<N>(desc) {}
    int getLatencySamples() const noexcept override { return os[0].latencySamples(); }
    float getTailEnergyDb() const noexcept override { return -120; }

  protected:
    void prepareCircuit(double rate, int factor) noexcept
    {
        sr = sampleRate(rate);
        this->prepareParams(sr);
        for (auto &o : os)
            o.prepareFactor(sr, factor);
        for (int i = 0; i < N; ++i)
        {
            const auto &d = this->desc().params[i];
            coefficient[i] = d.isDiscreteKind() || d.kind == ParamKind::Toggle
                                 ? 1.0
                                 : -std::expm1(-1 / (.001 * std::max(1.0f, d.smoothMs) * sr));
        }
        resetCircuit();
    }
    void resetCircuit() noexcept
    {
        clock = 0;
        for (auto &o : os)
            o.reset();
        for (int i = 0; i < N; ++i)
            p[i] = this->value(i);
    }
    void tick() noexcept
    {
        for (int i = 0; i < N; ++i)
        {
            const double target = this->value(i);
            p[i] += coefficient[i] * (target - p[i]);
            if (std::abs(target - p[i]) < 1e-14)
                p[i] = target;
        }
    }
    static ParamDesc knob(const char *id, const char *label, int cc, float ms = 20) noexcept
    {
        auto p = continuous(id, label, "", 0, 1, .5f, cc);
        p.smoothMs = ms;
        p.flags |= PF_OwnSmoothing;
        return p;
    }
    static ParamDesc toggle(const char *id, const char *label, bool def, uint16_t flags = PF_Config) noexcept
    {
        return {id,    label, "", ParamKind::Toggle, Taper::Linear, 0, 1, def ? 1.f : 0.f, 1, 0, nullptr,
                flags, 0,     -1};
    }
    static double bounded(double x) noexcept { return std::clamp(safe(x), -16.0, 16.0); }
    double sr = 48000;
    unsigned clock = 0;
    std::array<double, N> p{}, coefficient{};
    std::array<OversamplerAdapter, 2> os;
};
} // namespace luthier::reference::common
