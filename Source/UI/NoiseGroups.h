#pragma once

/*  The CHARACTER tab's STRING NOISE and PICK groups (gui-integration.md 4.4,
    string-squeak.md 9, pick-noise.md 8).

    STRING NOISE holds exactly what string-squeak.md 9 lists: squeak amount,
    probability, finger moisture and pressure, the winding material, the style
    presets and the noise-event strip. PICK holds the pick's fields and the
    three noise amounts. The strum striker dropdown pick-noise.md 8 also names
    arrives with strum-dynamics.md.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Noise/NoiseEngine.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/*  string-squeak.md 9.1: the last 8 seconds of noise events as ticks,
    coloured by class and sized by level. It answers "what is that noise?",
    which is what makes the noise controls tunable at all. Drains the engine's
    event ring at 30 Hz and greys out after 2 s without an event
    (gui-engine-dataflow.md). */
class NoiseEventStrip : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit NoiseEventStrip (LuthierAudioProcessor& processor);
    ~NoiseEventStrip() override;

    void paint (juce::Graphics&) override;

    static constexpr int preferredHeight = 24;
    static constexpr double kWindowSeconds = 8.0;
    static constexpr double kStaleSeconds = 2.0;

    /** For tests: drain now and report how many events are on screen. */
    int pollNow();
    int getNumShown() const noexcept { return shown.size(); }
    bool isStale() const noexcept;

    static juce::Colour colourFor (NoiseClass c);

private:
    void timerCallback() override;

    struct Tick { double time; NoiseClass noiseClass; float level; };

    LuthierAudioProcessor& processor;
    juce::Array<Tick> shown;
    double lastEventTime = -1.0e9;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseEventStrip)
};

//==============================================================================
class NoiseGroups : public juce::Component,
                    private juce::Timer
{
public:
    explicit NoiseGroups (LuthierAudioProcessor& processor);
    ~NoiseGroups() override;

    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    /*  string-squeak.md 8: the style presets. Applying one writes its four
        values as one undoable action; the box then reads "<name> (modified)"
        as soon as any of them moves. */
    struct SqueakStyle { const char* name; double amount, probability, moisture, pressure; };
    static const SqueakStyle& getSqueakStyle (int index);
    static constexpr int kNumSqueakStyles = 5;

    void applySqueakStyle (int index);

    /** What the style box should say for the current values. */
    juce::String describeSqueakStyle() const;

    NoiseEventStrip& getEventStrip() noexcept { return eventStrip; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    // STRING NOISE
    juce::Label stringNoiseHeading, pickHeading;
    juce::ComboBox styleBox;
    LuthierSlider squeakAmount { "Squeak" }, squeakProbability { "Probability" },
                  squeakMoisture { "Moisture" }, squeakPressure { "Pressure" },
                  squeakMinTravel { "Min travel" };
    LuthierChoice windingMaterial { "Winding" };
    NoiseEventStrip eventStrip;

    // PICK
    LuthierChoice pickMaterial { "Material" };
    LuthierSlider pickThickness { "Thickness" }, pickTip { "Tip radius" }, pickBevel { "Bevel" },
                  pickWear { "Wear" }, pickAngle { "Angle" }, pickClick { "Click" },
                  pickChirp { "Chirp" }, pickScrape { "Scrape" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseGroups)
};

} // namespace luthier
