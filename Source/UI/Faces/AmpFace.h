#pragma once

/*  Amp faces (proposals/visual-polish.md 2): each amp model drawn as its
    family's front - Tolex or tweed, grille cloth, faceplate, a generic name
    plate and a pilot light that follows Standby - with the same six knobs and
    three switches the AMP section has today, in the same order.

    The painter is standalone: give it a rectangle and a state and it draws the
    whole face, knobs included. A panel with live knob components sets
    drawKnobs (and drawSwitches) false, paints the face underneath, and places
    its components on layoutAmpFace()'s rectangles, with FaceKnobLookAndFeel
    set to knobCapFor (model) so the knobs wear the model's caps.

    The face never draws outside `bounds`. It is deterministic, so callers
    should cache it in an image and repaint only when the model, the palette,
    the size or a shown value changes (visual-polish.md 0.3).
*/

#include "KnobCaps.h"
#include "../../DSP/Amp/AmpEngine.h"

#include <array>

namespace luthier::faces
{

/** The knobs in AMP-section order. */
enum AmpKnob { gainKnob, bassKnob, midKnob, trebleKnob, presenceKnob, masterKnob, numAmpKnobs };

/** The switches in AMP-section order. */
enum AmpSwitch { brightSwitch, midBoostSwitch, standbySwitch, numAmpSwitches };

struct AmpFaceState
{
    std::array<float, numAmpKnobs> knobs { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.7f };   ///< normalised 0-1
    bool bright = false, midBoost = false, standby = false;

    float drive = 0.0f;         ///< 0-1, the level the amp reports: the valves glow with it
    bool driveStale = false;    ///< the reading is old: the glow goes grey

    bool drawKnobs = true;      ///< false when live knob components sit on the face
    bool drawSwitches = true;   ///< false when live toggle components sit on the face
    bool enabled = true;
};

struct AmpFaceLayout
{
    juce::Rectangle<float> cabinet, faceplate, grille, logo, vent, pilot;
    std::array<juce::Rectangle<float>, numAmpKnobs> knobs;      ///< knob plus its value arc
    std::array<juce::Rectangle<float>, numAmpKnobs> labels;
    std::array<juce::Rectangle<float>, numAmpSwitches> switches;
    std::array<juce::Rectangle<float>, 2> inputs;
    bool combo = false;
};

/** Where everything goes for this model in `bounds`. */
AmpFaceLayout layoutAmpFace (juce::Rectangle<float> bounds, AmpModel);

/** Paints the whole face into `bounds`. */
void paintAmpFace (juce::Graphics&, juce::Rectangle<float> bounds, AmpModel, const AmpFaceState&);

/** The knob cap the model's face uses. */
KnobCap knobCapFor (AmpModel) noexcept;

/** Every string the face can draw, for the trademark check. */
juce::StringArray ampFaceTexts (AmpModel);

} // namespace luthier::faces
