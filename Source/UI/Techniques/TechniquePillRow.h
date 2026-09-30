#pragma once

/*  Easy Mode's Techniques pill row (gui-techniques-updates.md 2).

        [SCRAPE] [SLIDE] [SLAP] [MUTE] [TAP] [BEND]   [MUTE: OFF]

    Its own component, below the Playing strip's existing controls, so the
    strip's other occupants lay out around it. Click arms, hold opens a
    popover with the technique's top controls (non-modal; Escape closes),
    right-click opens the full sub-tab in Advanced mode. The last control is
    muting-rhythm.md 7's 4-way Mute button (Off, Light, Heavy, Extreme).

    The popover is the pills' "…" for slide and bend too
    (slide-technique-controls.md 4, microtonal-bends.md 5): SLIDE's carries the
    source and mode selectors, BEND's the global range and vibrato.
*/

#include "TechniquePages.h"
#include "../MuteGroup.h"

namespace luthier
{

//==============================================================================
/** A technique's top 3-5 controls, floating over the editor. */
class TechniquePopover : public juce::Component
{
public:
    TechniquePopover (LuthierAudioProcessor& processor, TechniqueSlot slot);

    TechniqueSlot getSlot() const noexcept { return slot; }
    ControlFlow& getControls() noexcept { return flow; }

    std::function<void()> onClose;
    void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kWidth = 340;
    int getPreferredHeight() const;

private:
    TechniqueSlot slot;
    ControlFlow flow;
    juce::TextButton closeButton { "x" };
    juce::Label title;
};

//==============================================================================
class TechniquePillRow : public juce::Component
{
public:
    explicit TechniquePillRow (LuthierAudioProcessor& processor);
    ~TechniquePillRow() override;

    /** Right-click / Enter on a pill: the editor opens TECHNIQUES in Advanced mode at this sub-tab. */
    std::function<void (int subTab)> onOpenSubTab;

    TechniquePill& getPill (TechniqueSlot slot) noexcept { return *pills[(int) slot]; }
    EasyMuteButton& getMuteButton() noexcept { return muteButton; }

    /** Hold: opens (or moves to) the popover for `slot`, over the top-level component. */
    TechniquePopover* openPopover (TechniqueSlot slot);
    TechniquePopover* getPopover() const noexcept { return popover.get(); }
    void closePopover();

    void resized() override;

    static constexpr int preferredHeight = TechniquePill::preferredHeight + 4;

private:
    LuthierAudioProcessor& processor;
    juce::OwnedArray<TechniquePill> pills;
    EasyMuteButton muteButton;
    std::unique_ptr<TechniquePopover> popover;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TechniquePillRow)
};

} // namespace luthier
