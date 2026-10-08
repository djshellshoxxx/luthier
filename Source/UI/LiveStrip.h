#pragma once

/*  The Live strip (live-performance.md section 10).

    A collapsible bar that attaches under the header when Live Mode is on:
    snapshots, the setlist triptych, tap tempo, the morph knob and its two slots,
    the kill switch and the monitor level.

    Two rules from the spec shape the whole component.

    The first is that nothing here may open a modal dialog (live-performance 0.5).
    Renaming a snapshot, picking a morph slot and choosing a setlist all happen in
    a popup menu or in place, and every confirmation is dismissed by the control
    that raised it.

    The second is that in Live Mode every hit target grows to 44 px
    (live-performance 10). A guitarist operating this with a foot, or with one
    hand while holding a neck, cannot be asked to hit a 20-px button.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "LiveSetup.h"   // SPEC-SWEEP: LP-11
#include "JamWidgets.h"   // FEAT-JAM

namespace luthier
{

class LuthierAudioProcessor;

/** SPEC-SWEEP: GI-4 - one of the sixteen snapshot colour tags, for the LIVE
    tab's colour button as well as the strip. */
juce::Colour getSnapshotTagColour (int tag);

//==============================================================================
/** The bank of eight snapshot buttons plus its prev/next pair
    (live-performance 2). */
class SnapshotStrip : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    static constexpr int kButtonsShown = 8;

    explicit SnapshotStrip (LuthierAudioProcessor& processor);
    ~SnapshotStrip() override;

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    /** SPEC-SWEEP: A11Y-9 - one accessible, focusable button per shown slot,
        titled with its number and label; pressing it recalls (or captures an
        empty slot), as a click does. */
    juce::Button* getSlotAccessor (int slot) const;

    /** SPEC-SWEEP: GI-73 - the text a pad shows: its number and its label cut
        to twelve characters. */
    static juce::String getPadText (int index, const juce::String& label);
    static constexpr int kPadLabelChars = 12;

    /** SPEC-SWEEP: GI-86 - gui-integration 14's empty-slot hint. */
    static constexpr const char* kEmptySlotHint = "Shift-click to save current state here.";
    bool isShowingEmptyHint() const noexcept { return showEmptyHint; }

private:
    class SlotAccessor;
    juce::OwnedArray<SlotAccessor> slotAccessors;
    void updateSlotAccessors();

    juce::Rectangle<int> buttonBounds (int slot) const;
    int slotAt (juce::Point<int> position) const;

    /** The first snapshot of the bank of eight the strip is showing. */
    int bankStart() const;

    void showSlotMenu (int index);

    LuthierAudioProcessor& processor;

    juce::TextButton prevButton { "<" }, nextButton { ">" };

    int lastCurrent = -1;
    int lastCount = -1;

    bool showEmptyHint = false;   // SPEC-SWEEP: GI-86

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapshotStrip)
};

//==============================================================================
/** Previous, current and next setlist entries (live-performance 4). */
class SetlistTriptych : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    explicit SetlistTriptych (LuthierAudioProcessor& processor);
    ~SetlistTriptych() override;

    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showSetlistMenu();

    LuthierAudioProcessor& processor;

    juce::String previousText, currentText, nextText;

    // SPEC-SWEEP: LP-5 - no FileChooser here: the menu lists the setlists in
    // the user's folder and nothing on the live surface opens a dialog.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetlistTriptych)
};

//==============================================================================
/** A pad that takes taps and shows the tempo it worked out
    (live-performance 5). */
class TapPad : public juce::Component,
               public juce::SettableTooltipClient,
               private juce::Timer
{
public:
    explicit TapPad (LuthierAudioProcessor& processor);
    ~TapPad() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    /** Lit on each beat, in the accent colour, as the spec asks. */
    bool beatLit = false;
    double lastBeatMs = 0.0;
    double displayedBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapPad)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::Transition, "TapPad", {}, [this] { timerCallback(); } };
};

//==============================================================================
class LiveStrip : public juce::Component,
                  private juce::Timer
{
public:
    /** live-performance 10: every hit target grows to this in Live Mode. */
    static constexpr int kTouchTargetHeight = 44;

    static constexpr int preferredHeight = kTouchTargetHeight + 2 * Metrics::gridHalf;

    explicit LiveStrip (LuthierAudioProcessor& processor);
    ~LiveStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** For tests: the JAM pill (FEAT-JAM). */
    JamPill& getJamPill() noexcept { return *jamPill; }

    /** Shows the JAM pill only while jam_enabled is on; the timer calls it. */
    void refreshJamPill();

private:
    void timerCallback() override;

    void refreshMorphControls();
    void showSlotMenu (bool slotB);

    LuthierAudioProcessor& processor;

    std::unique_ptr<SnapshotStrip> snapshotStrip;
    std::unique_ptr<SetlistTriptych> triptych;
    std::unique_ptr<TapPad> tapPad;
    std::unique_ptr<LiveActionButton> ccButton;   // SPEC-SWEEP: LP-11
    std::unique_ptr<JamPill> jamPill;   // FEAT-JAM: jam-mode 8.2, after Tap while jam_enabled is on

    // --- morph --------------------------------------------------------------------
    juce::TextButton morphEnable { "MORPH" };
    juce::TextButton slotAButton { "A" }, slotBButton { "B" };
    juce::Slider morphSlider { juce::Slider::RotaryHorizontalVerticalDrag,
                               juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;   // SPEC-SWEEP: LP-16

    // --- kill and monitor -----------------------------------------------------------
    juce::TextButton killButton { "KILL" };
    juce::Slider monitorLevel { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label monitorLabel;

    bool lastKillActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveStrip)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "LiveStrip" };
};

} // namespace luthier
