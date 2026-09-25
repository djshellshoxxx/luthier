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

    The arrangement follows the rectangle: one row of knobs on a wide face, two
    rows of three in a narrow column (the Advanced AMP section), and on a short
    card the name moves onto the faceplate. A face may also have no switches at
    all (the Easy rig strip's card, which has only the knobs). Labels are fitted
    to their rectangles: the full name, else the short one (GAIN BASS MID TREB
    PRES MSTR), else nothing - they never collide.

    The face never draws outside `bounds`. It is deterministic, so callers
    should cache it in an image and repaint only when the model, the palette,
    the size or a shown value changes (visual-polish.md 0.3).

    visual-polish.md 4's VU meter sits on the face where there is room for it:
    on a head's covering between the name and the vent, on a combo's grille
    at the top right. Its dial is part of the face; the needle is live.
*/

#include "KnobCaps.h"
#include "FaceMaterials.h"
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

    float vu = 0.0f;            ///< 0-1 across the VU meter's scale (vuPositionFor): the master output
    bool vuStale = false;       ///< the reading is old: the needle goes grey

    bool drawKnobs = true;      ///< false when live knob components sit on the face
    bool drawSwitches = true;   ///< false when live toggle components sit on the face
    bool drawValves = true;     ///< false when the caller draws the glow itself (paintAmpFaceValves)
    bool drawMeter = true;      ///< false when the caller draws the VU needle itself (paintAmpFaceMeter)
    bool hasSwitches = true;    ///< false for a face with no switches at all (the Easy card)
    bool enabled = true;
};

struct AmpFaceLayout
{
    juce::Rectangle<float> cabinet, faceplate, grille, logo, vent, pilot;
    juce::Rectangle<float> channelLeds;                                 ///< the row of channel lamps, if the model has them
    juce::Rectangle<float> meter;                                       ///< the VU meter (visual-polish.md 4); empty where there is no room
    std::array<juce::Rectangle<float>, numAmpKnobs> knobs;              ///< knob plus its value arc
    std::array<juce::Rectangle<float>, numAmpKnobs> labels;
    std::array<juce::Rectangle<float>, numAmpSwitches> switches;        ///< empty when the face has none
    std::array<juce::Rectangle<float>, numAmpSwitches> switchLabels;
    std::array<juce::Rectangle<float>, 2> inputs;                       ///< empty when there is no room
    juce::Rectangle<float> inputLabel;
    int knobRows = 1;
    bool combo = false;
    bool hasSwitches = true;
};

/** Where everything goes for this model in `bounds`. */
AmpFaceLayout layoutAmpFace (juce::Rectangle<float> bounds, AmpModel, bool withSwitches = true);

/** Paints the whole face into `bounds`. */
void paintAmpFace (juce::Graphics&, juce::Rectangle<float> bounds, AmpModel, const AmpFaceState&);

/** Just the valves glowing behind the vent (nothing on a model without one),
    for a panel that caches the rest of the face and redraws only the glow as
    the drive changes (visual-polish.md 0.3 and 4). */
void paintAmpFaceValves (juce::Graphics&, juce::Rectangle<float> bounds, AmpModel, const AmpFaceState&);

/** Just the VU meter's needle (nothing on a face too small for a meter), for the
    same panel: the dial is part of the cached face, the needle follows the
    master output at the meters' 30 Hz (visual-polish.md 4). */
void paintAmpFaceMeter (juce::Graphics&, juce::Rectangle<float> bounds, AmpModel, const AmpFaceState&);

/** What the face prints under each knob, switch and input for this layout. */
struct AmpFaceLabels
{
    std::array<FittedPrint, numAmpKnobs> knobs;
    std::array<FittedPrint, numAmpSwitches> switches;
    FittedPrint input;
};

/** The six knob labels share one height and one form (all full or all short),
    and so do the three switch labels, so a row reads as one. */
AmpFaceLabels fitAmpFaceLabels (const AmpFaceLayout&);

/** A knob's or a switch's label, in full (GAIN, PRESENCE) or short (GAIN, PRES). */
juce::String ampKnobLabel (AmpKnob, bool shortForm);
juce::String ampSwitchLabel (AmpSwitch, bool shortForm);

/** The knob cap the model's face uses. */
KnobCap knobCapFor (AmpModel) noexcept;

/** Whether the model's standby is a lit rocker rather than a mini toggle. */
bool standbyIsRocker (AmpModel) noexcept;

/** Every string the face can draw, for the trademark check. */
juce::StringArray ampFaceTexts (AmpModel);

} // namespace luthier::faces
