#pragma once

/*  The CHARACTER tab's three phase-2b groups (REALISM-A):

      STRING AGING   (string-aging.md 7)   after STRING NOISE
      ENVIRONMENT    (environment.md 7)    where character-wear 10's environment was
      BODY COUPLING  (body-coupling.md 5)  after SETUP

    Every control is a parameter attachment (ui-wiring.md 2); the live
    readouts drain published atomics at the rates gui-engine-dataflow.md gives
    (4 Hz aging, 10 Hz environment) and grey out after 2 s without a change.
    The wolf map is computed here, on the message thread, from the bank's
    design whenever the design or the scaling moves - never streamed.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Coupling/BodyCouplingBank.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class StringAgingGroup : public juce::Component,
                         private juce::Timer
{
public:
    explicit StringAgingGroup (LuthierAudioProcessor& processor);
    ~StringAgingGroup() override;

    int preferredHeight() const;
    void resized() override;
    void paint (juce::Graphics&) override;

    /** string-aging.md 6: one string new; the rest untouched. */
    void restring (int stringIndex);

    /** string-aging.md 6: hours to 0 as a parameter gesture and every
        string's state cleared, as one undo step. */
    void restringAll();

    bool isStale() const noexcept;

    static constexpr int rowHeight = 18;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading;
    LuthierSlider hours { "String age" }, corrosivity { "Hand corrosivity" }, detail { "Aging detail" };
    LuthierChoice coating { "Coating" }, accrual { "Age while playing" };
    juce::OwnedArray<juce::TextButton> restringButtons;
    juce::TextButton restringAllButton { "Restring all" };

    juce::uint32 lastSerial = 0;
    double lastChange = -1.0e9;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringAgingGroup)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "StringAgingGroup" };
};

//==============================================================================
/** environment.md 7: the low-E offset over the last 60 s. */
class EnvironmentSparkline : public juce::Component
{
public:
    void push (float cents);
    void paint (juce::Graphics&) override;

    void setStale (bool s) { if (stale != s) { stale = s; repaint(); } }

    static constexpr int kPoints = 600;   ///< 60 s at 10 Hz

private:
    std::array<float, kPoints> ring {};
    int head = 0, count = 0;
    bool stale = false;
};

class EnvironmentGroup : public juce::Component,
                         private juce::Timer
{
public:
    explicit EnvironmentGroup (LuthierAudioProcessor& processor);
    ~EnvironmentGroup() override;

    int preferredHeight() const;
    void resized() override;

    /*  environment.md 3.2: Retune, shared with the tuner drift. The reference
        becomes the parts' current temperatures and env_tuned_at_c is written
        to the mean string temperature, rounded to 0.1 C, as one undo step. */
    void retune();

    bool isStale() const noexcept;
    bool isConvolutionNoteShowing() const noexcept { return convolutionNote.isVisible(); }

    /** The readout text, for the tests. */
    juce::String getReadoutText() const { return readout.getText(); }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading, readout, centsReadout, convolutionNote;
    LuthierSlider temperature { "Ambient temperature" }, tunedAt { "Tuned at" }, humidity { "Humidity" };
    LuthierChoice profile { "Session profile" }, clock { "Profile clock" };
    juce::TextButton retuneButton { "Retune" };
    EnvironmentSparkline sparkline;

    juce::uint32 lastSerial = 0;
    double lastChange = -1.0e9;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvironmentGroup)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "EnvironmentGroup" };
};

//==============================================================================
/** body-coupling.md 5: the bank's modes as the bridge sees them. */
class BodyModeList : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    void setModes (const juce::Array<BodyCouplingBank::ModeView>& m) { modes = m; repaint(); }
    void paint (juce::Graphics&) override;

    static constexpr int rowHeight = 12;
    static constexpr int maxRows = 8;

private:
    juce::Array<BodyCouplingBank::ModeView> modes;
};

/** body-coupling.md 5: the wolf map, strings x frets 0-19. */
class WolfMap : public juce::Component,
                public juce::SettableTooltipClient
{
public:
    static constexpr int kFrets = 19;
    static constexpr float kWarnLoss = 0.30f;

    void setMap (const std::vector<float>& cells, int numStrings);
    void paint (juce::Graphics&) override;

    float getCell (int s, int fret) const noexcept;
    int getNumStrings() const noexcept { return strings; }

private:
    std::vector<float> map;
    int strings = 0;
};

class BodyCouplingGroup : public juce::Component,
                          private juce::Timer
{
public:
    explicit BodyCouplingGroup (LuthierAudioProcessor& processor);
    ~BodyCouplingGroup() override;

    int preferredHeight() const;
    void resized() override;

    /** Recomputes the mode list and the wolf map if the design or scaling moved. */
    void refreshNow();

    const WolfMap& getWolfMap() const noexcept { return wolfMap; }

    /** The map from an engine, for the UI and the tests. */
    static std::vector<float> computeWolfMap (const LuthierAudioProcessor& processor, int& numStrings);

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading, mapHeading;
    LuthierSlider mass { "Body mode mass" }, q { "Body mode Q" }, tuning { "Body mode tuning" };
    LuthierChoice modes { "Coupling modes" };
    BodyModeList modeList;
    WolfMap wolfMap;
    juce::TextButton tapButton { "Tap" };

    juce::uint64 lastDigest = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodyCouplingGroup)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "BodyCouplingGroup" };
};

} // namespace luthier
