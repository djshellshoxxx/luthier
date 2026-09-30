#pragma once

/*  The CHARACTER tab's convenience mirrors of technique controls
    (gui-techniques-updates.md 5, two-hand-tapping.md 6, microtonal-bends.md 5,
    slide-technique-controls.md 4). The primary home of each is its
    TECHNIQUES sub-tab; these are the same parameters, attached again.

      TAPPING (Right Hand)     strength curve, lateral flick, auto pull-off
      MICROTONAL (Playing)     global range, vibrato, quantise
      SLIDE CONTROLS           an expandable section: source, mode, contact,
                               speed limit, auto-vibrato

    CHARACTER has no Right Hand or PLAYING group in this build yet
    (fingerstyle-attack.md is another workstream's), so the two sections sit
    together at the foot of the tab, titled with the group they belong to.
*/

#include "TechniquePages.h"

namespace luthier
{

class TechniqueMirrors : public ControlFlow
{
public:
    explicit TechniqueMirrors (LuthierAudioProcessor& processor);

    /** The slide section's expander. */
    juce::TextButton& getSlideExpander() noexcept { return slideExpander; }
    bool isSlideExpanded() const noexcept { return slideFlow.isVisible(); }

    std::function<void()> onHeightChanged;

private:
    juce::TextButton slideExpander { "SLIDE CONTROLS  +" };
    ControlFlow slideFlow;
};

} // namespace luthier
