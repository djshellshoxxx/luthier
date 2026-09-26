#pragma once

/*  The REALISM-C groups: noise-floor.md 5, sustain-and-decay.md 8 and
    tuning-stability.md 6.

    CHARACTER tab (gui-integration.md 4.4):
      - TUNING STABILITY, which the old tuner-drift section becomes: the
        amount, the six scales, auto-retune, and the per-string offset strip
        (click a bar to retune that string).
      - NOISE FLOOR, after aged electronics: the style, the region, a Guitar
        and a Rig knob row, the position pad and the noise meter, and the
        "on Aux 8" switch.
      - SUSTAIN SHAPE: the eight shape parameters in Attack / Decay / Release
        rows, the style, and the live per-string tension readout.

    Column 1 STRINGS gains DecayRow: the sustain style and the decay sketch.
    The Easy headstock popover gains a StabilityBadge on each string row.

    Every live readout drains an atomic on a timer and greys out after 2 s
    without an update (gui-engine-dataflow.md). Every control is a parameter
    attachment (ui-wiring.md 2) except the retune actions, which are commands
    (tuning-stability.md 3).
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../Model/Workshop/PartLibrary.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** noise-floor.md 5: a top-down sketch of the player and the amp. Drag sets
    the distance, the wheel the angle; the hum gain in dB is shown beneath. */
class PositionPad : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit PositionPad (LuthierAudioProcessor& processor);
    ~PositionPad() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int padWidth = 120, padHeight = 90, preferredHeight = padHeight + 14;

    /** Sets the distance from a y position in the pad, for the tests. */
    void setDistanceFromY (float y);

private:
    void timerCallback() override;
    void write (const char* id, double plain, bool gesture);
    double plain (const char* id) const;

    LuthierAudioProcessor& processor;
    double shownAngle = -1.0, shownDistance = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionPad)
};

//==============================================================================
/** noise-floor.md 5: summed noise-floor RMS in dB re the reference pluck. */
class NoiseMeter : public juce::Component, private juce::Timer
{
public:
    explicit NoiseMeter (LuthierAudioProcessor& processor);
    ~NoiseMeter() override;

    void paint (juce::Graphics&) override;
    bool isStale() const noexcept;
    double getShownDb() const noexcept { return shownDb; }
    void pollNow();

    static constexpr int preferredHeight = 18;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    double shownDb = -240.0, lastUpdateTime = -1.0e9;
    juce::uint32 lastUpdates = 0;
};

//==============================================================================
class NoiseFloorGroup : public juce::Component, private juce::Timer
{
public:
    explicit NoiseFloorGroup (LuthierAudioProcessor& processor);
    ~NoiseFloorGroup() override;

    int preferredHeight() const;
    void resized() override;

    NoiseMeter& getMeter() noexcept { return meter; }
    PositionPad& getPad() noexcept { return pad; }
    juce::ComboBox& getStyleBox() noexcept { return styleBox; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::Label heading, guitarLabel, rigLabel;
    juce::ComboBox styleBox;
    LuthierChoice mains { "Mains" };
    LuthierKnob hum { "Hum", LuthierKnob::Size::Small }, fluorescent { "Fluor.", LuthierKnob::Size::Small },
                passive { "Passive", LuthierKnob::Size::Small }, cable { "Cable", LuthierKnob::Size::Small },
                radio { "Radio", LuthierKnob::Size::Small };
    LuthierKnob ground { "Ground", LuthierKnob::Size::Small }, hiss { "Hiss", LuthierKnob::Size::Small },
                microphonics { "Mic'phonic", LuthierKnob::Size::Small };
    PositionPad pad;
    NoiseMeter meter;
    LuthierToggle toAux8 { "Noise floor on Aux 8" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseFloorGroup)
};

//==============================================================================
/** sustain-and-decay.md 8: each string's tension-modulation cents, live. */
class PitchOffsetReadout : public juce::Component, private juce::Timer
{
public:
    explicit PitchOffsetReadout (LuthierAudioProcessor& processor);
    ~PitchOffsetReadout() override;

    void paint (juce::Graphics&) override;
    bool isStale() const noexcept;
    void pollNow();
    double getShownCents (int s) const noexcept { return shown[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    static constexpr int preferredHeight = 20;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    std::array<double, kMaxStrings> shown {};
    double lastUpdateTime = -1.0e9;
    juce::uint32 lastBlocks = 0;
};

class SustainShapeGroup : public juce::Component, private juce::Timer
{
public:
    explicit SustainShapeGroup (LuthierAudioProcessor& processor);
    ~SustainShapeGroup() override;

    int preferredHeight() const;
    void resized() override;

    PitchOffsetReadout& getReadout() noexcept { return readout; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::Label heading, attackLabel, decayLabel, releaseLabel;
    juce::ComboBox styleBox;
    LuthierSlider transient { "Transient" }, attackTime { "Attack time" },
                  fastShare { "Fast share" }, fastRatio { "Fast ratio" }, tension { "Tension pitch" },
                  releaseTime { "Release time" }, releaseSag { "Release sag" }, releaseRing { "Release ring" };
    PitchOffsetReadout readout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SustainShapeGroup)
};

//==============================================================================
/** tuning-stability.md 6: 6-12 bars at +/-20 c full scale, coloured by the
    dominant cause with a dot glyph for monochrome, the breakdown in the
    tooltip; click a bar to retune that string. */
class OffsetStrip : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit OffsetStrip (LuthierAudioProcessor& processor);
    ~OffsetStrip() override;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isStale() const noexcept;
    void pollNow();
    int stringAt (juce::Point<int> p) const;
    double getShownCents (int s) const noexcept { return shown[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    static constexpr int barHeight = 11;
    static int preferredHeightFor (int numStrings) { return numStrings * (barHeight + 2) + 4; }

    static juce::Colour colourFor (int cause);
    static juce::String describe (LuthierAudioProcessor& processor, int stringIndex);

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    std::array<double, kMaxStrings> shown {};
    std::array<int, kMaxStrings> dominant {};
    double lastUpdateTime = -1.0e9;
    juce::uint32 lastBlocks = 0;
    int numStrings = 6;
};

class TuningStabilityGroup : public juce::Component, private juce::Timer
{
public:
    explicit TuningStabilityGroup (LuthierAudioProcessor& processor);
    ~TuningStabilityGroup() override;

    int preferredHeight() const;
    void resized() override;

    OffsetStrip& getStrip() noexcept { return strip; }

    /** The derived figure for the fitted capo (6: "Capo bias: +2.2 to +8.6 c"). */
    static juce::String describeCapoBias (LuthierAudioProcessor& processor);

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    LuthierSlider amount { "Instability" }, settling { "Settling" }, nut { "Nut binding" },
                  backlash { "Tuner backlash" }, creep { "Bridge / saddle" }, memory { "Bend memory" },
                  capo { "Capo bias" };
    LuthierChoice autoRetune { "Auto retune" };
    OffsetStrip strip;
    juce::Label capoFigure;
    int shownStrings = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningStabilityGroup)
};

//==============================================================================
/** sustain-and-decay.md 8: Column 1 STRINGS' DECAY row - the style and a sketch
    of the open low E's envelope over 4 s, legacy dotted, current solid. */
class DecaySketch : public juce::Component, private juce::Timer
{
public:
    explicit DecaySketch (LuthierAudioProcessor& processor);
    ~DecaySketch() override;

    void paint (juce::Graphics&) override;

    /** The predicted envelope in dB at t, from the same formulas as the engine. */
    static double envelopeDb (double t, double t60, double fastShare, double fastRatio);

    /** How many times the parameters moved and the sketch was redrawn. */
    int getRedrawCount() const noexcept { return redraws; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    std::array<double, 4> shownValues { -1.0, -1.0, -1.0, -1.0 };
    int redraws = 0;
};

class DecayRow : public juce::Component, private juce::Timer
{
public:
    explicit DecayRow (LuthierAudioProcessor& processor);
    ~DecayRow() override;

    void resized() override;

    static constexpr int preferredHeight = 16 + 24 + 4 + 64;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::Label heading;
    juce::ComboBox styleBox;
    DecaySketch sketch;
};

//==============================================================================
/** tuning-stability.md 6: the Easy headstock popover's per-string "+3 c" and
    its Retune button. */
class StabilityBadge : public juce::Component, private juce::Timer
{
public:
    StabilityBadge (LuthierAudioProcessor& processor, int stringIndex);
    ~StabilityBadge() override;

    void resized() override;
    juce::String getText() const { return label.getText(); }

    static constexpr int preferredWidth = 84;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    int stringIndex;
    juce::Label label;
    juce::TextButton retune { "Retune" };
};

//==============================================================================
/** tuning-stability.md 6: the Workshop inspector's derived figures for a
    tuners or nut part ("Backlash on low E: 7.1 c"). Empty for other slots. */
juce::StringArray describeTuningFigures (LuthierAudioProcessor& processor, GuitarSlot slot, const Part* part);

} // namespace luthier
