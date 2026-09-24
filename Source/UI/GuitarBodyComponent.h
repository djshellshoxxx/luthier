#pragma once

/*  The guitar illustration.

    The brief asks for a photo-realistic image of the selected instrument. Shipping
    photographs would mean licensing images of trademarked instruments, so this
    draws the guitar instead: GuitarRenderer (guitar-illustration.md) draws the
    parts guitar the engine is playing - body, neck, headstock, bridge, pickups,
    strings and finish each from its part. Change a part and the picture changes
    with it; move a pickup in the Workshop and the pickup moves.

    That trade buys something a photograph could not give: the picture is always
    correct, including for a custom instrument the user just built.

    The overlaid controls are live: click a pickup to select it, drag the volume
    and tone knobs, click the selector switch to change position.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "Guitar/GuitarRenderer.h"
#include "Guitar/StringAnimator.h"   // animated-strings.md 4.3
#include "Guitar/StringMotionPolicy.h"   // cpu-quality-modes.md 6

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/*  The headstock popover (gui-integration.md 3.1).

    Section 3.1 asks the headstock for "per-string tuning, capo, temperament".
    Two of those three are here; the third is not built anywhere in the plugin and
    the popover says so rather than leaving a gap the user has to guess at.

    Per-string tuning is the reason this exists rather than being a mirror of the
    Advanced column. `TuningEngine::StringTuning::detuneCents` is round-tripped by
    `PresetManager` - it is saved with the preset and restored from it - and until
    now nothing in the UI could set it. A preset field with no way to author it is
    the same class of gap as a panel nobody constructs.

    The detune sliders write engine state rather than a parameter, because there
    is no per-string parameter to attach to. That costs automation and MIDI Learn
    on these six controls, which is a real limitation and is recorded in GAPS.md
    rather than hidden: the fix is per-string parameters, which is a parameter
    count change and a preset schema question, not a UI one.
*/
class TuningPopover : public juce::Component
{
public:
    explicit TuningPopover (LuthierAudioProcessor& processor);
    ~TuningPopover() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::Rectangle<int> preferredSize (int numStrings);

private:
    void refreshNoteNames();

    LuthierAudioProcessor& processor;

    /*  Section 3.1 asks the headstock popover for "per-string tuning, capo,
        temperament". Capo used to be a line here saying it was not built. */
    std::unique_ptr<LuthierChoice> preset, temperament, capo;
    std::unique_ptr<LuthierKnob> concertA;

    juce::OwnedArray<juce::Slider> detuneSliders;
    juce::StringArray noteNames;

    int numStrings = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningPopover)
};

//==============================================================================
/*  The bridge popover (gui-integration.md 3.1).

    "Click opens the whammy popover (only if a whammy is fitted)". Fitted means
    the bridge type is not the hardtail: a fixed bridge has no arm, and a popover
    offering whammy range on an instrument that cannot whammy would be a control
    that does nothing.

    Every control here is bound to the same parameter as its twin in Advanced
    column 1, so this is a legitimate second route under ground rule 1 rather than
    a separate copy of the state.
*/
class WhammyPopover : public juce::Component
{
public:
    explicit WhammyPopover (LuthierAudioProcessor& processor);
    ~WhammyPopover() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** True when the fitted bridge has an arm, which is what section 3.1 makes
        the popover conditional on. */
    static bool isWhammyFitted (LuthierAudioProcessor& processor);

    static juce::Rectangle<int> preferredSize();

private:
    LuthierAudioProcessor& processor;

    std::unique_ptr<LuthierChoice> bridgeType;
    std::unique_ptr<LuthierKnob> position, downRange, upRange, springs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhammyPopover)
};

//==============================================================================
class GuitarBodyComponent : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit GuitarBodyComponent (LuthierAudioProcessor& processor);
    ~GuitarBodyComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Called when the user clicks a pickup, so the Advanced panel can follow. */
    std::function<void (int pickupSlot)> onPickupSelected;

    /*  Section 3.1's two popovers. Public so a test can open them without
        synthesising a mouse event on a component that has no desktop peer. */
    void showTuningPopover();

    /** Does nothing on a hardtail, which is what section 3.1 specifies. */
    void showWhammyPopover();

    /** What the illustration currently shows, for tests and the Workshop. */
    const GuitarScene& getScene() const noexcept { return scene; }

    // animated-strings.md 4.3: the strings' frame driver, and one 30 Hz tick for tests.
    StringAnimator& getStringAnimator() noexcept { return animator; }
    void tickForTesting() { timerCallback(); }
    const GuitarOverlay& getOverlayForTesting() const noexcept { return overlay; }
    juce::AffineTransform getMmToPxForTesting() { ensureTransform(); return mmToPx; }

private:
    void timerCallback() override;

    /** Rebuilds the static scene when the guitar (or the palette) changed. */
    void rebuildScene (bool force);
    void rebuildCache();

    // animated-strings.md 4.4.
    void ensureTransform();
    bool fillMotionGeometry (StringMotionGeometry&);

    juce::Point<float> toMm (juce::Point<float> px) const;
    GuitarRegion regionAt (juce::Point<float> px) const;
    int knobAt (juce::Point<float> px) const;
    juce::String knobParameter (int knob) const;
    int engineSlotFor (int fileIndex) const;
    static int pickupIndexFor (GuitarRegion) noexcept;
    juce::Rectangle<int> screenAreaOf (GuitarRegion) const;

    /** The tooltip for whatever is under the cursor, so every hit region says
        what clicking it does before it is clicked (section 20). */
    juce::String describeHoverTarget (juce::Point<float> position) const;

    int getBridgeTypeIndex() const;

    LuthierAudioProcessor& processor;

    GuitarScene scene;
    juce::AffineTransform mmToPx;
    juce::Image cache;
    float cacheScale = 1.0f;
    int ticksSinceKeyCheck = 0;

    GuitarOverlay overlay;
    GuitarRegion hoveredRegion = GuitarRegion::none;

    // animated-strings.md: declared after the scene it reads.
    StringAnimator animator;
    bool cacheOmitsSpeaking = false;

    int draggingKnob = -1;
    double dragStartValue = 0.0;
    int dragStartY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarBodyComponent)
};

} // namespace luthier
