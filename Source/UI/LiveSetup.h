#pragma once

/*  SPEC-SWEEP additions to the live surface (live-performance.md 2, 3, 5, 6, 7,
    9; gui-integration.md 4.4 and 19).

      - LiveActionButton: assigns a footswitch / CC to each live action (LP-11,
        LP-21, LP-26, LP-29, LP-37). A popup menu, never a modal dialog, so it
        can sit on the Live strip (live-performance 0.5).
      - MorphSetupPanel: the automatable morph position (LP-16), the per-
        parameter morph exclusions (LP-18) and the Bezier control points (LP-19)
        on the LIVE tab.
      - MonitorSetupPanel: the monitor mix's level, pan and three-band EQ
        (LP-31, GI-119) on the LIVE tab.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Live/LiveInput.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** Opens the live-action CC menu. Choosing an action arms learning for it (the
    next CC that arrives is assigned); choosing it again while armed cancels. */
class LiveActionButton : public juce::TextButton,
                         private juce::Timer
{
public:
    LiveActionButton (LuthierAudioProcessor& processor, const juce::String& idleText);
    ~LiveActionButton() override;

    /** The menu the button shows, exposed for tests. Item ids: 1 + action to
        learn, 100 + action to forget, 200 to cancel learning. */
    juce::PopupMenu buildMenu() const;
    void applyMenuResult (int result);

private:
    void timerCallback() override;
    void refreshText();

    LuthierAudioProcessor& processor;
    juce::String idleText;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveActionButton)
};

//==============================================================================
class MorphSetupPanel : public juce::Component,
                        private juce::Timer
{
public:
    explicit MorphSetupPanel (LuthierAudioProcessor& processor);
    ~MorphSetupPanel() override;

    static constexpr int kRowHeight = 26;

    /** Rows it wants, which depends on whether the curve is a Bezier. */
    int getPreferredHeight() const;

    /** The exclusions menu, exposed for tests: one sub-menu per parameter
        group, discrete parameters first; item id = 1 + parameter index. */
    juce::PopupMenu buildExclusionMenu() const;
    void applyExclusionMenuResult (int result);

    juce::Slider& getBezierSlider (int index) { return bezier[(size_t) juce::jlimit (0, 3, index)]; }
    LuthierSlider& getPositionControl() noexcept { return position; }

    void resized() override;

    std::function<void()> onLayoutChanged;

private:
    void timerCallback() override;
    void refresh();

    LuthierAudioProcessor& processor;

    LuthierSlider position { "Morph position" };
    juce::TextButton exclusionsButton { "Exclusions..." };
    juce::Label exclusionsSummary;

    juce::Label bezierLabel;
    std::array<juce::Slider, 4> bezier;

    LiveActionButton ccButton;

    bool lastBezierShown = false;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphSetupPanel)
};

//==============================================================================
class MonitorSetupPanel : public juce::Component,
                          private juce::Timer
{
public:
    explicit MonitorSetupPanel (LuthierAudioProcessor& processor);
    ~MonitorSetupPanel() override;

    static constexpr int kPreferredHeight = 58;

    enum Control { level = 0, pan, low, mid, high, numControls };
    juce::Slider& getControl (Control c) { return sliders[(size_t) c]; }

    void resized() override;

private:
    void timerCallback() override;
    void refresh();

    LuthierAudioProcessor& processor;

    std::array<juce::Slider, numControls> sliders;
    std::array<juce::Label, numControls> labels;

    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorSetupPanel)
};

} // namespace luthier
