#pragma once

/*  The CHARACTER tab's SLIDE group (gui-integration.md 4.4, slide-guitar.md 7).

    Shown only while Slide Mode is on: the mode, pressure (with what that
    pressure means - rattling, seated, choking), slant, damping behind the bar,
    the intonation assist marked as the playability aid it is, noise and clank,
    and the bar's material and mass mirrored read-only from the Workshop part.

    It also carries slide-guitar.md 6's warning: a setup with less than 2.2 mm
    of bass action was not built for slide. The setup is never changed for the
    user; the Slide setup style is one click away and the choice is theirs.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

class SlideGroup : public juce::Component,
                   private juce::Timer
{
public:
    explicit SlideGroup (LuthierAudioProcessor& processor);
    ~SlideGroup() override;

    /** 0 while Slide Mode is off: the group is not there at all. */
    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called when Slide Mode turns on or off, so the owner can re-lay-out. */
    std::function<void()> onShownChanged;

    bool isSlideModeOn() const;

    /** What the timer does, now: follows Slide Mode, the mode's damping and the setup. */
    void refresh() { timerCallback(); }
    bool isLowActionWarningShowing() const noexcept { return lowAction.isVisible(); }

    /** "Rattling", "Seated" or "Choking" for a pressure (slide-guitar.md 3). */
    static juce::String describePressure (double pressure);

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading, barMirror, pressureState, lowAction;
    juce::TextButton useSlideSetup { "Use Slide setup" };

    LuthierChoice mode { "Mode" };
    LuthierSlider pressure { "Pressure" }, slant { "Slant" }, damping { "Damp behind" },
                  assist { "Assist (aid)" }, noise { "Noise" }, clank { "Clank" };

    bool shown = false;
    int lastMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlideGroup)
};

} // namespace luthier
