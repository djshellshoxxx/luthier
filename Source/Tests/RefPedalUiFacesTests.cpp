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
        std::vector<int> indices;
        for(const auto& control:layout.controls){CHECK(control.paramIdx>=0&&control.paramIdx<d->numParams);CHECK(std::find(indices.begin(),indices.end(),control.paramIdx)==indices.end());indices.push_back(control.paramIdx);}
    }
    auto missing=PedalFaceFactory::make("missing.model");CHECK(missing!=nullptr);
}

LUTHIER_TEST(RefPedalUiFaces, keyboardKnobUsesFineAndCoarseSteps) {
    KnobControl knob;knob.setValue(.5,juce::dontSendNotification);
    CHECK(knob.keyPressed({juce::KeyPress::upKey,juce::ModifierKeys(),0}));CHECK_NEAR(knob.getValue(),.51,1e-6);
    CHECK(knob.keyPressed({juce::KeyPress::upKey,juce::ModifierKeys(juce::ModifierKeys::shiftModifier),0}));CHECK_NEAR(knob.getValue(),.61,1e-6);
    CHECK(!knob.keyPressed({juce::KeyPress::leftKey,juce::ModifierKeys(juce::ModifierKeys::altModifier),0}));CHECK_NEAR(knob.getValue(),.61,1e-6);
}

#include "../UI/ReferencePedals/BoardCanvas.h"
#include "../UI/ReferencePedals/BoardRoot.h"
LUTHIER_TEST(RefPedalUiFaces, everyDescriptorHasARealFaceOrInspectorControl) {
    PedalRegistry::registerAll();
    for(const auto* desc:PedalRegistry::all()) {
        BoardModel model;CHECK(model.apply(cmd::Add{desc->modelId}));BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);
        std::vector<juce::Component*> found;
        std::function<void(juce::Component&)> collect=[&](auto& root){for(auto* child:root.getChildren()){if(child->getProperties().contains("referenceParamId"))found.push_back(child);collect(*child);}};collect(*view);
        for(int i=0;i<desc->numParams;++i)if(!desc->params[i].has(PF_Engage)) {
            auto it=std::find_if(found.begin(),found.end(),[&](auto* c){return c->getProperties()["referenceParamId"].toString()==desc->params[i].id;});
            CHECK(it!=found.end());if(it==found.end())continue;
            CHECK(dynamic_cast<juce::Slider*>(*it)||dynamic_cast<juce::ComboBox*>(*it)||dynamic_cast<juce::Button*>(*it));
            if(auto* slider=dynamic_cast<juce::Slider*>(*it)){slider->setValue(.73,juce::sendNotificationSync);CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[(size_t)i],slider->getValue(),.001);}
        }
    }
}
namespace {
struct GestureCapture final : juce::AudioProcessorParameter::Listener {
    std::vector<bool> gestures;void parameterValueChanged(int,float) override{}
    void parameterGestureChanged(int,bool begin) override{gestures.push_back(begin);}
};
class FaceAttachmentProcessor final : public juce::AudioProcessor {
public:
    const juce::String getName() const override{return "Face attachment fixture";}
    void prepareToPlay(double,int) override{} void releaseResources() override{}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override{}
    bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return 0;} bool hasEditor() const override{return false;}
    juce::AudioProcessorEditor* createEditor() override{return nullptr;} int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;} void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {}; } void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock&) override{} void setStateInformation(const void*,int) override{}
};
}
LUTHIER_TEST(RefPedalUiFaces, physicalLaneAttachmentsAndConfigurationUseRealTargets) {
    PedalRegistry::registerAll();FaceAttachmentProcessor processor;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for(int n=1;n<=4;++n){const auto id="ref_s01_p"+juce::String(n).paddedLeft('0',2);layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},id,0.f,1.f,.5f));}
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"ref_s01_enabled",1},"Enable",true));
    for(const auto& id:{"ref_s01_in_db","ref_s01_out_db"})layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},id,-12.f,12.f,0.f));
    juce::AudioProcessorValueTreeState state(processor,nullptr,"face_parameters",std::move(layout));
    BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));BoardCanvas canvas(model,&state);auto* view=canvas.viewFor(model.getOrder()[0]);
    juce::Slider* gain=nullptr;juce::ComboBox* quality=nullptr;juce::Button* bypass=nullptr;
    std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* c:parent.getChildren()){
        if(c->getComponentID()=="ref_s01_p01")gain=dynamic_cast<juce::Slider*>(c);
        if(c->getProperties()["referenceParamId"].toString()=="gen_boost_quality")quality=dynamic_cast<juce::ComboBox*>(c);
        if(c->getComponentID()=="ref_s01_enabled")bypass=dynamic_cast<juce::Button*>(c);collect(*c);}};collect(*view);
    CHECK(gain!=nullptr);CHECK(quality!=nullptr);CHECK(bypass!=nullptr);if(!gain||!quality||!bypass)return;
    CHECK(gain->getTextFromValue(.5).contains("dB"));CHECK_NEAR(gain->getValueFromText("15 dB"),.5,1e-6);CHECK_NEAR(gain->getDoubleClickReturnValue(),.4,1e-6);
    GestureCapture capture;auto* hostGain=state.getParameter("ref_s01_p01");hostGain->addListener(&capture);
    CHECK(gain->keyPressed({juce::KeyPress::upKey,juce::ModifierKeys(),0}));
    CHECK(capture.gestures==std::vector<bool>({true,false}));
    juce::MouseWheelDetails wheel;wheel.deltaY=1.f;gain->mouseWheelMove(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{},juce::ModifierKeys(),0,0,0,0,0,gain,gain,juce::Time(),{},juce::Time(),0,false),wheel);
    CHECK(capture.gestures==std::vector<bool>({true,false,true,false}));hostGain->removeListener(&capture);
    gain->setValue(.73,juce::sendNotificationSync);CHECK_NEAR(state.getRawParameterValue("ref_s01_p01")->load(),.73,.001);
    CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[0],0,1e-6); // Host service owns persistence; attachment makes no duplicate model edit.
    quality->setSelectedId(3,juce::sendNotificationSync);CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[4],1,1e-6);
    state.getParameter("ref_s01_enabled")->setValueNotifyingHost(0);CHECK(!bypass->getToggleState());
}
LUTHIER_TEST(RefPedalUiFaces, compactKnobHasUnclippedNaturalValueEditor)
{
    PedalRegistry::registerAll();BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardRoot root(model);root.setSize(1280,800);auto* view=root.boardCanvas().viewFor(model.getOrder()[0]);KnobControl* gain=nullptr;
    for(auto* child:view->getChildren())if(child->getProperties()["referenceParamId"].toString()=="gen_boost_gain")gain=dynamic_cast<KnobControl*>(child);
    CHECK(gain!=nullptr);if(!gain)return;CHECK(gain->getTextBoxPosition()==juce::Slider::NoTextBox);gain->showValueEditor();CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);
    juce::TextEditor* entry=nullptr;std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* child:parent.getChildren()){if(auto* editor=dynamic_cast<juce::TextEditor*>(child))if(editor->getTitle().contains("natural value"))entry=editor;collect(*child);}};collect(root);
    CHECK(entry!=nullptr);if(!entry)return;entry->setText("15 dB",false);entry->onReturnKey();CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[0],.5,1e-6);
    CHECK(model.apply(cmd::Remove{model.getOrder()[0]}));int callouts=0;for(auto* child:root.getChildren())if(dynamic_cast<juce::CallOutBox*>(child))++callouts;CHECK(callouts==0);
}
LUTHIER_TEST(RefPedalUiFaces, actualMetersRequireFullIdentityAndAvailability)
{
    PedalRegistry::registerAll();BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));
    BoardCanvas canvas(model);const auto id=model.getOrder()[0];auto* view=canvas.viewFor(id);PedalMeters meters;meters.present=true;meters.identity=InstanceId();meters.enabled=true;meters.model.available=UiTransport|UiLoop|UiTempo|UiFaults;meters.model.transport=2;meters.model.loopLength=48000;meters.model.playhead=12000;meters.model.bpm=123.5;meters.model.faults=4;
    CHECK(!view->updateMeters(meters));CHECK(view->meterSummary().isEmpty());meters.identity=id;
    CHECK(view->updateMeters(meters));CHECK(view->meterSummary().contains("PLAY"));CHECK(view->meterSummary().contains("12000/48000"));CHECK(view->meterSummary().contains("123.5 BPM"));CHECK(view->meterSummary().contains("FAULT"));CHECK(!view->updateMeters(meters));
    meters.model.available=UiTransport;CHECK(view->updateMeters(meters));CHECK(!view->meterSummary().contains("BPM"));CHECK(!view->meterSummary().contains("48000"));CHECK(!view->meterSummary().contains("FAULT"));
    meters.present=false;CHECK(view->updateMeters(meters));CHECK(view->meterSummary().contains("pending"));
}
