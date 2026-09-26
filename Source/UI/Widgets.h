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

#include "AnimationPolicy.h"   // cpu-quality-modes 6
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

/** gui-integration 16 items 12-13: the read-only automation ID, and "Show in
    Options -> Shortcuts" for a control a shortcut also drives. */
constexpr int kAutomationIdMenuId = 12;
constexpr int kShowShortcutMenuId = 13;

/** ui-wiring.md 21: names an attached control for screen readers after its parameter. */
void labelForScreenReaders (juce::Component& control, LuthierAudioProcessor& processor,
                            const juce::String& parameterId, const juce::String& tooltip);

/** gui-integration 11.2: a MOD source card's drag description is this prefix and its slot. */
inline constexpr const char* kModSourceDragPrefix = "luthier.modsource:";

/** The source slot a drag carries, or -1 if it is not a mod source. */
int modSourceSlotFromDrag (const juce::var& description);

/*  gui-integration 11.2: a dropped source becomes a route at 25% depth - one
    undo entry. Returns false when the control's routes are full. */
bool addModulationFromDrop (LuthierAudioProcessor& processor, int sourceSlot, const juce::String& parameterId);

/** The shortcut action that drives a parameter (Slide Mode's S), or empty. */
juce::String shortcutActionForParameter (const juce::String& parameterId);

/** Set by the editor: opens Options -> Accessibility's shortcut table filtered to an action. */
extern std::function<void (const juce::String& actionId)> showShortcutInOptions;

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
    /*  global-search.md 3.2 (FEAT-SEARCH): the constructor and destructor add
        and remove this control from search::LiveControls, so the palette can
        find any parameter's control without a registration list. Defined in
        Search/LiveControls.cpp. */
    LearnTarget();
    virtual ~LearnTarget();
    LearnTarget (const LearnTarget&) = delete;
    LearnTarget& operator= (const LearnTarget&) = delete;

    /** The parameter this control edits, or empty if it is not attached yet. */
    virtual juce::String getLearnParameterId() const = 0;
};

//==============================================================================
/** A rotary control with its label below and its value above. */
class LuthierKnob : public juce::Component,
                    public juce::SettableTooltipClient,
                    public LearnTarget,
                    public juce::DragAndDropTarget   // gui-integration 11.2
{
public:
    enum class Size { Small, Normal, Large, Macro };

    // gui-integration 11.2: a MOD source dropped here routes to this knob at 25%.
    bool isInterestedInDragSource (const SourceDetails& d) override { return processor != nullptr && modSourceSlotFromDrag (d.description) >= 0; }
    void itemDragEnter (const SourceDetails&) override { dropHighlight = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHighlight = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHighlight = false;
        repaint();

        if (processor != nullptr)
            addModulationFromDrop (*processor, modSourceSlotFromDrag (d.description), paramId);
    }

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
    bool dropHighlight = false;   // a mod source is being dragged over (11.2)
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
                      public juce::SettableTooltipClient,
                      public juce::DragAndDropTarget   // gui-integration 11.2
{
public:
    bool isInterestedInDragSource (const SourceDetails& d) override { return processor != nullptr && modSourceSlotFromDrag (d.description) >= 0; }
    void itemDropped (const SourceDetails& d) override
    {
        if (processor != nullptr)
            addModulationFromDrop (*processor, modSourceSlotFromDrag (d.description), paramId);
    }

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

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "LevelMeter" };
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
    double clipLatchedAtMs = -1.0e12;   // cpu-quality-modes 6

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputLed)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "OutputLed" };
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

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "FeedbackLed" };
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
    void setNumLines (int lines) { numLines = juce::jlimit (1, 24, lines); }

    void paint (juce::Graphics&) override;

    /*  ui-wiring.md 11: the stream keeps the last 200 lines, stops scrolling
        500 ms after the last record, and does no work under reduced motion.
        gui-integration 5: Options -> Appearance can hide it. */
    static constexpr int kMaxLines = 200;
    static constexpr double kStopAfterMs = 500.0;
    static bool isEnabledByUser();
    static void setEnabledByUser (bool enabled);

    /** One tick of the timer, with the clock passed in (tests). */
    void update (double nowMs);
    bool isScrolling() const noexcept { return scrolling; }
    int getNumLinesKept() const noexcept { return lines.size(); }

private:
    void timerCallback() override;

    LuthierAudioProcessor* processor = nullptr;
    juce::StringArray lines;
    int numLines = 10;
    int lastRecordCount = 0;
    float scrollOffset = 0.0f;
    bool scrolling = false;
    double lastArrivalMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DataStreamDisplay)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::Decorative, "DataStreamDisplay" };
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
