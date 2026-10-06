#include "TestFramework.h"
#include "../Licence/OfflineActivation.h"
#include "../Licence/DeactivationQueue.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
struct QueueTransport : Transport
{
    int calls = 0;
    int failAfter = -1;

    Result get (const juce::String&, int) override { return {}; }

    Result post (const juce::String&, const juce::String&, int) override
    {
        ++calls;
        if (failAfter >= 0 && calls > failAfter)
            return { false, 503, {}, "offline" };
        return { true, 200, R"({"ok":true})", {} };
    }
};
}

LUTHIER_TEST (OfflineActivation, base32RoundTripsArbitraryEnvelopeBytes)
{
    const juce::String source = R"({"licence":"abc","sig":"def"})";
    const auto encoded = OfflineActivation::encodeBase32 (
        source.toRawUTF8(), (size_t) source.getNumBytesAsUTF8());

    juce::MemoryBlock decoded;
    CHECK (OfflineActivation::decodeBase32 (encoded, decoded));

    const juce::String restored (
        static_cast<const char*> (decoded.getData()), decoded.getSize());
    CHECK (restored == source);
}

LUTHIER_TEST (OfflineActivation, challengeCarriesFiveHashedFingerprints)
{
    const auto challenge = OfflineActivation::createChallenge ("LTHR-TEST", "1.0.0");
    CHECK (challenge.text.isNotEmpty());
    CHECK (challenge.nonce.length() == 16);

    juce::MemoryBlock decoded;
    CHECK (OfflineActivation::decodeBase32 (challenge.text, decoded));

    const juce::String json (
        static_cast<const char*> (decoded.getData()), decoded.getSize());
    const auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();

    CHECK (root != nullptr);
    if (root == nullptr)
        return;
    CHECK (root->getProperty ("key").toString() == "LTHR-TEST");
    CHECK (root->getProperty ("product").toString() == "com.luthieraudio.luthier");
    CHECK (root->getProperty ("fp").getArray() != nullptr);
    if (root->getProperty ("fp").getArray() != nullptr)
        CHECK (root->getProperty ("fp").getArray()->size() == 5);
}

LUTHIER_TEST (DeactivationQueue, keepsFailedSeatReleasesAndDrainsLater)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("luthier-deactivation-test", {}, true);
    dir.createDirectory();

    DeactivationQueue queue (dir.getChildFile ("queue.json"));
    CHECK (queue.enqueue ("lic_one", { "a", "b", "c", "d", "e" }));
    CHECK (queue.enqueue ("lic_two", { "a", "b", "c", "d", "e" }));
    CHECK (queue.size() == 2);

    QueueTransport flaky;
    flaky.failAfter = 1;
    CHECK (queue.drain (flaky, "https://example.invalid/v1/deactivate") == 1);
    CHECK (queue.size() == 1);

    QueueTransport online;
    CHECK (queue.drain (online, "https://example.invalid/v1/deactivate") == 1);
    CHECK (queue.size() == 0);

    dir.deleteRecursively();
}
