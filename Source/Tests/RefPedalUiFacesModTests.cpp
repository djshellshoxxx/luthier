// OWNER: WP21
#include "TestFramework.h"
#include "RefPedalUiTestSupport.h"
#include "../UI/ReferencePedals/PedalFaceFactory.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalUiFacesMod, namedFacesKeepPhysicalFootprints)
{
    struct Expected {const char* id;float width,depth;};
    const Expected models[] {{"boss-ce2",70,125},{"boss-ce2w",73,129},{"mxr-phase90-script",111,60},{"boss-dd3t",73,129},{"tc-ditto",48,93},{"ehx-nano-360",70,115},{"boss-rc1",73,129},{"boss-rc5",73,129}};
    for(const auto& model:models){auto face=PedalFaceFactory::make(model.id);CHECK(face!=nullptr);CHECK_NEAR(face->designSizeMm().x,model.width,.01);CHECK_NEAR(face->designSizeMm().y,model.depth,.01);}
}
#include "../ReferencePedals/PedalRegistry.h"
LUTHIER_TEST(RefPedalUiFacesMod, physicalLooperFootswitchIsRealMomentaryControl)
{
    PedalRegistry::TestScope scope;
    const ParamDesc params[] {
        {"tc_ditto_loop_level","LOOP LEVEL","",ParamKind::Continuous,Taper::Linear,0,1,.5f,1,0,nullptr,PF_Automatable,20,-1},
        {"tc_ditto_fs","FOOTSWITCH","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1}
    };
    const PedalDesc desc {"tc-ditto","tc_ditto_",1,"Ditto artwork fixture","Looper",nullptr,ChannelMode::Stereo,BypassMode::True,2,params,0,false,false,false,0};
    CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));
    auto face=PedalFaceFactory::make("tc-ditto");FaceLayout layout;face->layout(layout);
    CHECK(layout.controls.size()==2);if(layout.controls.size()!=2)return;
    CHECK(layout.controls[1].paramIdx==1);CHECK(layout.controls[1].kind==FaceControlKind::Button);
    CHECK(layout.controls[1].bounds==layout.footswitch);
}
#include "../UI/ReferencePedals/BoardCanvas.h"
LUTHIER_TEST(RefPedalUiFacesMod, looperMainFootswitchSendsRealEdgesWithoutBypassAlias)
{
    PedalRegistry::TestScope scope;
    const ParamDesc params[]{{"tc_ditto_loop_level","LEVEL","",ParamKind::Continuous,Taper::Linear,0,1,.5f,1,0,nullptr,PF_Automatable,20,-1},{"tc_ditto_fs","PEDAL","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,0,0,-1}};
    const PedalDesc desc{"tc-ditto","tc_ditto_",1,"Looper UI fixture","Fixture",nullptr,ChannelMode::Stereo,BypassMode::True,2,params,0,false,false,false,0};CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));
    BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"tc-ditto"}))return;BoardCanvas canvas(model);const auto id=model.getOrder()[0];auto* view=canvas.viewFor(id);juce::Button* main=nullptr;
    for(auto* child:view->getChildren())if(child->getComponentID()=="tc_ditto_fs")main=dynamic_cast<juce::Button*>(child);
    CHECK(main!=nullptr);if(!main)return;CHECK(!main->getClickingTogglesState());
    main->setState(juce::Button::buttonDown);CHECK_NEAR(model.getInstance(id)->params[1],1,1e-6);main->setState(juce::Button::buttonNormal);CHECK_NEAR(model.getInstance(id)->params[1],0,1e-6);CHECK(model.getInstance(id)->enabled);
}
LUTHIER_TEST(RefPedalUiFacesMod, descriptorFootswitchModeReplacesGenericSlotMode)
{
    PedalRegistry::TestScope scope;const char* choices[]{"Latch","Momentary","Auto"};const ParamDesc params[]{{"tc_ditto_fs_mode","Footswitch mode","",ParamKind::Enum,Taper::Linear,0,2,2,1,3,choices,PF_Config,0,-1},{"tc_ditto_fs","PEDAL","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,0,0,-1}};
    const PedalDesc desc{"tc-ditto","tc_ditto_",1,"Looper mode fixture","Fixture",nullptr,ChannelMode::Stereo,BypassMode::True,2,params,0,false,false,false,0};CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"tc-ditto"}))return;BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);juce::Component* generic=nullptr;juce::Component* own=nullptr;
    std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* c:parent.getChildren()){if(c->getTitle()=="Slot footswitch mode")generic=c;if(c->getProperties()["referenceParamId"].toString()=="tc_ditto_fs_mode")own=c;collect(*c);}};collect(*view);CHECK(generic&&own);if(generic&&own){CHECK(!generic->isVisible());CHECK(own->isVisible());}
}
LUTHIER_TEST(RefPedalUiFacesMod, looperContextLearnUsesItsRealMomentaryParameter)
{
    PedalRegistry::TestScope scope;const ParamDesc params[]{{"tc_ditto_fs","PEDAL","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,0,0,-1}};const PedalDesc desc{"tc-ditto","tc_ditto_",1,"Learn fixture","Fixture",nullptr,ChannelMode::Stereo,BypassMode::True,1,params,0,false,false,false,0};CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"tc-ditto"}))return;BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);int captured=-99;view->onLearn=[&](auto id,int parameter,bool continuous){CHECK(id==view->id());CHECK(!continuous);captured=parameter;};view->requestFootswitchLearn();CHECK(captured==0);
}
