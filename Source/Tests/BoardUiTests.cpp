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
LUTHIER_TEST (RefPedalUi, canvasUsesPhysicalFaceSize)
{
    PedalRegistry::registerAll();BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);CHECK(view!=nullptr);if(!view)return;
    CHECK_NEAR(view->getWidth(),54*1.6,.5);CHECK_NEAR(view->getHeight(),90*1.6,.5);
}
LUTHIER_TEST (RefPedalUi, inspectorIsNonmodalAndClosesWithItsPedal)
{
    PedalRegistry::registerAll();BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardRoot root(model);root.setSize(1100,520);auto* view=root.boardCanvas().viewFor(model.getOrder()[0]);
    view->showInspector();CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);
    int callouts=0;for(auto* child:root.getChildren())if(dynamic_cast<juce::CallOutBox*>(child))++callouts;CHECK(callouts==1);
    CHECK(model.apply(cmd::Remove{model.getOrder()[0]}));
    callouts=0;for(auto* child:root.getChildren())if(dynamic_cast<juce::CallOutBox*>(child))++callouts;CHECK(callouts==0);
}

#include "../UI/ReferencePedals/DragController.h"
LUTHIER_TEST(RefPedalUi, dragDwellThrottleFinalCommitAndUndo)
{
    PedalRegistry::registerAll();BoardModel model;
    for(int i=0;i<4;++i)CHECK(model.apply(cmd::Add{"generic.boost"}));
    const auto before=model.getOrder();DragController drag(model);CHECK(drag.begin(before[0],0));
    drag.update({2,0,Placement::beforeExistingPre},0);drag.update({2,0,Placement::beforeExistingPre},79);CHECK(model.getOrder()==before);
    drag.update({2,0,Placement::beforeExistingPre},80);CHECK(model.getOrder()[2]==before[0]);
    drag.update({1,0,Placement::beforeExistingPre},81);drag.update({1,0,Placement::beforeExistingPre},170);CHECK(model.getOrder()[2]==before[0]);
    CHECK(drag.finish({3,0,Placement::postCab},171));CHECK(model.getOrder()[3]==before[0]);CHECK(model.getInstance(before[0])->placement==Placement::postCab);
    CHECK(model.undo());CHECK(model.getOrder()==before);CHECK(model.getInstance(before[0])->placement==Placement::beforeExistingPre);
}
LUTHIER_TEST(RefPedalUi, cancelledDragPreservesExistingRedoAndApplyOnDrop)
{
    PedalRegistry::registerAll();BoardModel model;for(int i=0;i<3;++i)CHECK(model.apply(cmd::Add{"generic.boost"}));
    const auto before=model.getOrder();CHECK(model.apply(cmd::SetEnabled{before[1],false}));CHECK(model.undo());CHECK(model.canRedo());
    DragController drag(model);CHECK(drag.begin(before[0],0));drag.update({2,0,Placement::postCab},0);drag.update({2,0,Placement::postCab},80);CHECK(model.getOrder()!=before);
    CHECK(drag.cancel());CHECK(model.getOrder()==before);CHECK(model.getInstance(before[0])->placement==Placement::beforeExistingPre);CHECK(model.canRedo());CHECK(model.redo());CHECK(!model.getInstance(before[1])->enabled);
    CHECK(drag.begin(before[0],100,false));drag.update({2,0,Placement::beforeExistingPre},101);drag.update({2,0,Placement::beforeExistingPre},500);CHECK(model.getOrder()==before);
    CHECK(drag.finish({2,0,Placement::beforeExistingPre},501));CHECK(model.getOrder()[2]==before[0]);
}

#include <map>
#include <iostream>
LUTHIER_TEST(RefPedalUi, tenThousandSeededOperationsKeepModelAndViewCoherent)
{
    PedalRegistry::registerAll();BoardModel model;for(int i=0;i<16;++i)CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardCanvas canvas(model);DragController drag(model);uint32_t seed=0x4c555448;
    auto next=[&]{seed=1664525u*seed+1013904223u;return seed;};
    std::map<juce::String,PedalView*> identities;
    const double start=juce::Time::getMillisecondCounterHiRes();
    for(int operation=0;operation<10000;++operation)
    {
        const auto before=model.getOrder();const auto id=before[next()%before.size()];const int index=(int)(next()%before.size());
        switch(next()%10)
        {
            case 0: CHECK(drag.begin(id,0));drag.update({index,0,Placement::beforeExistingPre},0);drag.update({index,0,Placement::beforeExistingPre},80);CHECK(drag.finish({index,0,Placement::beforeExistingPre},90));break;
            case 1: CHECK(drag.begin(id,0));drag.update({index,0,Placement::postCab},0);drag.update({index,0,Placement::postCab},80);CHECK(drag.cancel());CHECK(model.getOrder()==before);break;
            case 2: CHECK(model.apply(cmd::SetParam{id,0,(next()%1001)/1000.f}));break;
            case 3: CHECK(model.apply(cmd::SetEnabled{id,!model.getInstance(id)->enabled}));break;
            case 4: CHECK(model.apply(cmd::SetMounting{id,(Mounting)(next()%8)}));break;
            case 5: CHECK(model.apply(cmd::SetPlacement{id,(Placement)(next()%3)}));break;
            case 6: CHECK(model.apply(cmd::Move{id,index}));break;
            case 7: if(model.canUndo()){CHECK(model.undo());for(auto it=identities.begin();it!=identities.end();)if(!model.getInstance(InstanceId(it->first)))it=identities.erase(it);else ++it;CHECK(model.redo());}break;
            case 8: CHECK(model.apply(cmd::Remove{id}));CHECK(model.apply(cmd::Add{"generic.boost",index}));break;
            case 9: {auto ui=model.getBoardSettings().ui.clone();if(!ui.isObject())ui=juce::var(new juce::DynamicObject());ui.getDynamicObject()->setProperty("liveReorder",(next()&1)!=0);model.setUiState(ui);break;}
        }
        const auto order=model.getOrder();CHECK(order.size()==16);int previous=-1;std::map<juce::String,PedalView*> current;
        for(size_t i=0;i<order.size();++i)
        {
            auto* view=canvas.viewFor(order[i]);CHECK(view!=nullptr);if(!view)continue;
            const auto key=order[i].toString();if(auto it=identities.find(key);it!=identities.end())CHECK(view==it->second);
            CHECK(view->getX()>previous);CHECK(view->getExplicitFocusOrder()==(int)i+10);previous=view->getX();current.emplace(key,view);
        }
        CHECK(current.size()==16);identities=std::move(current);
    }
    std::cout<<"UI seeded operations: seed=0x4c555448 count=10000 ms="<<(juce::Time::getMillisecondCounterHiRes()-start)<<"\n";
}

#include "../UI/ReferencePedals/PedalFaceCache.h"
LUTHIER_TEST(RefPedalUi, cachedSixteenPedalPaintBudget)
{
    PedalRegistry::registerAll();BoardModel model;for(int i=0;i<16;++i)CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardCanvas canvas(model);PedalFaceCache::clear();
    const auto region=juce::Rectangle<int>(0,0,1100,300);CHECK(canvas.createComponentSnapshot(region).isValid());
    const auto warmBytes=PedalFaceCache::bytesUsed();CHECK(warmBytes>0);CHECK(warmBytes<=PedalFaceCache::maxBytes);
    std::vector<double> frames,wall;frames.reserve(300);wall.reserve(300);
    for(int frame=0;frame<300;++frame){const auto start=juce::Time::getMillisecondCounterHiRes();const auto cpu=luthier::tests::threadCpuTimeSeconds();CHECK(canvas.createComponentSnapshot(region).isValid());frames.push_back(1000*(luthier::tests::threadCpuTimeSeconds()-cpu));wall.push_back(juce::Time::getMillisecondCounterHiRes()-start);}
    CHECK(PedalFaceCache::bytesUsed()==warmBytes);std::sort(frames.begin(),frames.end());std::sort(wall.begin(),wall.end());
    std::cout<<"UI paint snapshot: 16 real pedals, 17 series cables, 300 viewport frames, p95="<<frames[284]<<"ms p99="<<frames[296]<<"ms threadCPU; wallp95="<<wall[284]<<"ms wallp99="<<wall[296]<<"ms cache="<<warmBytes<<" bytes\n";
    CHECK(wall[284]<16);CHECK(wall[296]<32); // CI 2x tolerance, section15; O0 focused harness.
}
LUTHIER_TEST(RefPedalUi, canvasGhostCancellationAndPlacementDropUseOneTransaction)
{
    PedalRegistry::registerAll();BoardModel model;for(int i=0;i<3;++i)CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardCanvas canvas(model);const auto before=model.getOrder();auto* view=canvas.viewFor(before[0]);const int count=canvas.getNumChildComponents();
    view->onDrag(*view,{300,130},{20,20},false);CHECK(canvas.dragActive());CHECK(canvas.getNumChildComponents()==count+1);CHECK_NEAR(view->getAlpha(),.35,1./255);
    CHECK(canvas.keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0}));CHECK(!canvas.dragActive());CHECK(canvas.getNumChildComponents()==count);CHECK_NEAR(view->getAlpha(),1,1e-6);CHECK(model.getOrder()==before);
    view->onDrag(*view,{900,20},{20,20},false);view->onDrag(*view,{900,20},{20,20},true);
    CHECK(!canvas.dragActive());CHECK(model.getOrder()[2]==before[0]);CHECK(model.getInstance(before[0])->placement==Placement::postCab);
    CHECK(model.undo());CHECK(model.getOrder()==before);CHECK(model.getInstance(before[0])->placement==Placement::beforeExistingPre);
}
LUTHIER_TEST(RefPedalUi, settingsAreOwnedNonmodalAndCloseOnEscapeAndResize)
{
    BoardModel model;BoardRoot root(model);root.setSize(1280,800);
    auto count=[&]{int n=0;for(auto* child:root.getChildren())if(dynamic_cast<juce::CallOutBox*>(child))++n;return n;};
    for(int repeat=0;repeat<5;++repeat){root.openSettings();CHECK(count()==1);CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);CHECK(root.keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0}));CHECK(count()==0);}
    root.openSettings();root.setSize(480,260);CHECK(count()==0);
}
