#include "TestFramework.h"
#include "../Licence/LicenceStore.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (LicenceStore, keepsSignedEnvelopeSeparateFromClockMetadata)
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("luthier-licence-test", {}, true);

    LicenceStore store (root);
    const juce::String envelope = R"({"licence":"YWJj","sig":"ZGVm"})";

    CHECK (store.saveEnvelope (envelope));
    CHECK (store.loadEnvelope().has_value());
    CHECK (*store.loadEnvelope() == envelope);

    const auto first = juce::Time (2026, 9, 1, 0, 0);
    const auto older = juce::Time (2026, 8, 1, 0, 0);
    CHECK (store.updateLastSeen (first));
    CHECK (store.updateLastSeen (older));
    CHECK (store.loadLastSeen() == first);

    CHECK (store.removeEnvelope());
    CHECK (! store.loadEnvelope().has_value());
    root.deleteRecursively();
}
