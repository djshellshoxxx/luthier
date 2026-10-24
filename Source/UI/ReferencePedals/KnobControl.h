// OWNER: WP19
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class KnobControl final : public juce::Slider
{
public:
    KnobControl();
    bool keyPressed(const juce::KeyPress&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    double startValue=0;
};
}
