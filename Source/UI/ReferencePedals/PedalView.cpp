// OWNER: WP17 / WP19
#include "PedalView.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
#include "PedalFaceCache.h"
#include "KnobControl.h"
#include "ModelFace.h"
namespace luthier::reference
{
namespace
{
class FaceSelector final : public juce::ComboBox
{
public:
    void setPhysical(bool enabled){physical=enabled;for(auto* child:getChildren())if(dynamic_cast<juce::Label*>(child))child->setVisible(!enabled);}
    void paint(juce::Graphics& g) override {
        if(!physical){juce::ComboBox::paint(g);return;}
        auto body=getLocalBounds().toFloat().reduced(1);g.setColour(Palette::backgroundDeep);g.fillRoundedRectangle(body,2);g.setColour(Palette::textPrimary);g.drawRoundedRectangle(body,2,1);
        const float fraction=getNumItems()>1?(float)getSelectedItemIndex()/(getNumItems()-1):0.f;const float x=body.getX()+4+fraction*juce::jmax(0.f,body.getWidth()-8);g.drawLine(body.getX()+3,body.getCentreY(),body.getRight()-3,body.getCentreY(),1);g.fillEllipse(x-2,body.getCentreY()-3,4,6);
    }
private:bool physical=false;
};
class InspectorHost final : public juce::Component
{
public:
    InspectorHost(juce::Component& body,PedalView& original):content(&body),owner(&original)
    {
        setWantsKeyboardFocus(true);setOpaque(true);setFocusContainerType(FocusContainerType::focusContainer);
        addAndMakeVisible(viewport);viewport.setOpaque(false);viewport.setViewedComponent(&body,false);viewport.setScrollBarsShown(true,false);setSize(350,420);
    }
    ~InspectorHost() override
    {
        viewport.setViewedComponent(nullptr,false);
        if(content&&owner){owner->addChildComponent(content);content->setVisible(false);}
    }
    void paint(juce::Graphics& g) override {g.fillAll(Palette::panel.withAlpha(1.f));}
    void resized() override {viewport.setBounds(getLocalBounds());}
    void focusOfChildComponentChanged(FocusChangeType) override
    {
        auto* focused=juce::Component::getCurrentlyFocusedComponent();if(!focused||!content||!content->isParentOf(focused))return;
        const auto bounds=content->getLocalArea(focused,focused->getLocalBounds());int y=viewport.getViewPositionY();
        if(bounds.getY()<y)y=bounds.getY();else if(bounds.getBottom()>y+viewport.getHeight())y=bounds.getBottom()-viewport.getHeight();viewport.setViewPosition(0,juce::jmax(0,y));
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(!owner)return false;
        if(key.getKeyCode()==juce::KeyPress::escapeKey){auto safe=owner;juce::MessageManager::callAsync([safe]{if(safe)safe->closeInspector(true);});return true;}
        return owner->keyPressed(key);
    }
private:
    juce::Component::SafePointer<juce::Component> content;
    juce::Component::SafePointer<PedalView> owner;
    juce::Viewport viewport;
};
}
PedalView::PedalView(BoardModel& m,InstanceId id,juce::AudioProcessorValueTreeState* p):model(m),instanceId(id),parameters(p)
{
    setWantsKeyboardFocus(true);setFocusContainerType(FocusContainerType::focusContainer);
    addAndMakeVisible(details);addAndMakeVisible(footswitch);addChildComponent(display);addChildComponent(inspector);inspector.addAndMakeVisible(meterLabel);meterLabel.setTitle("Pedal DSP readback");display.setTitle("Looper DSP display");display.setJustificationType(juce::Justification::centred);display.setFont(juce::FontOptions(10.f));display.setColour(juce::Label::backgroundColourId,juce::Colours::black);display.setColour(juce::Label::textColourId,juce::Colours::white);
    for(auto* c:std::array<juce::Component*,9>{&placement,&mounting,&channel,&inputTrim,&outputTrim,&lane,&profile,&provenance,&footMode})inspector.addAndMakeVisible(c);
    inputLabel.setText("Input trim (dB)",juce::dontSendNotification);outputLabel.setText("Output trim (dB)",juce::dontSendNotification);laneLabel.setText("Routing lane (0 = first)",juce::dontSendNotification);
    for(auto* label:{&inputLabel,&outputLabel,&laneLabel})inspector.addAndMakeVisible(label);
    details.setTitle("Pedal information and extended controls");details.setTooltip("Estimated circuit model. Open all controls, placement, mounting and MIDI settings.");details.onClick=[this]{showInspector();};
    footswitch.setClickingTogglesState(true);
    footswitch.onClick=[this]{if(refreshing)return;if(learnMode&&onLearn){onLearn(instanceId,-1,false);refresh();}else if(!enabledAttachment)post(cmd::SetEnabled{instanceId,footswitch.getToggleState()});};
    placement.addItem("PRE - before effects",1);placement.addItem("LOOP - before cabinet",2);placement.addItem("POST - after cabinet",3);placement.setTitle("Pedal placement");
    placement.onChange=[this]{if(!refreshing)post(cmd::SetPlacement{instanceId,(Placement)(placement.getSelectedId()-1)});};
    for(int i=0;i<=7;++i)mounting.addItem(toString((Mounting)i),i+1);
    mounting.setTitle("Pedal mounting");mounting.onChange=[this]{if(!refreshing)post(cmd::SetMounting{instanceId,(Mounting)(mounting.getSelectedId()-1)});};
    channel.addItem("MIDI: no explicit channel",1);for(int i=1;i<=16;++i)channel.addItem("MIDI channel "+juce::String(i),i+1);
    channel.setTitle("Pedal MIDI channel");channel.onChange=[this]{if(!refreshing)post(cmd::SetSlotChannel{instanceId,channel.getSelectedId()-1,profile.getToggleState()});};
    footMode.addItem("Footswitch: Latch",1);footMode.addItem("Footswitch: Momentary",2);footMode.addItem("Footswitch: Auto",3);footMode.setTitle("Slot footswitch mode");footMode.onChange=[this]{if(!refreshing)post(cmd::SetSlotChannel{instanceId,channel.getSelectedId()-1,profile.getToggleState(),footMode.getSelectedId()-1});};
    profile.onClick=[this]{if(!refreshing)post(cmd::SetSlotChannel{instanceId,channel.getSelectedId()-1,profile.getToggleState()});};
    for(auto* s:{&inputTrim,&outputTrim,&lane}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);}
    inputTrim.setRange(-12,12,.1);outputTrim.setRange(-12,12,.1);lane.setRange(0,3,1);
    inputTrim.setTitle("Input trim dB");outputTrim.setTitle("Output trim dB");lane.setTitle("Routing lane");
    inputTrim.setTextValueSuffix(" dB");outputTrim.setTextValueSuffix(" dB");
    inputTrim.onDragStart=[this]{++gesture;};outputTrim.onDragStart=inputTrim.onDragStart;
    inputTrim.onValueChange=[this]{if(!refreshing&&!trimAttached[0])post(cmd::SetTrim{instanceId,(float)inputTrim.getValue(),(float)outputTrim.getValue(),gesture});};
    outputTrim.onValueChange=[this]{if(!refreshing&&!trimAttached[1])post(cmd::SetTrim{instanceId,(float)inputTrim.getValue(),(float)outputTrim.getValue(),gesture});};lane.onValueChange=[this]{if(!refreshing)post(cmd::SetLane{instanceId,(int)lane.getValue()});};
    provenance.setJustificationType(juce::Justification::topLeft);refresh();
}
PedalView::~PedalView()
{
    onFocus=nullptr;onLearn=nullptr;onDrag=nullptr;onCancelDrag=nullptr;onNavigate=nullptr;
    closeInspector();
    clearAttachments();
}
void PedalView::clearAttachments(){enabledAttachment.reset();buttonAttachments.clear();comboAttachments.clear();sliderAttachments.clear();attached.fill(false);trimAttached.fill(false);}
std::unique_ptr<juce::AccessibilityHandler> PedalView::createAccessibilityHandler(){return std::make_unique<juce::AccessibilityHandler>(*this,juce::AccessibilityRole::group);}
void PedalView::post(BoardCommand command)
{
    if(std::holds_alternative<cmd::SetParam>(command)||std::holds_alternative<cmd::SetEnabled>(command)||std::holds_alternative<cmd::SetTrim>(command)||std::holds_alternative<cmd::SetSlotChannel>(command))
    {model.apply(command);return;}
    juce::Component::SafePointer<PedalView> self(this);
    juce::MessageManager::callAsync([self,command=std::move(command)]{if(self)self->model.apply(command);});
}
void PedalView::rebuildControls(const PedalInstance& instance)
{
    clearAttachments();controls.clear();selectors.clear();actions.clear();switches.clear();labels.clear();parameterControls.fill(nullptr);controlParams.clear();selectorParams.clear();actionParams.clear();switchParams.clear();
    modelId=instance.modelId;descriptor=PedalRegistry::find(modelId.c_str());face=PedalFaceFactory::make(modelId.c_str());face->layout(faceLayout);
    if(descriptor)for(int i=0;i<descriptor->numParams;++i)
    {
        const auto& param=descriptor->params[i];if(param.has(PF_Engage))continue;
        auto* label=labels.add(new juce::Label());label->setText(param.label,juce::dontSendNotification);label->setFont(juce::FontOptions(12.f));inspector.addAndMakeVisible(label);
        const auto title=juce::String(descriptor->displayName)+" "+param.label;
        if(param.kind==ParamKind::Toggle)
        {
            auto* button=switches.add(new juce::ToggleButton(param.label));switchParams.push_back(i);parameterControls[(size_t)i]=button;button->setTitle(title);
            button->onClick=[this,button,i]{if(refreshing)return;if(learnMode&&onLearn){onLearn(instanceId,i,false);refresh();}else if(!attached[(size_t)i])post(cmd::SetParam{instanceId,i,button->getToggleState()?1.f:0.f});};
        }
        else if(param.kind==ParamKind::Momentary||param.kind==ParamKind::Trigger)
        {
            auto* button=actions.add(new FootswitchButton(param.label));actionParams.push_back(i);parameterControls[(size_t)i]=button;button->setTitle(title);
            button->onStateChange=[this,button,i,wasDown=false]() mutable
            {
                const bool down=button->isDown();if(down==wasDown)return;wasDown=down;if(refreshing)return;
                if(learnMode&&onLearn){if(down)onLearn(instanceId,i,false);}else post(cmd::SetParam{instanceId,i,down?1.f:0.f});
            };
        }
        else if(param.isDiscreteKind()&&param.choices)
        {
            auto* selector=selectors.add(new FaceSelector());selectorParams.push_back(i);parameterControls[(size_t)i]=selector;
            for(int n=0;n<param.steps;++n)selector->addItem(param.choices[n],n+1);selector->setTitle(title);
            selector->onChange=[this,selector,i]{if(refreshing)return;if(learnMode&&onLearn)onLearn(instanceId,i,false);else if(!attached[(size_t)i])post(cmd::SetParam{instanceId,i,(float)(selector->getSelectedId()-1)/(descriptor->params[i].steps-1)});};
        }
        else
        {
            auto* slider=controls.add(new KnobControl());controlParams.push_back(i);parameterControls[(size_t)i]=slider;
            slider->setRange(0,1,param.kind==ParamKind::Toggle?1:.001);slider->setDoubleClickReturnValue(true,param.defaultNormalised());slider->setTitle(title);
            slider->valueFromTextFunction=[this,i](const juce::String& text){return descriptor->params[i].toNormalised(text.getDoubleValue());};
            slider->textFromValueFunction=[this,i](double norm){const auto& d=descriptor->params[i];return juce::String(d.fromNormalised(norm),d.max-d.min>100?0:2)+" "+d.unit;};
            slider->onDragStart=[this,i]{++gesture;if(learnMode&&onLearn)onLearn(instanceId,i,true);};
            slider->onValueChange=[this,slider,i]{slider->setTooltip(slider->getTitle()+": "+slider->getTextFromValue(slider->getValue())+". Enter or right click edits the value.");if(refreshing)return;if(learnMode&&onLearn){onLearn(instanceId,i,true);refresh();}else if(!attached[(size_t)i])post(cmd::SetParam{instanceId,i,(float)slider->getValue(),gesture});};
        }
        auto* component=parameterControls[(size_t)i];component->getProperties().set("referenceParamId",param.id);component->getProperties().set("referenceParamIndex",i);component->setComponentID(param.id);inspector.addAndMakeVisible(component);
    }
    if(parameters&&instance.slot>=0)
    {
        auto& state=*parameters;const auto prefix="ref_s"+juce::String(instance.slot+1).paddedLeft('0',2)+"_";const auto& map=PedalRegistry::laneMap(modelId.c_str());
        for(int i=0;descriptor&&i<descriptor->numParams;++i)
        {
            const int laneIndex=map.paramToLane[i];auto* component=parameterControls[(size_t)i];if(laneIndex<0||!component)continue;
            const auto parameter=prefix+"p"+juce::String(laneIndex+1).paddedLeft('0',2);if(!state.getParameter(parameter))continue;
            attached[(size_t)i]=true;component->setComponentID(parameter);
            if(auto* slider=dynamic_cast<juce::Slider*>(component))sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,parameter,*slider));
            else if(auto* selector=dynamic_cast<juce::ComboBox*>(component))comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,parameter,*selector));
            else if(auto* button=dynamic_cast<juce::ToggleButton*>(component))buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,parameter,*button));
            else attached[(size_t)i]=false;
        }
        if(state.getParameter(prefix+"enabled")){footswitch.setComponentID(prefix+"enabled");enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,prefix+"enabled",footswitch);}
        for(auto entry:std::array<std::pair<juce::Slider*,juce::String>,2>{{{&inputTrim,"in_db"},{&outputTrim,"out_db"}}})if(state.getParameter(prefix+entry.second)){trimAttached[entry.first==&inputTrim?0:1]=true;entry.first->setComponentID(prefix+entry.second);sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,prefix+entry.second,*entry.first));}
    }
    // Slot lanes are normalised host parameters. Restore the pedal's physical
    // formatter/default after attachment construction replaces Slider callbacks.
    for(int n=0;n<controls.size();++n)
    {
        const int index=controlParams[(size_t)n];auto* slider=controls[n];const auto& d=descriptor->params[index];
        slider->setDoubleClickReturnValue(true,d.defaultNormalised());
        slider->valueFromTextFunction=[this,index](const juce::String& text){return descriptor->params[index].toNormalised(text.getDoubleValue());};
        slider->textFromValueFunction=[this,index](double norm){const auto& param=descriptor->params[index];return juce::String(param.fromNormalised(norm),param.max-param.min>100?0:2)+" "+param.unit;};
    }
    provenance.setText(juce::String(descriptor?descriptor->displayName:modelId.c_str())+"\n"+(descriptor&&descriptor->inspiredBy?"Inspired by "+juce::String(descriptor->inspiredBy)+"; independent and unaffiliated.":"Original Luthier pedal.")+"\nCircuit calibration: ESTIMATED. Physical dimensions follow the documented/estimated artwork specification.",juce::dontSendNotification);
    placeControls();
}
void PedalView::refresh()
{
    const auto oldTitle=getTitle();const auto instance=model.getInstance(instanceId);if(!instance)return;const juce::ScopedValueSetter<bool> guard(refreshing,true);
    if(modelId!=instance->modelId||!face)rebuildControls(*instance);
    for(int n=0;n<controls.size();++n){const int index=controlParams[(size_t)n];if(!attached[(size_t)index])controls[n]->setValue(instance->params[(size_t)index],juce::dontSendNotification);}
    for(int n=0;n<selectors.size();++n){const int index=selectorParams[(size_t)n];if(!attached[(size_t)index])selectors[n]->setSelectedId(1+(int)descriptor->params[index].fromNormalised(instance->params[(size_t)index]),juce::dontSendNotification);}
    for(int n=0;n<switches.size();++n){const int index=switchParams[(size_t)n];if(!attached[(size_t)index])switches[n]->setToggleState(instance->params[(size_t)index]>=.5f,juce::dontSendNotification);}
    if(!enabledAttachment)footswitch.setToggleState(instance->enabled,juce::dontSendNotification);
    footswitch.setEnabled(descriptor&&!instance->missing);footswitch.setTitle("Bypass "+juce::String(descriptor?descriptor->displayName:modelId.c_str()));
    placement.setSelectedId((int)instance->placement+1,juce::dontSendNotification);mounting.setSelectedId((int)instance->mounting+1,juce::dontSendNotification);
    const auto midi=instance->extras.getProperty("midi",juce::var());channel.setSelectedId(1+(int)midi.getProperty("chan",0),juce::dontSendNotification);profile.setToggleState(midi.getProperty("useProfile",false),juce::dontSendNotification);footMode.setSelectedId(1+(int)midi.getProperty("fsMode",0),juce::dontSendNotification);
    if(!trimAttached[0])inputTrim.setValue(instance->inDb,juce::dontSendNotification);if(!trimAttached[1])outputTrim.setValue(instance->outDb,juce::dontSendNotification);lane.setValue(instance->lane,juce::dontSendNotification);
    const auto order=model.getOrder();const int position=(int)std::distance(order.begin(),std::find(order.begin(),order.end(),instanceId));setExplicitFocusOrder(position+10);
    setTitle(juce::String(descriptor?descriptor->displayName:modelId.c_str())+", position "+juce::String(position+1)+" of "+juce::String((int)order.size())+", "+(instance->enabled?"on":"off")+", "+placement.getText());
    setDescription("Alt arrows reorder. Space bypasses. Delete removes. I opens pedal information. "+meterSummary());if(!oldTitle.isEmpty()&&oldTitle!=getTitle())if(auto* handler=getAccessibilityHandler())handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);resized();repaint();
}
juce::Point<int> PedalView::physicalSize() const {const auto size=face?face->designSizeMm()*1.6f:juce::Point<float>(120,200);return {juce::roundToInt(size.x),juce::roundToInt(size.y)};}
juce::Point<float> PedalView::inputJack() const {for(const auto& jack:faceLayout.jacks)if(jack.kind==FaceJack::Kind::In)return jack.position;return {0,getHeight()*.42f};}
juce::Point<float> PedalView::outputJack() const {for(const auto& jack:faceLayout.jacks)if(jack.kind==FaceJack::Kind::Out)return jack.position;return {(float)getWidth(),getHeight()*.42f};}
void PedalView::placeControls()
{
    if(!face)return;face->layout(faceLayout);std::array<const FaceControl*,kMaxParams> physical {};
    for(const auto& control:faceLayout.controls)if(control.paramIdx>=0&&control.paramIdx<kMaxParams)physical[(size_t)control.paramIdx]=&control;
    int y=130,labelIndex=0;bool momentaryMain=false;
    if(descriptor)for(int index=0;index<descriptor->numParams;++index)
    {
        if(descriptor->params[index].has(PF_Engage))continue;auto* component=parameterControls[(size_t)index];auto* label=labels[labelIndex++];if(!component)continue;
        if(const auto* position=physical[(size_t)index])
        {
            addAndMakeVisible(component);label->setVisible(false);if(auto* selector=dynamic_cast<FaceSelector*>(component)){selector->setPhysical(true);selector->setTooltip(selector->getTitle()+": "+selector->getText());}if(auto* toggle=dynamic_cast<juce::ToggleButton*>(component))toggle->setButtonText("");
            if(auto* button=dynamic_cast<FootswitchButton*>(component)){button->setFaceGeometry(position->bounds);if(position->bounds==faceLayout.footswitch)momentaryMain=true;}
            else{component->setBounds(position->bounds.toNearestInt());if(auto* slider=dynamic_cast<juce::Slider*>(component)){slider->setSliderStyle(position->kind==FaceControlKind::Treadle?juce::Slider::LinearVertical:juce::Slider::RotaryHorizontalVerticalDrag);slider->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);slider->setPopupDisplayEnabled(true,true,nullptr);slider->setTooltip(slider->getTitle()+": "+slider->getTextFromValue(slider->getValue())+". Enter or right click edits the value.");if(position->kind==FaceControlKind::Treadle){slider->getProperties().set("physicalTreadle",true);slider->setBounds(10,34,getWidth()-20,juce::jmax(40,getHeight()-110));}}}
        }
        else
        {
            inspector.addAndMakeVisible(component);label->setVisible(true);if(auto* selector=dynamic_cast<FaceSelector*>(component))selector->setPhysical(false);if(auto* toggle=dynamic_cast<juce::ToggleButton*>(component))toggle->setButtonText(descriptor->params[index].label);label->setBounds(8,y,320,18);y+=18;component->setBounds(8,y,320,30);y+=36;
            if(auto* slider=dynamic_cast<juce::Slider*>(component)){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,80,22);}
        }
    }
    provenance.setBounds(8,6,320,88);meterLabel.setBounds(8,94,320,28);meterLabel.setText(meterSummary(),juce::dontSendNotification);
    const auto* faceModel=dynamic_cast<ModelFace*>(face.get());const bool lcd=faceModel&&faceModel->faceProfile().detail==FaceProfile::Detail::LooperDisplay;display.setVisible(lcd);if(lcd){display.setBounds(21,16,75,24);display.setText(meterSummary().isEmpty()?"NO READBACK":meterSummary(),juce::dontSendNotification);display.toFront(false);}
    for(auto* component:std::array<juce::Component*,7>{&placement,&mounting,&inputTrim,&outputTrim,&lane,&channel,&profile}){juce::Label* label=component==&inputTrim?&inputLabel:component==&outputTrim?&outputLabel:component==&lane?&laneLabel:nullptr;if(label){label->setBounds(8,y,320,18);y+=18;}component->setBounds(8,y,320,28);y+=32;}
    bool descriptorMode=false;for(int i=0;descriptor&&i<descriptor->numParams;++i)if(juce::String(descriptor->params[i].id).endsWith("_fs_mode"))descriptorMode=true;footMode.setVisible(!descriptorMode);if(!descriptorMode){footMode.setBounds(8,y,320,28);y+=32;}
    if(momentaryMain){inspector.addAndMakeVisible(footswitch);footswitch.setFaceGeometry({8.f,(float)y,320.f,36.f});y+=40;}
    else{addAndMakeVisible(footswitch);footswitch.setFaceGeometry(faceLayout.footswitch);}
    inspector.setSize(340,y+8);if(details.getParentComponent()==this)details.setBounds(juce::jmax(0,getWidth()-39),juce::jmax(0,getHeight()-16),37,14);details.toFront(false);
}
void PedalView::resized(){placeControls();}
void PedalView::placeCaptionIn(juce::Component& host)
{
    host.addAndMakeVisible(details);details.setButtonText("EST | Info");details.setTitle(getTitle()+", estimated circuit, information and all controls");
    details.setBounds(getX(),getBottom()+6,getWidth(),18);
}
void PedalView::InspectorDismissal::mouseDown(const juce::MouseEvent& event)
{
    if(!owner.inspectorCallout||juce::Component::getCurrentlyModalComponent())return;auto* target=event.originalComponent;
    if(target==&owner.details||owner.details.isParentOf(target)||target==owner.inspectorCallout.get()||owner.inspectorCallout->isParentOf(target))return;
    owner.closeInspector();
}
void PedalView::showInspector()
{
    if(inspectorCallout){closeInspector();return;}
    auto* parent=getTopLevelComponent();const auto target=parent->getLocalArea(&details,details.getLocalBounds());
    inspectorHost=std::make_unique<InspectorHost>(inspector,*this);
    inspectorHost->setSize(juce::jlimit(160,350,parent->getWidth()-32),juce::jlimit(120,420,parent->getHeight()-48));
    inspectorCallout=std::make_unique<juce::CallOutBox>(*inspectorHost,target,parent);
    // Explicit ownership avoids the modal asynchronous CallOutBox callback and
    // guarantees the inspector cannot outlive its pedal/model or hide a timer.
    juce::Desktop::getInstance().addGlobalMouseListener(&inspectorDismissal);
    inspectorCallout->setVisible(true);inspectorCallout->toFront(false);inspectorHost->grabKeyboardFocus();
}
void PedalView::closeInspector(bool restoreFocus)
{
    if(!inspectorCallout)return;juce::Desktop::getInstance().removeGlobalMouseListener(&inspectorDismissal);
    inspectorCallout.reset();inspectorHost.reset();if(restoreFocus&&isShowing())grabKeyboardFocus();
}
void PedalView::paint(juce::Graphics& g)
{
    AnimationPolicy::notePaint(*this);const auto instance=model.getInstance(instanceId);if(!instance)return;
    FaceState state;state.enabled=meterSeen?(meters.present&&meters.enabled):instance->enabled;state.tailActive=meterSeen&&meters.present&&meters.tailEnergyDb>-90;state.missing=instance->missing||!descriptor;state.highContrast=!Palette::textured;
    if(face){const auto image=PedalFaceCache::get(*face,modelId.c_str(),descriptor?descriptor->revision:1,{getWidth(),getHeight()},g.getInternalContext().getPhysicalPixelScaleFactor(),state);g.drawImage(image,getLocalBounds().toFloat());}
    if(!state.enabled){if(Palette::textured){g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRoundedRectangle(getLocalBounds().toFloat(),5);}else{g.setColour(Palette::textPrimary);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2),4,1);}}
    if(!faceLayout.led.isEmpty()){g.setColour(state.enabled&&!state.missing?Palette::clip:Palette::clip.darker(.75f));g.fillEllipse(faceLayout.led);}
    const auto* faceModel=dynamic_cast<ModelFace*>(face.get());
    if(faceModel&&faceModel->faceProfile().detail==FaceProfile::Detail::LooperRing)
    {
        const auto centre=faceModel->faceProfile().ledMm*1.6f;const float radius=24;const double progress=meters.present&&(meters.model.available&UiLoop)&&meters.model.loopLength>0?(double)meters.model.playhead/meters.model.loopLength:0;
        for(int segment=0;segment<24;++segment){const float angle=(segment/24.f)*juce::MathConstants<float>::twoPi;const auto point=centre+juce::Point<float>(std::sin(angle),-std::cos(angle))*radius;const bool lit=meters.present&&(meters.model.available&UiLoop)&&segment<(int)(juce::jlimit(0.,1.,progress)*24);g.setColour(lit?Palette::accentBright:Palette::edge);g.fillEllipse(point.x-2,point.y-2,4,4);}
    }
    if(meterSeen&&meters.present&&(meters.model.available&UiTransport)&&!faceLayout.led.isEmpty()){g.setColour(meters.model.transport==1?juce::Colours::red:meters.model.transport==3?juce::Colours::orange:meters.model.transport==2?juce::Colours::limegreen:Palette::edge);g.fillEllipse(faceLayout.led);}
    if(state.tailActive&&(modelId=="generic.delay"||modelId=="generic.reverb")){g.setColour(Palette::warning);g.fillEllipse(faceLayout.led.translated(9,0));}
    if(state.missing){g.setColour(Palette::warning);g.setFont(juce::FontOptions(11.f));g.drawFittedText("Missing model\n"+juce::String(modelId),getLocalBounds().reduced(6),juce::Justification::centred,4);}
    if(hasKeyboardFocus(true)||learnMode){g.setColour(Palette::accentBright);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),5,Palette::textured?2.f:3.f);}
}
void PedalView::focusGained(FocusChangeType){repaint();if(onFocus)onFocus(*this);}
bool PedalView::keyPressed(const juce::KeyPress& key)
{
    const auto code=key.getKeyCode();const auto mods=key.getModifiers();auto order=model.getOrder();const auto it=std::find(order.begin(),order.end(),instanceId);const int pos=(int)std::distance(order.begin(),it);
    if(code==juce::KeyPress::escapeKey&&onCancelDrag&&onCancelDrag()){dragging=false;return true;}
    if(mods.isAltDown())
    {
        if(mods.isShiftDown()&&(code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey))
        {const auto item=model.getInstance(instanceId);model.apply(cmd::SetPlacement{instanceId,(Placement)juce::jlimit(0,2,(int)item->placement+(code==juce::KeyPress::leftKey?-1:1))});return true;}
        if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey || code==juce::KeyPress::homeKey || code==juce::KeyPress::endKey)
        {int n=code==juce::KeyPress::homeKey?0:code==juce::KeyPress::endKey?(int)order.size()-1:pos+(code==juce::KeyPress::leftKey?-1:1);model.apply(cmd::Move{instanceId,juce::jlimit(0,(int)order.size()-1,n)});return true;}
        if(code==juce::KeyPress::upKey||code==juce::KeyPress::downKey){const auto i=model.getInstance(instanceId);model.apply(cmd::SetLane{instanceId,juce::jlimit(0,3,i->lane+(code==juce::KeyPress::upKey?-1:1))});return true;}
    }
    if(!mods.isCommandDown()&&(code=='I'||code=='i')){showInspector();return true;}
    if(code==juce::KeyPress::deleteKey||code==juce::KeyPress::backspaceKey){post(cmd::Remove{instanceId});return true;}
    if(mods.isCommandDown()&&(code=='D'||code=='d')){post(cmd::Duplicate{instanceId});return true;}
    if(code==juce::KeyPress::spaceKey||code==juce::KeyPress::returnKey)
    {
        for(const auto& control:faceLayout.controls)if(control.kind==FaceControlKind::Button&&control.bounds==faceLayout.footswitch)
            if(auto* button=dynamic_cast<juce::Button*>(parameterControls[(size_t)control.paramIdx])){button->triggerClick();return true;}
        footswitch.triggerClick();return true;
    }
    if(!mods.isCommandDown()&&(code=='l'||code=='L'))
    {
        auto* focused=juce::Component::getCurrentlyFocusedComponent();
        for(int i=0;i<kMaxParams;++i)if(auto* control=parameterControls[(size_t)i])if(control==focused||control->isParentOf(focused))
        {if(onLearn)onLearn(instanceId,i,descriptor->params[i].kind==ParamKind::Continuous);return true;}
        for(const auto& control:faceLayout.controls)if(control.kind==FaceControlKind::Button&&control.bounds==faceLayout.footswitch){if(onLearn)onLearn(instanceId,control.paramIdx,false);return true;}
        requestFootswitchLearn();return true;
    }
    if(code==juce::KeyPress::F10Key&&mods.isShiftDown()){contextMenu();return true;}
    if(code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey){if(onNavigate)onNavigate(instanceId,code==juce::KeyPress::leftKey?-1:1);return true;}
    return false;
}
void PedalView::mouseDown(const juce::MouseEvent& e){if(e.mods.isPopupMenu()){contextMenu();return;}grabOffset=e.position.toInt();grabKeyboardFocus();}
void PedalView::mouseDrag(const juce::MouseEvent& e)
{
    if(e.getDistanceFromDragStart()<4)return;dragging=true;
    if(onDrag)onDrag(*this,e.getEventRelativeTo(getParentComponent()).position.toInt(),grabOffset,false);
}
void PedalView::mouseUp(const juce::MouseEvent& e){if(dragging){dragging=false;if(onDrag)onDrag(*this,e.getEventRelativeTo(getParentComponent()).position.toInt(),grabOffset,true);}}
void PedalView::contextMenu()
{
    juce::PopupMenu menu;menu.addItem(1,"Move earlier");menu.addItem(2,"Move later");menu.addItem(3,"Duplicate");menu.addItem(4,"Remove");menu.addItem(5,"Learn footswitch");
    juce::Component::SafePointer<PedalView> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[self](int n){if(!self)return;const auto order=self->model.getOrder();const int pos=(int)std::distance(order.begin(),std::find(order.begin(),order.end(),self->instanceId));
        if(n==1||n==2)self->model.apply(cmd::Move{self->instanceId,juce::jlimit(0,(int)order.size()-1,pos+(n==1?-1:1))});
        if(n==3)self->post(cmd::Duplicate{self->instanceId});if(n==4)self->post(cmd::Remove{self->instanceId});if(n==5)self->requestFootswitchLearn();});
}
}

namespace luthier::reference
{
juce::String PedalView::meterSummary() const
{
    if(!meterSeen)return {};
    if(!meters.present)return "DSP data pending";
    juce::StringArray parts;const auto& data=meters.model;
    if(data.available&UiTransport){const char* names[]{"EMPTY","REC","PLAY","OVERDUB","STOP"};parts.add(data.transport>=0&&data.transport<5?names[data.transport]:"TRANSPORT "+juce::String(data.transport));}
    if(data.available&UiLoop)parts.add(juce::String((juce::int64)data.playhead)+"/"+juce::String((juce::int64)data.loopLength)+" samples");
    if((data.available&UiTempo)&&std::isfinite(data.bpm)&&data.bpm>0)parts.add(juce::String(data.bpm,1)+" BPM");
    if((data.available&UiFaults)&&data.faults!=0)parts.add("FAULT "+juce::String::toHexString((int)data.faults));
    return parts.isEmpty()?"DSP active":parts.joinIntoString(" | ");
}
bool PedalView::updateMeters(const PedalMeters& next)
{
    if(next.present&&next.identity!=instanceId)return false;
    const auto& a=meters.model;const auto& b=next.model;
    if(meterSeen&&meters.present==next.present&&meters.enabled==next.enabled&&meters.tailEnergyDb==next.tailEnergyDb&&a.available==b.available&&a.transport==b.transport&&a.loopLength==b.loopLength&&a.playhead==b.playhead&&a.bpm==b.bpm&&a.progress==b.progress&&a.faults==b.faults)return false;
    const bool announce=!meterSeen||meters.enabled!=next.enabled||meters.model.transport!=next.model.transport||meters.model.faults!=next.model.faults;
    meters=next;meterSeen=true;const auto text=meterSummary();setDescription("Alt arrows reorder. Space bypasses. Delete removes. I opens pedal information. "+text);meterLabel.setText(text,juce::dontSendNotification);juce::String compact=text;if(next.present){juce::StringArray lines;const auto& data=next.model;if(data.available&UiTransport){const char* names[]{"EMPTY","REC","PLAY","OVERDUB","STOP"};lines.add(data.transport>=0&&data.transport<5?names[data.transport]:"STATE");}if((data.available&UiTempo)&&std::isfinite(data.bpm)&&data.bpm>0)lines.add(juce::String(data.bpm,1)+" BPM");else if((data.available&UiLoop)&&data.loopLength>0)lines.add(juce::String((int)juce::jlimit(0.,100.,100.*data.playhead/data.loopLength))+"%");compact=lines.isEmpty()?"NO DATA":lines.joinIntoString(" ");}display.setText(compact,juce::dontSendNotification);display.setTooltip(text);if(announce)if(auto* handler=getAccessibilityHandler())handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);repaint();return true;
}
}

namespace luthier::reference {
void PedalView::requestFootswitchLearn(){for(const auto& control:faceLayout.controls)if(control.kind==FaceControlKind::Button&&control.bounds==faceLayout.footswitch){if(onLearn)onLearn(instanceId,control.paramIdx,false);return;}if(onLearn)onLearn(instanceId,-1,false);}
}
