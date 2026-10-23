// OWNER: WP17
#include "PedalView.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
#include "PedalFaceCache.h"
namespace luthier::reference
{
PedalView::PedalView(BoardModel& m,InstanceId id):model(m),instanceId(id)
{
    setWantsKeyboardFocus(true);setFocusContainerType(FocusContainerType::focusContainer);
    for(auto* c:std::array<juce::Component*,8>{&footswitch,&placement,&mounting,&channel,&inputTrim,&outputTrim,&lane,&profile})addAndMakeVisible(c);
    footswitch.setClickingTogglesState(true);
    footswitch.onClick=[this]{if(learnMode && onLearn){onLearn(instanceId,-1,false);refresh();}else post(cmd::SetEnabled{instanceId,footswitch.getToggleState()});};
    placement.addItem("PRE",1);placement.addItem("LOOP",2);placement.addItem("POST",3);placement.setTitle("Pedal placement");
    placement.onChange=[this]{if(!refreshing)post(cmd::SetPlacement{instanceId,(Placement)(placement.getSelectedId()-1)});};
    for(int i=0;i<=7;++i)mounting.addItem(toString((Mounting)i),i+1);
    mounting.setTitle("Pedal mounting");mounting.onChange=[this]{if(!refreshing)post(cmd::SetMounting{instanceId,(Mounting)(mounting.getSelectedId()-1)});};
    channel.addItem("MIDI: none",1);for(int i=1;i<=16;++i)channel.addItem("MIDI ch "+juce::String(i),i+1);
    channel.setTitle("Pedal MIDI channel");channel.onChange=[this]{if(!refreshing)post(cmd::SetSlotChannel{instanceId,channel.getSelectedId()-1,profile.getToggleState()});};
    profile.onClick=[this]{if(!refreshing)post(cmd::SetSlotChannel{instanceId,channel.getSelectedId()-1,profile.getToggleState()});};
    for(auto* s:{&inputTrim,&outputTrim,&lane}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,42,18);}
    inputTrim.setRange(-12,12,.1);outputTrim.setRange(-12,12,.1);lane.setRange(0,3,1);
    inputTrim.setTitle("Input trim dB");outputTrim.setTitle("Output trim dB");lane.setTitle("Routing lane");
    inputTrim.setTextValueSuffix(" dB");outputTrim.setTextValueSuffix(" dB");
    inputTrim.onValueChange=[this]{if(!refreshing)post(cmd::SetTrim{instanceId,(float)inputTrim.getValue(),(float)outputTrim.getValue(),gesture});};
    outputTrim.onValueChange=inputTrim.onValueChange;
    lane.onValueChange=[this]{if(!refreshing)post(cmd::SetLane{instanceId,(int)lane.getValue()});};
    refresh();
}
void PedalView::post(BoardCommand c)
{
    // Removing or changing a face can destroy the clicked component; dispatch after its callback returns.
    juce::Component::SafePointer<PedalView> self(this);
    juce::MessageManager::callAsync([self,c=std::move(c)]{if(self)self->model.apply(c);});
}
void PedalView::refresh()
{
    const auto instance=model.getInstance(instanceId);if(!instance)return;
    const juce::ScopedValueSetter<bool> guard(refreshing,true);
    if(modelId!=instance->modelId || !face)
    {
        modelId=instance->modelId;descriptor=PedalRegistry::find(modelId.c_str());face=PedalFaceFactory::make(modelId.c_str());
        controls.clear();selectors.clear();labels.clear();controlParams.clear();selectorParams.clear();
        if(descriptor)for(int i=0;i<descriptor->numParams;++i)
        {
            const auto& p=descriptor->params[i];if(p.has(PF_Engage))continue;
            auto* label=labels.add(new juce::Label());label->setText(p.label,juce::dontSendNotification);label->setFont(11.f);label->setJustificationType(juce::Justification::centred);addAndMakeVisible(label);
            if(p.isDiscreteKind() && p.choices)
            {
                auto* cb=selectors.add(new juce::ComboBox());selectorParams.push_back(i);
                for(int k=0;k<p.steps;++k)cb->addItem(p.choices[k],k+1);
                cb->setTitle(juce::String(descriptor->displayName)+" "+p.label);addAndMakeVisible(cb);
                cb->onChange=[this,cb,i]{if(refreshing)return;if(learnMode&&onLearn)onLearn(instanceId,i,false);else post(cmd::SetParam{instanceId,i,(float)(cb->getSelectedId()-1)/(descriptor->params[i].steps-1)});};
            }
            else
            {
                auto* s=controls.add(new juce::Slider());controlParams.push_back(i);
                s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,60,18);
                const bool toggle=p.kind==ParamKind::Toggle || p.kind==ParamKind::Trigger || p.kind==ParamKind::Momentary;
                s->setRange(0,1,toggle?1:.001);s->setDoubleClickReturnValue(true,p.defaultNormalised());
                s->setTitle(juce::String(descriptor->displayName)+" "+p.label);s->setTooltip(juce::String(p.label)+" (double click resets; Shift drag fine)");
                s->textFromValueFunction=[this,i](double n){const auto& d=descriptor->params[i];return juce::String(d.fromNormalised(n),d.max-d.min>100?0:2)+" "+d.unit;};
                s->onDragStart=[this]{++gesture;};
                s->onValueChange=[this,s,i]{if(refreshing)return;if(learnMode&&onLearn){onLearn(instanceId,i,true);refresh();}else post(cmd::SetParam{instanceId,i,(float)s->getValue(),gesture});};
                addAndMakeVisible(s);
            }
        }
    }
    for(int i=0;i<controls.size();++i)controls[i]->setValue(instance->params[(size_t)controlParams[(size_t)i]],juce::dontSendNotification);
    for(int i=0;i<selectors.size();++i){const auto ix=selectorParams[(size_t)i];selectors[i]->setSelectedId(1+(int)descriptor->params[ix].fromNormalised(instance->params[(size_t)ix]),juce::dontSendNotification);}
    footswitch.setToggleState(instance->enabled,juce::dontSendNotification);footswitch.setButtonText(instance->enabled?"ON / bypass":"OFF / engage");
    footswitch.setEnabled(!instance->missing);footswitch.setTitle("Bypass "+juce::String(descriptor?descriptor->displayName:modelId.c_str()));
    placement.setSelectedId((int)instance->placement+1,juce::dontSendNotification);mounting.setSelectedId((int)instance->mounting+1,juce::dontSendNotification);
    const auto midi=instance->extras.getProperty("midi",juce::var());channel.setSelectedId(1+(int)midi.getProperty("chan",0),juce::dontSendNotification);profile.setToggleState(midi.getProperty("useProfile",false),juce::dontSendNotification);
    inputTrim.setValue(instance->inDb,juce::dontSendNotification);outputTrim.setValue(instance->outDb,juce::dontSendNotification);lane.setValue(instance->lane,juce::dontSendNotification);
    auto order=model.getOrder();auto it=std::find(order.begin(),order.end(),instanceId);const int pos=(int)std::distance(order.begin(),it);
    setExplicitFocusOrder(pos+10);setTitle(juce::String(descriptor?descriptor->displayName:modelId.c_str())+", position "+juce::String(pos+1)+" of "+juce::String((int)order.size())+", "+(instance->enabled?"on":"off")+", "+placement.getText());
    setDescription(instance->missing?"Missing model; passes audio through. Delete to remove.":"Alt arrows reorder. Space bypasses. Delete removes. Menu opens actions.");
    resized();repaint();
}
void PedalView::resized()
{
    auto r=getLocalBounds().reduced(5);r.removeFromTop(58);
    int l=0,kn=0,se=0;
    if(descriptor)for(int i=0;i<descriptor->numParams;++i)
    {
        const auto& p=descriptor->params[i];if(p.has(PF_Engage))continue;
        auto cell=juce::Rectangle<int>(5+(l%3)*(getWidth()-10)/3,58+(l/3)*83,(getWidth()-10)/3,81);
        labels[l]->setBounds(cell.removeFromTop(18));
        if(p.isDiscreteKind()&&p.choices)selectors[se++]->setBounds(cell.removeFromTop(28));else controls[kn++]->setBounds(cell);
        ++l;
    }
    auto bottom=getLocalBounds().reduced(6);bottom.removeFromTop(58+((l+2)/3)*83);
    auto row=bottom.removeFromTop(24);placement.setBounds(row.removeFromLeft(row.getWidth()/2));mounting.setBounds(row);
    inputTrim.setBounds(bottom.removeFromTop(24));outputTrim.setBounds(bottom.removeFromTop(24));lane.setBounds(bottom.removeFromTop(24));channel.setBounds(bottom.removeFromTop(24));profile.setBounds(bottom.removeFromTop(24));
    footswitch.setBounds(bottom.removeFromTop(44).reduced(12,2));
}
void PedalView::paint(juce::Graphics& g)
{
    AnimationPolicy::notePaint(*this);
    const auto i=model.getInstance(instanceId);if(!i)return;
    FaceState state;state.enabled=i->enabled;state.missing=i->missing;state.highContrast=!Palette::textured;
    if(face){const auto image=PedalFaceCache::get(*face,modelId.c_str(),descriptor?descriptor->revision:1,{getWidth(),getHeight()},g.getInternalContext().getPhysicalPixelScaleFactor(),state);g.drawImage(image,getLocalBounds().toFloat());}
    if(!i->enabled){g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRoundedRectangle(getLocalBounds().toFloat(),6);}
    g.setColour(Palette::textMuted);g.setFont(10.f);g.drawText(descriptor&&descriptor->inspiredBy?"Inspired by "+juce::String(descriptor->inspiredBy):"Original Luthier design",4,34,getWidth()-8,13,juce::Justification::centred);
    g.drawText("EST · research model",4,46,getWidth()-8,12,juce::Justification::centred);
    if(i->missing){g.setColour(Palette::warning);g.drawFittedText("Missing model: "+juce::String(modelId),getLocalBounds().reduced(8),juce::Justification::centred,4);}
    if(hasKeyboardFocus(true)||learnMode){g.setColour(Palette::accentBright);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),5,3);}
}
void PedalView::focusGained(FocusChangeType){repaint();if(onFocus)onFocus(*this);}
bool PedalView::keyPressed(const juce::KeyPress& key)
{
    const auto code=key.getKeyCode();const auto mods=key.getModifiers();auto order=model.getOrder();const auto it=std::find(order.begin(),order.end(),instanceId);const int pos=(int)std::distance(order.begin(),it);
    if(mods.isAltDown())
    {
        if(code==juce::KeyPress::leftKey || code==juce::KeyPress::rightKey || code==juce::KeyPress::homeKey || code==juce::KeyPress::endKey)
        {int n=code==juce::KeyPress::homeKey?0:code==juce::KeyPress::endKey?(int)order.size()-1:pos+(code==juce::KeyPress::leftKey?-1:1);model.apply(cmd::Move{instanceId,juce::jlimit(0,(int)order.size()-1,n)});return true;}
        if(code==juce::KeyPress::upKey||code==juce::KeyPress::downKey){const auto i=model.getInstance(instanceId);model.apply(cmd::SetLane{instanceId,juce::jlimit(0,3,i->lane+(code==juce::KeyPress::upKey?-1:1))});return true;}
    }
    if(code==juce::KeyPress::deleteKey||code==juce::KeyPress::backspaceKey){post(cmd::Remove{instanceId});return true;}
    if(mods.isCommandDown()&&(code=='D'||code=='d')){post(cmd::Duplicate{instanceId});return true;}
    if(code==juce::KeyPress::spaceKey||code==juce::KeyPress::returnKey){const auto i=model.getInstance(instanceId);model.apply(cmd::SetEnabled{instanceId,!i->enabled});return true;}
    if(code=='l'||code=='L'){if(onLearn)onLearn(instanceId,-1,false);return true;}
    if(code==juce::KeyPress::F10Key&&mods.isShiftDown()){contextMenu();return true;}
    if(code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey){if(auto* sibling=getParentComponent()->getChildComponent(juce::jlimit(0,(int)order.size()-1,pos+(code==juce::KeyPress::leftKey?-1:1))))sibling->grabKeyboardFocus();return true;}
    return false;
}
void PedalView::mouseDown(const juce::MouseEvent& e){if(e.mods.isPopupMenu()){contextMenu();return;}grabKeyboardFocus();}
void PedalView::mouseDrag(const juce::MouseEvent& e)
{
    if(e.getDistanceFromDragStart()<4)return;
    if(!dragging){dragging=true;model.beginTransaction("Move pedal");lastDragCommit=0;}
    const auto now=juce::Time::getMillisecondCounterHiRes();if(onDrag&&now-lastDragCommit>=125){lastDragCommit=now;onDrag(*this,e.getEventRelativeTo(getParentComponent()).position.toInt(),false);}
}
void PedalView::mouseUp(const juce::MouseEvent& e){if(dragging){if(onDrag)onDrag(*this,e.getEventRelativeTo(getParentComponent()).position.toInt(),true);dragging=false;model.endTransaction();}}
void PedalView::contextMenu()
{
    juce::PopupMenu menu;menu.addItem(1,"Move earlier");menu.addItem(2,"Move later");menu.addItem(3,"Duplicate");menu.addItem(4,"Remove");menu.addItem(5,"Learn footswitch");
    juce::Component::SafePointer<PedalView> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[self](int n){if(!self)return;const auto order=self->model.getOrder();const int pos=(int)std::distance(order.begin(),std::find(order.begin(),order.end(),self->instanceId));
        if(n==1||n==2)self->model.apply(cmd::Move{self->instanceId,juce::jlimit(0,(int)order.size()-1,pos+(n==1?-1:1))});
        if(n==3)self->post(cmd::Duplicate{self->instanceId});if(n==4)self->post(cmd::Remove{self->instanceId});if(n==5&&self->onLearn)self->onLearn(self->instanceId,-1,false);});
}
}
