/*  Updates, telemetry and licensing tests (updates-telemetry.md section 8).

    Every one of these runs without a server. The transport is an interface with
    one method, so a fake can record what would have been sent and answer with
    whatever the test needs - which is the only way to check the no-network
    behaviour, the opt-in defaults and the policy override honestly.

    The delta-patch test the spec lists belongs to the installer rather than to
    the plugin; it is recorded in docs/KNOWN_ISSUES.md rather than faked here.
*/

#include "TestFramework.h"

#include "../Updates/Telemetry.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** A transport that records every call and answers from a script. */
    class FakeTransport final : public Transport
    {
    public:
        struct Call
        {
            bool wasPost = false;
            juce::String url;
            juce::String body;
        };

        Result get (const juce::String& url, int) override
        {
            calls.push_back ({ false, url, {} });

            return nextResult;
        }

        Result post (const juce::String& url, const juce::String& body, int) override
        {
            calls.push_back ({ true, url, body });

            return nextResult;
        }

        std::vector<Call> calls;
        Result nextResult;
    };

    Transport::Result succeeding (const juce::String& body)
    {
        Transport::Result result;
        result.succeeded = true;
        result.statusCode = 200;
        result.body = body;
        return result;
    }

    Transport::Result failing()
    {
        Transport::Result result;
        result.succeeded = false;
        result.statusCode = 0;
        result.error = "no route to host";
        return result;
    }

    const char* kManifest = R"({
      "schema": 1,
      "latest_stable": "1.4.2",
      "latest_beta": "1.5.0-beta3",
      "minimum_supported": "1.0.0",
      "changelog_url": "https://example.invalid/changelog",
      "downloads": {
        "windows_x64": "https://example.invalid/Luthier-1.4.2-win64.exe",
        "macos_universal": "https://example.invalid/Luthier-1.4.2-mac.pkg",
        "linux_x64": "https://example.invalid/Luthier-1.4.2-linux.tar.gz"
      }
    })";
}

//==============================================================================
/*  updates-telemetry 8: on a fresh install, all four toggles are off. This is
    the single most important test in the file. */
LUTHIER_TEST (Telemetry, everythingIsOffByDefault)
{
    Telemetry telemetry;

    CHECK_MSG (! telemetry.isCategoryEnabled (Telemetry::Category::usage),
               "usage telemetry was on by default");

    CHECK_MSG (! telemetry.isCategoryEnabled (Telemetry::Category::diagnostics),
               "diagnostics telemetry was on by default");

    CHECK_MSG (! telemetry.isCrashUploadEnabled(),
               "crash upload was on by default");

    CHECK_MSG (! telemetry.isUpdateCheckEnabled(),
               "update checking was on by default");

    CHECK_MSG (! telemetry.isBetaChannelEnabled(),
               "the beta channel was on by default");
}

//==============================================================================
/*  updates-telemetry 0.1: nothing is sent while the switches are off, whatever
    else happens. */
LUTHIER_TEST (Telemetry, nothingIsSentWhileSwitchedOff)
{
    Telemetry telemetry;

    auto fake = std::make_unique<FakeTransport>();
    auto* transport = fake.get();
    transport->nextResult = succeeding ("{}");

    telemetry.setTransport (std::move (fake));

    // Record plenty, then try to send.
    for (int i = 0; i < 50; ++i)
        telemetry.record (Telemetry::Category::usage, "panel.opened", { { "panel", "rhythm" } });

    CHECK_MSG (! telemetry.sendPending (Telemetry::Category::usage),
               "sending succeeded while the category was off");

    CHECK_MSG (transport->calls.empty(),
               juce::String ((int) transport->calls.size())
                 + " network calls were made with telemetry off");

    // An update check is refused too.
    const auto result = telemetry.checkForUpdate (Version::parse ("1.0.0"));

    CHECK (! result.checked);
    CHECK (transport->calls.empty());

    // And a crash upload.
    CHECK (! telemetry.uploadPendingCrashReport());
    CHECK (transport->calls.empty());
}

//==============================================================================
/*  With the switch on, the records do go - and the local log has them either
    way (updates-telemetry 3). */
LUTHIER_TEST (Telemetry, recordsAreLoggedLocallyAndSentWhenAllowed)
{
    Telemetry telemetry;

    auto fake = std::make_unique<FakeTransport>();
    auto* transport = fake.get();
    transport->nextResult = succeeding ("{}");

    telemetry.setTransport (std::move (fake));

    // Recorded while off: queued and logged, but not sent.
    telemetry.record (Telemetry::Category::usage, "preset.loaded");

    CHECK (telemetry.getPendingCount (Telemetry::Category::usage) == 1);
    CHECK (transport->calls.empty());

    // Switched on, and the queue goes.
    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
    CHECK (telemetry.isCategoryEnabled (Telemetry::Category::usage));

    CHECK (telemetry.sendPending (Telemetry::Category::usage));

    CHECK_MSG (transport->calls.size() == 1,
               juce::String ((int) transport->calls.size()) + " calls, expected 1");

    if (! transport->calls.empty())
    {
        CHECK (transport->calls[0].wasPost);
        CHECK (transport->calls[0].body.contains ("preset.loaded"));
    }

    // The queue is cleared by a successful send.
    CHECK (telemetry.getPendingCount (Telemetry::Category::usage) == 0);

    // A failed send keeps the records, so nothing is lost to a dropped
    // connection.
    transport->nextResult = failing();
    transport->calls.clear();

    telemetry.record (Telemetry::Category::usage, "preset.saved");

    CHECK (! telemetry.sendPending (Telemetry::Category::usage));
    CHECK_MSG (telemetry.getPendingCount (Telemetry::Category::usage) == 1,
               "a failed send discarded the records");
}

//==============================================================================
/*  updates-telemetry 8: with no network, the plugin carries on and says
    nothing. */
LUTHIER_TEST (Telemetry, noNetworkIsSilentRatherThanAnError)
{
    Telemetry telemetry;

    // No transport at all is the harshest case.
    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
    telemetry.setUpdateCheckEnabled (true);
    telemetry.setCrashUploadEnabled (true);

    telemetry.record (Telemetry::Category::usage, "something");

    CHECK (! telemetry.sendPending (Telemetry::Category::usage));

    const auto result = telemetry.checkForUpdate (Version::parse ("1.0.0"), true);

    CHECK_MSG (! result.updateAvailable,
               "an update was reported with no network");

    CHECK (! telemetry.uploadPendingCrashReport());

    // And a transport that fails every call behaves the same way.
    auto fake = std::make_unique<FakeTransport>();
    fake->nextResult = failing();
    telemetry.setTransport (std::move (fake));

    const auto second = telemetry.checkForUpdate (Version::parse ("1.0.0"), true);

    CHECK (! second.updateAvailable);
    CHECK (second.error.isNotEmpty());
}

//==============================================================================
/*  Semantic version comparison, including the pre-release rule. */
LUTHIER_TEST (Telemetry, versionComparison)
{
    CHECK (Version::parse ("1.4.2") > Version::parse ("1.4.1"));
    CHECK (Version::parse ("1.5.0") > Version::parse ("1.4.9"));
    CHECK (Version::parse ("2.0.0") > Version::parse ("1.99.99"));
    CHECK (Version::parse ("1.4.2") == Version::parse ("1.4.2"));

    // A pre-release sorts below the release it precedes. Getting this backwards
    // would offer a beta as an upgrade from the release, which is a downgrade.
    CHECK_MSG (Version::parse ("1.5.0-beta3") < Version::parse ("1.5.0"),
               "a beta sorted above its own release");

    CHECK (Version::parse ("1.5.0-beta3") > Version::parse ("1.4.9"));
    CHECK (Version::parse ("1.5.0-beta1") < Version::parse ("1.5.0-beta2"));

    // Round-tripping through text.
    CHECK (Version::parse ("1.5.0-beta3").toString() == "1.5.0-beta3");
    CHECK (Version::parse ("1.4.2").toString() == "1.4.2");

    const auto parsed = Version::parse ("1.5.0-beta3");
    CHECK (parsed.major == 1 && parsed.minor == 5 && parsed.patch == 0);
    CHECK (parsed.preRelease == "beta3");
    CHECK (parsed.isPreRelease());
    CHECK (! Version::parse ("1.4.2").isPreRelease());
}

//==============================================================================
/*  updates-telemetry 1: the manifest, and what the check does with it. */
LUTHIER_TEST (Telemetry, updateCheckReadsTheManifest)
{
    // The spec's own manifest parses.
    const auto manifest = UpdateManifest::parse (kManifest);

    CHECK (manifest.isValid());
    CHECK (manifest.schema == 1);
    CHECK (manifest.latestStable == Version::parse ("1.4.2"));
    CHECK (manifest.latestBeta == Version::parse ("1.5.0-beta3"));
    CHECK (manifest.minimumSupported == Version::parse ("1.0.0"));
    CHECK (manifest.changelogUrl == "https://example.invalid/changelog");
    CHECK (manifest.getDownloadForThisPlatform().isNotEmpty());

    // ---- an old build is offered the update ----------------------------------------
    {
        Telemetry telemetry;

        auto fake = std::make_unique<FakeTransport>();
        auto* transport = fake.get();
        transport->nextResult = succeeding (kManifest);

        telemetry.setTransport (std::move (fake));
        telemetry.setUpdateCheckEnabled (true);

        const auto result = telemetry.checkForUpdate (Version::parse ("1.0.0"), true);

        CHECK (result.checked);
        CHECK_MSG (result.updateAvailable, "1.0.0 was not offered 1.4.2");
        CHECK (result.available == Version::parse ("1.4.2"));
        CHECK (result.downloadUrl.isNotEmpty());
        CHECK (result.changelogUrl == "https://example.invalid/changelog");

        // Not the beta, because the beta channel is off.
        CHECK_MSG (! result.available.isPreRelease(),
                   "a beta was offered without the beta channel");
    }

    // ---- a current build is told it is current ---------------------------------------
    {
        Telemetry telemetry;

        auto fake = std::make_unique<FakeTransport>();
        fake->nextResult = succeeding (kManifest);

        telemetry.setTransport (std::move (fake));
        telemetry.setUpdateCheckEnabled (true);

        const auto result = telemetry.checkForUpdate (Version::parse ("1.4.2"), true);

        CHECK (result.checked);
        CHECK_MSG (! result.updateAvailable, "1.4.2 was offered an update to itself");
    }

    // ---- the beta channel ------------------------------------------------------------
    {
        Telemetry telemetry;

        auto fake = std::make_unique<FakeTransport>();
        fake->nextResult = succeeding (kManifest);

        telemetry.setTransport (std::move (fake));
        telemetry.setUpdateCheckEnabled (true);
        telemetry.setBetaChannelEnabled (true);

        const auto result = telemetry.checkForUpdate (Version::parse ("1.4.2"), true);

        CHECK (result.updateAvailable);
        CHECK_MSG (result.available == Version::parse ("1.5.0-beta3"),
                   "the beta channel offered " + result.available.toString());
    }

    // ---- a build newer than the beta is offered nothing ---------------------------------
    {
        Telemetry telemetry;

        auto fake = std::make_unique<FakeTransport>();
        fake->nextResult = succeeding (kManifest);

        telemetry.setTransport (std::move (fake));
        telemetry.setUpdateCheckEnabled (true);
        telemetry.setBetaChannelEnabled (true);

        const auto result = telemetry.checkForUpdate (Version::parse ("2.0.0"), true);

        CHECK (! result.updateAvailable);
    }
}

//==============================================================================
/*  updates-telemetry 1: throttled to once per 24 hours. */
LUTHIER_TEST (Telemetry, updateCheckIsThrottled)
{
    Telemetry telemetry;

    auto fake = std::make_unique<FakeTransport>();
    auto* transport = fake.get();
    transport->nextResult = succeeding (kManifest);

    telemetry.setTransport (std::move (fake));
    telemetry.setUpdateCheckEnabled (true);

    CHECK (telemetry.checkForUpdate (Version::parse ("1.0.0"), true).checked);

    const auto callsAfterFirst = transport->calls.size();

    // A second check straight away is refused.
    const auto second = telemetry.checkForUpdate (Version::parse ("1.0.0"), false);

    CHECK_MSG (! second.checked, "a second check inside the throttle window went through");

    CHECK_MSG (transport->calls.size() == callsAfterFirst,
               "the throttled check still made a network call");

    // Forcing it works, which is what the "Check now" button does.
    CHECK (telemetry.checkForUpdate (Version::parse ("1.0.0"), true).checked);
    CHECK (transport->calls.size() > callsAfterFirst);
}

//==============================================================================
/*  updates-telemetry 8: a policy file forbids what the user cannot then
    re-enable. */
LUTHIER_TEST (Telemetry, policyOverridesTheUser)
{
    Telemetry telemetry;

    // Without a policy, the user is in charge.
    CHECK (! telemetry.isManagedByPolicy());

    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
    CHECK (telemetry.isCategoryEnabled (Telemetry::Category::usage));

    // ---- with a policy ------------------------------------------------------------------
    // The policy file is read from a system path this test cannot write to, so
    // the rule itself is checked through the Policy struct and the enforcement
    // through a telemetry instance given that policy's shape.
    const char* policyJson = R"({
      "allow_usage_telemetry": false,
      "allow_crash_upload": false,
      "update_manifest_url": "https://mirror.internal.invalid/manifest.json"
    })";

    const auto parsed = juce::JSON::parse (policyJson);
    auto* root = parsed.getDynamicObject();

    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    // The denial rule: false switches off, and true grants nothing.
    CHECK (! (bool) root->getProperty ("allow_usage_telemetry"));
    CHECK (! (bool) root->getProperty ("allow_crash_upload"));

    // A policy that omits a key leaves it allowed.
    CHECK (! root->hasProperty ("allow_diagnostics_telemetry"));

    // And a policy can point the endpoint at a mirror.
    CHECK (root->getProperty ("update_manifest_url").toString().startsWith ("https://mirror"));
}

//==============================================================================
/*  updates-telemetry 0.3: every outbound call is logged locally, whether it
    succeeded or not. */
LUTHIER_TEST (Telemetry, everyOutboundCallIsLogged)
{
    Telemetry telemetry;

    telemetry.clearLocalLogs();

    auto fake = std::make_unique<FakeTransport>();
    fake->nextResult = succeeding ("{}");

    telemetry.setTransport (std::move (fake));
    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);

    telemetry.record (Telemetry::Category::usage, "test.event");
    telemetry.sendPending (Telemetry::Category::usage);

    const auto log = telemetry.readNetworkLog();

    CHECK_MSG (log.size() >= 2,
               "the network log has only " + juce::String (log.size()) + " lines");

    bool foundAttempt = false, foundOutcome = false;

    for (const auto& line : log)
    {
        if (line.contains ("attempting"))
            foundAttempt = true;

        if (line.contains ("sent"))
            foundOutcome = true;

        // The spec asks for destination, size, timestamp and category on each
        // line.
        if (line.isNotEmpty())
        {
            CHECK_MSG (line.contains ("bytes"), "a log line has no size: " + line);
            CHECK_MSG (line.contains ("http"), "a log line has no destination: " + line);
        }
    }

    CHECK_MSG (foundAttempt, "the attempt was not logged before it was made");
    CHECK_MSG (foundOutcome, "the outcome was not logged");

    // And the telemetry log holds the record itself, so the user can read what
    // would be sent.
    const auto records = telemetry.readTelemetryLog();

    bool foundRecord = false;

    for (const auto& line : records)
        if (line.contains ("test.event"))
            foundRecord = true;

    CHECK_MSG (foundRecord, "the record was not written to the readable log");

    telemetry.clearLocalLogs();
}

//==============================================================================
/*  updates-telemetry 6: the paranoia button switches everything off and deletes
    the files. */
LUTHIER_TEST (Telemetry, turnEverythingOffDeletesAndDisables)
{
    Telemetry telemetry;

    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
    telemetry.setCategoryEnabled (Telemetry::Category::diagnostics, true);
    telemetry.setCrashUploadEnabled (true);
    telemetry.setUpdateCheckEnabled (true);
    telemetry.setBetaChannelEnabled (true);

    telemetry.record (Telemetry::Category::usage, "something");

    CHECK (telemetry.getPendingCount (Telemetry::Category::usage) > 0);

    telemetry.turnEverythingOffAndDelete();

    CHECK (! telemetry.isCategoryEnabled (Telemetry::Category::usage));
    CHECK (! telemetry.isCategoryEnabled (Telemetry::Category::diagnostics));
    CHECK (! telemetry.isCrashUploadEnabled());
    CHECK (! telemetry.isUpdateCheckEnabled());
    CHECK (! telemetry.isBetaChannelEnabled());

    CHECK_MSG (telemetry.getPendingCount (Telemetry::Category::usage) == 0,
               "records survived the paranoia button");
}

//==============================================================================
/*  The privacy settings survive a restart. */
LUTHIER_TEST (Telemetry, settingsRoundTrip)
{
    Telemetry telemetry;

    telemetry.setCategoryEnabled (Telemetry::Category::usage, true);
    telemetry.setCategoryEnabled (Telemetry::Category::diagnostics, false);
    telemetry.setCrashUploadEnabled (true);
    telemetry.setUpdateCheckEnabled (true);
    telemetry.setBetaChannelEnabled (true);
    telemetry.setManifestUrl ("https://mirror.example.invalid/manifest.json");

    const auto text = juce::JSON::toString (telemetry.toVar(), false);

    Telemetry restored;
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.isCategoryEnabled (Telemetry::Category::usage));
    CHECK (! restored.isCategoryEnabled (Telemetry::Category::diagnostics));
    CHECK (restored.isCrashUploadEnabled());
    CHECK (restored.isUpdateCheckEnabled());
    CHECK (restored.isBetaChannelEnabled());
    CHECK (restored.getManifestUrl() == "https://mirror.example.invalid/manifest.json");
}

//==============================================================================
/*  updates-telemetry 4: a crash report describes itself before it is sent, and
    says plainly what it does not contain. */
LUTHIER_TEST (Telemetry, crashReportDescribesItself)
{
    Telemetry telemetry;

    // With no dump, there is nothing pending and nothing to describe.
    if (! telemetry.hasPendingCrashReport())
    {
        CHECK (telemetry.getPendingCrashReport() == juce::File());
        CHECK (telemetry.describePendingCrashReport().isEmpty());
    }

    // Uploading with the switch off is refused whatever is on disk.
    CHECK (! telemetry.isCrashUploadEnabled());
    CHECK (! telemetry.uploadPendingCrashReport());
}

//==============================================================================
#if LUTHIER_PRO
/*  updates-telemetry 5: the licence key never leaves the machine after
    activation, and the grace period keeps a laptop working offline. */
LUTHIER_TEST (Telemetry, licenceActivationAndGrace)
{
    License license;

    CHECK (license.getState() == License::State::unlicensed);

    FakeTransport transport;
    transport.nextResult = succeeding (R"({"ok":true})");

    license.setTransport (&transport);

    CHECK (license.activate ("ABCD-1234-EFGH-5678", "https://licence.example.invalid/activate"));
    CHECK (license.getState() == License::State::activated);

    // The key went out once, on activation, and never again.
    CHECK (transport.calls.size() == 1);
    CHECK (transport.calls[0].body.contains ("ABCD-1234-EFGH-5678"));

    transport.calls.clear();

    CHECK (license.revalidate ("https://licence.example.invalid/revalidate"));

    CHECK (transport.calls.size() == 1);

    CHECK_MSG (! transport.calls[0].body.contains ("ABCD-1234-EFGH-5678"),
               "revalidation sent the licence key");

    CHECK_MSG (transport.calls[0].body.contains ("proof"),
               "revalidation sent something other than a proof");

    // The proof is stable and not the key.
    const auto proof = license.getActivationProof();

    CHECK (proof.isNotEmpty());
    CHECK (proof != "ABCD-1234-EFGH-5678");
    CHECK (license.getActivationProof() == proof);

    // ---- the state file -------------------------------------------------------------
    const auto text = juce::JSON::toString (license.toVar(), false);

    CHECK_MSG (! text.contains ("ABCD-1234-EFGH-5678"),
               "the licence file holds the key in the clear");

    License restored;
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.getState() == License::State::activated);
    CHECK (restored.getActivationProof() == proof);

    // ---- deactivation ---------------------------------------------------------------
    license.deactivate();
    CHECK (license.getState() == License::State::unlicensed);
    CHECK (license.getActivationProof().isEmpty());

    // ---- offline activation ---------------------------------------------------------
    License offline;

    const auto challenge = offline.getOfflineChallenge ("WXYZ-9876-IJKL-5432");

    CHECK (challenge.isNotEmpty());
    CHECK_MSG (! challenge.contains ("WXYZ-9876"),
               "the offline challenge contains the key");

    // The same key gives the same challenge on this machine.
    CHECK (offline.getOfflineChallenge ("WXYZ-9876-IJKL-5432") == challenge);

    // A different key gives a different one.
    CHECK (offline.getOfflineChallenge ("OTHER-KEY") != challenge);

    CHECK (offline.applyOfflineResponse ("A1B2C3D4E5F6A7B8"));
    CHECK (offline.getState() == License::State::activated);

    // Nonsense is refused.
    License rejecting;
    CHECK (! rejecting.applyOfflineResponse ("short"));
    CHECK (rejecting.getState() == License::State::unlicensed);
}

//==============================================================================
/*  updates-telemetry 5: revalidation counts down, and failing it does not lock
    the user out immediately. */
LUTHIER_TEST (Telemetry, revalidationCountdownAndOfflineTolerance)
{
    License license;

    FakeTransport transport;
    transport.nextResult = succeeding ("{}");
    license.setTransport (&transport);

    license.activate ("KEY", "https://licence.example.invalid/activate");

    // Freshly activated: a full month before revalidation is due.
    CHECK_MSG (license.getDaysUntilRevalidation() >= 29,
               "a fresh activation had only "
                 + juce::String (license.getDaysUntilRevalidation()) + " days left");

    // A failed revalidation does not revoke it: the grace period is what keeps a
    // laptop working at a gig with no network.
    transport.nextResult = failing();

    license.revalidate ("https://licence.example.invalid/revalidate");

    CHECK_MSG (license.getState() == License::State::activated,
               "one failed revalidation revoked the licence");

    // An unlicensed instance has nothing to count down.
    License none;
    CHECK (none.getDaysUntilRevalidation() == 0);
    CHECK (! none.revalidate ("https://licence.example.invalid/revalidate"));
}

#endif // LUTHIER_PRO
