#include "LicenceState.h"

namespace luthier
{

LicenceStatus LicenceState::evaluate (const LicenceTiming& timing, juce::Time now) noexcept
{
    if (! timing.signatureValid || ! timing.fingerprintMatches || timing.revoked)
        return LicenceStatus::unlicensed;

    if (timing.issued.toMilliseconds() <= 0
        || timing.revalidateAfter.toMilliseconds() <= 0
        || timing.validUntil.toMilliseconds() <= 0)
        return LicenceStatus::unlicensed;

    if (timing.trial)
    {
        const auto trialEnd = timing.issued + juce::RelativeTime::days ((double) kTrialDays);

        if (now > trialEnd)
            return LicenceStatus::unlicensed;
    }

    // A clock moved backwards by more than two days must never extend a trial,
    // offline window or grace period. It is treated as needing revalidation.
    if (timing.lastSeen.toMilliseconds() > 0
        && now < timing.lastSeen - juce::RelativeTime::days ((double) kRollbackToleranceDays))
        return LicenceStatus::revalidationDue;

    if (now <= timing.revalidateAfter)
        return LicenceStatus::activated;

    if (now <= timing.validUntil)
        return LicenceStatus::revalidationDue;

    if (now <= timing.validUntil + juce::RelativeTime::days ((double) kGraceDays))
        return LicenceStatus::grace;

    return LicenceStatus::unlicensed;
}

const char* LicenceState::getName (LicenceStatus state) noexcept
{
    switch (state)
    {
        case LicenceStatus::activated:       return "Activated";
        case LicenceStatus::revalidationDue:return "Revalidation due";
        case LicenceStatus::grace:           return "Grace";
        case LicenceStatus::unlicensed:
        default:                             return "Unlicensed";
    }
}

void LicenceState::update (const LicenceTiming& timing, juce::Time now) noexcept
{
    current.store ((int) evaluate (timing, now), std::memory_order_release);
}

} // namespace luthier
