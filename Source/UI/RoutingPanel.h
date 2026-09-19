#pragma once

/*  The ROUTING panel (routing-io.md section 8).

    Everything this panel edits lives in the RoutingMatrix rather than in the
    parameter tree. That is deliberate: aux trims, mute and solo are mixing
    decisions that belong to the session, not performance controls a host should
    be automating, and giving each of them a parameter would add forty entries to
    a list the user has to scroll past to find the ones that matter.

    The spec asks for this as a tab in Column 4's tab strip. Column 4 has no tab
    strip - it is a single scrolling list of sections - so the panel is built as
    one more section in that list, which is where every other rig control lives.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Routing/RoutingMatrix.h"
#include "../Parameters.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** One aux bus: name, mute, solo, gain trim and a meter. */
class AuxStrip : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    AuxStrip (LuthierAudioProcessor& processor, int busIndex);
    ~AuxStrip() override;

    /** Called by the panel's timer; repaints only when something moved. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 30;

private:
    RoutingMatrix& routing();

    LuthierAudioProcessor& processor;
    int bus = 0;

    juce::Slider gain { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    juce::Rectangle<int> muteBounds, soloBounds, meterBounds;

    float displayedLevel = 0.0f;
    bool lastMute = false, lastSolo = false, lastAudible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxStrip)
};

//==============================================================================
/** The twelve per-string outputs, compact: a mute and a trim each. Only shown
    when the host negotiated a layout that has them. */
class PerStringStrip : public juce::Component
{
public:
    explicit PerStringStrip (LuthierAudioProcessor& processor);
    ~PerStringStrip() override;

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 74;

private:
    LuthierAudioProcessor& processor;

    juce::Rectangle<int> cellBounds (int stringIndex) const;

    int lastNumStrings = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerStringStrip)
};

//==============================================================================
class RoutingPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit RoutingPanel (LuthierAudioProcessor& processor);
    ~RoutingPanel() override;

    /** Total height this panel wants, given the current layout. Changes when the
        host enables or disables the per-string buses. */
    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateMidiOutFromControls();
    void refreshFromRouting();

    LuthierAudioProcessor& processor;

    juce::Label layoutLabel, latencyLabel, sidechainLabel;

    juce::OwnedArray<AuxStrip> auxStrips;
    std::unique_ptr<PerStringStrip> perStringStrip;

    // --- sidechain --------------------------------------------------------------
    std::unique_ptr<LuthierToggle> sidechainToAmp;
    juce::Rectangle<int> sidechainMeterBounds;
    float sidechainLevel = 0.0f;

    // --- MIDI out ---------------------------------------------------------------
    std::unique_ptr<LuthierToggle> midiOutEnable, midiPassThrough, midiRhythm,
                                   midiStringActivity, midiCcBroadcast;
    juce::ComboBox midiChannel;
    juce::ComboBox macroCc[ParamIDs::kNumMacros];
    juce::Label macroCcLabels[ParamIDs::kNumMacros];

    BusLayout lastLayout = BusLayout::stereoOnly;
    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingPanel)
};

} // namespace luthier
