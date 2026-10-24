// OWNER: WP19
#include "TestFramework.h"
#include "../UI/ReferencePedals/PedalFaceFactory.h"
#include "../UI/ReferencePedals/KnobControl.h"
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

LUTHIER_TEST(RefPedalUiFaces, keyboardKnobUsesFineAndCoarseSteps) {
    KnobControl knob;knob.setValue(.5,juce::dontSendNotification);
    CHECK(knob.keyPressed({juce::KeyPress::upKey,juce::ModifierKeys(),0}));CHECK_NEAR(knob.getValue(),.51,1e-6);
    CHECK(knob.keyPressed({juce::KeyPress::upKey,juce::ModifierKeys(juce::ModifierKeys::shiftModifier),0}));CHECK_NEAR(knob.getValue(),.61,1e-6);
    CHECK(!knob.keyPressed({juce::KeyPress::leftKey,juce::ModifierKeys(juce::ModifierKeys::altModifier),0}));CHECK_NEAR(knob.getValue(),.61,1e-6);
}
