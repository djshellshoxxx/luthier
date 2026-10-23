// OWNER: WP22
#include "MappingCallout.h"
namespace luthier::reference
{
MappingCallout::MappingCallout(BoardModel& m,juce::var b):model(m),binding(b.clone())
{
    const char* sources[]{"CC","CC 14-bit","NRPN","Note","Note velocity","Poly aftertouch","Channel aftertouch","Pitch bend","Program change","Start","Stop","Continue","UI slider","Keyboard velocity"};
    for(int i=0;i<14;++i)source.addItem(sources[i],i+1);
    const char* modes[]{"Absolute","Relative","Toggle","Momentary","Trigger","Step"};for(int i=0;i<6;++i)mode.addItem(modes[i],i+1);
    const char* curves[]{"Linear","Log","Exponential","S-curve","Inverse","Step","Custom"};for(int i=0;i<7;++i)curve.addItem(curves[i],i+1);
    takeover.addItem("Jump",1);takeover.addItem("Pickup",2);takeover.setSelectedId(((int)binding.getProperty("flags",32)&16)?2:1);
    const auto src=binding.getProperty("src",juce::var());source.setSelectedId((int)src.getProperty("type",0)+1);mode.setSelectedId((int)binding.getProperty("mode",0)+1);curve.setSelectedId((int)binding.getProperty("curve",juce::var()).getProperty("id",0)+1);
    number.setRange(0,127,1);number.setValue(src.getProperty("num",0));channel.setRange(0,16,1);channel.setValue(binding.getProperty("chan",0));minimum.setRange(0,1,.001);maximum.setRange(0,1,.001);
    auto out=binding.getProperty("out",juce::var());minimum.setValue(out.isArray()&&out.size()==2?(double)out[0]:0.);maximum.setValue(out.isArray()&&out.size()==2?(double)out[1]:1.);
    for(auto* s:{&number,&channel,&minimum,&maximum}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,60,24);}
    const char* titles[]{"MIDI source","CC / note number","Channel (0 = omni)","Output minimum","Output maximum","Mapping mode","Response curve","Takeover"};
    std::array<juce::Component*,8> controls{&source,&number,&channel,&minimum,&maximum,&mode,&curve,&takeover};
    for(int i=0;i<8;++i){addAndMakeVisible(controls[(size_t)i]);controls[(size_t)i]->setTitle(titles[i]);auto* l=labels.add(new juce::Label());l->setText(titles[i],juce::dontSendNotification);addAndMakeVisible(l);}
    addAndMakeVisible(save);addAndMakeVisible(remove);save.onClick=[this]{apply();};remove.onClick=[this]{model.apply(cmd::SetMidiBinding{cmd::SetMidiBinding::Op::Remove,binding});remove.setEnabled(false);save.setEnabled(false);};setSize(380,340);
}
void MappingCallout::apply()
{
    auto* object=binding.getDynamicObject();if(!object)return;
    auto* src=new juce::DynamicObject();src->setProperty("type",source.getSelectedId()-1);src->setProperty("num",(int)number.getValue());object->setProperty("src",juce::var(src));object->setProperty("chan",(int)channel.getValue());object->setProperty("mode",mode.getSelectedId()-1);
    auto* c=new juce::DynamicObject();c->setProperty("id",curve.getSelectedId()-1);c->setProperty("p",2.);object->setProperty("curve",juce::var(c));object->setProperty("out",juce::Array<juce::var>{minimum.getValue(),maximum.getValue()});
    int flags=(int)object->getProperty("flags");flags|=32;if(takeover.getSelectedId()==2)flags|=16;else flags&=~16;object->setProperty("flags",flags);
    model.apply(cmd::SetMidiBinding{cmd::SetMidiBinding::Op::Edit,binding});
}
void MappingCallout::resized()
{
    std::array<juce::Component*,8> controls{&source,&number,&channel,&minimum,&maximum,&mode,&curve,&takeover};auto r=getLocalBounds().reduced(8);
    for(int i=0;i<8;++i){auto row=r.removeFromTop(35);labels[i]->setBounds(row.removeFromLeft(145));controls[(size_t)i]->setBounds(row.reduced(1));}
    auto row=r.removeFromTop(32);save.setBounds(row.removeFromLeft(row.getWidth()/2));remove.setBounds(row);
}
}
