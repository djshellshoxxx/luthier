// OWNER: WP23
#pragma once
#include "ExtendedHostAutomation.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
namespace luthier::reference
{
class ExtendedHostAutomationCapture
{
public:
    class EchoScope
    {
    public:
        explicit EchoScope (const ExtendedHostAutomationCapture& owner) noexcept : previous (echoWriter) { echoWriter = &owner; }
        ~EchoScope() { echoWriter = previous; }
    private: const ExtendedHostAutomationCapture* previous;
    };
    ExtendedHostAutomationCapture() noexcept { for (auto& s : serials) s.store (0); for (auto& v : values) v.store (0); }
    bool write (int index, float normalised) noexcept
    {
        if (index < 0 || index >= ExtendedHostAutomationSnapshot::count || echoWriter == this || ! std::isfinite (normalised)) return false;
        if (index == (int) ExtendedHostField::FeedbackKill && normalised < .5f) return false; // retain sub-block pulse
        const auto& p = extendedHostParameters[(size_t) index];
        float physical = p.minimum + std::clamp (normalised, 0.f, 1.f) * (p.maximum - p.minimum);
        if (p.interval > 0) physical = p.minimum + std::round ((physical - p.minimum) / p.interval) * p.interval;
        values[(size_t) index].store (std::clamp (physical, p.minimum, p.maximum), std::memory_order_relaxed);
        const auto old = serials[(size_t) index].fetch_add (1, std::memory_order_release);
        if (old == std::numeric_limits<uint32_t>::max()) serials[(size_t) index].fetch_add (1, std::memory_order_release);
        return true;
    }
    // Host state carries parameter intent, not a replayable action edge.
    void releaseRestoredActions() noexcept
    {
        constexpr size_t i = (size_t) ExtendedHostField::FeedbackKill;
        values[i].store (0.f, std::memory_order_relaxed);
        const auto old = serials[i].fetch_add (1, std::memory_order_release);
        if (old == std::numeric_limits<uint32_t>::max()) serials[i].fetch_add (1, std::memory_order_release);
    }
    ExtendedHostAutomationSnapshot snapshot() const noexcept
    {
        ExtendedHostAutomationSnapshot out;
        for (size_t i = 0; i < out.values.size(); ++i)
        { out.serials[i] = serials[i].load (std::memory_order_acquire); out.values[i] = values[i].load (std::memory_order_relaxed); }
        return out;
    }
    uint32_t serial (size_t i) const noexcept { return serials[i].load (std::memory_order_acquire); }
    float value (size_t i) const noexcept { return values[i].load (std::memory_order_relaxed); }
private:
    inline static thread_local const ExtendedHostAutomationCapture* echoWriter = nullptr;
    std::array<std::atomic<float>, ExtendedHostAutomationSnapshot::count> values;
    std::array<std::atomic<uint32_t>, ExtendedHostAutomationSnapshot::count> serials;
};
}
