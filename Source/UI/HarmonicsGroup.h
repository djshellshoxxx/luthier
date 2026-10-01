#pragma once

/*  harmonic-realism.md 7 (REALISM-B): the HARMONICS row of the CHARACTER tab's
    PICK group - touch pressure, finger width, touch time, graze time, thumb
    offset, the artificial and tapped offsets and the note mapping.

    Every control is a parameter and attaches like any other (ui-wiring.md 2);
    the physical ones are PhysicalRange rows in the pick family, so the
    advanced-range padlock marks them without code here.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

class HarmonicsGroup : public juce::Component
{
public:
    explicit HarmonicsGroup (LuthierAudioProcessor& processor);

    static constexpr int headingHeight = 20, rowHeight = 22, choiceHeight = 36, gap = 2;
    static constexpr int preferredHeight = headingHeight + gap + 5 * (rowHeight + gap) + 2 * (choiceHeight + gap) + 4;

    /** The tooltip text for an offset choice: the partial it sounds (7). */
    static juce::String describeOffset (int choiceIndex);

    void resized() override;

    LuthierSlider& getTouchPressure() noexcept { return pressure; }
    LuthierChoice& getNoteMapping() noexcept   { return mapping; }

private:
    LuthierAudioProcessor& processor;
    juce::Label heading;

    LuthierSlider pressure { "Harmonic touch" }, width { "Finger width" }, touchTime { "Touch time" },
                  graze { "Pinch / tap graze" }, thumbOffset { "Thumb offset" };
    LuthierChoice artificial { "Artificial offset" }, tapped { "Tapped offset" }, mapping { "Harmonic notes" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicsGroup)
};

} // namespace luthier
