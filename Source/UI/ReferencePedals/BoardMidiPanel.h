// OWNER: WP22
#pragma once
#include "../../ReferencePedals/BoardModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "OwnedCallout.h"
namespace luthier::reference
{
// Native accessible controls for board configuration, in a non-modal scrolling callout.
class BoardMidiPanel final : public juce::Component
{
public:
    explicit BoardMidiPanel(BoardModel&,juce::AudioProcessorValueTreeState* = nullptr);
    ~BoardMidiPanel() override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusOfChildComponentChanged(FocusChangeType) override;
    std::function<void()> onClose;
private:
    void scalar(const char* key,const char* title,double min,double max,double value,double interval=1);
    void toggle(const char* key,const char* title,bool);
    void addHostControls(juce::AudioProcessorValueTreeState&);
    BoardModel& model;
    OwnedCallout mappingPopup;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    juce::Component content;
    juce::Viewport viewport;
    juce::OwnedArray<juce::Component> widgets;
    int y=6;
};
}
