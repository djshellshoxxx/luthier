#pragma once

/*  Pedal faces (proposals/visual-polish.md 2): each pedal type drawn as a
    stompbox of its kind - an enclosure colour and finish, its knobs placed as a
    pedal's are, the type's generic name, an LED that follows bypass and a
    footswitch. Wah and volume are treadles (with a toe switch), the rotary
    speaker is a small cabinet with louvres, the spring reverb a covered tank
    unit and the graphic EQ a bank of sliders.

    Portrait bounds give an upright stompbox; bounds wider than tall give the
    same pedal lying on its side, for a rack row. A rack row that needs more
    height than width for its knobs asks for its side explicitly. On its side,
    the knobs take as many rows as keep them a usable size. As with the amp
    faces, a slot with live knob components sets drawKnobs false and places
    them on layoutPedalFace()'s rectangles, with FaceKnobLookAndFeel set to
    knobCapFor (type). The face never draws outside `bounds`.
*/

#include "KnobCaps.h"
#include "../../DSP/Effects/Pedal.h"

#include <array>

namespace luthier::faces
{

enum class PedalOrientation { automatic, upright, onItsSide };

struct PedalFaceState
{
    bool bypassed = false;
    int numKnobs = 0;
    std::array<float, Pedal::kMaxParams> values {};    ///< normalised 0-1
    juce::StringArray labels;                          ///< one short label per knob

    bool drawKnobs = true;      ///< false when live knob components sit on the face
    bool drawFootswitch = true;
    bool enabled = true;
    PedalOrientation orientation = PedalOrientation::automatic;

    /** The state of a live pedal: its knob count, labels and values. */
    static PedalFaceState from (const Pedal&);

    /** A type's knobs at their defaults, for previews and tests. */
    static PedalFaceState defaultsFor (PedalType);
};

struct PedalFaceLayout
{
    juce::Rectangle<float> enclosure, name, led, footswitch, treadle;
    std::array<juce::Rectangle<float>, Pedal::kMaxParams> knobs;   ///< knob plus its value arc (or slider slot)
    std::array<juce::Rectangle<float>, Pedal::kMaxParams> labels;
    int numKnobs = 0;
    bool portrait = true;
    bool sliders = false;       ///< the graphic EQ shows sliders, not knobs
};

PedalFaceLayout layoutPedalFace (juce::Rectangle<float> bounds, PedalType, int numKnobs,
                                 PedalOrientation = PedalOrientation::automatic);

void paintPedalFace (juce::Graphics&, juce::Rectangle<float> bounds, PedalType, const PedalFaceState&);

/** The height a pedal lying on its side at this width needs so that every
    knob keeps a usable size (and the graphic EQ's sliders a usable travel). */
float preferredHeightOnItsSide (float width, PedalType, int numKnobs);

/** The knob cap the type's face uses. */
KnobCap knobCapFor (PedalType) noexcept;

/** A knob label's short form, for a face with no room for the full one ("1.6 KHZ" -> "1.6K"). */
juce::String shortPedalLabel (const juce::String& label);

/** Every string the face can draw for this type (name and default knob labels), for the trademark check. */
juce::StringArray pedalFaceTexts (PedalType);

} // namespace luthier::faces
