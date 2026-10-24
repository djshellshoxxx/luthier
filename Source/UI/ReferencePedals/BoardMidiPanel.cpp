// OWNER: WP22
#include "BoardMidiPanel.h"
#include "MappingCallout.h"
namespace luthier::reference
{
BoardMidiPanel::BoardMidiPanel(BoardModel& m):model(m)
{
    addAndMakeVisible(viewport);viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);
    const auto s=model.getBoardSettings();
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
    if(const auto* bindings=s.midi.getProperty("bindings",juce::var()).getArray())for(const auto& binding:*bindings)
    {
        auto* edit=new juce::TextButton("Edit MIDI mapping "+binding.getProperty("id",0).toString());widgets.add(edit);content.addAndMakeVisible(edit);edit->setBounds(6,y,344,28);y+=32;
        juce::Component::SafePointer<juce::TextButton> button(edit);auto* board=&model;
        edit->onClick=[button,board,binding]{if(button)juce::CallOutBox::launchAsynchronously(std::make_unique<MappingCallout>(*board,binding),button->getParentComponent()->getLocalArea(button,button->getLocalBounds()),button->getParentComponent());};
    }
    content.setSize(360,y+8);setSize(380,420);
}
BoardMidiPanel::~BoardMidiPanel(){viewport.setViewedComponent(nullptr,false);}
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
