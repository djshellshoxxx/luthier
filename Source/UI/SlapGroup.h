#pragma once

/*  The SLAP technique's controls (string-slap-technique.md 1 and 6,
    gui-techniques-updates.md 1: the TECHNIQUES tab's SLAP sub-tab).

    Every control here is a parameter (426-450) and attaches like any other.
    The arm pill at the top is section 1's "arm" affordance; the STRIKE
    button is the Playing-strip trigger, through the technique layer's shared
    front (TechniqueTriggers::request), so it lands on the next block like a
    keyswitch would. The preset box writes section 4's factory slaps into the
    parameters, which is what a preset is here: nothing is stored beside them.

    The TECHNIQUES tab itself is not built yet (AdvancedPanel's note), so this
    group is a self-contained component for it to host; until then a test or
    the lead's hook can place it anywhere a Component goes.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Slap/SlapEngine.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class SlapGroup : public juce::Component,
                  private juce::Timer
{
public:
    explicit SlapGroup (LuthierAudioProcessor& processor);
    ~SlapGroup() override;

    static constexpr int headingHeight = 20;
    static constexpr int rowHeight = 22;
    static constexpr int choiceHeight = 36;
    static constexpr int maskHeight = 24;
    static constexpr int gap = 2;

    static constexpr int preferredHeight = headingHeight + gap
                                         + (Metrics::buttonHeight + gap)     // arm + strike
                                         + (rowHeight + gap)                 // presets
                                         + 3 * (choiceHeight + gap)          // type, trigger, body part
                                         + (maskHeight + gap)                // strings
                                         + 14 * (rowHeight + gap)            // the sliders and toggles
                                         + 4;

    /** 4: writes a factory slap into the parameters. */
    void applyPreset (SlapPreset preset);

    /** The Playing-strip strike: a slap now, on the type's strings. */
    void strike();

    void resized() override;

    // For tests.
    LuthierToggle& getArmToggle() noexcept       { return armToggle; }
    LuthierChoice& getTypeControl() noexcept     { return type; }
    juce::ComboBox& getPresetBox() noexcept      { return presetBox; }
    juce::TextButton& getStrikeButton() noexcept { return strikeButton; }
    juce::String getStatusText() const           { return status.getText(); }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading, status;
    LuthierToggle armToggle { "SLAP" };
    juce::TextButton strikeButton { "STRIKE" };
    juce::ComboBox presetBox;

    LuthierChoice type { "Slap type" }, trigger { "Trigger source" }, bodyPart { "Body tap resonance" };
    std::unique_ptr<StringMaskSelector> strings;

    LuthierSlider slapStrength { "Slap strength" }, slapPosition { "Slap position" },
                  thumbHardness { "Thumb hardness" }, fretContact { "Fret contact" },
                  popStrength { "Pop strength" }, popPosition { "Pop position" },
                  force { "Contact force" }, palmPosition { "Palm position" },
                  upRatio { "Rebound level" }, reboundGap { "Rebound gap" },
                  ghostLevel { "Ghost level" }, ghostDamping { "Ghost damping" },
                  ghostThreshold { "Ghost below" }, snapBack { "Snap-back" };

    LuthierToggle doubleThump { "Rebound" }, ghostMode { "Ghost mode" }, ghostAuto { "Auto ghost" };

    juce::uint32 lastFired = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapGroup)
};

} // namespace luthier
