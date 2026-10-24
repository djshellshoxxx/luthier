// OWNER: WP22
#include "BoardMidiPanel.h"
#include "MappingCallout.h"
#include "../../ReferencePedals/Midi/ExtendedHostAutomation.h"
#include "../../ReferencePedals/PedalRegistry.h"
namespace luthier::reference
{
namespace {
struct KillGesture final {
    explicit KillGesture(juce::RangedAudioParameter& p):parameter(p){}
    ~KillGesture(){release();}
    void change(bool next){if(next==down)return;if(next)parameter.beginChangeGesture();down=next;parameter.setValueNotifyingHost(down?1.f:0.f);if(!down)parameter.endChangeGesture();}
    void release(){if(down)change(false);}
    juce::RangedAudioParameter& parameter;bool down=false;
};
}

BoardMidiPanel::BoardMidiPanel(BoardModel& m,juce::AudioProcessorValueTreeState* parameters):model(m)
{
    setWantsKeyboardFocus(true);setFocusContainerType(FocusContainerType::focusContainer);addAndMakeVisible(viewport);viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);
    const auto s=model.getBoardSettings();
    if(parameters&&parameters->getParameter("ref_board_mix_law"))addHostControls(*parameters);else {
    scalar("midiChan","Board MIDI channel (0 = omni)",0,16,s.midiChan);
    scalar("tempoSource","Tempo: 0 internal / 1 host / 2 MIDI / 3 auto",0,3,(int)s.tempoSource);
    scalar("cpuCapPct","CPU budget percent",1,100,s.cpuCapPct);
    scalar("limiterCeilDb","Limiter ceiling dB",-6,6,s.limiterCeilDb,.1);
    toggle("limiterOn","Output limiter",s.limiterOn);toggle("reportLatency","Report latency to host",s.reportLatency);
    toggle("trails","Preserve bypass tails",s.trails);toggle("mixTail","Preserve tails when mix is dry",s.mixTail);
    scalar("numLanes","Parallel lanes",2,4,s.lanes.numLanes);scalar("parallelNorm","Parallel normalisation: 0 sqrt / 1 unity / 2 none",0,2,(int)s.lanes.norm);
    for(int i=0;i<kMaxBranches;++i){const auto suffix=juce::String(i);scalar(("laneGainDb"+suffix).toRawUTF8(),("Lane "+juce::String(i+1)+" gain dB").toRawUTF8(),-24,6,s.lanes.gainDb[(size_t)i],.1);toggle(("lanePhase"+suffix).toRawUTF8(),("Lane "+juce::String(i+1)+" invert phase").toRawUTF8(),s.lanes.phaseInvert[(size_t)i]);}
    scalar("abSelect","A/B selection: 0 A / 1 B / 2 A+B / 3 toggle",0,3,(int)s.abSelect);scalar("abOut","A/B output: 0 sum / 1 dual",0,1,(int)s.abOut);
    toggle("reampEnable","Reamp input enabled",s.reampEnable);toggle("reampReplace","Reamp replaces input",s.reampReplace);scalar("reampGainDb","Reamp gain dB",-24,12,s.reampGainDb,.1);
    const auto order=model.getOrder();
    auto choice=[this](const juce::String& title,const juce::StringArray& choices,int selected,std::function<void(int)> changed)
    {
        auto* label=new juce::Label();widgets.add(label);content.addAndMakeVisible(label);label->setText(title,juce::dontSendNotification);label->setBounds(6,y,344,20);y+=20;
        auto* combo=new juce::ComboBox();widgets.add(combo);content.addAndMakeVisible(combo);combo->setTitle(title);combo->addItemList(choices,1);combo->setSelectedId(selected+1,juce::dontSendNotification);combo->setBounds(6,y,344,28);y+=32;combo->onChange=[combo,changed]{changed(combo->getSelectedId()-1);};
    };
    auto customToggle=[this](const juce::String& title,bool initial,std::function<void(bool)> changed)
    {
        auto* b=new juce::ToggleButton(title);widgets.add(b);content.addAndMakeVisible(b);b->setToggleState(initial,juce::dontSendNotification);b->setBounds(6,y,344,28);y+=32;b->onClick=[b,changed]{changed(b->getToggleState());};
    };
    auto customSlider=[this](const juce::String& title,double initial,double lo,double hi,double step,std::function<void(double)> changed)
    {
        auto* label=new juce::Label();widgets.add(label);content.addAndMakeVisible(label);label->setText(title,juce::dontSendNotification);label->setBounds(6,y,344,20);y+=20;
        auto* slider=new juce::Slider();widgets.add(slider);content.addAndMakeVisible(slider);slider->setTitle(title);slider->setRange(lo,hi,step);slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);slider->setValue(initial,juce::dontSendNotification);slider->setBounds(6,y,344,28);y+=32;slider->onValueChange=[slider,changed]{changed(slider->getValue());};
    };
    juce::StringArray pedalNames {"None"};for(const auto& id:order){const auto p=model.getInstance(id);pedalNames.add(juce::String(p->modelId)+" ["+id.toString().substring(0,6)+"]");}
    auto selected=[order](const InstanceId& id){const auto it=std::find(order.begin(),order.end(),id);return it==order.end()?0:1+(int)std::distance(order.begin(),it);};
    for(int i=0;i<kMaxLoops;++i)
    {
        const auto loop=s.loops[(size_t)i];const auto title="Loop "+juce::String(i+1)+" ";
        customToggle(title+"used",loop.used,[this,i](bool v){auto l=model.getBoardSettings().loops[(size_t)i];l.used=v;model.apply(cmd::SetLoop{i,l});});
        customToggle(title+"enabled",loop.enabled,[this,i](bool v){auto l=model.getBoardSettings().loops[(size_t)i];l.enabled=v;model.apply(cmd::SetLoop{i,l});});
        choice(title+"first pedal",pedalNames,selected(loop.first),[this,i,order](int n){auto l=model.getBoardSettings().loops[(size_t)i];l.first=n>0?order[(size_t)n-1]:InstanceId::null();model.apply(cmd::SetLoop{i,l});});
        choice(title+"last pedal",pedalNames,selected(loop.last),[this,i,order](int n){auto l=model.getBoardSettings().loops[(size_t)i];l.last=n>0?order[(size_t)n-1]:InstanceId::null();model.apply(cmd::SetLoop{i,l});});
        choice(title+"kind",{"Serial","Parallel","Kill dry"},(int)loop.kind,[this,i](int n){auto l=model.getBoardSettings().loops[(size_t)i];l.kind=(LoopKind)n;model.apply(cmd::SetLoop{i,l});});
        customSlider(title+"blend",loop.blend,0,1,.001,[this,i](double v){auto l=model.getBoardSettings().loops[(size_t)i];l.blend=(float)v;model.apply(cmd::SetLoop{i,l});});
        customSlider(title+"send dB",loop.sendDb,-24,12,.1,[this,i](double v){auto l=model.getBoardSettings().loops[(size_t)i];l.sendDb=(float)v;model.apply(cmd::SetLoop{i,l});});
        customSlider(title+"return dB",loop.returnDb,-24,12,.1,[this,i](double v){auto l=model.getBoardSettings().loops[(size_t)i];l.returnDb=(float)v;model.apply(cmd::SetLoop{i,l});});
    }
    customToggle("Feedback enabled",s.fb.on,[this](bool v){auto f=model.getBoardSettings().fb;f.on=v;model.apply(cmd::SetFeedback{f});});
    choice("Feedback source",pedalNames,selected(s.fb.src),[this,order](int n){auto f=model.getBoardSettings().fb;f.src=n>0?order[(size_t)n-1]:InstanceId::null();model.apply(cmd::SetFeedback{f});});
    choice("Feedback destination",pedalNames,selected(s.fb.dst),[this,order](int n){auto f=model.getBoardSettings().fb;f.dst=n>0?order[(size_t)n-1]:InstanceId::null();model.apply(cmd::SetFeedback{f});});
    customSlider("Feedback gain",s.fb.gain,0,1.05,.001,[this](double v){auto f=model.getBoardSettings().fb;f.gain=(float)v;model.apply(cmd::SetFeedback{f});});
    customSlider("Feedback low-pass Hz",s.fb.lpHz,20,20000,1,[this](double v){auto f=model.getBoardSettings().fb;f.lpHz=(float)v;model.apply(cmd::SetFeedback{f});});
    customToggle("Allow feedback gain above 0.98",s.fb.runawayOk,[this](bool v){auto f=model.getBoardSettings().fb;f.runawayOk=v;model.apply(cmd::SetFeedback{f});});
    }
    auto* live=new juce::ToggleButton("Live reorder (off = apply on drop)");widgets.add(live);content.addAndMakeVisible(live);live->setTitle("Live audio reorder");live->setToggleState(s.ui.getProperty("liveReorder",true),juce::dontSendNotification);live->setBounds(6,y,344,28);y+=32;live->onClick=[this,live]{auto ui=model.getBoardSettings().ui.clone();if(!ui.isObject())ui=juce::var(new juce::DynamicObject());ui.getDynamicObject()->setProperty("liveReorder",live->getToggleState());model.setUiState(ui);};
    if(const auto* bindings=s.midi.getProperty("bindings",juce::var()).getArray())for(const auto& binding:*bindings)
    {
        auto* edit=new juce::TextButton("Edit MIDI mapping "+binding.getProperty("id",0).toString());widgets.add(edit);content.addAndMakeVisible(edit);edit->setBounds(6,y,344,28);y+=32;
        juce::Component::SafePointer<BoardMidiPanel> owner(this);juce::Component::SafePointer<juce::TextButton> button(edit);
        edit->onClick=[owner,button,binding]{if(!owner||!button)return;auto editor=std::make_unique<MappingCallout>(owner->model,binding);editor->onClose=[owner]{juce::MessageManager::callAsync([owner]{if(owner)owner->mappingPopup.close(true);});};owner->mappingPopup.open(std::move(editor),*owner->getTopLevelComponent(),*button);};
    }
    content.setSize(360,y+8);setSize(380,420);
}
BoardMidiPanel::~BoardMidiPanel(){mappingPopup.close();buttonAttachments.clear();comboAttachments.clear();sliderAttachments.clear();viewport.setViewedComponent(nullptr,false);}
void BoardMidiPanel::scalar(const char* key,const char* title,double lo,double hi,double value,double interval)
{
    auto* label=new juce::Label();widgets.add(label);content.addAndMakeVisible(label);label->setText(title,juce::dontSendNotification);label->setFont(12.f);label->setBounds(6,y,344,20);y+=20;
    auto* slider=new juce::Slider();widgets.add(slider);content.addAndMakeVisible(slider);slider->setTitle(title);slider->setRange(lo,hi,interval);slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,60,20);slider->setValue(value,juce::dontSendNotification);slider->setBounds(6,y,344,26);y+=30;
    const std::string k(key);slider->onValueChange=[this,slider,k]{juce::String reason;if(!model.apply(cmd::SetBoardParam{k,slider->getValue()},&reason))slider->setTooltip(reason);};
}
void BoardMidiPanel::toggle(const char* key,const char* title,bool value)
{
    auto* button=new juce::ToggleButton(title);widgets.add(button);content.addAndMakeVisible(button);button->setTitle(title);button->setToggleState(value,juce::dontSendNotification);button->setBounds(6,y,344,28);y+=30;
    const std::string k(key);button->onClick=[this,button,k]{model.apply(cmd::SetBoardParam{k,button->getToggleState()?1.:0.});};
}
void BoardMidiPanel::resized(){viewport.setBounds(getLocalBounds());}
}

namespace luthier::reference
{
bool BoardMidiPanel::keyPressed(const juce::KeyPress& key){if(key.getKeyCode()!=juce::KeyPress::escapeKey)return false;if(mappingPopup.isOpen())mappingPopup.close(true);else if(onClose)onClose();return true;}
void BoardMidiPanel::focusOfChildComponentChanged(FocusChangeType)
{
    auto* focused=juce::Component::getCurrentlyFocusedComponent();if(!focused||!content.isParentOf(focused))return;const auto r=content.getLocalArea(focused,focused->getLocalBounds());int top=viewport.getViewPositionY();if(r.getY()<top)top=r.getY();else if(r.getBottom()>top+viewport.getViewHeight())top=r.getBottom()-viewport.getViewHeight();viewport.setViewPosition(0,top);
}
void BoardMidiPanel::addHostControls(juce::AudioProcessorValueTreeState& state)
{
    for(const auto& d:extendedHostParameters)
    {
        auto* parameter=state.getParameter(d.id);if(!parameter)continue;
        if(juce::String(d.id)=="ref_board_fb_kill")
        {
            auto* button=new juce::TextButton("Kill feedback (hold)");widgets.add(button);content.addAndMakeVisible(button);button->setComponentID(d.id);button->setTitle(d.name);button->getProperties().set("referenceMomentary",true);button->setBounds(6,y,344,28);y+=32;
            auto gesture=std::make_shared<KillGesture>(*parameter);button->onStateChange=[button,gesture]{gesture->change(button->isDown());};continue;
        }
        const bool endpoint=juce::String(d.id).endsWith("_src")||juce::String(d.id).endsWith("_dst")||juce::String(d.id).endsWith("_first")||juce::String(d.id).endsWith("_last");
        if(d.choices||endpoint)
        {
            auto* label=new juce::Label();widgets.add(label);content.addAndMakeVisible(label);label->setText(d.name,juce::dontSendNotification);label->setBounds(6,y,344,18);y+=18;
            auto* combo=new juce::ComboBox();widgets.add(combo);content.addAndMakeVisible(combo);combo->setComponentID(d.id);combo->setTitle(d.name);combo->setBounds(6,y,344,28);y+=32;
            if(d.choices)combo->addItemList(juce::StringArray::fromTokens(d.choices,"|",""),1);else {combo->addItem("None",1);for(int slot=0;slot<kMaxSlots;++slot){juce::String name="Slot "+juce::String(slot+1)+" (empty)";for(const auto& id:model.getOrder())if(const auto p=model.getInstance(id);p&&p->slot==slot){const auto* desc=PedalRegistry::find(p->modelId.c_str());name="Slot "+juce::String(slot+1)+": "+(desc?desc->displayName:p->modelId.c_str());break;}combo->addItem(name,slot+2);}}
            comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,d.id,*combo));
        }
        else if(d.minimum==0&&d.maximum==1&&d.interval==1)
        {
            auto* button=new juce::ToggleButton(d.name);widgets.add(button);content.addAndMakeVisible(button);button->setComponentID(d.id);button->setTitle(d.name);button->setBounds(6,y,344,28);y+=32;buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,d.id,*button));
        }
        else
        {
            auto* label=new juce::Label();widgets.add(label);content.addAndMakeVisible(label);label->setText(d.name,juce::dontSendNotification);label->setBounds(6,y,344,18);y+=18;
            auto* slider=new juce::Slider();widgets.add(slider);content.addAndMakeVisible(slider);slider->setComponentID(d.id);slider->setTitle(d.name);slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,88,22);slider->setBounds(6,y,344,28);y+=32;sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,d.id,*slider));
        }
    }
}
}
