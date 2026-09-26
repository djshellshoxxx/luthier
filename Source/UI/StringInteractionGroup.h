#pragma once

/*  string-interaction.md 9 (REALISM-B): the CHARACTER tab's STRING INTERACTION
    group - air coupling, palm width, neighbour mute (with the fretting-hand
    style it is scaled by), release stagger and order, pole aperture and the
    muted-string thump. A small strip shows which strings the palm covers.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

class StringInteractionGroup : public juce::Component,
                               private juce::Timer
{
public:
    explicit StringInteractionGroup (LuthierAudioProcessor& processor);
    ~StringInteractionGroup() override;

    static constexpr int headingHeight = 20, rowHeight = 22, noteHeight = 14, stripHeight = 18, gap = 2;
    static constexpr int preferredHeight = headingHeight + gap + 7 * (rowHeight + gap) + noteHeight + gap + stripHeight + 4;

    /** The fretting-hand style line under the neighbour mute. */
    juce::String describeFrettingStyle() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::Label heading, styleNote;
    juce::Rectangle<int> palmStrip;
    std::array<float, 12> palm {};

    LuthierSlider air { "Air coupling" }, palmWidth { "Palm width" }, neighbour { "Neighbour mute" },
                  stagger { "Release stagger" }, order { "Stagger order" }, aperture { "Pole aperture" },
                  thump { "Muted-string thump" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringInteractionGroup)
};

} // namespace luthier
