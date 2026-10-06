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
    int freeAmps = 0, freeGuitars = 0;

    for (int i = 0; i < 13; ++i)            // AmpModel::Custom (13) is never free
        freeAmps += edition::isFreeAmpIndex (i) ? 1 : 0;

    for (int i = 0; i < 25; ++i)            // GuitarType::Custom is index 24
        freeGuitars += edition::isFreeGuitarIndex (i) ? 1 : 0;

    CHECK (freeAmps == edition::freeLimits.ampModels);
    CHECK (freeGuitars == edition::freeLimits.guitars);
    CHECK (edition::isFreeAmpIndex (0));     // American Twin
    CHECK (! edition::isFreeAmpIndex (1));   // Tweed Combo
    CHECK (edition::isFreeGuitarIndex (1));  // Classic T-Style
    CHECK (! edition::isFreeGuitarIndex (0));

    CHECK (edition::isFreePedalIndex (0));
    CHECK (edition::isFreePedalIndex (14));
    CHECK (! edition::isFreePedalIndex (15));
}
