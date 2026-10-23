// OWNER: WP17
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class BoardViewport final : public juce::Viewport
{
public:
    std::function<void(float,juce::Point<float>)> onZoom;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override
    {
        if(e.mods.isCommandDown()&&onZoom)onZoom(wheel.deltaY>0?1.1f:1.f/1.1f,e.position);
        else juce::Viewport::mouseWheelMove(e,wheel);
    }
    void mouseMagnify(const juce::MouseEvent& e,float scale) override
    {
        if(onZoom)onZoom(scale,e.position);
    }
};
}
