#pragma once

/*  Performance Assist's UI (auto-articulation.md 7) - FEAT-ASSIST workstream.

      - AssistPill: Easy mode's AUTO pill in the playing strip's mode column
        (7.1). Tap toggles; hold, or Down, opens the Amount popover. Its dot
        flashes for 120 ms on every decision.
      - AssistStyleBox: the style combo beside it (and in the PLAYING group);
        styles locked in Free carry "(Pro)".
      - PerformanceAssistGroup: Advanced mode's PLAYING group, first in the
        RHYTHM tab (7.2): the mode mirror, the switch, style, Amount, the nine
        rule switches, the decision list and the bypass notice. Collapsible.
      - AssistLabelOverlay: the "show what it did" labels on a fretboard (7.3),
        a child that lays itself over its host and never takes the mouse.

    Everything that writes a parameter does it through the attachment pattern
    (ui-wiring 2) or, for the switch and the rule bits, one ScopedUndoAction
    with action-and-undo's wording ("Turn on Performance Assist", "Turn off
    Slides rule"). Nothing here touches the audio thread's state except by
    parameters; the decisions come in through AssistDecisionLog.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnimationPolicy.h"   // cpu-quality-modes 6 (INTEGRATE-2)

#include "Theme.h"
#include "Widgets.h"
#include "../Model/Playing/AssistDecisionLog.h"
#include "../Model/Playing/AutoArticulationStyles.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
namespace AssistUi
{
    bool isEnabled (LuthierAudioProcessor&);

    /** One undo entry: "Turn on Performance Assist" / "Turn off ...". */
    void setEnabled (LuthierAudioProcessor&, bool on);
    void toggle (LuthierAudioProcessor&);

    int getRules (LuthierAudioProcessor&);

    /** One undo entry: "Turn off Slides rule". */
    void setRule (LuthierAudioProcessor&, int bitIndex, bool on);

    int getStyle (LuthierAudioProcessor&);
    int getAmountPercent (LuthierAudioProcessor&);

    /** "ON · Rock · 60%" (7.2's status line). */
    juce::String statusText (LuthierAudioProcessor&);

    /** "Performance Assist, on, style Rock, amount 60 percent" (8). */
    juce::String accessibleSummary (LuthierAudioProcessor&);

    /** Section 5's bypass reason, or empty. */
    juce::String noticeText (LuthierAudioProcessor&);

    /** 7.4: Options -> Visual aids -> Show Performance Assist labels (default on). */
    bool showLabels();
    void setShowLabels (bool on);

    /** Drains the engine's feed into the processor's log (once per UI frame). */
    void drain (LuthierAudioProcessor&, bool force = false);

    /** 7.5: the empty list's text. */
    constexpr const char* kEmptyListText = "Play something: what Performance Assist decides appears here.";

    /** 7.5 / editions.md 4.2: choosing a locked style in Free says what plays. */
    void showLockedStyleNotice (int styleIndex, juce::Component* near);
}

//==============================================================================
/** 7.3: the labels over a fretboard. The host gives the position of a string at
    a fret in its own coordinates; the overlay tracks the host's bounds. */
class AssistLabelOverlay : public juce::Component,
                           private juce::Timer,
                           private juce::ComponentListener
{
public:
    using PositionFn = std::function<juce::Point<float> (int stringIndex, double fret)>;

    AssistLabelOverlay (LuthierAudioProcessor& processor, juce::Component& host, PositionFn position);
    ~AssistLabelOverlay() override;

    struct Drawn
    {
        juce::String glyph;
        juce::Point<float> at;
        float opacity = 0.0f;
        int stringIndex = -1;
        double fret = 0.0;
        bool strum = false;
        juce::Line<float> arrow;
    };

    /** What paint() draws at `nowMs`: nothing with the option off (the log
        still fills). Under reduced motion every opacity is 0.9 or absent. */
    std::vector<Drawn> computeLabels (double nowMs) const;
    juce::Rectangle<int> lastLabelArea;   // cleared on the next tick

    void paint (juce::Graphics&) override;

    /** Tests: reduced motion without touching the user's setting. */
    void setReducedMotionForTests (int state) { reducedMotionOverride = state; }

private:
    void timerCallback() override;
    void componentMovedOrResized (juce::Component&, bool, bool) override;
    bool reducedMotion() const;

    LuthierAudioProcessor& processor;
    juce::Component& host;
    PositionFn positionOf;
    bool drewLast = false;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "AssistLabelOverlay" };   // cpu-quality-modes 6
    int reducedMotionOverride = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AssistLabelOverlay)
};

//==============================================================================
/** The style combo (7.1: 70 px in Easy). */
class AssistStyleBox : public LuthierChoice
{
public:
    explicit AssistStyleBox (LuthierAudioProcessor& processor);

private:
    LuthierAudioProcessor& processor;
};

//==============================================================================
/** 7.1's AUTO pill, 56 x 22. */
class AssistPill : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    explicit AssistPill (LuthierAudioProcessor& processor);
    ~AssistPill() override;

    static constexpr int kWidth = 56, kHeight = 22;
    static constexpr double kHoldMs = 450.0, kFlashMs = 120.0;

    /** The popover's "More in RHYTHM tab". */
    std::function<void()> onOpenRhythmTab;

    void showPopover();
    bool isPopoverOpen() const noexcept { return popoverBox != nullptr && popoverBox->isVisible(); }
    void closePopover();

    /** True while the state dot is lit by a decision (7.1). */
    bool isDotFlashing() const;

    /** What the timer does, now (tests). */
    void refresh() { timerCallback(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Tests: the pill's clock. */
    void setClockForTests (std::function<double()> clock) { testClock = std::move (clock); }

private:
    void timerCallback() override;
    double nowMs() const { return testClock ? testClock() : juce::Time::getMillisecondCounterHiRes(); }

    LuthierAudioProcessor& processor;
    // The popover is a child of the editor, not a desktop window: plugin hosts
    // handle a second top-level window badly, and it closes with the editor.
    std::unique_ptr<juce::Component> popoverContent;
    std::unique_ptr<juce::CallOutBox> popoverBox;
    double pressedAtMs = -1.0;
    bool holdFired = false;
    double flashUntilMs = -1.0;
    double lastSeenDecisionMs = -1.0e9;
    std::function<double()> testClock;
    int lastShown = -1;   // what the last repaint drew
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "AssistPill" };   // cpu-quality-modes 6

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AssistPill)
};

//==============================================================================
/** 7.2: the PLAYING group, first in the RHYTHM tab. */
class PerformanceAssistGroup : public juce::Component,
                               private juce::Timer,
                               private juce::ListBoxModel
{
public:
    explicit PerformanceAssistGroup (LuthierAudioProcessor& processor);
    ~PerformanceAssistGroup() override;

    int preferredHeight() const;

    /** Collapsing or expanding changed preferredHeight(). */
    std::function<void()> onLayoutChanged;

    /** The ? on the group. */
    std::function<void()> onHelp;

    void setCollapsed (bool shouldCollapse);
    bool isCollapsed() const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** What the timer does, now (tests). */
    void refresh() { timerCallback(); }

    // ---- for the tests ------------------------------------------------------------
    juce::String getStatusText() const { return status.getText(); }
    juce::String getNoticeText() const { return notice.getText(); }
    juce::ToggleButton& getEnableSwitch() noexcept { return enable; }
    juce::ToggleButton& getRuleSwitch (int bit) noexcept { return *rules[bit]; }
    AssistStyleBox& getStyleBox() noexcept { return style; }
    LuthierKnob& getAmountKnob() noexcept { return amount; }
    LuthierChoice& getModeMirror() noexcept { return mode; }
    juce::ListBox& getDecisionList() noexcept { return list; }
    juce::String getRowText (int row) const;

    /** Tab order (8): mode, toggle, style, amount, rules, list. */
    std::vector<juce::Component*> getFocusOrder();

private:
    void timerCallback() override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    juce::String getNameForRow (int row) override;

    LuthierAudioProcessor& processor;

    juce::Label heading, status, notice, rulesLabel, recentLabel;
    juce::TextButton collapse { "v" }, help { "?" };
    LuthierChoice mode { "Mode" };
    juce::ToggleButton enable { "Performance Assist" };
    AssistStyleBox style;
    LuthierKnob amount { "Amount", LuthierKnob::Size::Small };
    std::array<std::unique_ptr<juce::ToggleButton>, AssistRule::numRules> rules;
    juce::ListBox list;

    juce::uint32 listedCount = 0;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "PerformanceAssistGroup" };   // cpu-quality-modes 6

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceAssistGroup)
};

} // namespace luthier
