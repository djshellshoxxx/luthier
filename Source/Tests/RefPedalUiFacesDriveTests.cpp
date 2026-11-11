// OWNER: WP20
#include "TestFramework.h"
#include "../UI/ReferencePedals/PedalFaceFactory.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalUiFacesDrive, namedFacesUseSpecifiedMillimetres)
{
    struct Expected {const char* id;float width,depth;};
    const Expected models[] {{"boss-ds1",73,129},{"ibanez-ts808",70,125},{"boss-sd1",73,129},{"mxr-distortion-plus",59,111},{"proco-rat",120,95},{"mxr-dynacomp",111,60},{"ehx-big-muff-nyc",111,121},{"ehx-muff-wicker",120.7f,101.6f},{"ehx-green-russian",69.9f,114.3f},{"ehx-deluxe-muff",145,120}};
    for(const auto& model:models){auto face=PedalFaceFactory::make(model.id);CHECK(face!=nullptr);CHECK_NEAR(face->designSizeMm().x,model.width,.01);CHECK_NEAR(face->designSizeMm().y,model.depth,.01);}
}
#include "../ReferencePedals/PedalRegistry.h"
LUTHIER_TEST(RefPedalUiFacesDrive, frontControlsMapByExactDescriptorIdentity)
{
    // Artwork fixture, not a substitute for an implemented/calibrated DS-1 DSP.
    PedalRegistry::TestScope scope;
    const ParamDesc params[] {
        {"boss_ds1_dist","DIST","",ParamKind::Continuous,Taper::Linear,0,1,.5f,1,0,nullptr,PF_Automatable,20,-1},
        {"boss_ds1_tone","TONE","",ParamKind::Continuous,Taper::Linear,0,1,.5f,1,0,nullptr,PF_Automatable,20,-1},
        {"boss_ds1_level","LEVEL","",ParamKind::Continuous,Taper::Linear,0,1,.5f,1,0,nullptr,PF_Automatable,20,-1}
    };
    const PedalDesc desc {"boss-ds1","boss_ds1_",1,"DS-1 artwork fixture","Drive",nullptr,ChannelMode::Stereo,BypassMode::True,3,params,0,false,false,false,0};
    CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));
    auto face=PedalFaceFactory::make("boss-ds1");FaceLayout layout;face->layout(layout);
    CHECK(layout.controls.size()==3);if(layout.controls.size()!=3)return;
    CHECK(layout.controls[0].paramIdx==2);CHECK(layout.controls[1].paramIdx==1);CHECK(layout.controls[2].paramIdx==0);
    CHECK(layout.controls[0].bounds.getCentreX()<layout.controls[1].bounds.getCentreX());
    CHECK(layout.controls[1].bounds.getCentreX()<layout.controls[2].bounds.getCentreX());
}
