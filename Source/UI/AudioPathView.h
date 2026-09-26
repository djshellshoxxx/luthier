#pragma once

/*  Options -> Diagnostics -> "What's on the audio path right now"
    (gui-integration.md 20): a live block diagram of the DSP stages that are
    doing something, in signal order (engine.md 1), refreshed at 4 Hz. A stage
    that is bypassed, off or empty is drawn dim; one that is on is lit and
    says what it is (the amp model, how many pedals).

    Also the Workshop / Slide / advanced-ranges booleans gui-integration 5 asks
    Diagnostics to mirror, "for confirming what telemetry would see". */

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnimationPolicy.h"   // cpu-quality-modes 6

namespace luthier
{

class LuthierAudioProcessor;

class AudioPathView : public juce::Component,
                      private juce::Timer
{
public:
    explicit AudioPathView (LuthierAudioProcessor& processor);
    ~AudioPathView() override;

    void paint (juce::Graphics&) override;

    struct Stage
    {
        juce::String name, detail;
        bool active = false;
    };

    /** The stages as they stand now, in signal order. */
    std::vector<Stage> readStages() const;

    /** The three feature booleans, as one line ("Workshop edit: yes  Slide Mode: off  Advanced ranges: amp, pick"). */
    juce::String describeFlags() const;

    void refresh();

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "AudioPathView" };   // cpu-quality-modes 6
    std::vector<Stage> stages;
    juce::String flags;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPathView)
};

} // namespace luthier
