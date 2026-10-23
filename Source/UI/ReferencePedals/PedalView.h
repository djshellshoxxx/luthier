// OWNER: WP17
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../ReferencePedals/BoardModel.h"
#include "PedalFaceFactory.h"
namespace luthier::reference
{
class PedalView final : public juce::Component
{
public:
    PedalView (BoardModel&, InstanceId);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override { repaint(); }
    InstanceId id() const { return instanceId; }
    juce::Point<float> inputJack() const { return {0.f,getHeight()*.42f}; }
    juce::Point<float> outputJack() const { return {(float)getWidth(),getHeight()*.42f}; }
    std::function<void(InstanceId,int,bool)> onLearn;
    std::function<void(PedalView&,juce::Point<int>,bool)> onDrag;
    std::function<void(PedalView&)> onFocus;
    bool learnMode = false;
private:
    void contextMenu();
    void post (BoardCommand);
    BoardModel& model;
    const InstanceId instanceId;
    std::string modelId;
    std::unique_ptr<PedalFace> face;
    const PedalDesc* descriptor = nullptr;
    juce::TextButton footswitch {"Engage"};
    juce::ToggleButton profile {"Use pedal CC profile"};
    juce::ComboBox placement, mounting, channel;
    juce::OwnedArray<juce::Slider> controls;
    juce::OwnedArray<juce::ComboBox> selectors;
    juce::OwnedArray<juce::Label> labels;
    std::vector<int> controlParams, selectorParams;
    juce::Slider inputTrim, outputTrim, lane;
    bool refreshing=false, dragging=false;
    double lastDragCommit=0;
    uint32_t gesture=0;
};
}
