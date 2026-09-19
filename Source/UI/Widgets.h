#pragma once

/*  The shared controls.

    Every control in the plugin is one of these, which is what makes the promises
    in the brief true everywhere rather than in most places:

      - left-drag adjusts, with Shift for coarse and Ctrl/Cmd for ultra-fine
      - double-click resets to the default
      - right-click opens: Enter value, Reset, Copy, Paste, MIDI Learn,
        Assign to macro, Lock, Randomise
      - hover shows the value in place of the label, and a tooltip after 400 ms
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Modulation/ModMatrix.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** Opens the standard right-click menu for a parameter-backed control. */
void showParameterContextMenu (juce::Component& owner,
                               LuthierAudioProcessor& processor,
                               const juce::String& parameterId,
                               std::function<void()> onChanged = {});

/** The clipboard shared by every control's Copy / Paste. */
class ControlClipboard
{
public:
    static void store (double normalisedValue) noexcept;
    static bool hasValue() noexcept;
    static double retrieve() noexcept;

private:
    static double value;
    static bool filled;
};

//==============================================================================
/*  Anything a MIDI-learn arm can land on.

    gui-integration.md section 19 puts MIDI Learn on a header button as well as on
    the right-click menu, and ground rule 4 forbids a feature being reachable only
    by right-click. Arming has to work without knowing which control comes next,
    so the arm overlay finds its target by walking up from whatever was clicked
    until it finds one of these. */
struct LearnTarget
{
    virtual ~LearnTarget() = default;

    /** The parameter this control edits, or empty if it is not attached yet. */
    virtual juce::String getLearnParameterId() const = 0;
};

//==============================================================================
/** A rotary control with its label below and its value above. */
class LuthierKnob : public juce::Component,
                    public juce::SettableTooltipClient,
                    public LearnTarget
{
public:
    enum class Size { Small, Normal, Large, Macro };

    LuthierKnob (const juce::String& labelText, Size size = Size::Normal);
    ~LuthierKnob() override;

    /** Binds the knob to a parameter. Sets up the attachment, the tooltip, the
        right-click menu and the MIDI-learn indicator. */
    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId,
                   const juce::String& tooltip = {});

    juce::Slider& getSlider() noexcept { return slider; }
    const juce::String& getParameterId() const noexcept { return paramId; }
    juce::String getLearnParameterId() const override { return paramId; }

    void setLabelText (const juce::String& text);
    void setAccentColour (juce::Colour colour);

    /** Shows a small dice under the knob, for the macro row. */
    void setShowDiceAndLock (bool shouldShow);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static int preferredWidthFor (Size s) noexcept;
    static int preferredHeightFor (Size s) noexcept;

private:
    void updateMidiLearnIndicator();

    class KnobSlider : public juce::Slider
    {
    public:
        explicit KnobSlider (LuthierKnob& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        LuthierKnob& owner;
    };

    KnobSlider slider { *this };
    juce::String labelText, paramId;
    Size size;
    juce::Colour accent = Palette::accent;

    LuthierAudioProcessor* processor = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    bool showDiceAndLock = false;
    bool hovering = false;
    int mappedCc = -1;

    juce::Rectangle<int> diceBounds, lockBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierKnob)
};

//==============================================================================
/** A labelled combo box bound to a choice parameter. */
class LuthierChoice : public LearnTarget,
                      public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit LuthierChoice (const juce::String& labelText = {});
    ~LuthierChoice() override;

    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId,
                   const juce::String& tooltip = {});

    juce::ComboBox& getComboBox() noexcept { return box; }
    juce::String getLearnParameterId() const override { return paramId; }
    void setLabelText (const juce::String& text);

    /** Hides the label and gives the whole height to the box. */
    void setLabelVisible (bool visible);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int labelHeight = 14;

private:
    juce::ComboBox box;
    juce::String labelText, paramId;
    bool labelVisible = true;

    LuthierAudioProcessor* processor = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierChoice)
};

//==============================================================================
/** A toggle bound to a bool parameter, drawn as a flat button. */
class LuthierToggle : public LearnTarget,
                      public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit LuthierToggle (const juce::String& text);
    ~LuthierToggle() override;

    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId,
                   const juce::String& tooltip = {});

    juce::TextButton& getButton() noexcept { return button; }
    juce::String getLearnParameterId() const override { return paramId; }

    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::TextButton button;
    juce::String paramId;

    LuthierAudioProcessor* processor = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierToggle)
};

//==============================================================================
/** Horizontal or vertical slider bound to a float parameter, with a label. */
class LuthierSlider : public LearnTarget,
                      public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    LuthierSlider (const juce::String& labelText, bool vertical = false);
    ~LuthierSlider() override;

    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId,
                   const juce::String& tooltip = {});

    juce::Slider& getSlider() noexcept { return slider; }
    juce::String getLearnParameterId() const override { return paramId; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Slider slider;
    juce::String labelText, paramId;
    bool vertical;

    LuthierAudioProcessor* processor = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierSlider)
};

//==============================================================================
/** Stereo output meter with peak hold and a numeric readout. */
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter();
    ~LevelMeter() override;

    void setSource (LuthierAudioProcessor* processor);
    void setHorizontal (bool h) { horizontal = h; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor* processor = nullptr;
    bool horizontal = false;

    float levelL = 0.0f, levelR = 0.0f;
    float peakHoldL = 0.0f, peakHoldR = 0.0f;
    int holdCountL = 0, holdCountR = 0;
    float displayPeakDb = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

//==============================================================================
/** The output LED in the top-left: grey when silent, brightening to white as the
    level approaches 0 dBFS, and red above it until the signal drops back. */
class OutputLed : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    OutputLed();
    ~OutputLed() override;

    void setSource (LuthierAudioProcessor* processor);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor* processor = nullptr;
    float brightness = 0.0f;
    bool overThreshold = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputLed)
};

//==============================================================================
/** The scrolling readout of the plugin's internals: a real data stream, faded at
    the top and bottom, that stops when nothing is happening. */
class DataStreamDisplay : public juce::Component,
                          private juce::Timer
{
public:
    DataStreamDisplay();
    ~DataStreamDisplay() override;

    void setSource (LuthierAudioProcessor* processor);
    void setNumLines (int lines) { numLines = juce::jlimit (4, 24, lines); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor* processor = nullptr;
    juce::StringArray lines;
    int numLines = 10;
    int lastRecordCount = 0;
    float scrollOffset = 0.0f;
    bool scrolling = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DataStreamDisplay)
};

//==============================================================================
/** A titled panel that draws the section header and an optional separator. */
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel (const juce::String& title, bool raised = false);

    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setAccentColour (juce::Colour c) { accent = c; repaint(); }

    /** The area inside the header and padding, where children go. */
    juce::Rectangle<int> getContentBounds() const;

    void paint (juce::Graphics&) override;

    static constexpr int headerHeight = 20;

private:
    juce::String title;
    bool raised;
    juce::Colour accent = Palette::accent;
};

} // namespace luthier
