#include "TestFramework.h"
#include "../Licence/LicenceFile.h"

extern "C"
{
#include "monocypher-ed25519.h"
}

using namespace luthier;
using namespace luthier::tests;

namespace
{
struct SignedFixture
{
    LicencePublicKey key;
    juce::String envelope;
    juce::String payload;
};

SignedFixture makeFixture()
{
    std::array<unsigned char, 32> seed {};
    for (size_t i = 0; i < seed.size(); ++i)
        seed[i] = (unsigned char) (i + 1);

    std::array<unsigned char, 64> secret {};
    std::array<unsigned char, 32> publicKey {};
    crypto_ed25519_key_pair (secret.data(), publicKey.data(), seed.data());

    const juce::String payload =
        R"({"v":1,"kid":"test-2026","product":"com.luthieraudio.luthier","edition":"pro","licenseId":"lic_test","customer":"opaque","seat":1,"seats":3,"machine":{"fp":["a","b","c","d","e"],"min":3},"issued":"2026-09-01T00:00:00Z","validUntil":"2026-11-14T00:00:00Z","revalidateAfter":"2026-10-01T00:00:00Z","majorVersions":[1],"features":["all"],"trial":false})";

    std::array<unsigned char, 64> signature {};
    crypto_ed25519_sign (signature.data(), secret.data(),
                         reinterpret_cast<const unsigned char*> (payload.toRawUTF8()),
                         (size_t) payload.getNumBytesAsUTF8());

    auto* root = new juce::DynamicObject();
    root->setProperty ("licence",
        juce::Base64::toBase64 (payload.toRawUTF8(), (size_t) payload.getNumBytesAsUTF8()));
    root->setProperty ("sig",
        juce::Base64::toBase64 (signature.data(), signature.size()));

    SignedFixture f;
    f.key.kid = "test-2026";
    f.key.bytes = publicKey;
    f.envelope = juce::JSON::toString (juce::var (root), false);
    f.payload = payload;
    return f;
}
}

LUTHIER_TEST (LicenceFile, verifiesSignatureBeforeTrustingPayload)
{
    const auto fixture = makeFixture();
    juce::String error;

    const auto verified = LicenceFile::verifyEnvelope (
        fixture.envelope, { fixture.key }, &error);

    CHECK_MSG (verified.has_value(), error);
    CHECK (verified->licenseId == "lic_test");
    CHECK (verified->fingerprintMinimum == 3);
    CHECK (verified->fingerprints.size() == 5);
}

LUTHIER_TEST (LicenceFile, rejectsTamperedPayload)
{
    auto fixture = makeFixture();

    const auto envelopeVar = juce::JSON::parse (fixture.envelope);
    auto* root = envelopeVar.getDynamicObject();
    auto tampered = fixture.payload.replace ("lic_test", "lic_hacked");
    root->setProperty ("licence",
        juce::Base64::toBase64 (tampered.toRawUTF8(), (size_t) tampered.getNumBytesAsUTF8()));

    juce::String error;
    CHECK (! LicenceFile::verifyEnvelope (juce::JSON::toString (envelopeVar, false),
                                          { fixture.key }, &error).has_value());
}

LUTHIER_TEST (LicenceFile, rejectsUnknownOrMismatchedSigningKey)
{
    const auto fixture = makeFixture();
    auto wrong = fixture.key;
    wrong.kid = "wrong-kid";

    juce::String error;
    CHECK (! LicenceFile::verifyEnvelope (fixture.envelope, { wrong }, &error).has_value());
}

LUTHIER_TEST (LicenceFile, usesThreeOfFiveFingerprintTolerance)
{
    juce::StringArray licensed { "a", "b", "c", "d", "e" };

    CHECK (! LicenceFile::fingerprintsMatch (licensed, { "a", "b", "x", "y", "z" }, 3));
    CHECK (LicenceFile::fingerprintsMatch (licensed, { "a", "b", "c", "y", "z" }, 3));
    CHECK (LicenceFile::fingerprintsMatch (licensed, { "a", "b", "c", "d", "z" }, 3));
}
