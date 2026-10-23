// OWNER: WP22
#pragma once
#include "../../ReferencePedals/BoardModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class MappingCallout final : public juce::Component
{
public:
    MappingCallout(BoardModel&,juce::var binding);
    void resized() override;
private:
    void apply();
    BoardModel& model;
    juce::var binding;
    juce::ComboBox source,mode,curve,takeover;
    juce::Slider number,channel,minimum,maximum;
    juce::TextButton save{"Apply mapping"},remove{"Clear mapping"};
    juce::OwnedArray<juce::Label> labels;
};
}
