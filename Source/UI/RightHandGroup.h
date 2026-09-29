#pragma once

/*  fingerstyle-attack.md 7 (REALISM-B): the CHARACTER tab's RIGHT HAND group,
    and the Easy Mode Playing strip's Tool selector.

    RIGHT HAND: the style dropdown; a six-cell string row with a tool glyph
    per string (click cycles, right-click lists); stroke; flesh and nail
    release (with the cutoffs they give); thumb position; rest damping;
    Travis mute; hybrid snap; and mirrors of the PICK group's fingers switch
    and nail/flesh blend.

    A style writes its table on a user change only, on the message thread, as
    one undo entry (4); a preset load never re-applies it, because nothing
    but these controls ever calls applyStyle.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** One string's tool: a glyph that cycles on click and lists on right-click. */
class StringToolCell : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    StringToolCell (LuthierAudioProcessor& processor, int stringNumber);

    static juce::String glyphFor (int tool);

    int getTool() const;
    void setTool (int tool);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    LuthierAudioProcessor& processor;
    int stringNumber;   ///< 1 = high E (routing-io.md 3)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringToolCell)
};

//==============================================================================
class RightHandGroup : public juce::Component,
                       private juce::Timer
{
public:
    explicit RightHandGroup (LuthierAudioProcessor& processor);
    ~RightHandGroup() override;

    static constexpr int headingHeight = 20, rowHeight = 22, choiceHeight = 36, cellHeight = 30, noteHeight = 14, gap = 2;

    int preferredHeight() const;

    /** 4: writes a style's table as one undo entry. Message thread. */
    static void applyStyle (LuthierAudioProcessor& processor, int styleIndex);

    /** The style's name, with "(modified)" once the tools no longer match it. */
    static juce::String describeStyle (LuthierAudioProcessor& processor);

    void resized() override;

    juce::ComboBox& getStyleBox() noexcept { return styleBox; }
    StringToolCell& getCell (int index) noexcept { return *cells[(size_t) juce::jlimit (0, 5, index)]; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    juce::Label heading, styleLabel, cutoffNote, alternationNote;
    juce::ComboBox styleBox;
    std::array<std::unique_ptr<StringToolCell>, 6> cells;

    LuthierChoice stroke { "Stroke" };
    LuthierSlider flesh { "Flesh release" }, nail { "Nail release" }, thumbPosition { "Thumb position" },
                  restDamping { "Rest damping" }, travisMute { "Travis mute" }, hybridSnap { "Hybrid snap" },
                  nailVsFlesh { "Nail / flesh" }, alternation { "Alternation" };
    std::unique_ptr<LuthierToggle> fingers;
    bool hasAlternation = false;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightHandGroup)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "RightHandGroup" };
};

//==============================================================================
/*  gui-integration.md 3.3's Playing strip Tool selector: rh_style as a
    segmented control, Custom shown as "Mixed" (fingerstyle-attack.md 7). */
class RightHandToolSelector : public juce::Component,
                              private juce::Timer
{
public:
    explicit RightHandToolSelector (LuthierAudioProcessor& processor);
    ~RightHandToolSelector() override;

    static juce::String labelFor (int styleIndex);

    int getSelectedStyle() const noexcept { return selected; }
    juce::TextButton& getSegment (int index) noexcept { return *segments[(size_t) juce::jlimit (0, 6, index)]; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    std::array<std::unique_ptr<juce::TextButton>, 7> segments;
    int selected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightHandToolSelector)
};

} // namespace luthier
