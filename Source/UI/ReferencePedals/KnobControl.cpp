// OWNER: WP19
#include "KnobControl.h"
namespace luthier::reference
{
KnobControl::KnobControl(){setSliderStyle(RotaryHorizontalVerticalDrag);setTextBoxStyle(TextBoxBelow,false,60,18);setRange(0,1,.001);setWantsKeyboardFocus(true);}
bool KnobControl::keyPressed(const juce::KeyPress& key)
{
    if(key.getModifiers().isAltDown())return false;
    const auto c=key.getKeyCode();
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
void KnobControl::mouseDown(const juce::MouseEvent& event){startValue=getValue();juce::Slider::mouseDown(event);}
void KnobControl::mouseDrag(const juce::MouseEvent& event)
{
    if(event.mods.isShiftDown())setValue(startValue+(event.getDistanceFromDragStartX()-event.getDistanceFromDragStartY())/2000.,juce::sendNotificationSync);
    else juce::Slider::mouseDrag(event);
}
}
