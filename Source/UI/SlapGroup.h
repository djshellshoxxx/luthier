#pragma once

/*  The CHARACTER tab's SLAP group (bass-techniques.md 9, gui-integration.md
    0.7 / 4.4) - MODEL-GAPS workstream.

    "Slap strength, position, thumb hardness, fret contact; pop strength and
    position; double thump toggle and ratio; ghost level, damping, auto-ghost
    toggle and threshold" - plus section 6's fingerstyle pair, finger
    alternation and the rest stroke, which are bass amounts too.

    Shown only while the loaded guitar is a bass (0.1, gui-integration 0.7:
    "If the current guitar is not a bass, the SLAP group is not shown").
    Every control is an attached parameter, so it saves with the preset.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

class SlapGroup : public juce::Component,
                  private juce::Timer
{
public:
    explicit SlapGroup (LuthierAudioProcessor& processor);
    ~SlapGroup() override;

    /** 0 while the guitar is not a bass: the group is not there at all. */
    int preferredHeight() const;

    void resized() override;

    /** Called when the group appears or goes, so the owner can re-lay-out. */
    std::function<void()> onShownChanged;

    bool isBassLoaded() const;

    /** What the timer does, now. */
    void refresh() { timerCallback(); }

    /** bass-techniques 0.1's fixed empty-state message. */
    static constexpr const char* kInactiveMessage = "Bass techniques are inactive. Load a bass to use them.";

    /** For the tests: every parameter the group carries. */
    juce::StringArray getAttachedParameterIds() const;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading, slapLabel, popLabel, ghostLabel, fingerLabel;

    LuthierSlider slapStrength { "Strength" }, slapPosition { "Position" }, thumbHardness { "Thumb hardness" },
                  fretContact { "Fret contact" }, popStrength { "Strength" }, popPosition { "Position" },
                  upRatio { "Up-stroke" }, reboundGap { "Rebound gap" },
                  ghostLevel { "Level" }, ghostDamping { "Damping" },
                  ghostThreshold { "Below velocity" }, alternation { "Alternation" };
    LuthierToggle doubleThump { "Double thump" }, ghostAuto { "Auto ghost" }, restStroke { "Rest stroke" };

    bool shown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapGroup)
};

} // namespace luthier
