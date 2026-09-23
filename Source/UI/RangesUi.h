#pragma once

/*  The UI half of advanced-ranges.md: marking, the right-click unlock, the
    first-unlock explainer and keeping attached controls in step with a range
    that has been swapped under them.

    Kept out of Widgets.cpp because every piece of it is shared - the knob, the
    slider, the Options RANGES page and the right-click menu all go through the
    same few functions, and the rule each one enforces (marking follows the
    value, the explainer fires once, every change is undoable) should live in
    one place rather than be re-derived at every control.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PhysicalRange.h"

namespace luthier
{

class LuthierAudioProcessor;

namespace RangesUi
{
    /** UiPreferences keys. User-global, not preset state (advanced-ranges.md 6.2). */
    inline constexpr const char* kWarningColourKey      = "ranges_warning_colour";
    inline constexpr const char* kRandomiseInStockKey   = "randomise_respects_stock";
    inline constexpr const char* kExplainerShownKey     = "ranges_first_unlock_explained";

    /** The exact words of onboarding.md 7. */
    extern const char* const kExplainerText;

    /** The exact words of gui-integration.md 14 / advanced-ranges.md 6.3. */
    extern const char* const kLockedNoticeText;

    /** "Always show marked values as warning colour", default on. */
    bool markInWarningColour();
    void setMarkInWarningColour (bool shouldMark);

    /** "Randomise respects stock range", default on. Also pushes the value to
        the processor, which cannot read UiPreferences itself. */
    bool randomiseRespectsStock();
    void setRandomiseRespectsStock (LuthierAudioProcessor& processor, bool shouldRespect);

    //==========================================================================
    /*  Slider properties the look and feel reads to draw the warning portion of
        the arc. Plain values; absent on a slider whose parameter is not
        physical. Set by `tagSlider`, which is called on attach and on resync. */
    inline constexpr const char* kStockMinProperty = "luthierStockMin";
    inline constexpr const char* kStockMaxProperty = "luthierStockMax";

    void tagSlider (juce::Slider& slider, const juce::String& parameterId);

    /** True if this parameter is physical and its plain value is outside stock. */
    bool isMarked (const LuthierAudioProcessor& processor, const juce::String& parameterId);

    /** `text` with the `*` suffix when the value is marked (6.1). */
    juce::String markReadout (const LuthierAudioProcessor& processor,
                              const juce::String& parameterId,
                              const juce::String& text);

    //==========================================================================
    /*  Re-attaches every LuthierKnob and LuthierSlider under `root` whose
        parameter's live range no longer matches its slider's. Called by the
        editor when RangeState::getGeneration() moves. */
    void resyncControls (juce::Component& root);

    //==========================================================================
    /*  Applies a new range state as a user action: an undo entry, the clamp,
        the explainer on the first ever transition to advanced (6.4), and a
        warning notice when a narrowing moved values. Returns the clamp count.

        `anchor` positions the explainer; pass the control or page the user
        acted on. */
    int apply (LuthierAudioProcessor& processor,
               const RangeState& newState,
               const juce::String& undoDescription,
               juce::Component* anchor);

    /** Shows onboarding.md 7's popover if it has never been shown. */
    void showExplainerIfFirstTime (juce::Component& anchor);

    /** A family's display name, for undo descriptions and the summary list. */
    juce::String familyDisplayName (RangeFamily family);

    /** A plain value formatted with the parameter's own text function. */
    juce::String formatValue (const LuthierAudioProcessor& processor,
                              const juce::String& parameterId, float plain);

    /*  The padlock glyph shared by the header and the tab headers
        (gui-integration 2, 21). Open shackle = the preset opts in. */
    void drawPadlock (juce::Graphics& g, juce::Rectangle<float> bounds,
                      juce::Colour colour, bool open);

    /*  The header's range-lock indicator (gui-integration 2): shown next to
        the preset name while the preset has anything unlocked, secondary
        accent, and a click opens Options -> Ranges. */
    class PadlockButton : public juce::Button
    {
    public:
        PadlockButton();

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };
}

} // namespace luthier
