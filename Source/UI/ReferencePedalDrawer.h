#pragma once

// Independent experimental physical-reference pedal drawer. The legacy
// PedalRack, its GUI, models, and serialized slots are never modified.
#include <juce_gui_basics/juce_gui_basics.h>
#include "../ReferencePedals/Board.h"

namespace luthier
{
class LuthierAudioProcessor;

namespace reference
{
class Card final : public juce::Component
{
public:
    Card (LuthierAudioProcessor& processor, int position);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    int instanceId() const noexcept;
    Board& board() const noexcept;

    LuthierAudioProcessor& processor;
    int position = 0;
    juce::Label title;
    juce::ComboBox modelChoice;
    juce::ComboBox placementChoice;
    juce::TextButton footswitch {"On"};
    juce::TextButton earlier {"<"};
    juce::TextButton later {">"};
    std::array<juce::Slider, 3> knobs;
    std::array<juce::Label, 3> knobLabels;
    bool refreshing = false;
};

class Drawer final : public juce::Component
{
public:
    explicit Drawer (LuthierAudioProcessor& processor);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setOpen (bool value) noexcept;
    bool isOpen() const noexcept { return open; }
    int getPreferredHeight() const noexcept { return open ? 232 : 0; }
    void refresh();
private:
    LuthierAudioProcessor& processor;
    bool open = false;
    juce::TextButton close {"Close"};
    juce::ToggleButton boardOn {"Enable reference pedals"};
    juce::Label description;
    juce::Viewport viewport;
    juce::Component contents;
    juce::OwnedArray<Card> cards;
};
} // namespace reference
} // namespace luthier
