#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <optional>
#include <vector>

namespace luthier
{

struct LicencePublicKey
{
    juce::String kid;
    std::array<unsigned char, 32> bytes {};
};

struct VerifiedLicence
{
    int version = 0;
    juce::String kid;
    juce::String product;
    juce::String edition;
    juce::String licenseId;
    juce::String customer;
    int seat = 0;
    int seats = 0;
    juce::StringArray fingerprints;
    int fingerprintMinimum = 3;
    juce::Time issued;
    juce::Time validUntil;
    juce::Time revalidateAfter;
    bool trial = false;
    juce::var payload;
};

class LicenceFile
{
public:
    /** Verifies the signature over the exact base64-decoded payload bytes before
        parsing or trusting any payload field. Every supplied key is attempted,
        then the signed kid must identify the key that actually verified it. */
    static std::optional<VerifiedLicence> verifyEnvelope (
        const juce::String& envelopeJson,
        const std::vector<LicencePublicKey>& keys,
        juce::String* error = nullptr);

    static bool fingerprintsMatch (const juce::StringArray& licensed,
                                   const juce::StringArray& local,
                                   int minimumMatches) noexcept;

private:
    static bool decodeBase64 (const juce::String&, juce::MemoryBlock&);
};

} // namespace luthier
