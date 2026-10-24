// OWNER: WP17 / WP19
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../ReferencePedals/BoardModel.h"
#include "PedalFaceFactory.h"
#include "FootswitchButton.h"
namespace luthier::reference
{
class PedalView final : public juce::Component
{
public:
    PedalView(BoardModel&,InstanceId,juce::AudioProcessorValueTreeState* = nullptr);
    ~PedalView() override;
    void refresh();
    bool updateMeters(const PedalMeters&);
    juce::String meterSummary() const;
    void showInspector();
    void requestFootswitchLearn();
    void beginPositionGesture();
    void endPositionGesture(int acceptedIndex=-1);
    void closeInspector(bool restoreFocus=false);
    void placeCaptionIn(juce::Component&);
    juce::Point<int> physicalSize() const;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override {repaint();}
    void focusOfChildComponentChanged(FocusChangeType) override {if(hasKeyboardFocus(true)&&onFocus)onFocus(*this);repaint();}
    InstanceId id() const {return instanceId;}
    juce::Point<float> inputJack() const;
    juce::Point<float> outputJack() const;
    std::function<void(InstanceId,int,bool)> onLearn;
    std::function<void(PedalView&,juce::Point<int>,juce::Point<int>,bool)> onDrag;
    std::function<bool()> onCancelDrag;
    std::function<void(PedalView&)> onFocus;
    std::function<void(InstanceId,int)> onNavigate;
    bool learnMode=false;
private:
    void contextMenu();
    bool moveTo(int index);
    void post(BoardCommand);
    void rebuildControls(const PedalInstance&);
    void placeControls();
    void clearAttachments();
    BoardModel& model;
    const InstanceId instanceId;
    juce::AudioProcessorValueTreeState* parameters;
    std::string modelId;
    std::unique_ptr<PedalFace> face;
    FaceLayout faceLayout;
    const PedalDesc* descriptor=nullptr;
    FootswitchButton footswitch {"Engage"};
    juce::TextButton details {"EST ..."};
    juce::ToggleButton profile {"Use pedal CC profile"};
    juce::ComboBox placement,mounting,channel,footMode,position;
    juce::Component inspector;
    juce::Label provenance,inputLabel,outputLabel,laneLabel,meterLabel,display;
    PedalMeters meters;
    bool meterSeen=false;
    juce::OwnedArray<juce::Slider> controls;
    juce::OwnedArray<juce::ComboBox> selectors;
    juce::OwnedArray<FootswitchButton> actions;
    juce::OwnedArray<juce::ToggleButton> switches;
    juce::OwnedArray<juce::Label> labels;
    std::vector<int> controlParams,selectorParams,actionParams,switchParams;
    std::array<juce::Component*,kMaxParams> parameterControls {};
    std::array<bool,kMaxParams> attached {};
    std::array<bool,2> trimAttached {};
    juce::Slider inputTrim,outputTrim,lane;
    juce::RangedAudioParameter* activePositionGesture=nullptr;
    bool refreshing=false,dragging=false,positionAttached=false;
    juce::Point<int> grabOffset;
    uint32_t gesture=0;
    struct InspectorDismissal final : juce::MouseListener
    {
        explicit InspectorDismissal(PedalView& p):owner(p){}
        void mouseDown(const juce::MouseEvent&) override;
        PedalView& owner;
    } inspectorDismissal {*this};
    std::unique_ptr<juce::Component> inspectorHost;
    std::unique_ptr<juce::CallOutBox> inspectorCallout;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
};
}
