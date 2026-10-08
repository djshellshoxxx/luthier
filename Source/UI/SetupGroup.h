#pragma once

/*  The CHARACTER tab's SETUP group (gui-integration.md 4.4, fret-buzz.md 6).

    Action treble and bass, relief, nut depth per string, fret height, buzz
    threshold, sitar mode, the setup style presets, and the live buzz heatmap
    - the control that makes a setup comprehensible: lower the action and
    watch the low frets light up.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/*  fret-buzz.md 6.2: a small fretboard, each cell shaded by how close that
    string is to buzzing on that fret right now. Accent for buzzing, warning
    colour for within 0.05 mm, and a dot glyph as well as colour so it reads
    in monochrome (gui-integration 21). 30 Hz; greys after 2 s with nothing
    changing. */
class BuzzHeatmap : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit BuzzHeatmap (LuthierAudioProcessor& processor);
    ~BuzzHeatmap() override;

    void paint (juce::Graphics&) override;

    static constexpr int preferredHeight = 70;
    static constexpr float kNearMm = 0.05f;

    enum class CellState { clear, near, buzzing };

    /** What a cell shows, from its excess in mm. */
    static CellState stateFor (float excessMm) noexcept;

    bool isStale() const noexcept;

    /** SPEC-SWEEP (GD-13, gui-engine-dataflow 6.3): stale after 500 ms without a
        change, fading out over the next 200 ms rather than switching. 1 is
        fresh, 0 fully faded, for a map @p ageSeconds old. */
    static constexpr double kStaleSeconds = 0.5;
    static constexpr double kFadeSeconds = 0.2;
    static float freshnessFor (double ageSeconds) noexcept
    {
        return (float) juce::jlimit (0.0, 1.0, 1.0 - (ageSeconds - kStaleSeconds) / kFadeSeconds);
    }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::uint64 lastDigest = 0;
    double lastChange = -1.0e9;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BuzzHeatmap)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "BuzzHeatmap" };
};

//==============================================================================
class SetupGroup : public juce::Component,
                   private juce::Timer
{
public:
    explicit SetupGroup (LuthierAudioProcessor& processor);
    ~SetupGroup() override;

    int preferredHeight() const;
    void resized() override;

    /** fret-buzz.md 6.1: writes a style's action and relief as one undo step. */
    void applySetupStyle (int index);

    /** "<style>" or "<style> (modified)". */
    juce::String describeSetupStyle() const;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading;
    juce::ComboBox styleBox;
    LuthierSlider actionTreble { "Action treble" }, actionBass { "Action bass" },
                  relief { "Relief" }, fretHeight { "Fret height" }, threshold { "Buzz trim" };
    juce::OwnedArray<LuthierSlider> nutDepths;
    LuthierToggle sitarMode { "Sitar mode" };
    BuzzHeatmap heatmap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetupGroup)
};

} // namespace luthier
