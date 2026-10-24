// OWNER: WP19
#include "TestFramework.h"
#include "RefPedalUiTestSupport.h"
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
        BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{desc->modelId}))return;BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);
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
    std::vector<bool> gestures;std::function<void(bool)> onGesture;int valueChanges=0;void parameterValueChanged(int,float) override{++valueChanges;}
    void parameterGestureChanged(int,bool begin) override{gestures.push_back(begin);if(onGesture)onGesture(begin);}
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
    BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;BoardCanvas canvas(model,&state);auto* view=canvas.viewFor(model.getOrder()[0]);
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
    PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;
    BoardRoot root(model);root.setSize(1280,800);auto* view=root.boardCanvas().viewFor(model.getOrder()[0]);KnobControl* gain=nullptr;
    for(auto* child:view->getChildren())if(child->getProperties()["referenceParamId"].toString()=="gen_boost_gain")gain=dynamic_cast<KnobControl*>(child);
    CHECK(gain!=nullptr);if(!gain)return;CHECK(gain->getTextBoxPosition()==juce::Slider::NoTextBox);gain->showValueEditor();CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);
    juce::TextEditor* entry=nullptr;std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* child:parent.getChildren()){if(auto* editor=dynamic_cast<juce::TextEditor*>(child))if(editor->getTitle().contains("natural value"))entry=editor;collect(*child);}};collect(root);
    CHECK(entry!=nullptr);if(!entry)return;entry->setText("15 dB",false);entry->onReturnKey();CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[0],.5,1e-6);
    CHECK(model.apply(cmd::Remove{model.getOrder()[0]}));int callouts=0;for(auto* child:root.getChildren())if(dynamic_cast<juce::CallOutBox*>(child))++callouts;CHECK(callouts==0);
}
LUTHIER_TEST(RefPedalUiFaces, actualMetersRequireFullIdentityAndAvailability)
{
    PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;
    BoardCanvas canvas(model);const auto id=model.getOrder()[0];auto* view=canvas.viewFor(id);PedalMeters meters;meters.present=true;meters.identity=InstanceId();meters.enabled=true;meters.model.available=UiTransport|UiLoop|UiTempo|UiFaults;meters.model.transport=2;meters.model.loopLength=48000;meters.model.playhead=12000;meters.model.bpm=123.5;meters.model.faults=4;
    CHECK(!view->updateMeters(meters));CHECK(view->meterSummary().isEmpty());meters.identity=id;
    CHECK(view->updateMeters(meters));CHECK(view->meterSummary().contains("PLAY"));CHECK(view->meterSummary().contains("12000/48000"));CHECK(view->meterSummary().contains("123.5 BPM"));CHECK(view->meterSummary().contains("FAULT"));CHECK(!view->updateMeters(meters));
    meters.model.available=UiTransport;CHECK(view->updateMeters(meters));CHECK(!view->meterSummary().contains("BPM"));CHECK(!view->meterSummary().contains("48000"));CHECK(!view->meterSummary().contains("FAULT"));
    meters.present=false;CHECK(view->updateMeters(meters));CHECK(view->meterSummary().contains("pending"));
}
#include "../UI/ReferencePedals/BoardMidiPanel.h"
#include "../ReferencePedals/Midi/ExtendedHostAutomation.h"
LUTHIER_TEST(RefPedalUiFaces, everyExtendedHostParameterHasARealAttachedControl)
{
    FaceAttachmentProcessor processor;juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for(const auto& d:extendedHostParameters){if(d.choices)layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{d.id,1},d.name,juce::StringArray::fromTokens(d.choices,"|",""),(int)d.defaultValue));else if(d.minimum==0&&d.maximum==1&&d.interval==1)layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{d.id,1},d.name,d.defaultValue>.5));else layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{d.id,1},d.name,juce::NormalisableRange<float>(d.minimum,d.maximum,d.interval),d.defaultValue));}
    juce::AudioProcessorValueTreeState state(processor,nullptr,"extended_parameters",std::move(layout));BoardModel model;BoardMidiPanel panel(model,&state);std::map<juce::String,juce::Component*> actual;
    std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* c:parent.getChildren()){if(c->getComponentID().startsWith("ref_board_"))actual.emplace(c->getComponentID(),c);collect(*c);}};collect(panel);
    CHECK(actual.size()==extendedHostParameters.size());for(const auto& d:extendedHostParameters){auto it=actual.find(d.id);CHECK(it!=actual.end());if(it==actual.end())continue;CHECK(it->second->getTitle().isNotEmpty());CHECK(dynamic_cast<juce::Slider*>(it->second)||dynamic_cast<juce::ComboBox*>(it->second)||dynamic_cast<juce::Button*>(it->second));}
    auto* gain=dynamic_cast<juce::Slider*>(actual["ref_board_fb_gain"]);CHECK(gain!=nullptr);if(gain){{juce::Slider::ScopedDragNotification gesture(*gain);gain->setValue(.73,juce::sendNotificationSync);}CHECK_NEAR(state.getRawParameterValue("ref_board_fb_gain")->load(),.73,.001);CHECK_NEAR(model.getBoardSettings().fb.gain,.5,1e-6);}
    auto* mode=dynamic_cast<juce::ComboBox*>(actual["ref_board_mode"]);CHECK(mode!=nullptr);if(mode){mode->setSelectedId(2,juce::sendNotificationSync);CHECK_NEAR(state.getRawParameterValue("ref_board_mode")->load(),1,1e-6);CHECK(model.getBoardSettings().mode==Mode::Series);}
    auto* kill=dynamic_cast<juce::Button*>(actual["ref_board_fb_kill"]);CHECK(kill!=nullptr);if(kill){kill->setState(juce::Button::buttonDown);CHECK_NEAR(state.getRawParameterValue("ref_board_fb_kill")->load(),1,1e-6);kill->setState(juce::Button::buttonNormal);CHECK_NEAR(state.getRawParameterValue("ref_board_fb_kill")->load(),0,1e-6);}
    GestureCapture capture;auto* killParameter=state.getParameter("ref_board_fb_kill");killParameter->addListener(&capture);
    {auto closing=std::make_unique<BoardMidiPanel>(model,&state);juce::Button* held=nullptr;std::function<void(juce::Component&)> find=[&](auto& parent){for(auto* c:parent.getChildren()){if(c->getComponentID()=="ref_board_fb_kill")held=dynamic_cast<juce::Button*>(c);find(*c);}};find(*closing);CHECK(held!=nullptr);if(held)held->setState(juce::Button::buttonDown);CHECK_NEAR(killParameter->getValue(),1,1e-6);}
    CHECK_NEAR(killParameter->getValue(),0,1e-6);CHECK(capture.gestures==std::vector<bool>({true,false}));killParameter->removeListener(&capture);

}
#include "../UI/ReferencePedals/MappingCallout.h"
LUTHIER_TEST(RefPedalUiFaces, expressionMappingPreservesNestedMetadataAndScrolls)
{
    PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;auto binding=juce::JSON::parse(R"({"id":7,"paramId":"gen_boost_gain","flags":32,"src":{"type":0,"num":11,"vendor":"keep"},"curve":{"id":0,"p":1.5,"vendor":19}})");binding.getDynamicObject()->setProperty("instanceId",model.getOrder()[0].toString());CHECK(model.apply(cmd::SetMidiBinding{cmd::SetMidiBinding::Op::Add,binding}));
    MappingCallout editor(model,binding);editor.setSize(340,180);juce::ComboBox* source=nullptr;juce::Slider* number=nullptr;juce::TextButton* apply=nullptr;juce::Viewport* scrolling=nullptr;
    std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* child:parent.getChildren()){if(child->getTitle()=="MIDI source")source=dynamic_cast<juce::ComboBox*>(child);if(child->getTitle()=="CC / note number")number=dynamic_cast<juce::Slider*>(child);if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Apply mapping")apply=button;if(auto* viewport=dynamic_cast<juce::Viewport*>(child))scrolling=viewport;collect(*child);}};collect(editor);
    CHECK(source&&number&&apply&&scrolling);if(!source||!number||!apply||!scrolling)return;CHECK(scrolling->getViewedComponent()->getHeight()>scrolling->getHeight());source->setSelectedId(15,juce::sendNotificationSync);number->setValue(2,juce::sendNotificationSync);apply->onClick();const auto result=model.getBoardSettings().midi["bindings"][0];CHECK(result["src"]["type"].toString()=="virtualExpression");CHECK((int)result["src"]["num"]==2);CHECK(result["src"]["vendor"].toString()=="keep");CHECK((int)result["curve"]["vendor"]==19);CHECK_NEAR((double)result["curve"]["p"],1.5,1e-6);
}
LUTHIER_TEST(RefPedalUiFaces, slotFootswitchModeUsesModelAndPreservesUnknownMetadata)
{
    PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;auto pedal=*model.getInstance(model.getOrder()[0]);pedal.extras=juce::JSON::parse(R"({"vendor":17,"midi":{"vendorMidi":true,"chan":3,"useProfile":true}})");CHECK(model.replaceAll({pedal},model.getBoardSettings()));BoardCanvas canvas(model);auto* view=canvas.viewFor(pedal.instanceId);juce::ComboBox* mode=nullptr;
    std::function<void(juce::Component&)> find=[&](auto& parent){for(auto* child:parent.getChildren()){if(child->getTitle()=="Slot footswitch mode")mode=dynamic_cast<juce::ComboBox*>(child);find(*child);}};find(*view);CHECK(mode!=nullptr);if(!mode)return;CHECK(mode->isVisible());mode->setSelectedId(3,juce::sendNotificationSync);const auto changed=model.getInstance(pedal.instanceId);CHECK((int)changed->extras["midi"]["fsMode"]==2);CHECK((int)changed->extras["midi"]["chan"]==3);CHECK((bool)changed->extras["midi"]["vendorMidi"]);CHECK((int)changed->extras["vendor"]==17);
}
#include "../ParamMeta.h"
LUTHIER_TEST(RefPedalUiFaces, boardTempoHasItsRealBeatsPerMinuteUnit)
{
    juce::AudioParameterFloat tempo(juce::ParameterID{"ref_board_tempo_bpm",1},"Reference Board Tempo",juce::NormalisableRange<float>(20,300),120,juce::AudioParameterFloatAttributes().withLabel("BPM"));
    const auto unit=luthier::ParamMeta::getUnit(tempo);CHECK(unit!=luthier::ParamUnit::unknown);CHECK(luthier::ParamMeta::getUnitSuffix(unit)=="BPM");CHECK(luthier::ParamMeta::getUnitName(unit)=="beats per minute");CHECK(luthier::ParamMeta::getCategory(tempo.paramID)=="ref");
}
LUTHIER_TEST(RefPedalUiFaces, positionSelectorUsesRealHostIndexAndDisablesVacantPositions)
{
    FaceAttachmentProcessor processor;juce::AudioProcessorValueTreeState::ParameterLayout layout;layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"ref_s01_pos",1},"Position",0,15,0));juce::AudioProcessorValueTreeState state(processor,nullptr,"position_parameters",std::move(layout));PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;const auto before=model.getOrder();BoardCanvas canvas(model,&state);juce::ComboBox* position=nullptr;
    std::function<void(juce::Component&)> collect=[&](auto& parent){for(auto* child:parent.getChildren()){if(child->getComponentID()=="ref_s01_pos")position=dynamic_cast<juce::ComboBox*>(child);collect(*child);}};collect(*canvas.viewFor(before[0]));CHECK(position!=nullptr);if(!position)return;CHECK(position->getNumItems()==16);CHECK(position->isItemEnabled(1));CHECK(position->isItemEnabled(2));CHECK(!position->isItemEnabled(3));position->setSelectedId(2,juce::sendNotificationSync);CHECK_NEAR(state.getRawParameterValue("ref_s01_pos")->load(),1,1e-6);CHECK(model.getOrder()==before);
}
LUTHIER_TEST(RefPedalUiFaces, physicalSelectorKeepsItsUnclippedStateAfterThemeChange)
{
    PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;BoardCanvas canvas(model);auto* view=canvas.viewFor(model.getOrder()[0]);juce::ComboBox* selector=nullptr;for(auto* child:view->getChildren())if(child->getProperties()["referenceParamId"].toString()=="gen_boost_mode")selector=dynamic_cast<juce::ComboBox*>(child);CHECK(selector!=nullptr);if(!selector)return;selector->lookAndFeelChanged();int labels=0;for(auto* child:selector->getChildren())if(dynamic_cast<juce::Label*>(child)){++labels;CHECK(!child->isVisible());}CHECK(labels>0);
}
LUTHIER_TEST(RefPedalUiFaces, keyboardReorderRecordsAcceptedHostPositionInsideGesture)
{
    FaceAttachmentProcessor processor;juce::AudioProcessorValueTreeState::ParameterLayout layout;layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"ref_s01_pos",1},"Position",0,15,0));juce::AudioProcessorValueTreeState state(processor,nullptr,"reorder_parameters",std::move(layout));PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;const auto before=model.getOrder();BoardCanvas canvas(model,&state);GestureCapture capture;auto* parameter=state.getParameter("ref_s01_pos");parameter->addListener(&capture);CHECK(canvas.viewFor(before[0])->keyPressed({juce::KeyPress::rightKey,juce::ModifierKeys(juce::ModifierKeys::altModifier),0}));CHECK(model.getOrder()[1]==before[0]);CHECK_NEAR(state.getRawParameterValue("ref_s01_pos")->load(),1,1e-6);CHECK(capture.gestures==std::vector<bool>({true,false}));CHECK(model.undo());CHECK(model.getOrder()==before);parameter->removeListener(&capture);
}
LUTHIER_TEST(RefPedalUiFaces, discreteChoiceUsesNormalizedIndexAcrossNaturalOffset)
{
    PedalRegistry::TestScope scope;const char* choices[]{"1","2","3","4","5","6","7","8"};const ParamDesc params[]{{"fixture_memory","MEMORY","",ParamKind::Discrete,Taper::Linear,1,8,1,1,8,choices,PF_Config,0,-1}};const PedalDesc desc{"fixture.discrete","fixture_",1,"Discrete UI fixture","Fixture",nullptr,ChannelMode::Stereo,BypassMode::True,1,params,0,false,false,false,0};CHECK(PedalRegistry::add(desc,[]()->std::unique_ptr<IPedal>{return {};}));BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"fixture.discrete"}))return;BoardCanvas canvas(model);const auto id=model.getOrder()[0];juce::ComboBox* selector=nullptr;
    std::function<void(juce::Component&)> find=[&](auto& parent){for(auto* c:parent.getChildren()){if(c->getProperties()["referenceParamId"].toString()=="fixture_memory")selector=dynamic_cast<juce::ComboBox*>(c);find(*c);}};find(*canvas.viewFor(id));CHECK(selector!=nullptr);if(!selector)return;CHECK(model.apply(cmd::SetParam{id,0,1}));CHECK(selector->getSelectedId()==8);CHECK(model.apply(cmd::SetParam{id,0,0}));CHECK(selector->getSelectedId()==1);
}
LUTHIER_TEST(RefPedalUiFaces, liveDragRecordsOnlyAcceptedFinalPositionAndBalancesEveryExit)
{
    FaceAttachmentProcessor processor;juce::AudioProcessorValueTreeState::ParameterLayout layout;layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"ref_s01_pos",1},"Position",0,15,0));juce::AudioProcessorValueTreeState state(processor,nullptr,"drag_parameters",std::move(layout));PedalRegistry::registerAll();BoardModel model;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;const auto before=model.getOrder();GestureCapture capture;auto* parameter=state.getParameter("ref_s01_pos");parameter->addListener(&capture);
    bool expectChanged=true;auto reversed=before;std::reverse(reversed.begin(),reversed.end());capture.onGesture=[&](bool begin){CHECK(model.getOrder()==(begin||!expectChanged?before:reversed));};
    auto resetCapture=[&]{capture.gestures.clear();capture.valueChanges=0;};
    {
        BoardCanvas canvas(model,&state);auto* view=canvas.viewFor(before[0]);view->onDrag(*view,{500,130},{20,20},false);juce::Thread::sleep(90);view->onDrag(*view,{500,130},{20,20},false);CHECK(model.getOrder()[1]==before[0]);CHECK(capture.valueChanges==0);CHECK(capture.gestures==std::vector<bool>({true}));
        view->onDrag(*view,{500,130},{20,20},true);CHECK(!canvas.dragActive());CHECK(capture.valueChanges==1);CHECK(capture.gestures==std::vector<bool>({true,false}));CHECK_NEAR(state.getRawParameterValue("ref_s01_pos")->load(),1,1e-6);CHECK(model.undo());CHECK(model.getOrder()==before);parameter->setValueNotifyingHost(0);expectChanged=false;resetCapture();
        view->onDrag(*view,{500,130},{20,20},false);juce::Thread::sleep(90);view->onDrag(*view,{500,130},{20,20},false);CHECK(model.getOrder()[1]==before[0]);CHECK(canvas.cancelDrag());CHECK(model.getOrder()==before);CHECK(capture.valueChanges==0);CHECK(capture.gestures==std::vector<bool>({true,false}));CHECK(model.canRedo());resetCapture();
        view->onDrag(*view,{500,130},{20,20},false);view->onDrag(*view,{-20,130},{20,20},true);CHECK(model.getOrder()==before);CHECK(capture.valueChanges==0);CHECK(capture.gestures==std::vector<bool>({true,false}));resetCapture();
        view->onDrag(*view,{100,130},{20,20},false);view->onDrag(*view,{100,130},{20,20},true);CHECK(model.getOrder()==before);CHECK(capture.valueChanges==0);CHECK(capture.gestures==std::vector<bool>({true,false}));CHECK(model.canRedo());resetCapture();
        view->onDrag(*view,{500,130},{20,20},false);
    }
    CHECK(model.getOrder()==before);CHECK(capture.valueChanges==0);CHECK(capture.gestures==std::vector<bool>({true,false}));parameter->removeListener(&capture);
}
