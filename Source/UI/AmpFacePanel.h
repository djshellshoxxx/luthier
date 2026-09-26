#pragma once

#include "AnimationPolicy.h"   // cpu-quality-modes 6

/*  The amp's own face with its live controls on it (proposals/visual-polish.md
    2-4, gui-integration.md 3.2 and 4.3).

    The face is the painter in Faces/AmpFace.h, drawn once into a cached image
    and drawn again only when the model, the palette, the size or Standby
    changes (visual-polish.md 0.3 and 7). On it sit the same controls the AMP
    section has always had, attached to the same parameters: the six knobs -
    gain, bass, mid, treble, presence, master - on the face's knob positions in
    the model's caps, and, in the Advanced section, the bright, mid boost and
    standby switches on its switch positions. The Easy rig strip's card has only
    the knobs, as before. The model choice stays where it was, beside the face,
    because it is not a control an amp has on its front.

    Live, and nothing else moves (visual-polish.md 0.4 and 4): the pilot light
    follows Standby, and the valves glow with the drive the amp engine reports
    (AmpEngine::getSagAmount, polled at the meters' 30 Hz,
    gui-engine-dataflow.md 2). Only the vent is repainted as the glow changes.
    A reading that has stopped moving while it is still above zero means the
    engine has stopped reporting (the host stopped processing mid-note), and
    the glow greys out as stale; a reading resting at zero is an idle amp, not
    a stale one.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "Faces/AmpFace.h"

#include <array>

namespace luthier
{

class LuthierAudioProcessor;

class AmpFacePanel : public juce::Component,
                     private juce::Timer
{
public:
    enum class Style
    {
        section,   ///< Advanced column 3: six knobs and three switches
        card       ///< the Easy rig strip's amp card: six knobs
    };

    AmpFacePanel (LuthierAudioProcessor& processor, Style style);
    ~AmpFacePanel() override;

    /** Column 3 is 260 wide, which leaves the face 234 by this. */
    static constexpr int sectionHeight = 228;

    LuthierKnob& getKnob (faces::AmpKnob knob) noexcept { return *knobs[(size_t) knob]; }

    /** Nullptr on a card, which has no switches. */
    LuthierToggle* getSwitch (faces::AmpSwitch sw) noexcept { return switches[(size_t) sw].get(); }

    Style getStyle() const noexcept { return style; }
    AmpModel getShownModel() const noexcept { return shownModel; }

    /** Where the face is drawn, and its layout there, in this component's coordinates. */
    juce::Rectangle<float> getFaceBounds() const;
    faces::AmpFaceLayout getFaceLayout() const;

    /** What is on the face now: the model's pilot, the drive, stale or not. */
    faces::AmpFaceState getFaceState() const;

    /** How many times the face image has been drawn, for the cache test. */
    int getFaceRenderCount() const noexcept { return faceRenders; }

    /** What the timer does: reads the model, Standby and the drive, and repaints what changed. */
    void refresh();

    /** Seconds without a change after which a non-zero drive reading is stale. */
    static constexpr double staleAfterSeconds = 0.5;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void enablementChanged() override;

private:
    void timerCallback() override { refresh(); }

    void applyModel();
    void renderFace (float scale);
    bool parameterIsOn (const char* id) const;
    AmpModel modelFromParameter() const;

    struct CacheKey
    {
        AmpModel model = AmpModel::NumModels;
        bool standby = false, enabled = true;
        int width = 0, height = 0;
        float scale = 1.0f;
        juce::uint64 palette = 0;

        bool operator== (const CacheKey& o) const noexcept
        {
            return model == o.model && standby == o.standby && enabled == o.enabled && width == o.width
                && height == o.height && scale == o.scale && palette == o.palette;
        }

        bool operator!= (const CacheKey& o) const noexcept { return ! (*this == o); }
    };

    CacheKey keyFor (float scale) const;

    LuthierAudioProcessor& processor;
    const Style style;

    // Declared before the controls, so it outlives them.
    faces::FaceKnobLookAndFeel faceLookAndFeel;

    std::array<std::unique_ptr<LuthierKnob>, faces::numAmpKnobs> knobs;
    std::array<std::unique_ptr<LuthierToggle>, faces::numAmpSwitches> switches;

    AmpModel shownModel = AmpModel::NumModels;
    bool shownStandby = false;

    float shownDrive = 0.0f;
    bool shownStale = false;

    /*  cpu-quality-modes 6: the valve glow is Decorative. At Off the timer
        stops, the glow is drawn static from the drive parameter, and the
        policy's 4 Hz poll keeps the model and the pilot light current. */
    AnimationPolicy::Registration motion { *this, AnimationPolicy::Decorative, "AmpFacePanel",
                                           [this] { refresh(); }, [this] { refresh(); } };
    double lastSag = 0.0, lastSagChange = 0.0;

    juce::Image faceImage;
    CacheKey cachedKey;
    int faceRenders = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpFacePanel)
};

} // namespace luthier
