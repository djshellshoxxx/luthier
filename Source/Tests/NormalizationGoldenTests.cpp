/*  output-normalization.md ON-02, the golden half: with normalization off,
    the main output of every covered factory preset x guitar type x phrase
    hashes exactly as it did in the build before the feature landed.

    Tests/Golden/NormalizationOffHashes.json was generated from the
    integration HEAD before any normalization code touched the audio path.
    Regenerate it (after a merge that legitimately changes audio) with

        scripts/regen_normalization_hashes.sh          (default grid)
        LUTHIER_SLOW_TESTS=1 scripts/regen_normalization_hashes.sh   (full grid)

    which runs this suite with LUTHIER_NORMALIZATION_GOLDEN_WRITE=1.

    The same-build A/B half of ON-02 (normalizer compiled in but disabled
    versus the stage bypassed outright) is in NormalizationTests.cpp; it holds
    on any toolchain, where a hash only holds on the one that wrote it.
*/

#include "NormalizationTestUtil.h"

using namespace luthier;
using namespace luthier::normtest;

namespace
{
    bool writeMode()
    {
        return juce::SystemStats::getEnvironmentVariable ("LUTHIER_NORMALIZATION_GOLDEN_WRITE", {}).getIntValue() > 0;
    }
}

LUTHIER_TEST (NormalizationGolden, ON02_OffPathMatchesGoldenHashes)
{
    const bool full = slowTestsEnabled();
    const auto grid = goldenGrid (full);
    const auto file = goldenFile();

    if (writeMode())
    {
        // Two renders of the first combination first: a hash is only a useful
        // golden if the engine is deterministic from a fresh instance.
        const auto a = sha256 (renderCombo (grid.front()));
        const auto b = sha256 (renderCombo (grid.front()));
        CHECK_MSG (a == b, "the engine is not deterministic from a fresh instance; hashes would be useless");

        if (a != b)
            return;

        auto* hashes = new juce::DynamicObject();

        // Keep entries a partial run did not cover (a default run keeps the
        // full grid's extra entries).
        const auto oldJson = juce::JSON::parse (file);
        const auto oldHashes = oldJson.getProperty ("hashes", {});

        if (auto* old = oldHashes.getDynamicObject())
            for (const auto& prop : old->getProperties())
                hashes->setProperty (prop.name, prop.value);

        int done = 0;

        for (const auto& combo : grid)
        {
            const auto samples = renderCombo (combo);
            CHECK_MSG (! samples.empty(), combo.key() + " failed to load");
            hashes->setProperty (combo.key(), sha256 (samples));

            if (++done % 20 == 0)
                std::cout << "    hashed " << done << " / " << grid.size() << std::endl;
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("description", "SHA-256 of the float main output (interleaved L/R) with output "
                                          "normalization off: output-normalization.md ON-02. Regenerate with "
                                          "scripts/regen_normalization_hashes.sh.");
        root->setProperty ("toolchain", toolchainFingerprint());
        root->setProperty ("sampleRate", kSr);
        root->setProperty ("blockSize", kBlock);
        root->setProperty ("generated", juce::Time::getCurrentTime().toISO8601 (true));
        root->setProperty ("hashes", juce::var (hashes));

        file.getParentDirectory().createDirectory();
        CHECK (file.replaceWithText (juce::JSON::toString (juce::var (root), false)));
        std::cout << "    wrote " << file.getFullPathName() << std::endl;
        return;
    }

    const auto json = juce::JSON::parse (file);
    CHECK_MSG (json.isObject(), "missing golden file " + file.getFullPathName());

    if (! json.isObject())
        return;

    if (json.getProperty ("toolchain", {}).toString() != toolchainFingerprint())
    {
        // Bit-exact float output is only defined per toolchain; the A/B test
        // in NormalizationTests.cpp still covers this build.
        std::cout << "    golden hashes are for " << json.getProperty ("toolchain", {}).toString()
                  << ", this build is " << toolchainFingerprint() << ": skipped" << std::endl;
        return;
    }

    auto* hashes = json.getProperty ("hashes", {}).getDynamicObject();
    CHECK (hashes != nullptr);

    if (hashes == nullptr)
        return;

    int compared = 0, mismatched = 0;

    for (const auto& combo : grid)
    {
        const auto key = combo.key();

        if (! hashes->hasProperty (key))
            continue;

        const auto actual = sha256 (renderCombo (combo));
        ++compared;

        if (actual != hashes->getProperty (key).toString())
        {
            ++mismatched;
            CHECK_MSG (false, key + " differs from its golden hash with normalization off");
        }
    }

    std::cout << "    compared " << compared << " combinations, " << mismatched << " mismatched" << std::endl;
    CHECK (compared > 0);
}
