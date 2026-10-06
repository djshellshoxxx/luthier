#pragma once

/*  Updates, telemetry, crash reporting and licensing (updates-telemetry.md).

    Rule 1 of section 0 is the one everything else is arranged to make true:
    nothing leaves the machine without an explicit opt-in, and every switch is
    off by default. That is enforced structurally rather than by remembering to
    check - `Telemetry::send` refuses unless the matching category is on, and the
    only way to turn one on is a call the UI makes when the user ticks a box.

    Rule 3 is the other structural one: every outbound call is logged locally
    before it is attempted, in a file the user can read. A record that is written
    only on success would let a failed upload go unlogged, so the log entry comes
    first and is amended with the outcome.

    Network access goes through `Transport`, an interface with one method. The
    shipping implementation is an HTTPS POST; the tests substitute a fake. That
    is what lets the no-network behaviour, the policy override and the opt-in
    default all be tested without a server.
*/

#include <juce_core/juce_core.h>

// SHA-256, for the activation proof that lets the license key stay on the
// machine after the first activation (updates-telemetry 5).
#include <juce_cryptography/juce_cryptography.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace luthier
{

//==============================================================================
/** A semantic version, for the comparison update checks need. */
struct Version
{
    int major = 0, minor = 0, patch = 0;

    /** A pre-release tag: "beta3" in "1.5.0-beta3". Empty for a release. */
    juce::String preRelease;

    static Version parse (const juce::String& text);

    juce::String toString() const;

    bool isPreRelease() const noexcept { return preRelease.isNotEmpty(); }

    /** Semver ordering: a pre-release sorts *below* the release it precedes. */
    int compare (const Version& other) const noexcept;

    bool operator<  (const Version& other) const noexcept { return compare (other) <  0; }
    bool operator>  (const Version& other) const noexcept { return compare (other) >  0; }
    bool operator== (const Version& other) const noexcept { return compare (other) == 0; }
    bool operator!= (const Version& other) const noexcept { return compare (other) != 0; }
};

//==============================================================================
/** The update manifest (updates-telemetry 1). */
struct UpdateManifest
{
    int schema = 1;

    Version latestStable;
    Version latestBeta;
    Version minimumSupported;

    juce::String changelogUrl;

    /** Platform key to download URL. */
    std::map<juce::String, juce::String> downloads;

    bool isValid() const noexcept { return schema >= 1 && latestStable.major > 0; }

    static UpdateManifest parse (const juce::String& json);

    /** The download for the platform this build runs on, or empty. */
    juce::String getDownloadForThisPlatform() const;

    static juce::String getThisPlatformKey();
};

//==============================================================================
/** Anything that can make a network request.

    One method, so a test can substitute a fake and the plugin never has to know
    whether it is talking to a real server. */
class Transport
{
public:
    virtual ~Transport() = default;

    struct Result
    {
        bool succeeded = false;
        int statusCode = 0;
        juce::String body;
        juce::String error;
    };

    /** A GET. Blocking; always called from a worker thread. */
    virtual Result get (const juce::String& url, int timeoutMs) = 0;

    /** A POST. Blocking; always called from a worker thread. */
    virtual Result post (const juce::String& url, const juce::String& body, int timeoutMs) = 0;
};

/** The shipping transport: HTTPS through JUCE's URL class. */
std::unique_ptr<Transport> createHttpsTransport();

//==============================================================================
/** The enterprise policy file (updates-telemetry 7).

    An administrator drops a JSON file at a documented path and the plugin obeys
    it. A policy can only ever *remove* a permission - it can force telemetry off
    but never on - because a policy that could switch data collection on for a
    user who declined it would defeat the point of the opt-in. */
struct Policy
{
    bool present = false;

    bool allowUsageTelemetry = true;
    bool allowDiagnosticsTelemetry = true;
    bool allowCrashUpload = true;
    bool allowUpdateCheck = true;

    /** A private mirror to check for updates against, if the policy sets one. */
    juce::String updateManifestUrl;
    juce::String telemetryUrl;
    juce::String crashUploadUrl;

    static Policy load();
    static juce::File getPolicyFile();

    /** SPEC-SWEEP: UT-26 - tests point the policy at a temporary file; an
        empty File restores the system path. */
    static void setPolicyFileForTesting (const juce::File& file);
};

//==============================================================================
class Telemetry
{
public:
    /** updates-telemetry 3: two categories, opted into independently. */
    enum class Category { usage = 0, diagnostics, numCategories };

    static const char* getCategoryName (Category category) noexcept;

    Telemetry();
    ~Telemetry();

    /** Installs the transport. Without one, nothing can be sent at all, which
        is the state the tests use for the no-network case. */
    void setTransport (std::unique_ptr<Transport> transport);

    /** Re-reads the policy file. */
    void refreshPolicy();
    const Policy& getPolicy() const noexcept { return policy; }
    bool isManagedByPolicy() const noexcept { return policy.present; }

    //==========================================================================
    // The switches. Every one of these is false until the user says otherwise.

    void setCategoryEnabled (Category category, bool enabled);
    bool isCategoryEnabled (Category category) const;

    void setCrashUploadEnabled (bool enabled);
    bool isCrashUploadEnabled() const;

    void setUpdateCheckEnabled (bool enabled);
    bool isUpdateCheckEnabled() const;

    void setBetaChannelEnabled (bool enabled);
    bool isBetaChannelEnabled() const noexcept { return betaChannel; }

    //==========================================================================
    /** Records an event. Written to the local log whether or not it is ever
        sent, so the user can read exactly what would go. Never blocks. */
    void record (Category category, const juce::String& eventName,
                 const std::map<juce::String, juce::String>& fields = {});

    /** SPEC-SWEEP: UT-2 - whether a field may be recorded at all: an allowlisted
        key with a value that cannot be a path, an address or free text. */
    static bool isAllowedField (const juce::String& key, const juce::String& value);

    /** Sends the day's records, if the category is on and a day has passed.
        Worker thread. Returns false if there was nothing to send or it was not
        allowed. */
    bool sendPending (Category category);

    /** How many records are waiting. */
    int getPendingCount (Category category) const;

    //==========================================================================
    // The update check (updates-telemetry 1).

    struct UpdateResult
    {
        bool checked = false;
        bool updateAvailable = false;
        Version available;
        juce::String downloadUrl;
        juce::String changelogUrl;
        juce::String error;
    };

    /** Checks for an update. Worker thread; never touches the audio thread.
        Throttled to once per 24 hours unless `force` is set. */
    UpdateResult checkForUpdate (const Version& runningVersion, bool force = false);

    /** SPEC-SWEEP: UT-4 - the same check on a worker thread, never the message
        or audio thread; `onResult` is posted back to the message thread. The
        Telemetry must outlive the call (it is the processor's). */
    void checkForUpdateAsync (const Version& runningVersion, bool force,
                              std::function<void (const UpdateResult&)> onResult);

    /** When the last check happened, for the throttle and for the UI. */
    juce::Time getLastUpdateCheckTime() const { return lastUpdateCheck; }

    //==========================================================================
    // Crash reporting (updates-telemetry 4).

    /** True when a dump from a previous run is waiting to be offered. */
    bool hasPendingCrashReport() const;

    juce::File getPendingCrashReport() const;

    /** Exactly what would be uploaded, for the diff viewer the spec asks for. */
    juce::String describePendingCrashReport() const;

    /** Uploads it. Three attempts, then it stays on disk. Worker thread. */
    bool uploadPendingCrashReport();

    void discardPendingCrashReport();

    //==========================================================================
    // The local log (updates-telemetry 3).

    static juce::File getDiagnosticsDirectory();
    static juce::File getTelemetryLogFile();
    static juce::File getNetworkLogFile();

    /** Every outbound call, most recent last. */
    juce::StringArray readNetworkLog() const;

    juce::StringArray readTelemetryLog() const;

    //==========================================================================
    /** updates-telemetry 6: the paranoia button. Everything off, every local
        diagnostic file deleted. */
    void turnEverythingOffAndDelete();

    /** Just the logs. */
    void clearLocalLogs();

    //==========================================================================
    // Endpoints, editable for an enterprise proxy (updates-telemetry 6).

    void setManifestUrl (const juce::String& url);
    void setTelemetryUrl (const juce::String& url);
    void setCrashUploadUrl (const juce::String& url);

    juce::String getManifestUrl() const;
    juce::String getTelemetryUrl() const;
    juce::String getCrashUploadUrl() const;

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

    bool saveSettings() const;
    bool loadSettings();

    static juce::File getSettingsFile();

private:
    /** Writes a line to the network log before a call is attempted, and returns
        the line's index so the outcome can be appended. */
    void logNetworkCall (const juce::String& destination, const juce::String& category,
                         int bytes, const juce::String& outcome) const;

    /** True when the category is both user-enabled and policy-allowed. */
    bool isAllowed (Category category) const;

    std::unique_ptr<Transport> transport;

    Policy policy;

    // updates-telemetry 0.1: every one of these starts false.
    std::array<std::atomic<bool>, (size_t) Category::numCategories> categoryEnabled {};
    std::atomic<bool> crashUploadEnabled { false };
    std::atomic<bool> updateCheckEnabled { false };
    bool betaChannel = false;

    juce::String manifestUrl { "https://updates.luthieraudio.com/v1/manifest.json" };
    juce::String telemetryUrl { "https://telemetry.luthieraudio.com/v1/ingest" };
    juce::String crashUploadUrl { "https://crash.luthieraudio.com/v1/upload" };

    juce::Time lastUpdateCheck;

    mutable juce::CriticalSection recordLock;
    std::array<juce::StringArray, (size_t) Category::numCategories> pending;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Telemetry)
};

//==============================================================================
/** License activation (updates-telemetry 5).

    Optional: an open-source build never calls any of this. A commercial build
    activates once online and revalidates monthly, with a fortnight of grace so
    that a laptop taken to a gig without a network keeps working. */
#if LUTHIER_PRO
class License
{
public:
    enum class State { unlicensed = 0, activated, grace, expired, numStates };

    static const char* getStateName (State state) noexcept;

    License();

    void setTransport (Transport* t) noexcept { transport = t; }

    State getState() const noexcept { return state; }

    /** Days left before revalidation is required. */
    int getDaysUntilRevalidation() const;

    /** updates-telemetry 5: one online activation. Worker thread. */
    bool activate (const juce::String& licenseKey, const juce::String& url);

    /** The monthly revalidation. Sends a signed proof, never the key. */
    bool revalidate (const juce::String& url);

    /** One click, immediate, and it frees the seat. */
    void deactivate();

    //==========================================================================
    // Offline activation, for an air-gapped machine (updates-telemetry 5).

    /** The challenge the user carries to another machine. */
    juce::String getOfflineChallenge (const juce::String& licenseKey) const;

    /** The response they bring back. */
    bool applyOfflineResponse (const juce::String& response);

    //==========================================================================
    /** The proof of activation sent at revalidation. Derived from the key but
        not reversible to it, which is what lets the key stay on the machine. */
    juce::String getActivationProof() const;

    juce::var toVar() const;
    void fromVar (const juce::var& state);

    bool save() const;
    bool load();

    static juce::File getLicenseFile();

private:
    void updateStateFromDates();

    State state = State::unlicensed;

    /** Stored so revalidation can prove activation; never transmitted. */
    juce::String storedKeyHash;

    juce::Time activatedAt;
    juce::Time lastValidated;

    Transport* transport = nullptr;

    static constexpr int kRevalidationDays = 30;
    static constexpr int kGraceDays = 14;
};
#endif

} // namespace luthier
