#include "TestFramework.h"
#include "../Licence/LicenceFile.h"

extern "C"
{
#include "monocypher-ed25519.h"
#include "monocypher.h"
}

using namespace luthier;
using namespace luthier::tests;

// Guard the vendored Poly1305 arithmetic used by the crypto library. Expected
// tags come from RFC 8439 section 2.5.2 and an independent integer-mod-p model.
LUTHIER_TEST (LicenceFile, poly1305KnownAnswerAndHighLimbVectors)
{
    const unsigned char rfcKey[32] = {
        0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
        0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
        0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
        0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b
    };
    const char message[] = "Cryptographic Forum Research Group";
    const unsigned char rfcTag[16] = {
        0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
        0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9
    };
    unsigned char tag[16] {};
    crypto_poly1305 (tag, reinterpret_cast<const unsigned char*> (message),
                     sizeof (message) - 1, rfcKey);
    CHECK (std::equal (std::begin (tag), std::end (tag), std::begin (rfcTag)));

    std::array<unsigned char, 32> key;
    std::array<unsigned char, 1024> blocks;
    key.fill (0xff);
    blocks.fill (0xff);
    const unsigned char highLimbTag[16] = {
        0x25, 0xd4, 0x92, 0x6a, 0x53, 0xbb, 0x48, 0x0d,
        0xa2, 0x28, 0xec, 0x61, 0xe0, 0xa3, 0x1a, 0x38
    };
    crypto_poly1305 (tag, blocks.data(), blocks.size(), key.data());
    CHECK (std::equal (std::begin (tag), std::end (tag), std::begin (highLimbTag)));

    crypto_poly1305_ctx state;
    crypto_poly1305_init (&state, key.data());
    for (size_t offset = 0; offset < blocks.size(); ++offset)
        crypto_poly1305_update (&state, blocks.data() + offset, 1);
    crypto_poly1305_final (&state, tag);
    CHECK (std::equal (std::begin (tag), std::end (tag), std::begin (highLimbTag)));
}

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
