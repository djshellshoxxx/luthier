#pragma once

/*  proposals/visual-polish.md 4 (approved): "Stage and ambient touches - live
    indicators, not animation for its own sake".

    - VuMeter: an optional needle meter reading the master output, VU
      ballistics (300 ms), 0 VU at -18 dBFS, -20 to +3 on the scale. Shown in
      the Easy window beside the level meter; Options -> Appearance can hide it.
    - RoomLight: a backdrop for the ROOM card and section that warms and widens
      with the room's size and wet level, so the space you are playing in has a
      look.

    Both update at the meter rates gui-engine-dataflow.md allows (30 Hz for the
    needle, 10 Hz for the light), grey out when stale like every other live
    display, and are flat under High contrast (visual-polish.md 0.2).
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class VuMeter : public juce::Component,
                public juce::SettableTooltipClient,
                private juce::Timer
{
public:
    explicit VuMeter (LuthierAudioProcessor& processor);
    ~VuMeter() override;

    void paint (juce::Graphics&) override;

    static constexpr double kReferenceDbfs = -18.0;   ///< 0 VU
    static constexpr double kMinVu = -20.0, kMaxVu = 3.0;
    static constexpr double kIntegrationMs = 300.0;   ///< VU ballistics
    static constexpr double kStaleMs = 1000.0;

    /** Options -> Appearance's switch (visual-polish.md 4: "optional"). */
    static bool isEnabledByUser();
    static void setEnabledByUser (bool enabled);

    /** One tick with the level and the clock passed in (tests). */
    void update (double rmsLinear, double nowMs);

    double getNeedleVu() const noexcept { return needleVu; }
    bool isStale() const noexcept { return stale; }

    /** Where 0 - 1 along the arc a VU reading sits (the scale is not linear in dB). */
    static float scalePosition (double vu) noexcept;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    double needleVu = kMinVu;
    double lastLevel = -1.0, lastChangeMs = 0.0, lastTickMs = 0.0;
    bool stale = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VuMeter)
};

//==============================================================================
class RoomLight : public juce::Component,
                  private juce::Timer
{
public:
    explicit RoomLight (LuthierAudioProcessor& processor);
    ~RoomLight() override;

    void paint (juce::Graphics&) override;

    /** What the light shows now: 0-1 warmth (wet level) and 0-1 spread (size); 0 when the room is off. */
    float getWarmth() const noexcept { return warmth; }
    float getSpread() const noexcept { return spread; }

    /** Reads the room's parameters (the timer's work, public for tests). */
    void refresh();

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    float warmth = 0.0f, spread = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomLight)
};

} // namespace luthier
