// The 0.1 beta offers exactly one reference pedal, the four-stage phaser.
// Other models stay registered so saved sessions keep loading.
#include "TestFramework.h"
#include "RefPedalUiTestSupport.h"
#include "../ReferencePedals/ReleaseCatalogue.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/BoardModel.h"
#include "../UI/ReferencePedals/PedalBrowser.h"

using namespace luthier::reference;

namespace
{
template <class T> T* findChild (juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* match = dynamic_cast<T*> (child))
            return match;
        if (auto* nested = findChild<T> (*child))
            return nested;
    }
    return nullptr;
}
}

LUTHIER_TEST (RefPedalReleaseCatalogue, offersOnlyTheRegisteredPhaser)
{
    PedalRegistry::registerAll();
    CHECK (ReleaseCatalogue::offers ("mxr-phase90-script"));
    CHECK (! ReleaseCatalogue::offers ("generic.boost"));
    CHECK (! ReleaseCatalogue::offers ("boss-ce2"));
    CHECK (! ReleaseCatalogue::offers (static_cast<const char*> (nullptr)));

    int offered = 0;
    for (const auto* d : PedalRegistry::all())
        if (ReleaseCatalogue::offers (*d))
            ++offered;
    CHECK (offered == 1);

    const auto* phaser = PedalRegistry::find ("mxr-phase90-script");
    CHECK (phaser != nullptr);
    CHECK (PedalRegistry::make ("mxr-phase90-script") != nullptr);
    // Models deferred from this release are not registered at all.
    for (const char* deferred : { "boss-ce2", "boss-ce2w", "boss-dd3t", "mxr-dynacomp", "boss-rc5" })
        CHECK (PedalRegistry::find (deferred) == nullptr);
}

LUTHIER_TEST (RefPedalReleaseCatalogue, browserListsAndAddsOnlyThePhaser)
{
    PedalRegistry::registerAll();
    BoardModel model;
    PedalBrowser browser (model);
    browser.setSize (200, 400);
    auto* list = findChild<juce::ListBox> (browser);
    auto* add = findChild<juce::TextButton> (browser);
    CHECK (list != nullptr && add != nullptr);
    if (list == nullptr || add == nullptr)
        return;
    CHECK (list->getListBoxModel() != nullptr && list->getListBoxModel()->getNumRows() == 1);
    CHECK (add->isEnabled() && (bool) add->onClick);
    if (! add->onClick)
        return;
    add->onClick();
    CHECK (model.getInstanceCount() == 1);
    CHECK (model.getOrder().size() == 1);
}

LUTHIER_TEST (RefPedalReleaseCatalogue, savedNonCatalogueModelsStillLoad)
{
    // A session saved with a generic pedal (offered before this release) must
    // still restore it: the catalogue limits adding, not loading.
    PedalRegistry::registerAll();
    BoardModel model;
    CHECK (luthier::tests::requireReferenceEdit (ctx, model, cmd::Add { "generic.boost" }));
    CHECK (model.getInstanceCount() == 1);
}
