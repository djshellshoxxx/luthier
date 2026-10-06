#include "TestFramework.h"

#include "../Edition.h"
#include "../Support/Edition.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (Editions, compileTimeIdentityMatchesBuild)
{
#if LUTHIER_PRO
    CHECK (edition::isPro);
    CHECK (edition::has (edition::Feature::workshop));
    CHECK (edition::has (edition::Feature::licensing));
    CHECK (edition::limits.rackSlotsPre == 8);
    CHECK (edition::limits.snapshotsPerBank == 8);
#else
    CHECK (! edition::isPro);
    CHECK (! edition::has (edition::Feature::workshop));
    CHECK (! edition::has (edition::Feature::licensing));
    CHECK (edition::limits.guitars == 6);
    CHECK (edition::limits.ampModels == 7);
    CHECK (edition::limits.pedalTypes == 15);
    CHECK (edition::limits.rackSlotsPre == 4);
    CHECK (edition::limits.snapshotBanks == 1);
    CHECK (edition::limits.snapshotsPerBank == 4);
    CHECK (edition::limits.looperLayers == 1);
    CHECK (edition::limits.looperSeconds == 60);
    CHECK (edition::limits.audioExportSeconds == 300);
#endif
}

LUTHIER_TEST (Editions, runtimeSeamReturnsToCompileTimeIdentity)
{
    Editions::set (Edition::free);
    CHECK (Editions::current() == Edition::free);

    Editions::set (Edition::pro);
    CHECK (Editions::current() == Edition::pro);

    Editions::clearTestOverride();
    CHECK (Editions::isPro() == edition::isPro);
}

LUTHIER_TEST (Editions, freeCatalogTablesMatchSpecification)
{
    CHECK (edition::isFreeAmpIndex (0));
    CHECK (edition::isFreeAmpIndex (2));
    CHECK (! edition::isFreeAmpIndex (1));

    CHECK (edition::isFreePedalIndex (0));
    CHECK (edition::isFreePedalIndex (14));
    CHECK (! edition::isFreePedalIndex (15));
}
