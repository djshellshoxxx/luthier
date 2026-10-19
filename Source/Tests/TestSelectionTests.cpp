#include "TestFramework.h"
#include "TestSelection.h"

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

LUTHIER_TEST (TestRunner, shardingCoversEachTestExactlyOnce)
{
    std::vector<TestSelection> shards;
    for (int i = 1; i <= 4; ++i)
        shards.emplace_back (std::vector<std::string> { "--shard=" + std::to_string (i) + "/4" });
    for (int i = 0; i < 500; ++i)
    {
        const auto suite = "Suite" + std::to_string (i % 31);
        const auto name = "Case" + std::to_string (i);
        int included = 0;
        for (const auto& shard : shards)
            if (shard.includes (suite, name))
                ++included;
        CHECK (included == 1);
    }
}

LUTHIER_TEST (TestRunner, shardingHonoursFiltersAndIsCaseInsensitive)
{
    TestSelection selection ({ "--list", "--shard=2/3", "PRESET", "--skip=Combo" });
    CHECK (selection.listOnly);
    CHECK (selection.includes ("Presets", "load") == selection.includes ("PRESETS", "LOAD"));
    CHECK (! selection.includes ("Combo", "preset"));
    CHECK (! selection.includes ("Other", "load"));
}

LUTHIER_TEST (TestRunner, shardingRejectsInvalidRanges)
{
    for (const std::string& invalid : { "--shard=0/2", "--shard=3/2", "--shard=x/2", "--shard=1/0", "--shard=1/1garbage", "--shard=1" })
    {
        bool rejected = false;
        try { TestSelection selection ({ invalid }); }
        catch (const std::invalid_argument&) { rejected = true; }
        CHECK (rejected);
    }
}
