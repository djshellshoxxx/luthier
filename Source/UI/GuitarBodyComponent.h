#pragma once

/*  The guitar illustration.

    The brief asks for a photo-realistic image of the selected instrument. Shipping
    photographs would mean licensing images of trademarked instruments, so this
    draws the guitar instead: a vector illustration whose silhouette, pickups,
    hardware and control layout are generated from the *same* GuitarSpec the audio
    engine is using. Change the guitar and the picture changes with it; change the
    pickup positions in Advanced mode and the pickups move.

    That trade buys something a photograph could not give: the picture is always
    correct, including for a custom instrument the user just built.

    The overlaid controls are live: click a pickup to select it, drag the volume
    and tone knobs, click the selector switch to change position.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"

namespace luthier
{

class LuthierAudioProcessor;

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

private:
    void timerCallback() override;
    void rebuildGeometry();

    struct Geometry
    {
        juce::Path body;
        juce::Path soundHole;
        juce::Path pickguard;
        juce::Path neck;
        juce::Path headstock;

        juce::Rectangle<float> pickupBounds[3];
        int numPickups = 0;

        juce::Rectangle<float> bridgeBounds;
        juce::Rectangle<float> volumeKnob;
        juce::Rectangle<float> toneKnob;
        juce::Rectangle<float> selectorSwitch;

        bool acoustic = false;
        bool hasSoundHole = false;
        juce::Colour bodyColour;
        juce::Colour topColour;
    };

    /** Generates a guitar outline from a handful of proportions. Every factory
        shape is expressible this way, which is why a custom build still looks like
        a guitar rather than like a missing asset. */
    static juce::Path buildBodyOutline (juce::Rectangle<float> area,
                                        float upperBout, float waist, float lowerBout,
                                        float upperCutaway, float lowerCutaway,
                                        float offsetSkew, float squareness);

    static juce::Colour woodColourFor (int wood);

    LuthierAudioProcessor& processor;
    Geometry geometry;

    int cachedGuitarType = -1;
    int cachedNumPickups = -1;
    double cachedPickupPositions[3] = { -1.0, -1.0, -1.0 };

    int hoveredPickup = -1;
    int draggingKnob = -1;      // 0 = volume, 1 = tone
    double dragStartValue = 0.0;
    int dragStartY = 0;

    std::array<double, 12> stringLevels {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarBodyComponent)
};

} // namespace luthier
