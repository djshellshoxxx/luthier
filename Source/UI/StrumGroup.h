#pragma once

/*  The RHYTHM tab's STRUM group (gui-integration.md 4.4, strum-dynamics.md 6.3).

    Crossing velocity, acceleration, up-stroke speed, tilt, evenness, miss
    probability, the two striker dropdowns, chuck amount and chuck damping -
    the list 6.3 gives, in that order.

    Nine of the ten are parameters (strum-dynamics 7) and attach like every
    other control. Evenness is not: it already existed as the rhythm engine's
    own state, which genre kits set, so its slider drives the engine directly
    the way the rest of the RHYTHM panel does.

    The crossing knob is the plugin-global default (ambiguity-resolutions 6),
    which a pattern's crossing_sps or a genre kit's default sits above. A knob
    that silently does nothing is worse than no knob, so the group says which
    source is in charge, and when it is the kit, offers to hand the strum back
    to the knob.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../Rhythm/RhythmEngine.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class StrumGroup : public juce::Component,
                   private juce::Timer
{
public:
    explicit StrumGroup (LuthierAudioProcessor& processor);
    ~StrumGroup() override;

    static constexpr int headingHeight = 20;
    static constexpr int sourceHeight = 22;
    static constexpr int rowHeight = 22;
    static constexpr int choiceHeight = 36;
    static constexpr int gap = 2;

    static constexpr int preferredHeight = headingHeight + gap
                                         + sourceHeight + gap
                                         + 8 * (rowHeight + gap)
                                         + 2 * (choiceHeight + gap)
                                         + 4;

    /** Re-reads what is not a parameter: the evenness and the crossing source. */
    void refresh();

    /** The sentence under the heading, naming the crossing velocity's source. */
    juce::String describeCrossingSource() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For tests: the controls, so a test can drive them as a user would.
    LuthierSlider& getCrossingControl() noexcept     { return crossing; }
    LuthierSlider& getEvennessControl() noexcept     { return evenness; }
    LuthierChoice& getStrikerDownControl() noexcept  { return strikerDown; }
    LuthierChoice& getStrikerUpControl() noexcept    { return strikerUp; }
    juce::TextButton& getFollowKnobButton() noexcept { return followKnobButton; }
    juce::String getSourceText() const               { return sourceLabel.getText(); }

private:
    void timerCallback() override;

    RhythmEngine& rhythm();
    const RhythmEngine& rhythm() const;

    LuthierAudioProcessor& processor;

    juce::Label heading, sourceLabel;
    juce::TextButton followKnobButton { "USE KNOB" };

    LuthierSlider crossing { "Crossing" }, acceleration { "Acceleration" },
                  upRatio { "Up speed" }, tilt { "Tilt" }, evenness { "Evenness" },
                  misses { "Misses" }, chuckAmount { "Chuck" }, chuckDamping { "Chuck damping" };

    LuthierChoice strikerDown { "Down striker" }, strikerUp { "Up striker" };

    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumGroup)
};

} // namespace luthier
