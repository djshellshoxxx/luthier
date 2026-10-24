// OWNER: WP19
#include "KnobControl.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
namespace luthier::reference
{
KnobControl::KnobControl(){setSliderStyle(RotaryHorizontalVerticalDrag);setTextBoxStyle(TextBoxBelow,false,60,18);setRange(0,1,.001);setWantsKeyboardFocus(true);}
bool KnobControl::keyPressed(const juce::KeyPress& key)
{
    if(key.getModifiers().isAltDown())return false;
    const auto c=key.getKeyCode();
    if(c==juce::KeyPress::returnKey){showValueEditor();return true;}
    if(c==juce::KeyPress::upKey||c==juce::KeyPress::rightKey||c==juce::KeyPress::downKey||c==juce::KeyPress::leftKey)
    {
        const auto step=key.getModifiers().isCommandDown()?.001:key.getModifiers().isShiftDown()?.1:.01;
        juce::Slider::ScopedDragNotification drag(*this);
        setValue(getValue()+((c==juce::KeyPress::upKey||c==juce::KeyPress::rightKey)?step:-step),juce::sendNotificationSync);return true;
    }
    return juce::Slider::keyPressed(key);
}
void KnobControl::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel)
{
    const auto delta=wheel.deltaY!=0?wheel.deltaY:wheel.deltaX;if(delta!=0){juce::Slider::ScopedDragNotification drag(*this);setValue(getValue()+(delta>0?.01:-.01),juce::sendNotificationSync);}
}
void KnobControl::mouseDown(const juce::MouseEvent& event){if(event.mods.isPopupMenu()){showValueEditor();return;}startValue=getValue();juce::Slider::mouseDown(event);}
void KnobControl::mouseDrag(const juce::MouseEvent& event)
{
    if(event.mods.isPopupMenu())return;
    if(event.mods.isShiftDown())setValue(startValue+(event.getDistanceFromDragStartX()-event.getDistanceFromDragStartY())/2000.,juce::sendNotificationSync);
    else juce::Slider::mouseDrag(event);
}
}

namespace luthier::reference
{
KnobControl::~KnobControl(){closeValueEditor();}
void KnobControl::closeValueEditor(){valueCallout.reset();valueEditor.reset();}
void KnobControl::showValueEditor()
{
    closeValueEditor();auto* parent=getTopLevelComponent();if(!parent)return;
    valueEditor=std::make_unique<juce::TextEditor>();valueEditor->setTitle(getTitle()+" natural value");valueEditor->setText(getTextFromValue(getValue()),false);valueEditor->setSize(180,30);
    juce::Component::SafePointer<KnobControl> safe(this);
    valueEditor->onReturnKey=[safe]{if(!safe||!safe->valueEditor)return;{juce::Slider::ScopedDragNotification gesture(*safe);safe->setValue(safe->getValueFromText(safe->valueEditor->getText()),juce::sendNotificationSync);}juce::MessageManager::callAsync([safe]{if(safe)safe->closeValueEditor();});};
    valueEditor->onEscapeKey=[safe]{juce::MessageManager::callAsync([safe]{if(safe)safe->closeValueEditor();});};
    valueEditor->onFocusLost=valueEditor->onEscapeKey;
    valueCallout=std::make_unique<juce::CallOutBox>(*valueEditor,parent->getLocalArea(this,getLocalBounds()),parent);valueCallout->setVisible(true);valueCallout->toFront(false);valueEditor->selectAll();valueEditor->grabKeyboardFocus();
}
void KnobControl::paint(juce::Graphics& g)
{
    AnimationPolicy::notePaint(*this);
    if((bool)getProperties()["physicalTreadle"])
    {
        auto area=getLocalBounds().toFloat().reduced(2);g.setColour(Palette::backgroundDeep);g.fillRoundedRectangle(area,4);const float position=(float)valueToProportionOfLength(getValue());
        area=area.reduced(3).withTrimmedTop(8*(1-position)).withTrimmedBottom(8*position);g.setColour(Palette::panelRaised);g.fillRoundedRectangle(area,3);g.setColour(Palette::textMuted);for(float y=area.getY()+5;y<area.getBottom()-3;y+=7)g.drawHorizontalLine((int)y,area.getX()+4,area.getRight()-4);
        g.setColour(Palette::accentBright);g.drawLine(area.getX()+4,area.getY()+position*area.getHeight(),area.getRight()-4,area.getY()+position*area.getHeight(),2);return;
    }
    if(!isRotary()){juce::Slider::paint(g);return;}
    const float diameter=(float)juce::jmin(getWidth(),getHeight());auto circle=juce::Rectangle<float>(diameter-4,diameter-4).withCentre({getWidth()*.5f,diameter*.5f});
    if(Palette::textured)g.setGradientFill(juce::ColourGradient(juce::Colour(0xff666970),circle.getX(),circle.getY(),juce::Colour(0xff17191c),circle.getRight(),circle.getBottom(),false));else g.setColour(Palette::panelRaised);
    g.fillEllipse(circle);g.setColour(Palette::textPrimary);g.drawEllipse(circle,1);
    const float angle=-2.4f+4.8f*(float)valueToProportionOfLength(getValue());const auto centre=circle.getCentre();const float radius=circle.getWidth()*.37f;g.drawLine(centre.x+std::sin(angle)*radius*.2f,centre.y-std::cos(angle)*radius*.2f,centre.x+std::sin(angle)*radius,centre.y-std::cos(angle)*radius,2);
    if(hasKeyboardFocus(true)){g.setColour(Palette::accentBright);g.drawEllipse(circle.expanded(1),2);}
}
}
