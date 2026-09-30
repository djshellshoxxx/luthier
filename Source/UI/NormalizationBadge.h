#pragma once

/*  output-normalization.md 5.1 (secondary access): the header badge.

    40 px, visible only while normalization is on: "N +7.5". It sits in the
    header beside the output meter (the header's output LED in this build),
    and under the LevelMeter in Easy Mode's meter column. Clicking it, or
    Enter on it, opens Options -> AUDIO with the switch focused. It is a
    focusable Button in the header's Tab order, named for screen readers
    (9: "Output normalization, plus 7.5 decibels").
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace luthier
{

class LuthierAudioProcessor;

class NormalizationBadge final : public juce::Button,
                                 private juce::Timer
{
public:
    explicit NormalizationBadge (LuthierAudioProcessor& processor);
    ~NormalizationBadge() override;

    static constexpr int preferredWidth = 40;

    /** Opens Options -> AUDIO with the switch focused. Wired by the editor. */
    std::function<void()> onOpenOptions;

    /** Re-reads the status now (the timer does this at 10 Hz). */
    void refresh();

    juce::String getBadgeText() const { return text; }

    /** Enter (or space) on the focused badge clicks it (5.1). */
    bool keyPressed (const juce::KeyPress&) override;

    /** Tells the owner that visibility changed, so it can re-lay out. */
    std::function<void()> onVisibilityChanged;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    juce::String text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NormalizationBadge)
};

} // namespace luthier
