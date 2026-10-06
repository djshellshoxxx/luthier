#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace luthier
{

enum class LicenceStatus
{
    unlicensed = 0,
    activated,
    revalidationDue,
    grace
};

struct LicenceTiming
{
    juce::Time issued;
    juce::Time revalidateAfter;
    juce::Time validUntil;
    juce::Time lastSeen;
    bool signatureValid = false;
    bool fingerprintMatches = false;
    bool revoked = false;
    bool trial = false;
};

class LicenceState
{
public:
    static constexpr int kTrialDays = 60;
    static constexpr int kRevalidationDays = 30;
    static constexpr int kGraceDays = 14;
    static constexpr int kRollbackToleranceDays = 2;

    static LicenceStatus evaluate (const LicenceTiming&, juce::Time now) noexcept;
    static const char* getName (LicenceStatus) noexcept;

    void update (const LicenceTiming& timing, juce::Time now) noexcept;
    LicenceStatus get() const noexcept
    {
        return (LicenceStatus) current.load (std::memory_order_acquire);
    }

    bool exportsAllowed() const noexcept { return get() != LicenceStatus::unlicensed; }
    bool demoSilenceEnabled() const noexcept { return get() == LicenceStatus::unlicensed; }

private:
    std::atomic<int> current { (int) LicenceStatus::unlicensed };
};

} // namespace luthier
