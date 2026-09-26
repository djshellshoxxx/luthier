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

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "JamWidgets.h"   // FEAT-JAM

namespace luthier
{

class LuthierAudioProcessor;

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

private:
    juce::Rectangle<int> buttonBounds (int slot) const;
    int slotAt (juce::Point<int> position) const;

    /** The first snapshot of the bank of eight the strip is showing. */
    int bankStart() const;

    void showSlotMenu (int index);

    LuthierAudioProcessor& processor;

    juce::TextButton prevButton { "<" }, nextButton { ">" };

    int lastCurrent = -1;
    int lastCount = -1;

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

    std::unique_ptr<juce::FileChooser> chooser;

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
    std::unique_ptr<JamPill> jamPill;   // FEAT-JAM: jam-mode 8.2, after Tap while jam_enabled is on

    // --- morph --------------------------------------------------------------------
    juce::TextButton morphEnable { "MORPH" };
    juce::TextButton slotAButton { "A" }, slotBButton { "B" };
    juce::Slider morphSlider { juce::Slider::RotaryHorizontalVerticalDrag,
                               juce::Slider::NoTextBox };

    // --- kill and monitor -----------------------------------------------------------
    juce::TextButton killButton { "KILL" };
    juce::Slider monitorLevel { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label monitorLabel;

    bool lastKillActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveStrip)
};

} // namespace luthier
