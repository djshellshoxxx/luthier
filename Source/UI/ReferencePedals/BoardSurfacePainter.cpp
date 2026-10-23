// OWNER: WP18
#include "BoardSurfacePainter.h"
#include "../Theme.h"
namespace luthier::reference
{
Mounting BoardSurfacePainter::defaultMount (BoardStyle s) { return (Mounting)((int)s+1); }
void BoardSurfacePainter::paint (juce::Graphics& g, juce::Rectangle<float> r, BoardStyle s)
{
    static constexpr uint32_t colours[] {0xffa58a5a,0xff252628,0xff8a8e91,0xff56371f,0xff242321,0xffc9d3d6,0xff30251e};
    g.setColour(Palette::textured ? juce::Colour(colours[(int)s]) : Palette::panel);
    g.fillRoundedRectangle(r,8);
    g.setColour(Palette::edgeBright); g.drawRoundedRectangle(r.reduced(1),8,2);
    if (!Palette::textured) { g.setColour(Palette::textPrimary); g.drawText(toString(s),r.toNearestInt().removeFromTop(24),juce::Justification::centred); return; }
    juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(r.toNearestInt());
    juce::Random rand(0x4c555448+(int)s);
    if (s == BoardStyle::rail)
    {
        g.setColour(Palette::backgroundDeep); g.fillRect(r.reduced(8));
        for (float y : {r.getY()+r.getHeight()*.3f,r.getY()+r.getHeight()*.78f})
        { g.setGradientFill(juce::ColourGradient(juce::Colours::silver,r.getX(),y,juce::Colours::darkgrey,r.getX(),y+12,false));g.fillRoundedRectangle(r.getX()+8,y,r.getWidth()-16,12,6); }
        return;
    }
    if (s == BoardStyle::ceramic)
    {
        for(float x=r.getX();x<r.getRight();x+=96)for(float y=r.getY();y<r.getBottom();y+=96)
        { g.setColour(juce::Colour(0xff5a5750));g.drawRect(x,y,96.f,96.f,3.f);g.setColour(juce::Colours::white.withAlpha(.2f));g.drawLine(x+3,y+3,x+91,y+3); }
        return;
    }
    const int strokes = juce::jmin(1600,(int)(r.getWidth()*r.getHeight()/240));
    for(int i=0;i<strokes;++i)
    {
        float x=r.getX()+rand.nextFloat()*r.getWidth(),y=r.getY()+rand.nextFloat()*r.getHeight();
        g.setColour(juce::Colours::black.withAlpha(.03f+rand.nextFloat()*.08f));
        const float length = s==BoardStyle::velcro?2.f:40.f+rand.nextFloat()*100.f;
        g.drawLine(x,y,x+length,y+(s==BoardStyle::clamps?0:rand.nextFloat()*3),.7f);
    }
    if(s==BoardStyle::loose) for(float x=r.getX();x<r.getRight();x+=120) {g.setColour(juce::Colours::black.withAlpha(.4f));g.drawVerticalLine((int)x,r.getY(),r.getBottom());}
}
void BoardSurfacePainter::paintMount (juce::Graphics& g, juce::Rectangle<float> r, Mounting m, int seed)
{
    if(m==Mounting::none || m==Mounting::automatic) return;
    if(!Palette::textured) {g.setColour(Palette::textPrimary);g.drawRect(r.expanded(4),2);return;}
    if(m==Mounting::routed) {g.setColour(juce::Colours::black.withAlpha(.6f));g.fillRoundedRectangle(r.expanded(4),5);return;}
    if(m==Mounting::velcro || m==Mounting::ceramic)
    {
        g.setColour(juce::Colour(0xff171719));
        for(float frac:{.22f,.78f})g.fillRoundedRectangle(r.getX()-4,r.getY()+frac*r.getHeight(),r.getWidth()+8,10,2);
        return;
    }
    for(bool right:{false,true})
    {
        const float x=right?r.getRight()-3:r.getX()-11, y=r.getCentreY();
        g.setColour(juce::Colours::silver);g.fillRoundedRectangle(x,y-18,14,m==Mounting::clamps?64.f:36.f,2);
        const float cx=x+7;
        g.setColour(juce::Colours::darkgrey);g.fillEllipse(cx-4,y-4,8,8);
        g.setColour(juce::Colours::lightgrey);g.drawEllipse(cx-4,y-4,8,8,1);
        const float a=(float)((unsigned)seed%90)*juce::MathConstants<float>::pi/180;
        g.drawLine(cx-3*std::cos(a),y-3*std::sin(a),cx+3*std::cos(a),y+3*std::sin(a),1);
    }
}
}
