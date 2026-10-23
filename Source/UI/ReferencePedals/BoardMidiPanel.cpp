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
    if(const auto* bindings=s.midi.getProperty("bindings",juce::var()).getArray())for(const auto& binding:*bindings)
    {
        auto* edit=new juce::TextButton("Edit MIDI mapping "+binding.getProperty("id",0).toString());widgets.add(edit);content.addAndMakeVisible(edit);edit->setBounds(6,y,344,28);y+=32;
        juce::Component::SafePointer<juce::TextButton> button(edit);auto* board=&model;
        edit->onClick=[button,board,binding]{if(button)juce::CallOutBox::launchAsynchronously(std::make_unique<MappingCallout>(*board,binding),button->getScreenBounds(),nullptr);};
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
