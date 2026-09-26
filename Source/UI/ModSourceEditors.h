#pragma once

/*  SPEC-SWEEP: the MOD tab's two graphical source editors
    (modulation-matrix.md 1.1 and 1.3; MM-12, MM-22).

      - LfoBreakpointEditor: the custom LFO shape's eight points, dragged up
        and down. Shown on the LFO card when the shape is Custom.
      - StepGridEditor: the step sequencer's steps. A bar per step, dragged to
        its value; a click with Cmd/Ctrl (or a right-click) toggles its gate,
        an Alt-click its slide; the strip along the bottom is each step's
        probability.

    Both read the engine's state through a getter and write through a setter
    the card supplies (the card posts a ModSourceEdit, so the audio thread
    applies it), which keeps them free of the processor and testable on their
    own. */

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Modulation/ModSources.h"

namespace luthier
{

//==============================================================================
class LfoBreakpointEditor : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    LfoBreakpointEditor();

    std::function<double (int)> getPoint;               ///< -1..1
    std::function<void (int, double)> setPoint;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    /** Which point an x position is nearest, and the value a y position means. */
    int pointAt (float x) const noexcept;
    double valueAt (float y) const noexcept;

    static constexpr int preferredHeight = 48;

private:
    void edit (juce::Point<float> position);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoBreakpointEditor)
};

//==============================================================================
class StepGridEditor : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    StepGridEditor();

    std::function<int()> getLength;
    std::function<ModStepSequencer::Step (int)> getStep;
    std::function<void (int, const ModStepSequencer::Step&)> setStep;
    std::function<int()> getPlayingStep;                  ///< optional, for the cursor

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    int stepAt (float x) const noexcept;
    bool isInProbabilityRow (float y) const noexcept;

    static constexpr int preferredHeight = 80;
    static constexpr int probabilityRowHeight = 12;

private:
    void editValue (juce::Point<float> position);
    int length() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGridEditor)
};

} // namespace luthier
