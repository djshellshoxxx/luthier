#pragma once

/*  The CIRCUIT panel's two bespoke pieces (volume-knob-interaction.md 5).

    `CircuitResponseView` is the live visualiser: the magnitude of the circuit's
    H(s) from pickup to amp input, redrawn when anything in the network moves,
    with the resonant peak marked. It is the feature that teaches the
    interaction - turn the volume knob and watch the peak slide down and
    flatten - so it draws the network the audio is actually going through,
    selected pickups included, rather than one reconstructed from the knobs.

    `StandardValueChoice` is the "250k / 500k / 1M / custom" dropdown: a
    combo of the values people actually buy, bound to a continuous parameter.
    A value that is none of them reads as Custom, and the knob beside it is how
    a custom value is set.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Circuit/GuitarCircuit.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class CircuitResponseView : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit CircuitResponseView (LuthierAudioProcessor& processor);
    ~CircuitResponseView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Recomputes the curve if the network has changed. Returns true if it did. */
    bool refresh();

    /** The resonant peak currently marked, in Hz. For tests and the readout. */
    double getMarkedPeakHz() const noexcept { return peakHz; }

    static constexpr int kPoints = 160;
    static constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
    static constexpr double kTopDb = 12.0, kBottomDb = -36.0;

private:
    void timerCallback() override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    LuthierAudioProcessor& processor;

    CircuitComponents shown;
    bool hasShown = false;

    std::array<double, kPoints> curveDb {};
    double peakHz = 0.0, peakDb = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircuitResponseView)
};

//==============================================================================
class StandardValueChoice : public LearnTarget,
                            public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    /** `values` in the parameter's plain units; `names` the same length. */
    StandardValueChoice (const juce::String& labelText,
                         juce::Array<double> values,
                         juce::StringArray names);
    ~StandardValueChoice() override;

    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId,
                   const juce::String& tooltip = {});

    juce::String getLearnParameterId() const override { return paramId; }
    juce::ComboBox& getComboBox() noexcept { return box; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    /** The item id "Custom" occupies. */
    int getCustomItemId() const noexcept { return values.size() + 1; }

private:
    void showValue (float plain);

    juce::ComboBox box;
    juce::String labelText, paramId;
    juce::Array<double> values;
    juce::StringArray names;

    LuthierAudioProcessor* processor = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandardValueChoice)
};

} // namespace luthier
