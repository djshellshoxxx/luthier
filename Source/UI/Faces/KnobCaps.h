#pragma once

/*  Knob caps with character (proposals/visual-polish.md 3): the amp and pedal
    faces may use a cap that suits the model - chicken-head, pointer, skirted,
    speed, witch-hat - while keeping theme.md's value arc, the cream indicator
    and the hit area, so reading a value works the same everywhere.

    paintKnob() draws a whole knob (arc and cap) for a face painted without live
    controls. FaceKnobLookAndFeel does the same for a real slider: it lets the
    standard LookAndFeel draw the arc (with its advanced-range marking) and the
    bell, then paints the cap over the bell's footprint.
*/

#include "../Theme.h"

namespace luthier::faces
{

enum class KnobCap
{
    bell,            ///< the standard black bell (theme)
    skirtedNumbers,  ///< black skirt printed 1-10 with a silver centre cap
    pointer,         ///< brown radio knob with a pointer tab
    goldCap,         ///< black skirted knob with a gold top
    chickenHead,     ///< ivory chicken-head pointer
    chickenHeadBlack,
    speed,           ///< chrome speed knob, knurled
    chromeDome,      ///< black body with a chrome dome
    chromeSkirt,     ///< chrome skirt, black centre
    softTouch,       ///< small modern rubberised knob
    ribbed,          ///< black knurled pedal knob with a white line
    witchHat,        ///< tall conical knob
    creamRibbed      ///< cream knurled pedal knob
};

/** The body radius a knob gets in `area`, leaving room for the value arc outside it
    exactly as the standard knob does. */
float knobRadiusIn (juce::Rectangle<float> area) noexcept;

/** The pointer angle for a 0-1 value on the 270-degree scale. */
float knobAngleFor (float normalised) noexcept;

/** The value arc (track and fill) around a knob, as the standard knob draws it. */
void paintValueArc (juce::Graphics&, juce::Rectangle<float> area, float normalised, bool enabled);

/** Just the cap: centre, body radius, pointer angle (radians, 0 = up, clockwise). */
void paintKnobCap (juce::Graphics&, juce::Point<float> centre, float radius, float angle, KnobCap, bool enabled);

/** A whole knob in `area`: value arc and cap. */
void paintKnob (juce::Graphics&, juce::Rectangle<float> area, KnobCap, float normalised, bool enabled = true);

//==============================================================================
/** The standard LookAndFeel with a face's cap on its rotary sliders. */
class FaceKnobLookAndFeel : public LuthierLookAndFeel
{
public:
    explicit FaceKnobLookAndFeel (KnobCap cap = KnobCap::bell) : knobCap (cap) {}

    void setCap (KnobCap cap) noexcept { knobCap = cap; }
    KnobCap getCap() const noexcept { return knobCap; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

private:
    KnobCap knobCap;
};

} // namespace luthier::faces
