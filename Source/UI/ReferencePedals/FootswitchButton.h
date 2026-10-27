// OWNER: WP19
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class FootswitchButton final : public juce::TextButton
{
public:
    explicit FootswitchButton(const juce::String& text={}):juce::TextButton(text){}
    void setFaceGeometry(juce::Rectangle<float> physical);
    void paintButton(juce::Graphics&,bool,bool) override;
private:
    juce::Rectangle<float> face;
};
}
