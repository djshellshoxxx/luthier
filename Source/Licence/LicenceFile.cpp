#include "LicenceFile.h"

extern "C"
{
#include "monocypher-ed25519.h"
}

namespace luthier
{

namespace
{
void fail (juce::String* out, const juce::String& message)
{
    if (out != nullptr)
        *out = message;
}

juce::Time parseIso (const juce::var& value)
{
    const auto text = value.toString();
    return text.isNotEmpty() ? juce::Time::fromISO8601 (text) : juce::Time();
}
}

bool LicenceFile::decodeBase64 (const juce::String& text, juce::MemoryBlock& result)
{
    result.reset();
    juce::MemoryOutputStream stream (result, false);
    return juce::Base64::convertFromBase64 (stream, text);
}

std::optional<VerifiedLicence> LicenceFile::verifyEnvelope (
    const juce::String& envelopeJson,
    const std::vector<LicencePublicKey>& keys,
    juce::String* error)
{
    if (error != nullptr)
        error->clear();

    const auto envelope = juce::JSON::parse (envelopeJson);
    const auto* root = envelope.getDynamicObject();

    if (root == nullptr)
    {
        fail (error, "Licence envelope is not a JSON object.");
        return std::nullopt;
    }

    juce::MemoryBlock payloadBytes;
    juce::MemoryBlock signature;

    if (! decodeBase64 (root->getProperty ("licence").toString(), payloadBytes)
        || ! decodeBase64 (root->getProperty ("sig").toString(), signature)
        || payloadBytes.getSize() == 0
        || signature.getSize() != 64)
    {
        fail (error, "Licence envelope has invalid base64 payload or signature.");
        return std::nullopt;
    }

    const LicencePublicKey* verifiedWith = nullptr;

    for (const auto& key : keys)
    {
        const auto ok = crypto_ed25519_check (
            static_cast<const unsigned char*> (signature.getData()),
            key.bytes.data(),
            static_cast<const unsigned char*> (payloadBytes.getData()),
            payloadBytes.getSize());

        if (ok == 0)
        {
            verifiedWith = &key;
            break;
        }
    }

    if (verifiedWith == nullptr)
    {
        fail (error, "Licence signature is invalid.");
        return std::nullopt;
    }

    // Parse only after cryptographic verification of the exact bytes above.
    const juce::String payloadText (
        static_cast<const char*> (payloadBytes.getData()),
        payloadBytes.getSize());

    const auto payloadVar = juce::JSON::parse (payloadText);
    const auto* payload = payloadVar.getDynamicObject();

    if (payload == nullptr)
    {
        fail (error, "Signed licence payload is not valid JSON.");
        return std::nullopt;
    }

    VerifiedLicence result;
    result.version = (int) payload->getProperty ("v");
    result.kid = payload->getProperty ("kid").toString();
    result.product = payload->getProperty ("product").toString();
    result.edition = payload->getProperty ("edition").toString();
    result.licenseId = payload->getProperty ("licenseId").toString();
    result.customer = payload->getProperty ("customer").toString();
    result.seat = (int) payload->getProperty ("seat");
    result.seats = (int) payload->getProperty ("seats");
    result.issued = parseIso (payload->getProperty ("issued"));
    result.validUntil = parseIso (payload->getProperty ("validUntil"));
    result.revalidateAfter = parseIso (payload->getProperty ("revalidateAfter"));
    result.trial = (bool) payload->getProperty ("trial");
    result.payload = payloadVar;

    if (result.kid != verifiedWith->kid)
    {
        fail (error, "Licence key identifier does not match its signature key.");
        return std::nullopt;
    }

    if (result.version != 1
        || result.product != "com.luthieraudio.luthier"
        || ! result.edition.equalsIgnoreCase ("pro")
        || result.licenseId.isEmpty()
        || result.issued.toMilliseconds() <= 0
        || result.validUntil <= result.issued)
    {
        fail (error, "Signed licence payload has invalid required fields.");
        return std::nullopt;
    }

    if (const auto machine = payload->getProperty ("machine"); auto* machineObj = machine.getDynamicObject())
    {
        result.fingerprintMinimum = juce::jlimit (1, 5, (int) machineObj->getProperty ("min"));

        if (const auto* values = machineObj->getProperty ("fp").getArray())
            for (const auto& value : *values)
                if (value.toString().isNotEmpty())
                    result.fingerprints.addIfNotAlreadyThere (value.toString());
    }

    if (result.fingerprints.isEmpty())
    {
        fail (error, "Signed licence has no machine fingerprints.");
        return std::nullopt;
    }

    return result;
}

bool LicenceFile::fingerprintsMatch (const juce::StringArray& licensed,
                                     const juce::StringArray& local,
                                     int minimumMatches) noexcept
{
    int matches = 0;
    const int needed = juce::jlimit (1, 5, minimumMatches);

    for (const auto& hash : licensed)
        if (local.contains (hash) && ++matches >= needed)
            return true;

    return false;
}

} // namespace luthier
