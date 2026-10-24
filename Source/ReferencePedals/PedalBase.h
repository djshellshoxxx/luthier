// OWNER: WP1 (frozen interface I-1; changes only via errata request to the WP1 owner)
#pragma once

// Convenience base for pedals with a fixed parameter count N. Owns the
// normalised store and one smoother per parameter, validates input in
// setParam, detects Trigger edges and converts tapers, so a model only
// implements onParamChanged() and process(). Header-only, juce_audio_basics.

#include <array>
#include <atomic>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include "IPedal.h"

namespace luthier::reference
{

template <int N>
class PedalBase : public IPedal
{
public:
    static_assert (N >= 0 && N <= kMaxParams, "PedalBase<N>: N must be within kMaxParams");

    explicit PedalBase (const PedalDesc& description) noexcept : descRef (description)
    {
        jassert (description.numParams == N);
        for (int i = 0; i < N; ++i)
        {
            const auto& p = description.params[i];
            norm[(size_t) i] = p.defaultNormalised();
            natural[(size_t) i] = (float) p.fromNormalised ((double) norm[(size_t) i]);
            smoother[(size_t) i].setCurrentAndTargetValue (natural[(size_t) i]);
        }
    }

    const PedalDesc& desc() const noexcept override { return descRef; }

    // Clamps, rejects NaN/Inf (value ignored), applies the taper, smooths and
    // calls onParamChanged(). Trigger kind: a rising edge through 0.5 calls
    // onTrigger() and does not smooth.
    void setParam (int idx, float n) noexcept override
    {
        if (idx < 0 || idx >= N || ! std::isfinite (n))
            return;

        const auto& p = descRef.params[idx];
        n = juce::jlimit (0.0f, 1.0f, n);
        const float previous = norm[(size_t) idx];
        norm[(size_t) idx] = n;

        const float v = (float) p.fromNormalised ((double) n);
        natural[(size_t) idx] = v;

        if (p.kind == ParamKind::Trigger)
        {
            if (previous < 0.5f && n >= 0.5f)
                onTrigger (idx);
            return;
        }

        if (p.has (PF_OwnSmoothing) || p.isDiscreteKind() || p.kind == ParamKind::Toggle
            || p.kind == ParamKind::Momentary)
            smoother[(size_t) idx].setCurrentAndTargetValue (v);
        else
            smoother[(size_t) idx].setTargetValue (v);

        onParamChanged (idx, v);
    }

    float getParam (int idx) const noexcept override
    {
        return idx >= 0 && idx < N ? norm[(size_t) idx] : 0.f;
    }

    // Free functions of the descriptor, exposed for tests and the registry.
    static double fromNormalised (const ParamDesc& p, double n) noexcept { return p.fromNormalised (n); }
    static double toNormalised (const ParamDesc& p, double v) noexcept   { return p.toNormalised (v); }

    // Soft guard used at pedal outputs: finite and within +-16 (same ceiling as
    // the prototype `safe()` and the board limiter).
    static double softGuard (double x) noexcept
    {
        return std::isfinite (x) ? std::clamp (x, -16.0, 16.0) : 0.0;
    }

    // One-pole DC blocker, y[n] = x[n] - x[n-1] + R*y[n-1].
    struct DcGuard
    {
        double x1 = 0.0, y1 = 0.0;
        double process (double x, double R = 0.9995) noexcept
        {
            const double y = x - x1 + R * y1;
            x1 = x;
            y1 = std::abs (y) < 1.0e-30 ? 0.0 : y;
            return y1;
        }
        void reset() noexcept { x1 = y1 = 0.0; }
    };

protected:
    // Natural units after the taper, called on the audio (or message) thread
    // from setParam. Must not allocate.
    virtual void onParamChanged (int /*idx*/, float /*naturalValue*/) noexcept {}
    virtual void onTrigger (int /*idx*/) noexcept {}

    // Call from prepare(): sizes every smoother ramp from the descriptor's
    // smoothMs (default 20 ms) and snaps to the current values.
    void prepareParams (double sampleRate)
    {
        for (int i = 0; i < N; ++i)
        {
            const auto& p = descRef.params[i];
            const double ms = p.smoothMs > 0.0f ? (double) p.smoothMs : 20.0;
            smoother[(size_t) i].reset (sampleRate, ms * 0.001);
            smoother[(size_t) i].setCurrentAndTargetValue (natural[(size_t) i]);
        }
    }

    float value (int idx) const noexcept       { return natural[(size_t) idx]; }          // target, natural units
    float smoothedNow (int idx) const noexcept { return smoother[(size_t) idx].getCurrentValue(); }
    float nextSmoothed (int idx) noexcept      { return smoother[(size_t) idx].getNextValue(); }
    bool isSmoothing (int idx) const noexcept  { return smoother[(size_t) idx].isSmoothing(); }
    void skipSmoothed (int idx, int n) noexcept { smoother[(size_t) idx].skip (n); }

    const PedalDesc& descRef;
    std::array<float, (size_t) std::max (N, 1)> norm {};
    std::array<float, (size_t) std::max (N, 1)> natural {};
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, (size_t) std::max (N, 1)> smoother;
};

} // namespace luthier::reference
