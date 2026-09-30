#pragma once

/*  Shared pieces of the technique UI (gui-techniques-updates.md, TECHNIQUES
    workstream): the table of the six techniques the pills and the sub-tab
    rail are built from, the undo classes action-and-undo.md gains
    (engine-technique-layer.md 8), and the arm pill itself.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Widgets.h"
#include "../../DSP/Techniques/CascadeResolver.h"
#include "../../Rhythm/Muting.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The six techniques, in the pill row's order (gui-techniques-updates 2). */
enum class TechniqueSlot { scrape = 0, slide, slap, mute, tap, bend, numSlots };

struct TechniqueInfo
{
    TechniqueSlot slot;
    const char* name;            ///< "SCRAPE"
    const char* spokenName;      ///< "Scrape"
    const char* armParameterId;  ///< the arm switch; slide's is Slide Mode (the header toggle)
    CascadeTechnique cascade;
    const char* summary;         ///< the pill's tooltip
};

namespace TechniqueTable
{
    const TechniqueInfo& get (TechniqueSlot slot) noexcept;
    inline constexpr int count = (int) TechniqueSlot::numSlots;

    /** The CASCADE sub-tab's index in the rail (after the six). */
    inline constexpr int cascadeSubTab = count;

    bool isArmed (LuthierAudioProcessor& p, TechniqueSlot slot);

    /** The strings a technique is limited to (0 = all), for conflict messages. */
    int stringMask (LuthierAudioProcessor& p, TechniqueSlot slot);

    /** How many times the technique has fired, for the pill's dot. Any thread. */
    juce::uint32 fireCount (LuthierAudioProcessor& p, TechniqueSlot slot);

    /*  technique-cascade.md 6: the first conflict between this technique and
        another armed one on overlapping strings, as the pill's tooltip words
        it; empty when there is none. */
    juce::String conflictFor (LuthierAudioProcessor& p, TechniqueSlot slot);
}

//==============================================================================
/*  engine-technique-layer.md 8's undo classes. Each writes through the
    processor's undo stack (action-and-undo.md):

      technique-arm     arm / disarm; never grouped
      technique-param   a technique control; the same parameter within 200 ms is one entry
      mute-grid-paint   a mute cell; the same cell within 200 ms is one entry

    Live gesture triggers are audio events and never reach here. */
namespace TechniqueUndo
{
    inline constexpr int kMergeMs = 200;

    void setArmed (LuthierAudioProcessor& p, TechniqueSlot slot, bool armed);
    void setParameter (LuthierAudioProcessor& p, const juce::String& parameterId, float plainValue);
    void paintLiveMuteStep (LuthierAudioProcessor& p, int step, MuteType type);
    void paintPatternMuteStep (LuthierAudioProcessor& p, int step, MuteType type);

    /** For the tests: forget the merge window, as if 200 ms had passed. */
    void resetMergeWindow();

    /** Reads a parameter in its own units. */
    float getPlain (LuthierAudioProcessor& p, const juce::String& parameterId);
}

//==============================================================================
/*  An arm pill (gui-techniques-updates.md 2, 9, 10): rounded, filled with the
    accent when armed and outlined when not, with a small check glyph so the
    state is not colour alone, a dot that pulses when the technique fires, and
    a red slash when it conflicts with another armed technique.

    Click (or Space) toggles arm; press-and-hold opens the popover; right-click
    (or Enter) opens the sub-tab. The owner supplies those two through the
    callbacks. */
class TechniquePill : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    TechniquePill (LuthierAudioProcessor& processor, TechniqueSlot slot);
    ~TechniquePill() override;

    std::function<void (TechniqueSlot)> onHold;       ///< long-press: the popover
    std::function<void (TechniqueSlot)> onOpenSubTab; ///< right-click / Enter

    TechniqueSlot getSlot() const noexcept { return slot; }
    bool isArmed() const noexcept { return armed; }
    bool isShowingConflict() const noexcept { return conflict.isNotEmpty(); }
    const juce::String& getConflictText() const noexcept { return conflict; }

    /** What a single click does: toggles arm (an undoable technique-arm). */
    void toggle();

    /** Re-reads arm, conflict and firing state now (the timer does it at 30 Hz). */
    void refresh();

    /** The pill's accessible name: "Scrape technique, armed". */
    juce::String getAccessibleName() const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    static constexpr int kHoldMs = 450;
    static constexpr int preferredWidth = 74;
    static constexpr int preferredHeight = 22;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    TechniqueSlot slot;
    bool armed = false;
    juce::String conflict;
    juce::uint32 lastFire = 0;
    float flash = 0.0f;
    juce::uint32 pressedAt = 0;
    bool holdFired = false, pressed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TechniquePill)
};

} // namespace luthier
