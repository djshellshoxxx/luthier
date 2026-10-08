#include "LicenceClient.h"

namespace luthier
{

LicenceClient::LicenceClient (Transport& transport_,
                              LicenceStore store_,
                              std::vector<LicencePublicKey> trustedKeys)
    : transport (transport_), store (std::move (store_)), keys (std::move (trustedKeys))
{
}

juce::String LicenceClient::jsonRequest (const juce::String& key,
                                         const juce::StringArray& fingerprints,
                                         bool trial)
{
    auto* root = new juce::DynamicObject();

    if (key.isNotEmpty())
        root->setProperty ("key", key);

    root->setProperty ("trial", trial);

    juce::Array<juce::var> fp;
    for (const auto& item : fingerprints)
        fp.add (item);
    root->setProperty ("fingerprints", fp);

    return juce::JSON::toString (juce::var (root), false);
}

LicenceClient::Result LicenceClient::activate (const juce::String& key,
                                               const juce::String& endpoint,
                                               bool requestTrial)
{
    if ((! requestTrial && key.trim().isEmpty()) || endpoint.trim().isEmpty())
        return { ResultCode::invalidInput, "A licence key or trial request and endpoint are required.", {} };

    const auto fp = MachineFingerprint::collect();
    const auto response = transport.post (endpoint,
                                          jsonRequest (key.trim(), fp, requestTrial),
                                          15000);

    if (! response.succeeded)
    {
        const auto message = response.body.isNotEmpty() ? response.body : response.error;
        return { response.statusCode >= 400 ? ResultCode::rejected : ResultCode::networkError,
                 message, {} };
    }

    return acceptServerEnvelope (response.body);
}

LicenceClient::Result LicenceClient::revalidate (const juce::String& endpoint)
{
    if (endpoint.trim().isEmpty())
        return { ResultCode::invalidInput, "A revalidation endpoint is required.", {} };

    juce::String error;
    const auto current = currentVerified (&error);

    if (! current.has_value())
        return { ResultCode::invalidSignature, error, {} };

    auto* root = new juce::DynamicObject();
    root->setProperty ("licenseId", current->licenseId);

    juce::Array<juce::var> fp;
    for (const auto& value : MachineFingerprint::collect())
        fp.add (value);
    root->setProperty ("fingerprints", fp);

    const auto response = transport.post (endpoint,
                                          juce::JSON::toString (juce::var (root), false),
                                          15000);

    if (! response.succeeded)
        return { response.statusCode >= 400 ? ResultCode::rejected : ResultCode::networkError,
                 response.body.isNotEmpty() ? response.body : response.error, {} };

    return acceptServerEnvelope (response.body);
}

LicenceClient::Result LicenceClient::acceptServerEnvelope (const juce::String& body)
{
    juce::String error;
    const auto verified = LicenceFile::verifyEnvelope (body, keys, &error);

    if (! verified.has_value())
        return { ResultCode::invalidSignature, error, {} };

    const auto local = MachineFingerprint::collect();

    if (! LicenceFile::fingerprintsMatch (verified->fingerprints, local,
                                          verified->fingerprintMinimum))
        return { ResultCode::machineMismatch, "The signed licence is for a different machine.", {} };

    if (! store.saveEnvelope (body)
        || ! store.updateLastSeen (juce::Time::getCurrentTime()))
        return { ResultCode::storageError, "The verified licence could not be stored.", {} };

    return { ResultCode::ok, {}, verified };
}

void LicenceClient::deactivateLocal() noexcept
{
    // Local entitlement disappears immediately. Server-side seat release may be
    // queued separately, but a network failure must never leave this machine
    // pretending to be activated.
    store.removeEnvelope();
}

std::optional<VerifiedLicence> LicenceClient::currentVerified (juce::String* error) const
{
    const auto envelope = store.loadEnvelope();

    if (! envelope.has_value())
    {
        if (error != nullptr)
            *error = "No licence is installed.";
        return std::nullopt;
    }

    return LicenceFile::verifyEnvelope (*envelope, keys, error);
}

LicenceStatus LicenceClient::currentStatus (juce::Time now) const
{
    const auto verified = currentVerified();

    if (! verified.has_value())
        return LicenceStatus::unlicensed;

    LicenceTiming timing;
    timing.issued = verified->issued;
    timing.revalidateAfter = verified->revalidateAfter;
    timing.validUntil = verified->validUntil;
    timing.lastSeen = store.loadLastSeen();
    timing.signatureValid = true;
    timing.fingerprintMatches = LicenceFile::fingerprintsMatch (
        verified->fingerprints, MachineFingerprint::collect(), verified->fingerprintMinimum);
    timing.trial = verified->trial;

    return LicenceState::evaluate (timing, now);
}

} // namespace luthier
