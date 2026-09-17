#pragma once

/*  The drag-and-drop pedal rack.

    Eight slots. Each shows its pedal's type, a bypass footswitch, a mix control
    and whatever parameters that pedal declares - drawn straight from the pedal's
    own PedalParam descriptors, so adding a pedal to the engine adds its UI here
    with no extra work.

    Slots reorder by dragging one onto another.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Effects/EffectsChain.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class PedalSlotComponent : public juce::Component,
                           private juce::Timer
{
public:
    PedalSlotComponent (LuthierAudioProcessor& processor, bool postChain, int slotIndex);
    ~PedalSlotComponent() override;

    int getSlotIndex() const noexcept { return slotIndex; }
    bool isPostChain() const noexcept { return postChain; }

    /** Rebuilds the parameter knobs after the pedal type changes. */
    void rebuildControls();

    /** Height this slot wants, given its current pedal. */
    int getPreferredHeight() const;

    std::function<void (int fromSlot, int toSlot)> onReorderRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    bool postChain;
    int slotIndex;

    LuthierChoice typeSelector;
    LuthierToggle bypassToggle { "On" };
    LuthierKnob mixKnob { "Mix", LuthierKnob::Size::Small };

    juce::OwnedArray<LuthierKnob> paramKnobs;

    PedalType cachedType = PedalType::None;

    bool dragging = false;
    juce::Point<int> dragStart;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalSlotComponent)
};

//==============================================================================
class PedalRack : public juce::Component
{
public:
    PedalRack (LuthierAudioProcessor& processor, bool postChain);
    ~PedalRack() override;

    /** Total height the rack needs. */
    int getPreferredHeight() const;

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void reorder (int fromSlot, int toSlot);

    LuthierAudioProcessor& processor;
    bool postChain;

    juce::OwnedArray<PedalSlotComponent> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalRack)
};

} // namespace luthier
