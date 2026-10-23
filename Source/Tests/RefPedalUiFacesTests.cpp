// OWNER: WP19
#include "TestFramework.h"
#include "../UI/ReferencePedals/PedalFaceFactory.h"
#include "../ReferencePedals/PedalRegistry.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalUiFaces, descriptorsHaveRealFacesAndControlMappings) {
    PedalRegistry::registerAll();
    for(const auto* d:PedalRegistry::all()){
        auto face=PedalFaceFactory::make(d->modelId);CHECK(face!=nullptr);FaceLayout layout;face->layout(layout);
        CHECK(layout.jacks.size()>=2);CHECK(!layout.footswitch.isEmpty());
        for(int i=0;i<d->numParams;++i)if(!d->params[i].has(PF_Engage))CHECK(std::any_of(layout.controls.begin(),layout.controls.end(),[i](const auto& c){return c.paramIdx==i;}));
    }
    auto missing=PedalFaceFactory::make("missing.model");CHECK(missing!=nullptr);
}
