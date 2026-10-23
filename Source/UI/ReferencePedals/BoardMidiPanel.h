// OWNER: WP22
#pragma once
#include "../../ReferencePedals/BoardModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
// Native accessible controls for board configuration, in a non-modal scrolling callout.
class BoardMidiPanel final : public juce::Component
{
public:
    explicit BoardMidiPanel(BoardModel&);
    ~BoardMidiPanel() override;
    void resized() override;
private:
    void scalar(const char* key,const char* title,double min,double max,double value,double interval=1);
    void toggle(const char* key,const char* title,bool);
    BoardModel& model;
    juce::Component content;
    juce::Viewport viewport;
    juce::OwnedArray<juce::Component> widgets;
    int y=6;
};
}
