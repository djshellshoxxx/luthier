#include "TestFramework.h"
#include "../Licence/LicenceState.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
LicenceTiming validTiming()
{
    LicenceTiming t;
    t.issued = juce::Time (2026, 9, 1, 0, 0);
    t.revalidateAfter = juce::Time (2026, 9, 30, 0, 0);
    t.validUntil = juce::Time (2026, 10, 14, 0, 0);
    // Ordinary state transitions must not also simulate a clock rollback.
    // The rollback test sets its own later lastSeen explicitly.
    t.lastSeen = t.issued;
    t.signatureValid = true;
    t.fingerprintMatches = true;
    return t;
}
}

LUTHIER_TEST (LicenceState, followsActivatedDueGraceAndUnlicensedSequence)
{
    auto t = validTiming();

    CHECK (LicenceState::evaluate (t, juce::Time (2026, 9, 15, 0, 0))
           == LicenceStatus::activated);
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 10, 5, 0, 0))
           == LicenceStatus::revalidationDue);
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 10, 20, 0, 0))
           == LicenceStatus::grace);
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 10, 29, 0, 0))
           == LicenceStatus::unlicensed);
}

LUTHIER_TEST (LicenceState, rejectsTamperedRevokedAndWrongMachine)
{
    auto t = validTiming();

    t.signatureValid = false;
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 9, 15, 0, 0))
           == LicenceStatus::unlicensed);

    t.signatureValid = true;
    t.fingerprintMatches = false;
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 9, 15, 0, 0))
           == LicenceStatus::unlicensed);

    t.fingerprintMatches = true;
    t.revoked = true;
    CHECK (LicenceState::evaluate (t, juce::Time (2026, 9, 15, 0, 0))
           == LicenceStatus::unlicensed);
}

LUTHIER_TEST (LicenceState, trialExpiresAtSixtyDays)
{
    auto t = validTiming();
    t.trial = true;
    t.revalidateAfter = t.issued + juce::RelativeTime::days (30.0);
    t.validUntil = t.issued + juce::RelativeTime::days (60.0);

    CHECK (LicenceState::evaluate (t, t.issued + juce::RelativeTime::days (59.0))
           != LicenceStatus::unlicensed);
    CHECK (LicenceState::evaluate (t, t.issued + juce::RelativeTime::days (61.0))
           == LicenceStatus::unlicensed);
}

LUTHIER_TEST (LicenceState, clockRollbackRequiresRevalidation)
{
    auto t = validTiming();
    t.lastSeen = juce::Time (2026, 9, 20, 0, 0);

    CHECK (LicenceState::evaluate (t, juce::Time (2026, 9, 17, 0, 0))
           == LicenceStatus::revalidationDue);
}
