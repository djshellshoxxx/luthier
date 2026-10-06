#pragma once

/*  The TECHNIQUES tab's sub-tab pages (gui-techniques-updates.md 1).

    Every control is the standard attached widget (ui-wiring.md 2): a
    LuthierKnob, LuthierChoice or LuthierToggle bound to its parameter, with a
    tooltip, the right-click menu, MIDI learn and gesture undo. A page is a
    flow of titled sections; ControlFlow lays them out at whatever width the
    column has.

      SCRAPE  string-scraping.md 2 - the scrape_* parameters
      SLIDE   slide-technique-controls.md 1
      SLAP    string-slap-technique.md 1 - slap_*, pop_*, ghost_*, double_thump_*
      MUTE    muting-rhythm.md 3 (MuteGroup)
      TAP     two-hand-tapping.md 3
      BEND    microtonal-bends.md 2, with the scale loader and the drawn curve
      CASCADE technique-cascade.md 6 - live technique x string grid, conflicts
*/

#include "TechniqueUi.h"
#include "../AnimationPolicy.h"
#include "../MuteGroup.h"
#include "../../DSP/Techniques/MicrotonalScale.h"

namespace luthier
{

//==============================================================================
/** Lays out attached controls in titled rows that wrap. */
class ControlFlow : public juce::Component
{
public:
    explicit ControlFlow (LuthierAudioProcessor& processor);

    void addHeading (const juce::String& text);
    LuthierKnob& addKnob (const char* parameterId, const juce::String& label, const juce::String& tooltip);
    LuthierChoice& addChoice (const char* parameterId, const juce::String& label, const juce::String& tooltip);
    LuthierToggle& addToggle (const char* parameterId, const juce::String& label, const juce::String& tooltip);
    juce::TextButton& addButton (const juce::String& text, const juce::String& tooltip, std::function<void()> onClick);

    /** Any other component, at a fixed height across the full width. */
    void addWide (juce::Component& component, int height);

    /** As addWide, and the flow owns it. */
    void addOwnedWide (juce::Component* component, int height);

    /** The height the flow needs at `width`. */
    int getHeightForWidth (int width) const;

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Every attached parameter on the page, for the tests. */
    juce::StringArray getParameterIds() const;

protected:
    LuthierAudioProcessor& processor;

private:
    struct Item
    {
        juce::Component* component = nullptr;
        int width = 0, height = 0;
        bool heading = false, wide = false;
        juce::String text;
    };

    int layout (int width, bool apply);

    std::vector<Item> items;
    juce::OwnedArray<juce::Component> owned;
    juce::StringArray parameterIds;
    juce::Array<std::pair<juce::Rectangle<int>, juce::String>> headings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlFlow)
};

//==============================================================================
class ScrapePage : public ControlFlow
{
public:
    explicit ScrapePage (LuthierAudioProcessor& processor);
};

class SlidePage : public ControlFlow
{
public:
    explicit SlidePage (LuthierAudioProcessor& processor);
};

class SlapPage : public ControlFlow
{
public:
    explicit SlapPage (LuthierAudioProcessor& processor);
};

class MutePage : public ControlFlow
{
public:
    explicit MutePage (LuthierAudioProcessor& processor);
    MuteGroup& getMuteGroup() noexcept { return group; }

private:
    MuteGroup group;
};

class TapPage : public ControlFlow
{
public:
    explicit TapPage (LuthierAudioProcessor& processor);
};

//==============================================================================
/** microtonal-bends.md 2's "user-drawn envelope shape": five points, dragged. */
class BendCurveEditor : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit BendCurveEditor (LuthierAudioProcessor& processor);
    ~BendCurveEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    /** What a drag to `value` (0-1) on point `index` does. */
    void setPoint (int index, double value);

private:
    void timerCallback() override { repaint(); }
    int pointAt (juce::Point<float>) const;

    LuthierAudioProcessor& processor;
    int dragging = -1;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "BendCurveEditor" };   // cpu-quality-modes 6 (CQ-22)
};

class BendPage : public ControlFlow
{
public:
    explicit BendPage (LuthierAudioProcessor& processor);

    /** 3: loads a .scl or .tun as the custom scale (one undo entry). False, with the reason shown, if it will not parse. */
    bool loadScale (const juce::File& file);
    juce::String getScaleStatus() const { return scaleStatus.getText(); }

private:
    juce::Label scaleStatus;
    BendCurveEditor curve;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
/** technique-cascade.md 6 / gui-techniques-updates 10: strings across, techniques down. */
class CascadeView : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit CascadeView (LuthierAudioProcessor& processor);
    ~CascadeView() override;

    void paint (juce::Graphics&) override;

    /** Re-reads armed state, live activity and conflicts (the timer does it at 30 Hz). */
    void refresh();

    /** For the tests: what the grid shows. */
    bool isArmedRow (TechniqueSlot slot) const noexcept { return armed[(size_t) slot]; }
    bool isActiveCell (TechniqueSlot slot, int string) const noexcept;
    const juce::StringArray& getConflicts() const noexcept { return conflicts; }

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    std::array<bool, (size_t) TechniqueTable::count> armed {};
    std::array<int, kMaxStrings> activity {};
    juce::StringArray conflicts;
    int numStrings = 6;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "CascadeView" };   // cpu-quality-modes 6 (CQ-22)
};

class CascadePage : public ControlFlow
{
public:
    explicit CascadePage (LuthierAudioProcessor& processor);
    CascadeView& getView() noexcept { return view; }

private:
    CascadeView view;
};

} // namespace luthier
