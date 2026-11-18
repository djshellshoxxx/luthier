#include "TestFramework.h"
#include "TestSelection.h"
#include <string>
#include <vector>

using namespace luthier::tests;

LUTHIER_TEST (TestRunner, skipExcludesTheNamedSuite)
{
    TestSelection selection ({ "--skip=Combo" });
    CHECK (! selection.includes ("Combo", "renderIsDeterministicAfterReset"));
    CHECK (selection.includes ("Presets", "roundTrip"));
    CHECK (selection.includes ("Other", "usesComboInput"));
}

LUTHIER_TEST (TestRunner, exclusionsOverridePositiveFilters)
{
    TestSelection selection ({ "render", "--skip=cOmBo" });
    CHECK (! selection.includes ("Combo", "render"));
    CHECK (selection.includes ("Engine", "render"));
    CHECK (! selection.includes ("Presets", "roundTrip"));
}

LUTHIER_TEST (TestRunner, repeatedExclusionsAndListingUseTheSameSelection)
{
    TestSelection selection ({ "--list", "--skip=Combo", "--skip=Presets" });
    CHECK (selection.listOnly);
    CHECK (! selection.includes ("Combo", "render"));
    CHECK (! selection.includes ("Presets", "roundTrip"));
    CHECK (selection.includes ("Engine", "render"));
}

LUTHIER_TEST (TestRunner, existingSubstringFiltersRemainCaseInsensitive)
{
    TestSelection selection ({ "-l", "TuNiNg", "roundTrip" });
    CHECK (selection.listOnly);
    CHECK (selection.includes ("TuningStability", "settles"));
    CHECK (selection.includes ("Presets", "aRoundTrip"));
    CHECK (! selection.includes ("Engine", "render"));
}

LUTHIER_TEST (TestRunner, shardsPartitionTheSelectionExactly)
{
    constexpr int count = 4;
    std::vector<int> owners (23, 0);
    for (int index = 1; index <= count; ++index)
    {
        TestSelection selection ({ "--shard=" + std::to_string (index) + "/" + std::to_string (count) });
        CHECK (selection.valid);
        for (int ordinal = 0; ordinal < (int) owners.size(); ++ordinal)
            if (selection.inShard (ordinal))
                ++owners[(size_t) ordinal];
    }
    for (auto owner : owners)
        CHECK (owner == 1);
}

LUTHIER_TEST (TestRunner, malformedShardsAreRejected)
{
    for (const char* spec : { "--shard=0/4", "--shard=5/4", "--shard=1/0", "--shard=x/4", "--shard=2", "--shard=-1/4", "--shard=1/4/2" })
        CHECK (! TestSelection ({ spec }).valid);
    TestSelection single ({ "--shard=1/1" });
    CHECK (single.valid);
    CHECK (single.inShard (0) && single.inShard (7));
}
