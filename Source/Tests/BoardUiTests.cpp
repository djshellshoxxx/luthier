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

LUTHIER_TEST (RefPedalUi, stateRoundTripRetainsUnknownFields)
{
    auto* root=new juce::DynamicObject();root->setProperty("vendor", "keep-me");
    auto* pop=new juce::DynamicObject();pop->setProperty("vendorWindow",73);root->setProperty("popout",juce::var(pop));
    const juce::var original(root);
    auto s=BoardUiState::read(original);s.windowMode="drawer";s.zoom=1.4f;s.scrollX=99;
    auto encoded=s.write(original);auto decoded=BoardUiState::read(encoded);
    CHECK(encoded["vendor"].toString()=="keep-me");CHECK((int)encoded["popout"]["vendorWindow"]==73);
    CHECK(decoded.windowMode=="drawer");CHECK_NEAR(decoded.zoom,1.4,1e-6);CHECK(decoded.scrollX==99);
}
LUTHIER_TEST (RefPedalUi, keyboardReorderUsesOneUndoStep)
{
    BoardModel model;CHECK(model.apply(cmd::Add{"prototype.ts808"}));CHECK(model.apply(cmd::Add{"prototype.ds1"}));
    BoardCanvas canvas(model);const auto before=model.getOrder();
    CHECK(canvas.viewFor(before[0])->keyPressed({juce::KeyPress::rightKey,juce::ModifierKeys(juce::ModifierKeys::altModifier),0}));
    CHECK(model.getOrder()[1]==before[0]);CHECK(model.undo());CHECK(model.getOrder()==before);
}

LUTHIER_TEST (RefPedalUi, focusedPedalSelectsMidiTargetWithoutUndoStep)
{
    PedalRegistry::registerAll();BoardModel model;
    CHECK(model.apply(cmd::Add{"prototype.ts808"}));CHECK(model.apply(cmd::Add{"prototype.ds1"}));
    BoardRoot root(model);const auto order=model.getOrder();
    root.boardCanvas().viewFor(order[1])->focusGained(juce::Component::focusChangedDirectly);
    CHECK(model.getBoardSettings().midi["selectedInstance"].toString()==order[1].toString());
    CHECK(model.undo());CHECK(model.getInstanceCount()==1);
}

LUTHIER_TEST (RefPedalUi, rejectedEditRestoresNativeControlState)
{
    PedalRegistry::registerAll();BoardModel model;CHECK(model.apply(cmd::Add{"prototype.ts808"}));
    BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);juce::TextButton* bypass=nullptr;
    for(auto* child:view->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getTitle().startsWith("Bypass "))bypass=button;
    CHECK(bypass!=nullptr);if(!bypass)return;
    bypass->setToggleState(false,juce::dontSendNotification);CHECK(!bypass->getToggleState());
    CHECK(!model.apply(cmd::Move{InstanceId::null(),999}));CHECK(bypass->getToggleState()==model.getInstance(model.getOrder()[0])->enabled);
}
