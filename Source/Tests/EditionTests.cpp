#include "TestFramework.h"

#include "../Edition.h"
#include "../Support/Edition.h"
#include "../PluginProcessor.h"

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

    // editions.md 2.2: 15 of 22 pedals plus "None"; Flanger (14) is Pro, Tremolo (15) Free.
    int freePedals = 0;

    for (int i = 1; i < 24; ++i)
        freePedals += edition::isFreePedalIndex (i) ? 1 : 0;

    CHECK (freePedals == edition::freeLimits.pedalTypes);
    CHECK (edition::isFreePedalIndex (0));
    CHECK (! edition::isFreePedalIndex (14));
    CHECK (edition::isFreePedalIndex (15));
    CHECK (! edition::isFreePedalIndex (16));   // Rotary

    // 5.1.3: every Pro choice maps to a Free one of the same family; Free choices map to themselves.
    for (int i = 0; i < 25; ++i)
        CHECK (edition::isPro ? edition::effectiveGuitarIndex (i) == i
                              : (edition::isFreeGuitarIndex (edition::effectiveGuitarIndex (i)) || i == 24));

    for (int i = 0; i < 14; ++i)
        CHECK (edition::isPro ? edition::effectiveAmpIndex (i) == i
                              : (edition::isFreeAmpIndex (edition::effectiveAmpIndex (i)) || i == 13));
}

/*  editions.md 5.1.3 / 5.1.6: Free plays a Pro guitar, amp or pedal as the
    nearest Free one (a Pro pedal bypassed), and the stored values stay as the
    preset wrote them. Pro plays exactly what is stored. */
LUTHIER_TEST (Editions, freePlaysTheNearestFreeModelAndKeepsTheStoredValue)
{
    LuthierAudioProcessor p;
    p.prepareToPlay (48000.0, 256);

    auto set = [&p] (const juce::String& id, int choice)
    {
        auto* prm = p.getState().getParameter (id);
        jassert (prm != nullptr);
        if (prm != nullptr) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) choice));
    };
    auto stored = [&p] (const juce::String& id)
    {
        auto* prm = p.getState().getParameter (id);
        return prm != nullptr ? juce::roundToInt (prm->convertFrom0to1 (prm->getValue())) : -1;
    };

    set (ParamIDs::guitarType, 0);                 // Stratocaster: Pro
    set (ParamIDs::ampModel, 4);                   // Plexi: Pro
    set (ParamIDs::slotType (false, 0), 5);        // Octaver: Pro
    set (ParamIDs::slotType (true, 0), 15);        // Tremolo: Free
    p.getParameterBridge().applyAllNow();

    CHECK ((int) p.getEngine().getGuitarType() == (edition::isPro ? 0 : 1));
    CHECK ((int) p.getEngine().getAmpEngine().getModel() == (edition::isPro ? 4 : 5));
    CHECK ((int) p.getEngine().getPreEffects().getSlotType (0) == (edition::isPro ? 5 : 0));
    CHECK ((int) p.getEngine().getPostEffects().getSlotType (0) == 15);

    CHECK (stored (ParamIDs::guitarType) == 0);
    CHECK (stored (ParamIDs::ampModel) == 4);
    CHECK (stored (ParamIDs::slotType (false, 0)) == 5);

    // A second full apply (a prepare, a preset's structural pass) loads nothing again.
    p.getParameterBridge().applyAllNow();
    CHECK ((int) p.getEngine().getGuitarType() == (edition::isPro ? 0 : 1));
    CHECK (stored (ParamIDs::guitarType) == 0);
}
