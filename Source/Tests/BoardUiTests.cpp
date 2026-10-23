// OWNER: WP17
#include "TestFramework.h"
#include "../UI/ReferencePedals/BoardUiState.h"
#include "../UI/ReferencePedals/BoardCanvas.h"
#include "../UI/ReferencePedals/BoardRoot.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalRegistry.h"
using namespace luthier::reference;

LUTHIER_TEST (RefPedalUi, geometryKeepsTitlebarReachable)
{
    const juce::Rectangle<int> work (0, 0, 800, 600);
    const auto r = BoardUiState::clampWindow ({-4000, -4000, 9000, 6000}, work);
    CHECK (work.contains (r));
    CHECK (r.getWidth() <= 768);
    CHECK (r.getHeight() <= 568);
}
LUTHIER_TEST (RefPedalUi, viewUsesModelOrderAndKeepsIdentity)
{
    PedalRegistry::registerAll();
    BoardModel model;
    CHECK (model.apply (cmd::Add { "generic.boost" }));
    CHECK (model.apply (cmd::Add { "generic.overdrive" }));
    BoardCanvas canvas (model);
    const auto order = model.getOrder();
    auto* first = canvas.viewFor (order[0]);
    CHECK (first != nullptr);
    CHECK (model.apply (cmd::Move {order[0], 1}));
    CHECK (canvas.viewFor (order[0]) == first);
    CHECK (canvas.viewFor (order[1])->getX() < first->getX());
    CHECK (model.undo());
    CHECK (first->getX() < canvas.viewFor (order[1])->getX());
}
LUTHIER_TEST (RefPedalUi, rejectedEditIsVisible)
{
    BoardModel model;
    BoardRoot root (model);
    CHECK (!model.apply (cmd::Move {InstanceId::null(),999}));
    CHECK (root.statusText().isNotEmpty());
}
