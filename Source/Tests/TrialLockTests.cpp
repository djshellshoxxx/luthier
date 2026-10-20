#include "TestFramework.h"

#include "../Updates/Telemetry.h"
#include "../Support/Edition.h"

using namespace luthier;
using namespace luthier::tests;

// The 60-day Pro trial + unlock code (spec/trial-lock.md). Pro-only: the Free
// binary has no licensing code, so the whole suite compiles out there.
#if LUTHIER_PRO

namespace
{
    juce::int64 nowMs()           { return juce::Time::getCurrentTime().toMilliseconds(); }
    juce::int64 daysMs (double d) { return (juce::int64) (d * 24.0 * 60.0 * 60.0 * 1000.0); }

    // Drive License::fromVar directly, so a test can place the trial anywhere in
    // time without waiting real days or writing files.
    juce::var makeLicenceVar (bool hasKey,
                              juce::int64 trialStartedMs,
                              juce::int64 trialExpiresMs,
                              juce::int64 lastSeenMs)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("keyHash", hasKey ? juce::String ("deadbeefdeadbeef") : juce::String());
        o->setProperty ("activatedAt", (juce::int64) 0);
        o->setProperty ("lastValidated", hasKey ? nowMs() : (juce::int64) 0);
        o->setProperty ("trialStartedAt", trialStartedMs);
        o->setProperty ("trialExpiresAt", trialExpiresMs);
        o->setProperty ("lastSeen", lastSeenMs);
        return juce::var (o);
    }

    // The production salt is not a secret (only the code is); matching it lets the
    // test build a digest for a throwaway code.
    const char* kSalt = "luthier-trial-unlock-v1";
    const char* kRealDigest = "04302da4e9cadcdf221fd7b25ca2ccf22e7d04cf0305792a16cf16764c5c5f87";
}

LUTHIER_TEST (TrialLock, unexpiredTrialUnlocksPro)
{
    License lic;
    lic.fromVar (makeLicenceVar (false, nowMs() - daysMs (1), nowMs() + daysMs (59), nowMs()));

    CHECK (lic.getState() == License::State::trial);
    CHECK (lic.proFeaturesUnlocked());
    CHECK (lic.getTrialDaysLeft() >= 58);
    CHECK (lic.getTrialDaysLeft() <= 60);
}

LUTHIER_TEST (TrialLock, expiredTrialLocksProToFree)
{
    License lic;
    lic.fromVar (makeLicenceVar (false, nowMs() - daysMs (61), nowMs() - daysMs (1), nowMs() - daysMs (1)));

    CHECK (lic.getState() == License::State::expired);
    CHECK (! lic.proFeaturesUnlocked());
    CHECK (lic.getTrialDaysLeft() == 0);
}

LUTHIER_TEST (TrialLock, runtimeEditionFollowsProUnlocked)
{
    Editions::clearTestOverride();

    Editions::setProUnlocked (false);
    CHECK (Editions::current() == Edition::free);   // a Pro build degrades to Free when locked
    CHECK (! Editions::isProUnlocked());

    Editions::setProUnlocked (true);
    CHECK (Editions::current() == Edition::pro);
    CHECK (Editions::isProUnlocked());

    // Leave the global gate unlocked so sibling edition tests see the default.
    Editions::setProUnlocked (true);
    Editions::clearTestOverride();
}

LUTHIER_TEST (TrialLock, clockRollbackCannotReviveTrial)
{
    License lic;
    // Naively the trial still has 10 days, but the machine has already seen a time
    // 100 days ahead - the trial really ended, and winding the clock back must not
    // revive it.
    lic.fromVar (makeLicenceVar (false, nowMs() - daysMs (50), nowMs() + daysMs (10), nowMs() + daysMs (100)));

    CHECK (lic.getState() == License::State::expired);
    CHECK (! lic.proFeaturesUnlocked());
}

LUTHIER_TEST (TrialLock, paidLicenceIgnoresTrialClock)
{
    License lic;
    // An activated licence whose trial window is long gone must still be activated.
    lic.fromVar (makeLicenceVar (true, nowMs() - daysMs (200), nowMs() - daysMs (140), nowMs()));

    CHECK (lic.getState() == License::State::activated);
    CHECK (lic.proFeaturesUnlocked());
    CHECK (lic.getTrialDaysLeft() == 0);   // not governed by the trial clock
}

LUTHIER_TEST (TrialLock, unlockCodeReupsTrialOnRepeat)
{
    // Back up any real licence file so writing one in this test cannot clobber it.
    const auto file = License::getLicenseFile();
    const bool hadFile = file.existsAsFile();
    const auto backup = hadFile ? file.loadFileAsString() : juce::String();

    // Use a throwaway code via a test digest, so the real code never appears here.
    const juce::String testCode = "open-sesame-not-the-real-one";
    const auto testDigest = juce::SHA256 ((juce::String (kSalt) + testCode).toUTF8()).toHexString();
    License::setUnlockHashForTesting (testDigest);

    License lic;
    lic.fromVar (makeLicenceVar (false, nowMs() - daysMs (61), nowMs() - daysMs (1), nowMs() - daysMs (1)));
    CHECK (lic.getState() == License::State::expired);

    CHECK (! lic.enterUnlockCode ("definitely-wrong"));
    CHECK (lic.getState() == License::State::expired);

    CHECK (lic.enterUnlockCode (testCode));
    CHECK (lic.getState() == License::State::trial);
    CHECK (lic.proFeaturesUnlocked());
    CHECK (lic.getTrialDaysLeft() >= 59);

    // A second entry re-ups another fresh window.
    CHECK (lic.enterUnlockCode (testCode));
    CHECK (lic.getState() == License::State::trial);
    CHECK (lic.getTrialDaysLeft() >= 59);

    // Restore the real digest and the pre-test licence file.
    License::setUnlockHashForTesting (kRealDigest);
    if (hadFile)
        file.replaceWithText (backup);
    else
        file.deleteFile();
}

#endif // LUTHIER_PRO
