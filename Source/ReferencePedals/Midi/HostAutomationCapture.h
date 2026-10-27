// OWNER: WP23
#pragma once
#include "../BoardTypes.h"
#include <atomic>
#include <cmath>
#include <limits>
namespace luthier::reference
{
// Host callbacks publish actual write payloads separately from APVTS's model
// echoes. A message-thread echo scope suppresses only that thread's callbacks.
class HostAutomationCapture
{
public:
    class EchoScope
    {
    public:
        explicit EchoScope (const HostAutomationCapture& owner) noexcept : previous (echoWriter) { echoWriter = &owner; }
        ~EchoScope() { echoWriter = previous; }
        EchoScope (const EchoScope&) = delete;
        EchoScope& operator= (const EchoScope&) = delete;
    private: const HostAutomationCapture* previous;
    };
    HostAutomationCapture() noexcept { for (auto& serial : serials) serial.store (0); for (auto& value : values) value.store (0); }
    bool write (int index, float normalised) noexcept
    {
        if (index < 0 || index >= HostAutomationSnapshot::count || echoWriter == this || ! std::isfinite (normalised)) return false;
        normalised = std::clamp (normalised, 0.f, 1.f);
        float physical = normalised;
        if (index == 2) physical = 20.f + 280.f * normalised;
        else if (index >= 3)
        {
            const int field = (index - 3) % 20;
            if (field == 17 || field == 18) physical = -12.f + 24.f * normalised;
            else if (field == 19) physical = std::round (15.f * normalised);
        }
        values[(size_t) index].store (physical, std::memory_order_relaxed);
        const auto old = serials[(size_t) index].fetch_add (1, std::memory_order_release);
        if (old == std::numeric_limits<uint32_t>::max()) serials[(size_t) index].fetch_add (1, std::memory_order_release);
        return true;
    }
    HostAutomationSnapshot snapshot() const noexcept
    {
        HostAutomationSnapshot out;
        for (size_t i = 0; i < out.values.size(); ++i)
        { out.serials[i] = serials[i].load (std::memory_order_acquire); out.values[i] = values[i].load (std::memory_order_relaxed); }
        return out;
    }
    uint32_t serial (size_t i) const noexcept { return serials[i].load (std::memory_order_acquire); }
    float value (size_t i) const noexcept { return values[i].load (std::memory_order_relaxed); }
private:
    inline static thread_local const HostAutomationCapture* echoWriter = nullptr;
    std::array<std::atomic<float>, HostAutomationSnapshot::count> values;
    std::array<std::atomic<uint32_t>, HostAutomationSnapshot::count> serials;
};
}
