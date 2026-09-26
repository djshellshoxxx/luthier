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
/*  The standard right-click menu for a parameter-backed control.

    Split into build / apply / show rather than written as one function, because
    a menu that can only be shown can only be tested by a human looking at it.
    `buildParameterContextMenu` returns the menu without putting it on screen, so
    a test can walk it with juce::PopupMenu::MenuItemIterator and fail when an
    entry goes missing; `applyParameterMenuResult` is what the menu's callback
    does with a result id, so a test can drive the outcome without a modal loop.
    `showParameterContextMenu` is the two of them either side of showMenuAsync,
    and is what every control actually calls.

    Section 5 of modulation-matrix.md is the reason this matters: right-click is
    the quick route to a modulation route, and it is a secondary path - the MOD
    tab cards are primary, per ground rule 4 - so nothing else would have noticed
    it break.
*/

/** The base id for the Modulate submenu. A result id in
    [kModulateMenuBase, kModulateMenuBase + ModSourceSlots::count) selects a
    modulation source for the parameter under the cursor. */
constexpr int kModulateMenuBase = 1000;

/** Builds the menu for a parameter without showing it. Empty if there is no such
    parameter. */
juce::PopupMenu buildParameterContextMenu (LuthierAudioProcessor& processor,
                                           const juce::String& parameterId);

/** Performs what a menu result id means. `owner` positions the value-entry
    callout; everything else ignores it. */
void applyParameterMenuResult (int result,
                               juce::Component& owner,
                               LuthierAudioProcessor& processor,
                               const juce::String& parameterId,
                               std::function<void()> onChanged = {});

/** Result ids for advanced-ranges.md's two right-click items (gui-integration
    16 items 9 and 10). */
constexpr int kUnlockRangeMenuId = 10;
constexpr int kRestrictRangeMenuId = 11;

/*  advanced-ranges.md 6.3: a drag on a physical control that has reached the
    edge of its stock range while that range is locked. Shows the fixed inline
    notice at `owner`; the control itself simply stops at the edge. Returns
    true if it showed. */
bool showLockedRangeNoticeIfAtEdge (juce::Component& owner,
                                    LuthierAudioProcessor& processor,
                                    const juce::String& parameterId,
                                    const juce::Slider& slider);

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
class ModArcHub;   // SPEC-SWEEP UW-35 (Widgets.cpp)

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

    /** Re-attaches, because the parameter's live range may have been swapped
        since the attachment was made (advanced-ranges.md 1.2). See RangesUi. */
    void resyncRange();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static int preferredWidthFor (Size s) noexcept;
    static int preferredHeightFor (Size s) noexcept;

    /** SPEC-SWEEP (UW-35 / GD-17): called by one shared 30 Hz timer for every
        attached knob. Repaints when this knob's modulation arc has moved by more
        than half a pixel (or appeared / gone), so an LFO-driven arc is live
        without the user touching anything. Returns true if it repainted. */
    bool pollModulationArc();
    int getArcRepaintCount() const noexcept { return arcRepaints; }

    /** Tests: poll even when the knob is not on screen. */
    void setPollArcWhileHidden (bool b) noexcept { pollWhileHidden = b; }

    /** The shared hub's refresh rate. */
    static constexpr int kModArcRefreshHz = 30;

private:
    void updateMidiLearnIndicator();

    std::unique_ptr<juce::SharedResourcePointer<ModArcHub>> arcHub;   // UW-35
    bool lastArcModulated = false;
    float lastArcNorm = 0.0f;
    int arcRepaints = 0;
    bool pollWhileHidden = false;
    int modIndex = -1;

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

        /** One locked-range notice per drag, not one per mouse move. */
        bool noticeShownThisDrag = false;
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

    /** SPEC-SWEEP (UW-14, ui-wiring 2): momentary - the parameter is on while
        the button is held and off when it is let go, each inside a gesture.
        Call after attachTo. */
    void setMomentary (bool shouldBeMomentary);
    bool isMomentary() const noexcept { return momentary; }

    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::TextButton button;
    juce::String paramId;
    bool momentary = false, momentaryHeld = false;

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

    /** As LuthierKnob::resyncRange. */
    void resyncRange();

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

    /** SPEC-SWEEP (GD-8, gui-engine-dataflow 3): 60 Hz; red above 0 dBFS and
        held 400 ms before it re-evaluates; unlit after 100 ms with no new
        block from the master bus. */
    static constexpr int kRefreshHz = 60;
    static constexpr double kRedHoldMs = 400.0;
    static constexpr double kStaleMs = 100.0;
    static juce::Colour darkColour() noexcept { return juce::Colour (0xff5a5f66); }
    static juce::Colour redColour() noexcept  { return juce::Colour (0xfff2544e); }

    /** One refresh at @p nowMs (the timer passes the real clock; tests their own). */
    void tick (double nowMs);
    bool isRed() const noexcept { return overThreshold; }
    bool isLit() const noexcept { return brightness > 0.0f || overThreshold; }
    float getBrightness() const noexcept { return brightness; }

private:
    void timerCallback() override;

    LuthierAudioProcessor* processor = nullptr;
    float brightness = 0.0f;
    bool overThreshold = false;
    double redSinceMs = -1.0e9;
    double lastFreshMs = -1.0e9;
    juce::uint32 lastBlockCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputLed)
};

//==============================================================================
/** ambiguity-resolutions.md 1.3: the "Feedback" indicator. Dark with no loop,
    glowing amber as the loop feeds the strings, and lit solid when it has
    entered a resonant state and is sustaining a note by itself. */
class FeedbackLed : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit FeedbackLed (LuthierAudioProcessor& processor);
    ~FeedbackLed() override;

    void paint (juce::Graphics&) override;

    float getShownActivity() const noexcept { return activity; }
    bool isShowingResonance() const noexcept { return resonant; }

    /** What the timer does, for the tests. */
    void refresh();

    /** gui-engine-dataflow.md 22: the LED drains at 30 Hz (MODEL-GAPS, TODO 2k). */
    static constexpr int kRefreshHz = 30;
    int getRefreshIntervalMs() const noexcept { return getTimerInterval(); }

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    float activity = 0.0f;
    bool resonant = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackLed)
};

//==============================================================================
/** ambiguity-resolutions.md 2.2's `ebow_string_mask`: HELD (the default: any
    string with a note down), or particular strings, one button each for the
    strings the current guitar has. Writes the integer parameter directly. */
class StringMaskSelector : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    StringMaskSelector (LuthierAudioProcessor& processor, const juce::String& parameterId);
    ~StringMaskSelector() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getMask() const noexcept { return mask; }

    /** What a click on a cell does: -1 is HELD, otherwise a string index. */
    void toggle (int cell);

private:
    void timerCallback() override;
    juce::Rectangle<int> cellBounds (int cell) const;

    LuthierAudioProcessor& processor;
    juce::RangedAudioParameter* parameter = nullptr;
    int mask = 0;
    int numStrings = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringMaskSelector)
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


//==============================================================================
/*  A transient message strip, for telling the user why the window just did
    something they did not ask for.

    gui-integration.md 4.5 needs one: below 1000 points Advanced Mode is
    unavailable, so the window forces Easy, and a mode that changes itself
    without saying why is the silent degradation ground rule 0.2 forbids.

    It is a plain component rather than an overlay on purpose. An overlay steals
    focus and has to be dismissed, which is far too much ceremony for "your
    window is too narrow"; this sits in the layout, says its piece, and takes
    itself away after a few seconds. Clicking it dismisses it early.

    The auto-hide calls onVisibilityChanged, so the host can re-lay-out and give
    the space back. Nothing here calls that from inside show(): the caller is
    usually in the middle of its own resized(), and re-entering it would be a
    loop.
*/
class InlineNotice : public juce::Component,
                     private juce::Timer
{
public:
    InlineNotice();
    ~InlineNotice() override;

    enum class Level { info, warning };

    /** Shows the message. Does not call onVisibilityChanged - the caller decides
        when to re-lay-out, because it may already be doing so. */
    void show (const juce::String& message, Level level = Level::info,
               int millisecondsToLive = defaultLifetimeMs);

    /** Hides it and calls onVisibilityChanged. Safe to call when already hidden,
        in which case it does nothing at all. */
    void dismiss();

    const juce::String& getMessage() const noexcept { return message; }

    /** Called when the notice hides itself, so the host can reclaim the space. */
    std::function<void()> onVisibilityChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 26;
    static constexpr int defaultLifetimeMs = 6000;

private:
    void timerCallback() override;

    juce::String message;
    Level level = Level::info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InlineNotice)
};

} // namespace luthier
