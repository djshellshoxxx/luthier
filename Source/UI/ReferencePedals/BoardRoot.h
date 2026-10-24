// OWNER: WP17
#pragma once
#include "BoardCanvas.h"
#include "PedalBrowser.h"
#include "BoardUiState.h"
#include "BoardViewport.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace luthier {class LuthierAudioProcessor;}
namespace luthier::reference
{
class BoardRoot final : public juce::Component, public juce::DragAndDropContainer, private BoardModel::Listener, private juce::Timer
{
public:
    explicit BoardRoot(BoardModel&, LuthierAudioProcessor* = nullptr);
    ~BoardRoot() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
    juce::String statusText() const {return status.getText();}
    void setCompact(bool);
    void saveView();
    std::function<void()> onDock, onPopout, onToggle, onReturnFocus;
    BoardCanvas& boardCanvas(){return canvas;}
private:
    void refresh();
    void settingsChanged() override {refresh();}
    void orderChanged() override {refresh();}
    void rejected(const juce::String& text) override {status.setText(text,juce::dontSendNotification);}
    void timerCallback() override;
    void zoomTo(float);
    void flushView();
    void showAddMenu();
    void showSettings();
    BoardModel& model;
    LuthierAudioProcessor* processor;
    BoardCanvas canvas;
    BoardViewport viewport;
    juce::Viewport toolbarViewport;
    juce::Component toolbar;
    PedalBrowser browser;
    juce::TextButton add{"+ Add"},undo{"Undo"},redo{"Redo"},fit{"Fit"},minus{"−"},plus{"+"},dock{"Dock"},popout{"Pop out"},settings{"Settings"};
    juce::ToggleButton enabled{"Board ON"},cables{"Cables"},learn{"MIDI Learn"},keepOnTop{"Keep on top"};
    juce::Slider mix,tempo;
    juce::ComboBox style,mode,mixLaw;
    juce::Label status,zoomLabel;
    bool compact=false,refreshing=false;
    float zoom=1.f;
    uint32_t mixGesture=0;
    bool pendingView=false;
    double viewChangedAt=0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment,tempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    InstanceId learnId=InstanceId::null();int learnParam=0;bool learnContinuous=true;bool learnBoard=false;
    double learnStart=0;
};
}
