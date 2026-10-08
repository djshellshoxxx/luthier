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
