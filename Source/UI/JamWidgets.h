#pragma once

/*  Jam mode's small controls (jam-mode.md 8.1 and 8.2), shared by the JAM tab,
    the Easy rhythm strip and the Live Strip.

    - JamDots: the (1)(2)(3)(4)(5) intensity row. A juce::Slider underneath, so
      it has the slider's value interface for screen readers and binds with a
      SliderAttachment like any control.
    - JamPill: the band's state as a button. Easy: the first press arms, then
      a press starts or stops. Live: a tap starts or stops, a long press is
      FILL.
    - JamStripGroup: the Easy rhythm strip's JAM group - pill, style,
      intensity dots and the Band volume mini-knob.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Widgets.h"
#include "AnimationPolicy.h"
#include "../Jam/JamStatus.h"
#include "AnimationPolicy.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class JamDots : public juce::Slider
{
public:
    JamDots();

    void attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId, const juce::String& label);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    /** The value a click at x picks. */
    int valueAtX (float x) const noexcept;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamDots)
};

//==============================================================================
class JamPill : public juce::Button,
                private juce::Timer
{
public:
    enum class Mode { easy, live };

    JamPill (LuthierAudioProcessor& processor, Mode mode);
    ~JamPill() override;

    /** What a press does now (tests and the keyboard use it too). */
    void press (bool longPress = false);

    /** The text it shows: JAM, ARMED, COUNT, PLAYING or ENDING. */
    juce::String getPillText() const { return pillText; }

    /** Re-reads the band's state; the timer does this at 30 Hz. */
    void refresh();

    static constexpr int kLongPressMs = 500;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    Mode mode;

    // cpu-quality-modes 6: the band's state (ARMED, COUNT, PLAYING...) is a live readout.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "JamPill" };
    juce::String pillText { "JAM" };
    JamState state = JamState::off;
    double downAt = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamPill)
};

//==============================================================================
/** 8.2's shortcuts - J start/stop, Shift+J fill, Alt+J arm - through the
    rebindable registry. Nothing while a text field has focus (JM-49). */
namespace JamShortcuts
{
    /** `focused` is the component with keyboard focus (the current one by default). */
    bool handle (LuthierAudioProcessor& processor, const juce::KeyPress& key,
                 juce::Component* focused = juce::Component::getCurrentlyFocusedComponent());
}

//==============================================================================
class JamStripGroup : public juce::Component
{
public:
    explicit JamStripGroup (LuthierAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics&) override;

    static constexpr int preferredWidth = 88 + 4 + 118 + 4 + 76 + 4 + 40;

    /** The style box at its narrowest: jam-mode 8.2 never hides the band's
        style, level or state, so the strip gives this much at least. */
    static constexpr int minimumWidth = 88 + 4 + 56 + 4 + 76 + 4 + 40;

    JamPill& getPill() noexcept               { return pill; }
    juce::ComboBox& getStyleBox() noexcept    { return style.getComboBox(); }
    JamDots& getIntensity() noexcept          { return intensity; }
    juce::Slider& getBandVolume() noexcept    { return bandVolume; }

private:
    LuthierAudioProcessor& processor;
    JamPill pill;
    LuthierChoice style;
    JamDots intensity;
    juce::Slider bandVolume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamStripGroup)
};

} // namespace luthier
