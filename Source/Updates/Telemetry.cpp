#include "Telemetry.h"
#include "../Support/ConfigRecovery.h"

namespace luthier
{

//==============================================================================
Version Version::parse (const juce::String& text)
{
    Version version;

    auto working = text.trim();

    // A pre-release tag comes after a hyphen: "1.5.0-beta3".
    const int hyphen = working.indexOfChar ('-');

    if (hyphen >= 0)
    {
        version.preRelease = working.substring (hyphen + 1);
        working = working.substring (0, hyphen);
    }

    const auto parts = juce::StringArray::fromTokens (working, ".", "");

    if (parts.size() > 0) version.major = parts[0].getIntValue();
    if (parts.size() > 1) version.minor = parts[1].getIntValue();
    if (parts.size() > 2) version.patch = parts[2].getIntValue();

    return version;
}

juce::String Version::toString() const
{
    auto text = juce::String (major) + "." + juce::String (minor) + "." + juce::String (patch);

    if (preRelease.isNotEmpty())
        text += "-" + preRelease;

    return text;
}

int Version::compare (const Version& other) const noexcept
{
    if (major != other.major) return major < other.major ? -1 : 1;
    if (minor != other.minor) return minor < other.minor ? -1 : 1;
    if (patch != other.patch) return patch < other.patch ? -1 : 1;

    /*  Semver's pre-release rule: 1.5.0-beta3 comes *before* 1.5.0.

        Getting this backwards would be worse than not checking at all - a user
        on the release would be offered the beta as an "update", which is a
        downgrade onto less tested code.
    */
    const bool aPre = isPreRelease();
    const bool bPre = other.isPreRelease();

    if (aPre && ! bPre) return -1;
    if (! aPre && bPre) return 1;
    if (! aPre && ! bPre) return 0;

    return preRelease.compare (other.preRelease);
}

//==============================================================================
juce::String UpdateManifest::getThisPlatformKey()
{
   #if JUCE_WINDOWS
    return "windows_x64";
   #elif JUCE_MAC
    return "macos_universal";
   #elif JUCE_LINUX
    return "linux_x64";
   #else
    return "unknown";
   #endif
}

juce::String UpdateManifest::getDownloadForThisPlatform() const
{
    const auto key = getThisPlatformKey();

    if (const auto entry = downloads.find (key); entry != downloads.end())
        return entry->second;

    return {};
}

UpdateManifest UpdateManifest::parse (const juce::String& json)
{
    UpdateManifest manifest;

    const auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return manifest;

    manifest.schema = juce::jmax (0, (int) root->getProperty ("schema"));

    manifest.latestStable     = Version::parse (root->getProperty ("latest_stable").toString());
    manifest.latestBeta       = Version::parse (root->getProperty ("latest_beta").toString());
    manifest.minimumSupported = Version::parse (root->getProperty ("minimum_supported").toString());

    manifest.changelogUrl = root->getProperty ("changelog_url").toString();

    if (auto* downloads = root->getProperty ("downloads").getDynamicObject())
        for (const auto& property : downloads->getProperties())
            manifest.downloads[property.name.toString()] = property.value.toString();

    return manifest;
}

//==============================================================================
namespace
{
    /** The shipping transport. */
    class HttpsTransport final : public Transport
    {
    public:
        Result get (const juce::String& url, int timeoutMs) override
        {
            return request (url, {}, false, timeoutMs);
        }

        Result post (const juce::String& url, const juce::String& body, int timeoutMs) override
        {
            return request (url, body, true, timeoutMs);
        }

    private:
        static Result request (const juce::String& urlText, const juce::String& body,
                               bool isPost, int timeoutMs)
        {
            Result result;

            // updates-telemetry 0.1 in spirit: nothing but HTTPS, so a manifest
            // URL edited to point at plain HTTP cannot downgrade the connection.
            if (! urlText.startsWithIgnoreCase ("https://"))
            {
                result.error = "Only HTTPS endpoints are allowed.";
                return result;
            }

            juce::URL url (urlText);

            if (isPost)
                url = url.withPOSTData (body);

            int statusCode = 0;

            // InputStreamOptions is not assignable, so the whole chain is built
            // as one expression rather than amended after the fact.
            const auto options = juce::URL::InputStreamOptions (
                                     isPost ? juce::URL::ParameterHandling::inPostData
                                            : juce::URL::ParameterHandling::inAddress)
                                   .withConnectionTimeoutMs (timeoutMs)
                                   .withExtraHeaders ("Content-Type: application/json\r\n")
                                   .withStatusCode (&statusCode);

            if (auto stream = url.createInputStream (options))
            {
                result.body = stream->readEntireStreamAsString();
                result.statusCode = statusCode;
                result.succeeded = statusCode >= 200 && statusCode < 300;

                if (! result.succeeded)
                    result.error = "HTTP " + juce::String (statusCode);
            }
            else
            {
                result.error = "Could not reach " + urlText;
            }

            return result;
        }
    };
}

std::unique_ptr<Transport> createHttpsTransport()
{
    return std::make_unique<HttpsTransport>();
}

//==============================================================================
juce::File Policy::getPolicyFile()
{
    // A documented, system-wide path, so an administrator can deploy it.
   #if JUCE_WINDOWS
    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("luthier-policy.json");
   #elif JUCE_MAC
    return juce::File ("/Library/Application Support/Luthier/luthier-policy.json");
   #else
    return juce::File ("/etc/luthier/luthier-policy.json");
   #endif
}

Policy Policy::load()
{
    Policy policy;

    const auto file = getPolicyFile();

    if (! file.existsAsFile())
        return policy;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return policy;

    policy.present = true;

    /*  A policy can only remove a permission.

        Each flag defaults to allowed and is only ever set to false by the file,
        so a policy that says `"allow_usage_telemetry": true` grants nothing the
        user had not already granted. That is deliberate: an administrator can
        forbid data collection, but cannot switch it on behind a user who
        declined it.
    */
    auto readDenial = [root] (const char* key, bool& destination)
    {
        if (root->hasProperty (key) && ! (bool) root->getProperty (key))
            destination = false;
    };

    readDenial ("allow_usage_telemetry", policy.allowUsageTelemetry);
    readDenial ("allow_diagnostics_telemetry", policy.allowDiagnosticsTelemetry);
    readDenial ("allow_crash_upload", policy.allowCrashUpload);
    readDenial ("allow_update_check", policy.allowUpdateCheck);

    policy.updateManifestUrl = root->getProperty ("update_manifest_url").toString();
    policy.telemetryUrl      = root->getProperty ("telemetry_url").toString();
    policy.crashUploadUrl    = root->getProperty ("crash_upload_url").toString();

    return policy;
}

//==============================================================================
const char* Telemetry::getCategoryName (Category category) noexcept
{
    switch (category)
    {
        case Category::usage:       return "usage";
        case Category::diagnostics: return "diagnostics";
        case Category::numCategories:
        default:                    return "usage";
    }
}

Telemetry::Telemetry()
{
    for (auto& flag : categoryEnabled)
        flag.store (false, std::memory_order_relaxed);

    refreshPolicy();
}

Telemetry::~Telemetry() = default;

void Telemetry::setTransport (std::unique_ptr<Transport> t)
{
    transport = std::move (t);
}

void Telemetry::refreshPolicy()
{
    policy = Policy::load();

    // A policy that forbids something switches it off now rather than merely
    // blocking it later, so the privacy dashboard shows the true state.
    if (! policy.allowUsageTelemetry)
        categoryEnabled[(size_t) Category::usage].store (false, std::memory_order_relaxed);

    if (! policy.allowDiagnosticsTelemetry)
        categoryEnabled[(size_t) Category::diagnostics].store (false, std::memory_order_relaxed);

    if (! policy.allowCrashUpload)
        crashUploadEnabled.store (false, std::memory_order_relaxed);

    if (! policy.allowUpdateCheck)
        updateCheckEnabled.store (false, std::memory_order_relaxed);

    if (policy.updateManifestUrl.isNotEmpty()) manifestUrl = policy.updateManifestUrl;
    if (policy.telemetryUrl.isNotEmpty())      telemetryUrl = policy.telemetryUrl;
    if (policy.crashUploadUrl.isNotEmpty())    crashUploadUrl = policy.crashUploadUrl;
}

//==============================================================================
void Telemetry::setCategoryEnabled (Category category, bool enabled)
{
    if (! juce::isPositiveAndBelow ((int) category, (int) Category::numCategories))
        return;

    // Policy wins. A user ticking a box a policy forbids gets nothing.
    const bool allowedByPolicy = (category == Category::usage)
                                   ? policy.allowUsageTelemetry
                                   : policy.allowDiagnosticsTelemetry;

    categoryEnabled[(size_t) category].store (enabled && allowedByPolicy,
                                              std::memory_order_relaxed);
}

bool Telemetry::isCategoryEnabled (Category category) const
{
    if (! juce::isPositiveAndBelow ((int) category, (int) Category::numCategories))
        return false;

    return categoryEnabled[(size_t) category].load (std::memory_order_relaxed);
}

void Telemetry::setCrashUploadEnabled (bool enabled)
{
    crashUploadEnabled.store (enabled && policy.allowCrashUpload, std::memory_order_relaxed);
}

bool Telemetry::isCrashUploadEnabled() const
{
    return crashUploadEnabled.load (std::memory_order_relaxed);
}

void Telemetry::setUpdateCheckEnabled (bool enabled)
{
    updateCheckEnabled.store (enabled && policy.allowUpdateCheck, std::memory_order_relaxed);
}

bool Telemetry::isUpdateCheckEnabled() const
{
    return updateCheckEnabled.load (std::memory_order_relaxed);
}

void Telemetry::setBetaChannelEnabled (bool enabled)
{
    betaChannel = enabled;
}

bool Telemetry::isAllowed (Category category) const
{
    return isCategoryEnabled (category);
}

//==============================================================================
bool Telemetry::isAllowedField (const juce::String& key, const juce::String& value)
{
    /*  SPEC-SWEEP: UT-2 - updates-telemetry 3's allowlist. Only non-identifying
        facts: the host and its setup, which panel or feature, counts and CPU.
        A value that looks like a path is dropped even under an allowed key,
        because a host name field is one careless call away from a file name. */
    static const juce::StringArray allowed { "host", "format", "wrapper", "sampleRate", "blockSize",
                                             "os", "version", "panel", "tab", "page", "feature",
                                             "source", "count", "cpu", "error", "code", "enabled" };

    if (! allowed.contains (key))
        return false;

    return ! (value.containsChar ('/') || value.containsChar ('\\') || value.containsChar ('@')
              || value.length() > 64);
}

void Telemetry::record (Category category, const juce::String& eventName,
                        const std::map<juce::String, juce::String>& fields)
{
    if (! juce::isPositiveAndBelow ((int) category, (int) Category::numCategories))
        return;

    auto* object = new juce::DynamicObject();

    object->setProperty ("t", juce::Time::getCurrentTime().toISO8601 (true));
    object->setProperty ("category", getCategoryName (category));
    object->setProperty ("event", eventName);

    /*  updates-telemetry 3: no personally identifying information.

        Only the non-identifying host facts the spec names are attached: the
        host's name, the sample rate, the block size and the plugin format. No
        file names, no preset names, no user content, no identifiers of any
        kind - not even a random installation id, which would be pseudonymous
        rather than anonymous.
    */
    for (const auto& [key, value] : fields)
        if (isAllowedField (key, value))   // SPEC-SWEEP: UT-2 - enforced, not remembered
            object->setProperty (key, value);

    const auto line = juce::JSON::toString (juce::var (object), true);

    // Written to the local log whether or not it is ever sent, which is what
    // makes "the user can read exactly what would be sent" true.
    const auto logFile = getTelemetryLogFile();
    logFile.getParentDirectory().createDirectory();
    logFile.appendText (line + juce::newLine);

    const juce::ScopedLock sl (recordLock);

    // A cap, so a long session cannot grow the queue without bound.
    if (pending[(size_t) category].size() < 2000)
        pending[(size_t) category].add (line);
}

int Telemetry::getPendingCount (Category category) const
{
    if (! juce::isPositiveAndBelow ((int) category, (int) Category::numCategories))
        return 0;

    const juce::ScopedLock sl (recordLock);
    return pending[(size_t) category].size();
}

bool Telemetry::sendPending (Category category)
{
    if (! isAllowed (category))
        return false;

    juce::StringArray toSend;

    {
        const juce::ScopedLock sl (recordLock);

        if (pending[(size_t) category].isEmpty())
            return false;

        toSend = pending[(size_t) category];
    }

    const auto body = "{\"records\":[" + toSend.joinIntoString (",") + "]}";

    // updates-telemetry 0.3: logged before it is attempted, so a failure is
    // visible in the log rather than absent from it.
    logNetworkCall (telemetryUrl, getCategoryName (category),
                    (int) body.getNumBytesAsUTF8(), "attempting");

    if (transport == nullptr)
    {
        logNetworkCall (telemetryUrl, getCategoryName (category),
                        (int) body.getNumBytesAsUTF8(), "no transport; not sent");
        return false;
    }

    const auto result = transport->post (telemetryUrl, body, 10000);

    logNetworkCall (telemetryUrl, getCategoryName (category),
                    (int) body.getNumBytesAsUTF8(),
                    result.succeeded ? "sent" : ("failed: " + result.error));

    if (! result.succeeded)
        return false;

    {
        const juce::ScopedLock sl (recordLock);
        pending[(size_t) category].clear();
    }

    return true;
}

//==============================================================================
Telemetry::UpdateResult Telemetry::checkForUpdate (const Version& runningVersion, bool force)
{
    UpdateResult result;

    if (! isUpdateCheckEnabled())
    {
        result.error = "Update checks are switched off.";
        return result;
    }

    // updates-telemetry 1: throttled to once per 24 hours.
    if (! force && lastUpdateCheck != juce::Time()
          && (juce::Time::getCurrentTime() - lastUpdateCheck).inHours() < 24.0)
    {
        result.error = "Checked within the last day.";
        return result;
    }

    logNetworkCall (manifestUrl, "update-check", 0, "attempting");

    if (transport == nullptr)
    {
        // updates-telemetry 8: with no network the plugin carries on and the
        // user sees nothing. The failure is logged, not surfaced.
        logNetworkCall (manifestUrl, "update-check", 0, "no transport; not sent");
        result.error = "No network.";
        return result;
    }

    const auto response = transport->get (manifestUrl, 10000);

    logNetworkCall (manifestUrl, "update-check", (int) response.body.getNumBytesAsUTF8(),
                    response.succeeded ? "received" : ("failed: " + response.error));

    lastUpdateCheck = juce::Time::getCurrentTime();

    if (! response.succeeded)
    {
        result.error = response.error;
        return result;
    }

    const auto manifest = UpdateManifest::parse (response.body);

    if (! manifest.isValid())
    {
        result.error = "The update manifest could not be read.";
        return result;
    }

    result.checked = true;
    result.changelogUrl = manifest.changelogUrl;

    // The beta channel offers betas as well, not instead: a user on the beta
    // channel whose beta is older than the current release gets the release.
    const auto candidate = (betaChannel && manifest.latestBeta > manifest.latestStable)
                             ? manifest.latestBeta
                             : manifest.latestStable;

    if (candidate > runningVersion)
    {
        result.updateAvailable = true;
        result.available = candidate;
        result.downloadUrl = manifest.getDownloadForThisPlatform();
    }

    return result;
}

//==============================================================================
juce::File Telemetry::getDiagnosticsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Diagnostics");
}

juce::File Telemetry::getTelemetryLogFile()
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m");

    return getDiagnosticsDirectory().getChildFile ("telemetry-" + stamp + ".log");
}

juce::File Telemetry::getNetworkLogFile()
{
    return getDiagnosticsDirectory().getChildFile ("network.log");
}

void Telemetry::logNetworkCall (const juce::String& destination, const juce::String& category,
                                int bytes, const juce::String& outcome) const
{
    const auto file = getNetworkLogFile();

    file.getParentDirectory().createDirectory();

    // updates-telemetry 0.3: destination, size, timestamp, payload category.
    const auto line = juce::Time::getCurrentTime().toISO8601 (true)
                        + "  " + category.paddedRight (' ', 14)
                        + juce::String (bytes).paddedLeft (' ', 8) + " bytes  "
                        + destination + "  " + outcome;

    file.appendText (line + juce::newLine);

    // Rotating, as the spec asks: a log that grows for ever is a log nobody
    // reads and a disk nobody expected to fill.
    if (file.getSize() > 1024 * 512)
    {
        const auto lines = juce::StringArray::fromLines (file.loadFileAsString());

        juce::StringArray kept;

        for (int i = juce::jmax (0, lines.size() - 2000); i < lines.size(); ++i)
            kept.add (lines[i]);

        file.replaceWithText (kept.joinIntoString (juce::newLine));
    }
}

juce::StringArray Telemetry::readNetworkLog() const
{
    return juce::StringArray::fromLines (getNetworkLogFile().loadFileAsString());
}

juce::StringArray Telemetry::readTelemetryLog() const
{
    return juce::StringArray::fromLines (getTelemetryLogFile().loadFileAsString());
}

//==============================================================================
bool Telemetry::hasPendingCrashReport() const
{
    return getPendingCrashReport() != juce::File();
}

juce::File Telemetry::getPendingCrashReport() const
{
    const auto directory = getDiagnosticsDirectory();

    if (! directory.isDirectory())
        return {};

    juce::File newest;
    juce::Time newestTime;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "crash-*.dmp"))
    {
        const auto file = entry.getFile();

        if (newest == juce::File() || file.getLastModificationTime() > newestTime)
        {
            newest = file;
            newestTime = file.getLastModificationTime();
        }
    }

    return newest;
}

juce::String Telemetry::describePendingCrashReport() const
{
    const auto dump = getPendingCrashReport();

    if (dump == juce::File())
        return {};

    /*  updates-telemetry 4: the user sees exactly what would be sent.

        The dump itself is binary, so what is shown is its size and the
        troubleshooting file beside it, which is the part that is readable and
        the part anyone would want to check before sending.
    */
    juce::String description;

    description << "Crash dump: " << dump.getFileName() << "\n"
                << "Size: " << juce::File::descriptionOfSizeInBytes (dump.getSize()) << "\n"
                << "Written: " << dump.getLastModificationTime().toString (true, true) << "\n\n";

    const auto report = dump.withFileExtension (".txt");

    if (report.existsAsFile())
    {
        description << "Accompanying troubleshooting report:\n\n"
                    << report.loadFileAsString();
    }
    else
    {
        description << "No troubleshooting report accompanies this dump.";
    }

    description << "\n\nCrash dumps contain no audio, MIDI or preset data.";

    return description;
}

bool Telemetry::uploadPendingCrashReport()
{
    if (! isCrashUploadEnabled())
        return false;

    const auto dump = getPendingCrashReport();

    if (dump == juce::File())
        return false;

    juce::MemoryBlock contents;

    if (! dump.loadFileAsData (contents))
        return false;

    const auto body = "{\"dump\":\"" + juce::Base64::toBase64 (contents.getData(), contents.getSize())
                        + "\",\"name\":\"" + dump.getFileName() + "\"}";

    if (transport == nullptr)
    {
        logNetworkCall (crashUploadUrl, "crash", (int) body.getNumBytesAsUTF8(),
                        "no transport; not sent");
        return false;
    }

    // updates-telemetry 4: three attempts, then it stays on disk.
    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        logNetworkCall (crashUploadUrl, "crash", (int) body.getNumBytesAsUTF8(),
                        "attempt " + juce::String (attempt));

        const auto result = transport->post (crashUploadUrl, body, 30000);

        if (result.succeeded)
        {
            logNetworkCall (crashUploadUrl, "crash", (int) body.getNumBytesAsUTF8(), "sent");

            dump.deleteFile();
            dump.withFileExtension (".txt").deleteFile();

            return true;
        }

        logNetworkCall (crashUploadUrl, "crash", (int) body.getNumBytesAsUTF8(),
                        "failed: " + result.error);
    }

    return false;
}

void Telemetry::discardPendingCrashReport()
{
    const auto dump = getPendingCrashReport();

    if (dump == juce::File())
        return;

    dump.deleteFile();
    dump.withFileExtension (".txt").deleteFile();
}

//==============================================================================
void Telemetry::clearLocalLogs()
{
    getNetworkLogFile().deleteFile();

    const auto directory = getDiagnosticsDirectory();

    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "telemetry-*.log"))
        entry.getFile().deleteFile();

    const juce::ScopedLock sl (recordLock);

    for (auto& queue : pending)
        queue.clear();
}

void Telemetry::turnEverythingOffAndDelete()
{
    // updates-telemetry 6: every switch off, every diagnostic file gone.
    for (int i = 0; i < (int) Category::numCategories; ++i)
        setCategoryEnabled ((Category) i, false);

    setCrashUploadEnabled (false);
    setUpdateCheckEnabled (false);
    setBetaChannelEnabled (false);

    clearLocalLogs();

    const auto directory = getDiagnosticsDirectory();

    if (directory.isDirectory())
        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*"))
            entry.getFile().deleteFile();

    saveSettings();
}

//==============================================================================
void Telemetry::setManifestUrl (const juce::String& url)
{
    // A policy's endpoint is not overridable by the user: that is the point of
    // routing an enterprise deployment through a mirror.
    if (policy.updateManifestUrl.isEmpty())
        manifestUrl = url;
}

void Telemetry::setTelemetryUrl (const juce::String& url)
{
    if (policy.telemetryUrl.isEmpty())
        telemetryUrl = url;
}

void Telemetry::setCrashUploadUrl (const juce::String& url)
{
    if (policy.crashUploadUrl.isEmpty())
        crashUploadUrl = url;
}

juce::String Telemetry::getManifestUrl() const    { return manifestUrl; }
juce::String Telemetry::getTelemetryUrl() const   { return telemetryUrl; }
juce::String Telemetry::getCrashUploadUrl() const { return crashUploadUrl; }

//==============================================================================
juce::var Telemetry::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("usageTelemetry", isCategoryEnabled (Category::usage));
    root->setProperty ("diagnosticsTelemetry", isCategoryEnabled (Category::diagnostics));
    root->setProperty ("crashUpload", isCrashUploadEnabled());
    root->setProperty ("updateCheck", isUpdateCheckEnabled());
    root->setProperty ("betaChannel", betaChannel);
    root->setProperty ("manifestUrl", manifestUrl);
    root->setProperty ("telemetryUrl", telemetryUrl);
    root->setProperty ("crashUploadUrl", crashUploadUrl);
    root->setProperty ("lastUpdateCheck", lastUpdateCheck.toMilliseconds());

    return { root };
}

void Telemetry::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    // Each setter applies the policy, so a settings file written before a policy
    // was deployed cannot re-enable what the policy now forbids.
    setCategoryEnabled (Category::usage, (bool) root->getProperty ("usageTelemetry"));
    setCategoryEnabled (Category::diagnostics, (bool) root->getProperty ("diagnosticsTelemetry"));
    setCrashUploadEnabled ((bool) root->getProperty ("crashUpload"));
    setUpdateCheckEnabled ((bool) root->getProperty ("updateCheck"));
    setBetaChannelEnabled ((bool) root->getProperty ("betaChannel"));

    if (root->hasProperty ("manifestUrl"))    setManifestUrl (root->getProperty ("manifestUrl").toString());
    if (root->hasProperty ("telemetryUrl"))   setTelemetryUrl (root->getProperty ("telemetryUrl").toString());
    if (root->hasProperty ("crashUploadUrl")) setCrashUploadUrl (root->getProperty ("crashUploadUrl").toString());

    if (root->hasProperty ("lastUpdateCheck"))
        lastUpdateCheck = juce::Time ((juce::int64) root->getProperty ("lastUpdateCheck"));
}

juce::File Telemetry::getSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("privacy.json");
}

bool Telemetry::saveSettings() const
{
    const auto file = getSettingsFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (toVar(), true));
}

bool Telemetry::loadSettings()
{
    const auto file = getSettingsFile();

    // SPEC-SWEEP ER-65: an unreadable file is kept aside and reported.
    const auto parsed = ConfigRecovery::loadObject (file, "Telemetry");

    if (parsed.getDynamicObject() == nullptr)
        return false;

    fromVar (parsed);
    return true;
}

//==============================================================================
const char* License::getStateName (State state) noexcept
{
    switch (state)
    {
        case State::unlicensed: return "Unlicensed";
        case State::activated:  return "Activated";
        case State::grace:      return "Grace period";
        case State::expired:    return "Expired";
        case State::numStates:
        default:                return "Unlicensed";
    }
}

License::License() = default;

juce::String License::getActivationProof() const
{
    if (storedKeyHash.isEmpty())
        return {};

    /*  updates-telemetry 5: revalidation sends a proof, never the key.

        The proof is a hash of the stored key hash and the activation date, so
        the server can confirm this machine activated without the key ever
        leaving it again after the first activation.
    */
    juce::SHA256 hash ((storedKeyHash + juce::String (activatedAt.toMilliseconds())).toUTF8());

    return hash.toHexString();
}

void License::updateStateFromDates()
{
    if (storedKeyHash.isEmpty())
    {
        state = State::unlicensed;
        return;
    }

    const double daysSince = (juce::Time::getCurrentTime() - lastValidated).inDays();

    if (daysSince <= (double) kRevalidationDays)
        state = State::activated;
    else if (daysSince <= (double) (kRevalidationDays + kGraceDays))
        state = State::grace;
    else
        state = State::expired;
}

int License::getDaysUntilRevalidation() const
{
    if (storedKeyHash.isEmpty())
        return 0;

    const double daysSince = (juce::Time::getCurrentTime() - lastValidated).inDays();

    return juce::jmax (0, kRevalidationDays - (int) daysSince);
}

bool License::activate (const juce::String& licenseKey, const juce::String& url)
{
    if (licenseKey.trim().isEmpty() || transport == nullptr)
        return false;

    const auto body = "{\"key\":\"" + licenseKey.trim() + "\"}";

    const auto result = transport->post (url, body, 15000);

    if (! result.succeeded)
        return false;

    // The key itself is never stored: only a hash of it, which is enough to
    // prove activation later and useless to anyone who reads the file.
    storedKeyHash = juce::SHA256 (licenseKey.trim().toUTF8()).toHexString();

    activatedAt = juce::Time::getCurrentTime();
    lastValidated = activatedAt;

    updateStateFromDates();

    return true;
}

bool License::revalidate (const juce::String& url)
{
    if (storedKeyHash.isEmpty() || transport == nullptr)
    {
        updateStateFromDates();
        return false;
    }

    const auto body = "{\"proof\":\"" + getActivationProof() + "\"}";

    const auto result = transport->post (url, body, 15000);

    if (result.succeeded)
    {
        lastValidated = juce::Time::getCurrentTime();
        updateStateFromDates();
        return true;
    }

    // A failed revalidation is not an immediate lockout: the grace period is
    // what keeps a laptop working at a gig with no network.
    updateStateFromDates();

    return false;
}

void License::deactivate()
{
    storedKeyHash.clear();
    activatedAt = juce::Time();
    lastValidated = juce::Time();
    state = State::unlicensed;

    getLicenseFile().deleteFile();
}

juce::String License::getOfflineChallenge (const juce::String& licenseKey) const
{
    if (licenseKey.trim().isEmpty())
        return {};

    // A machine-specific challenge, so the response cannot be reused elsewhere.
    const auto machine = juce::SystemStats::getUniqueDeviceID();

    juce::SHA256 hash ((licenseKey.trim() + "|" + machine).toUTF8());

    return hash.toHexString().toUpperCase().substring (0, 32);
}

bool License::applyOfflineResponse (const juce::String& response)
{
    const auto cleaned = response.trim().toUpperCase();

    if (cleaned.length() < 16)
        return false;

    storedKeyHash = cleaned;
    activatedAt = juce::Time::getCurrentTime();
    lastValidated = activatedAt;

    updateStateFromDates();

    return true;
}

juce::var License::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("keyHash", storedKeyHash);
    root->setProperty ("activatedAt", activatedAt.toMilliseconds());
    root->setProperty ("lastValidated", lastValidated.toMilliseconds());

    return { root };
}

void License::fromVar (const juce::var& state_)
{
    auto* root = state_.getDynamicObject();

    if (root == nullptr)
        return;

    storedKeyHash = root->getProperty ("keyHash").toString();
    activatedAt = juce::Time ((juce::int64) root->getProperty ("activatedAt"));
    lastValidated = juce::Time ((juce::int64) root->getProperty ("lastValidated"));

    updateStateFromDates();
}

juce::File License::getLicenseFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("license.json");
}

bool License::save() const
{
    const auto file = getLicenseFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

bool License::load()
{
    const auto file = getLicenseFile();

    // SPEC-SWEEP ER-65: an unreadable file is kept aside and reported.
    const auto parsed = ConfigRecovery::loadObject (file, "Licensing");

    if (parsed.getDynamicObject() == nullptr)
        return false;

    fromVar (parsed);
    return true;
}

} // namespace luthier
