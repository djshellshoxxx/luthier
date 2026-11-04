// OWNER: WP19
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class KnobControl final : public juce::Slider
{
public:
    KnobControl();
    ~KnobControl() override;
    void paint(juce::Graphics&) override;
    void showValueEditor();
    bool keyPressed(const juce::KeyPress&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void closeValueEditor();
    std::unique_ptr<juce::TextEditor> valueEditor;
    std::unique_ptr<juce::CallOutBox> valueCallout;
    double startValue=0;
};
}
