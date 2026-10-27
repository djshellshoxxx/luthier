// OWNER: WP19
#include "FootswitchButton.h"
#include "../Theme.h"
namespace luthier::reference
{
void FootswitchButton::setFaceGeometry(juce::Rectangle<float> physical)
{
    const auto hit=physical.withSizeKeepingCentre(juce::jmax(44.f,physical.getWidth()),juce::jmax(44.f,physical.getHeight())).toNearestInt();
    setBounds(hit);face=physical.translated(-(float)hit.getX(),-(float)hit.getY());repaint();
}
void FootswitchButton::paintButton(juce::Graphics& g,bool over,bool down)
{
    auto r=face.isEmpty()?getLocalBounds().toFloat().reduced(3):face;
    const bool plate=r.getWidth()>r.getHeight()*1.25f||r.getHeight()>44;
    if(down)r.translate(0,1);
    if(Palette::textured)g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd4d2c9),r.getX(),r.getY(),juce::Colour(0xff6d6b66),r.getRight(),r.getBottom(),false));
    else g.setColour(Palette::panelRaised);
    if(plate)g.fillRoundedRectangle(r,4);else g.fillEllipse(r);
    g.setColour(Palette::textured?juce::Colour(0xff343333):Palette::textPrimary);
    if(plate){g.drawRoundedRectangle(r,4,1.2f);const auto inner=r.reduced(4);g.fillRoundedRectangle(inner,3);g.setColour(Palette::textMuted.withAlpha(.45f));for(float y=inner.getY()+6;y<inner.getBottom()-4;y+=5)g.drawHorizontalLine((int)y,inner.getX()+3,inner.getRight()-3);}
    else{g.drawEllipse(r,1.2f);g.drawEllipse(r.reduced(3),.7f);}
    if(hasKeyboardFocus(false)||over){g.setColour(Palette::accentBright);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),4,2);}
    // State is available as text as well as the separate jewel LED.
    if(getClickingTogglesState()){g.setColour(Palette::textPrimary);g.setFont(juce::FontOptions(9.f));g.drawText(getToggleState()?"ON":"OFF",getLocalBounds(),juce::Justification::centred);}
}
}
