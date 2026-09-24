#pragma once

/*  The drag-and-drop pedal rack.

    Eight slots. Each shows its pedal's type, a bypass footswitch, a mix control
    and whatever parameters that pedal declares - drawn straight from the pedal's
    own PedalParam descriptors, so adding a pedal to the engine adds its UI here
    with no extra work.

    A slot with a pedal in it shows the pedal itself (proposals/visual-polish.md
    2): its face lying on its side for the rack row, from Faces/PedalFace.h,
    drawn once into a cached image and again only when the type, bypass, the
    palette or the size changes. The live controls sit on the face: the
    parameter knobs on its knob positions in the type's caps, the mix knob as
    the last knob on the face, and the bypass toggle as the footswitch. The LED
    follows bypass. The type selector stays above the face and the slot number
    down the left.

    Slots reorder by dragging one onto another: a drag that starts on the face
    (anywhere that is not a control) moves the slot.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "Faces/PedalFace.h"
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

    /*  A slot shown on its own (Easy mode's popover) sizes itself to its
        pedal when the type changes, so a pedal picked into an empty slot gets
        its whole face, not the empty slot's 34 points. The Advanced rack
        leaves this off and lays its slots out itself. */
    void setSizesToContent (bool shouldSize) noexcept { sizesToContent = shouldSize; }

    /** Height this slot wants, given its current pedal, at its width (or the
        Advanced column's, before it has one). */
    int getPreferredHeight() const;
    int getPreferredHeightFor (int width) const;

    /** The width the Advanced column gives a rack (column 3's 260, less its padding). */
    static constexpr int nominalWidth = 234;

    std::function<void (int fromSlot, int toSlot)> onReorderRequested;

    //==========================================================================
    /** What the tests look at: the face, its layout and the controls on it. */
    PedalType getShownType() const noexcept { return cachedType; }
    juce::Rectangle<float> getFaceBounds() const;
    faces::PedalFaceLayout getFaceLayout() const;
    int getNumParameterKnobs() const noexcept { return paramKnobs.size(); }
    LuthierKnob* getParameterKnob (int index) const noexcept { return paramKnobs[index]; }
    LuthierKnob& getMixKnob() noexcept { return mixKnob; }
    LuthierToggle& getBypassToggle() noexcept { return bypassToggle; }
    int getFaceRenderCount() const noexcept { return faceRenders; }

    /** What the timer does: follows the type and bypass. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { refresh(); }

    bool bypassFromParameter() const;
    void updateBypassTooltip();
    faces::PedalFaceState faceState() const;
    void renderFace (float scale);

    struct CacheKey
    {
        PedalType type = PedalType::NumTypes;
        bool bypassed = false, enabled = true;
        int knobs = 0, width = 0, height = 0;
        float scale = 1.0f;
        juce::uint64 palette = 0;

        bool operator== (const CacheKey& o) const noexcept
        {
            return type == o.type && bypassed == o.bypassed && enabled == o.enabled && knobs == o.knobs
                && width == o.width && height == o.height && scale == o.scale && palette == o.palette;
        }

        bool operator!= (const CacheKey& o) const noexcept { return ! (*this == o); }
    };

    CacheKey keyFor (float scale) const;

    LuthierAudioProcessor& processor;
    bool postChain;
    int slotIndex;

    // Declared before the controls, so it outlives them.
    faces::FaceKnobLookAndFeel faceLookAndFeel;

    LuthierChoice typeSelector;
    LuthierToggle bypassToggle { "On" };
    LuthierKnob mixKnob { "Mix", LuthierKnob::Size::Small };

    juce::OwnedArray<LuthierKnob> paramKnobs;

    PedalType cachedType = PedalType::None;
    bool shownBypass = false;
    juce::StringArray faceLabels;     ///< the parameters' short names, then MIX

    juce::Image faceImage;
    CacheKey cachedKey;
    int faceRenders = 0;

    bool dragging = false;
    bool sizesToContent = false;
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

    /** The slots, for the tests. */
    int getNumSlots() const noexcept { return slots.size(); }
    PedalSlotComponent* getSlot (int index) const noexcept { return slots[index]; }

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
