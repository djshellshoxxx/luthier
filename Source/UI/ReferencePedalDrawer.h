#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ReferencePedals/BoardRoot.h"
namespace luthier
{
class LuthierAudioProcessor;
namespace reference
{
class Drawer final : public juce::Component
{
public:
    explicit Drawer(LuthierAudioProcessor&);
    ~Drawer() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void setOpen(bool) noexcept;
    bool isOpen() const noexcept{return open;}
    int getPreferredHeight() const noexcept{return open?height:0;}
    void refresh();
    void attachBoard();
    BoardRoot& boardRoot(){return root;}
    std::function<void()> onHeightChanged;
private:
    LuthierAudioProcessor& processor;
    BoardRoot root;
    bool open=false;
    int height=232,dragHeight=232;
};
}
}
