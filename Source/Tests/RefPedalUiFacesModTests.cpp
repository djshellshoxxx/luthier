// OWNER: WP21
#include "TestFramework.h"
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
