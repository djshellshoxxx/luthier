#pragma once

/*  piano-roll-chord-display.md 4, "Naming": the name the guitar shows for the
    notes sounding now.

    - One pitch class: the note, no octave ("G#").
    - Two: a fifth apart is a power chord ("E5"); anything else lists both,
      lowest first ("C E").
    - Three or more: ChordDetector's symbol ("Am7", "G/B", "Cadd9"), or the
      pitch classes lowest first when its confidence is under its floor.

    Spelling: the loaded tune's key signature when there is one; otherwise
    sharps, except B flat and E flat, which are always flats.
*/

#include <juce_core/juce_core.h>

namespace luthier::ChordNaming
{
    enum class Spelling { sharpsExceptBbEb, sharps, flats };

    /** A pitch class's name (0 = C). */
    juce::String spell (int pitchClass, Spelling spelling);

    /** The name for these MIDI notes (any order, duplicates allowed); empty for none. */
    juce::String nameFor (const int* midiNotes, int numNotes, Spelling spelling = Spelling::sharpsExceptBbEb);
}
