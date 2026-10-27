// OWNER: WP17 / WP20 / global-search integration
#include "ComboHarness.h"
#include "RefPedalUiTestSupport.h"
#include "../PluginEditor.h"
#include "../UI/ReferencePedalDrawer.h"
#include "../UI/ReferencePedals/BoardWindow.h"
#include "../UI/Search/SearchNavigator.h"
#include "../UI/Search/CommandPalette.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../UI/ReferencePedals/ReferenceSearch.h"
#include <set>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue(bool); }
#endif
namespace {
using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
template<class T> T* refSearchChild(juce::Component& root)
{
    if(auto* value=dynamic_cast<T*>(&root))return value;
    for(auto* child:root.getChildren())if(auto* value=refSearchChild<T>(*child))return value;
    return nullptr;
}
void refSearchDrain()
{
    for(int i=0;i<128;++i){
       #if JUCE_MAC
        if(CFRunLoopRunInMode(kCFRunLoopDefaultMode,0,true)!=kCFRunLoopRunHandledSource)break;
       #else
        if(!juce::detail::dispatchNextMessageOnSystemQueue(true))break;
       #endif
    }
}
struct RefSearchWrites final:juce::AudioProcessorParameter::Listener {
    int writes=0;void parameterValueChanged(int,float) override{++writes;}void parameterGestureChanged(int,bool) override{}
};
struct RefSearchEditor {
    explicit RefSearchEditor(luthier::combo::Rig& rig):owner(rig.p().createEditor())
    {
        editor=dynamic_cast<LuthierAudioProcessorEditor*>(owner.get());
        if(editor){editor->setSize(1600,1000);editor->addToDesktop(0);editor->setVisible(true);root=refSearchChild<BoardRoot>(*editor);refSearchDrain();}
    }
    std::unique_ptr<juce::AudioProcessorEditor> owner;LuthierAudioProcessorEditor* editor=nullptr;juce::Component::SafePointer<BoardRoot> root;
};
}
LUTHIER_TEST(RefPedalSearch, actualActivationFocusesAndOperatesDockAndPopoutControls)
{
    using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
    combo::Rig rig;auto& model=rig.p().getEngine().getReferencePedals().model();
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.delay"}))return;
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;
    RefSearchEditor shown(rig);CHECK(shown.editor&&shown.root);if(!shown.editor||!shown.root)return;
    auto& nav=shown.editor->getSearch();const auto rootIdentity=shown.root.getComponent();
    for(bool popout:{false,true})
    {
        if(popout)shown.root->onPopout();else shown.root->onDock();refSearchDrain();
        for(const auto* id:{"ref_board_mix","ref_board_fb_gain","ref_board_fb_kill","ref_s01_p01","ref_s01_p08","ref_s01_in_db","ref_s01_enabled","ref_s01_pos"})
        {
            auto* found=nav.getIndex().find("param:"+juce::String(id));CHECK_MSG(found!=nullptr,id);if(!found)return;const auto item=*found;
            nav.openPalette(item.title);auto& palette=nav.getPalette();int row=-1;for(int r=0;r<(int)palette.getRows().size();++r)if(palette.getRows()[(size_t)r].itemId==item.id){row=r;break;}
            CHECK_MSG(row>=0,juce::String(id)+": actual palette result missing");if(row<0)return;palette.selectRow(row);palette.activateSelected(ActivationKind::goOnly);refSearchDrain();
            CHECK_MSG(!palette.isOpen(),juce::String(id)+": "+palette.getFooterText());
            const auto last=nav.getLastNavigation();auto* control=last.control.getComponent();CHECK_MSG(last.ok&&control!=nullptr,id);if(!last.ok||!control)return;
            CHECK(control->getComponentID()==id);CHECK(nav.getLastFocusRequest()==control);CHECK(nav.getHighlighter().getTarget()==control);CHECK(!nav.getHighlighter().getRingBounds().isEmpty());CHECK(shown.root.getComponent()==rootIdentity);
            CHECK((shown.root->findParentComponentOfClass<BoardWindow>()!=nullptr)==popout);
            auto* parameter=rig.state().getParameter(id);CHECK(parameter!=nullptr);if(!parameter)return;RefSearchWrites writes;parameter->addListener(&writes);
            if(auto* slider=dynamic_cast<juce::Slider*>(control)){const double before=slider->getValue();{juce::Slider::ScopedDragNotification gesture(*slider);slider->setValue(before>(slider->getMinimum()+slider->getMaximum())*.5?slider->getMinimum():slider->getMaximum(),juce::sendNotificationSync);}CHECK(writes.writes>0);slider->setValue(before,juce::sendNotificationSync);}
            else if(auto* combo=dynamic_cast<juce::ComboBox*>(control)){const int before=combo->getSelectedId();int next=0;for(int i=0;i<combo->getNumItems();++i){const int candidate=combo->getItemId(i);if(candidate!=before&&combo->isItemEnabled(candidate)){next=candidate;break;}}CHECK(next>0);if(next>0)combo->setSelectedId(next,juce::sendNotificationSync);CHECK(writes.writes>0);combo->setSelectedId(before,juce::sendNotificationSync);}
            else if(auto* button=dynamic_cast<juce::Button*>(control)){if((bool)button->getProperties()["referenceMomentary"]){button->setState(juce::Button::buttonDown);button->setState(juce::Button::buttonNormal);CHECK(writes.writes>=2);}else{const bool before=button->getToggleState();button->setToggleState(!before,juce::sendNotificationSync);CHECK(writes.writes>0);button->setToggleState(before,juce::sendNotificationSync);}}
            else CHECK_MSG(false,"Search target is not an actual interactive native control");
            parameter->removeListener(&writes);refSearchDrain();
        }
    }
    juce::Component::SafePointer<juce::Component> highlighted=nav.getHighlighter().getTarget();shown.owner.reset();CHECK(shown.root==nullptr);CHECK(highlighted==nullptr);refSearchDrain();
}
LUTHIER_TEST(RefPedalSearch, allHostIdsRemainIndexedAndVacantLanesRefuseWithoutWriting)
{
    using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
    combo::Rig rig;RefSearchEditor shown(rig);CHECK(shown.editor&&shown.root);if(!shown.editor||!shown.root)return;auto& nav=shown.editor->getSearch();int count=0,vacant=0;
    for(auto* raw:rig.p().getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))
    {
        const auto id=parameter->getParameterID();if(!id.startsWith("ref_"))continue;++count;
        auto* found=nav.getIndex().find("param:"+id);CHECK_MSG(found!=nullptr,id);if(!found)continue;const auto item=*found;CHECK(item.title.trim().isNotEmpty());
        auto* provider=nav.getIndex().getProviderFor(item);CHECK(provider!=nullptr);if(!provider)continue;
        if(id.startsWith("ref_s"))
        {
            ++vacant;CHECK_MSG(provider->availabilityOf(item)!=Availability::available,id);CHECK(nav.subtitleFor(item,provider->availabilityOf(item)).isNotEmpty());
            const float before=parameter->getValue();RefSearchWrites writes;parameter->addListener(&writes);const auto outcome=nav.activate(item,ActivationKind::goOnly,false);CHECK(outcome.status==SearchNavigator::Outcome::Status::refused);CHECK(outcome.message.isNotEmpty());CHECK(!nav.getLastNavigation().ok);CHECK(writes.writes==0);CHECK(!nav.nudge(id,1,false));ValueReading reading;reading.item=&item;reading.parameterId=id;reading.resolved.ok=true;reading.resolved.normalised=before>.5f?0.f:1.f;CHECK(nav.applyValue(reading).status==SearchNavigator::Outcome::Status::refused);CHECK(writes.writes==0);CHECK_NEAR(parameter->getValue(),before,1e-6);parameter->removeListener(&writes);
        }
    }
    CHECK(count==373);CHECK(vacant==320);
}

LUTHIER_TEST(RefPedalSearch, all373RoutesReflectRealPopulatedControlsAndUnusedLanes)
{
    using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
    combo::Rig rig;auto& model=rig.p().getEngine().getReferencePedals().model();
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::SetBoardParam{"cpuCapPct",100}))return;
    for(int i=0;i<kMaxSlots;++i)if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{i%2==0?"generic.delay":"generic.boost"}))return;
    RefSearchEditor shown(rig);CHECK(shown.editor&&shown.root);if(!shown.editor||!shown.root)return;
    shown.root->onDock();auto& nav=shown.editor->getSearch();int count=0,available=0,unused=0;
    for(auto* raw:rig.p().getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))
    {
        const auto id=parameter->getParameterID();if(!id.startsWith("ref_"))continue;++count;
        auto* found=nav.getIndex().find("param:"+id);CHECK_MSG(found!=nullptr,id);if(!found)return;const auto item=*found;
        bool expected=true;
        if(id.startsWith("ref_s")&&id.substring(8).startsWith("p"))
        {
            const int slot=id.substring(5,7).getIntValue()-1;const int lane=id.substring(9).getIntValue()-1;
            const auto& map=PedalRegistry::laneMap(slot%2==0?"generic.delay":"generic.boost");expected=lane<map.numLanes&&map.laneToParam[(size_t)lane]>=0;
        }
        auto* provider=nav.getIndex().getProviderFor(item);CHECK(provider);if(!provider)return;CHECK_MSG((provider->availabilityOf(item)==Availability::available)==expected,id);
        const auto outcome=nav.activate(item,ActivationKind::goOnly,false);
        if(expected)
        {
            ++available;CHECK_MSG(outcome.status==SearchNavigator::Outcome::Status::done,id+": "+outcome.message);
            auto* control=nav.getLastNavigation().control.getComponent();CHECK_MSG(control&&control->getComponentID()==id,id);
            CHECK(nav.getLastNavigation().ok);CHECK(nav.getLastFocusRequest()==control);CHECK(nav.getHighlighter().getTarget()==control);CHECK(!nav.getHighlighter().getRingBounds().isEmpty());
        }
        else {++unused;CHECK(outcome.status==SearchNavigator::Outcome::Status::refused);CHECK(outcome.message.isNotEmpty());CHECK(!nav.getLastNavigation().ok);}
    }
    CHECK(count==373);CHECK(available+unused==373);CHECK(available>53);CHECK(unused>0);
}
LUTHIER_TEST(RefPedalSearch, retainedItemsFollowStorageIdentityAfterReorderReplaceAndRemove)
{
    using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
    combo::Rig rig;auto& model=rig.p().getEngine().getReferencePedals().model();
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.delay"}))return;
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;
    const auto first=model.getOrder().front();RefSearchEditor shown(rig);CHECK(shown.editor&&shown.root);if(!shown.editor||!shown.root)return;
    auto& nav=shown.editor->getSearch();auto* found=nav.getIndex().find("param:ref_s01_p08");CHECK(found);if(!found)return;const auto old=*found;
    CHECK(nav.activate(old,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::done);
    juce::Component::SafePointer<juce::Component> oldControl=nav.getLastNavigation().control;
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Move{first,1}))return;
    CHECK(nav.activate(old,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::done);CHECK(nav.getLastNavigation().control==oldControl);
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::SetModel{first,"generic.boost"}))return;
    CHECK(oldControl==nullptr);CHECK(nav.activate(old,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::refused);
    auto* current=nav.getIndex().find("param:ref_s01_p01");CHECK(current);if(!current)return;const auto live=*current;CHECK(live.title!=old.title);
    CHECK(nav.activate(live,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::done);
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Remove{first}))return;
    CHECK(nav.activate(live,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::refused);CHECK(!nav.getLastNavigation().ok);
    if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.delay"}))return;
    CHECK(model.getOrder().back()!=first);CHECK(nav.activate(old,ActivationKind::goOnly,false).status==SearchNavigator::Outcome::Status::done);
    CHECK(nav.getLastNavigation().control&&nav.getLastNavigation().control->getComponentID()=="ref_s01_p08");
}

LUTHIER_TEST(RefPedalSearch, unavailablePaletteSliderAndContextActionsPreserveReservedHostValues)
{
    using namespace luthier;using namespace luthier::reference;using namespace luthier::search;
    combo::Rig rig;RefSearchEditor shown(rig);CHECK(shown.editor&&shown.root);if(!shown.editor||!shown.root)return;auto& nav=shown.editor->getSearch();
    auto* parameter=rig.state().getParameter("ref_s01_p01");CHECK(parameter);if(!parameter)return;parameter->setValueNotifyingHost(.37f);
    auto* item=nav.getIndex().find("param:ref_s01_p01");CHECK(item);if(!item)return;nav.openPalette(item->title);auto& palette=nav.getPalette();int row=-1;
    for(int i=0;i<(int)palette.getRows().size();++i)if(palette.getRows()[(size_t)i].itemId==item->id){row=i;break;}
    CHECK(row>=0);if(row<0)return;palette.selectRow(row);refSearchDrain();
    auto* component=palette.getList().getComponentForRowNumber(row);CHECK(component);if(!component)return;
    RefSearchWrites writes;parameter->addListener(&writes);
    const auto point=juce::Point<float>((float)(component->getWidth()-130),(float)component->getHeight()*.5f);auto source=juce::Desktop::getInstance().getMainMouseSource();
    juce::MouseEvent event(source,point,juce::ModifierKeys::leftButtonModifier,1.f,0.f,0.f,0.f,0.f,component,component,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,false);
    component->mouseDown(event);component->mouseDrag(event);component->mouseUp(event);CHECK(writes.writes==0);CHECK_NEAR(parameter->getValue(),.37f,1e-6);
    palette.runSecondary(row,2);CHECK(writes.writes==0);CHECK_NEAR(parameter->getValue(),.37f,1e-6);CHECK(palette.getFooterText().isNotEmpty());parameter->removeListener(&writes);
}
