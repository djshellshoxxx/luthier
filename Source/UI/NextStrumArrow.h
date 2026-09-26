#pragma once

/*  SPEC-SWEEP (GD-10, gui-engine-dataflow 5): the next-strum arrow.

    A small component on a 60 Hz timer: an arrow pointing the way the next
    stroke goes, flashing briefly each time the rhythm engine plays a stroke,
    and hidden once the engine has not reported (driven a block) for 500 ms.
    It replaces the arrow glyph the Easy rhythm readout used to carry. */

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class LuthierAudioProcessor;

class NextStrumArrow : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    explicit NextStrumArrow (LuthierAudioProcessor& processor);
    ~NextStrumArrow() override;

    static constexpr int kRefreshHz = 60;
    static constexpr double kStaleMs = 500.0;
    static constexpr double kFlashMs = 90.0;

    /** One refresh at @p nowMs (the timer passes the real clock). */
    void tick (double nowMs);

    bool isShown() const noexcept { return shown; }
    bool isFlashing() const noexcept { return flashing; }
    bool pointsDown() const noexcept { return down; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::uint32 lastBlocks = 0, lastStrokes = 0;
    double lastReportMs = -1.0e9, flashUntilMs = -1.0e9;
    bool shown = false, flashing = false, down = true;
};

} // namespace luthier
