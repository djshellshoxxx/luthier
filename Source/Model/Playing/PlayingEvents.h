#pragma once

/*  The typed events that flow from the MIDI interpreter into the string engines.

    Everything here is a fixed-size POD in a fixed-capacity queue: the whole chain
    from MidiBuffer to excitation runs on the audio thread and must never allocate.
*/

#include "../../DSP/Common/DspCommon.h"

namespace luthier
{

//==============================================================================
enum class Technique
{
    Pluck,
    HammerOn,
    PullOff,
    Slide,
    Bend,
    Vibrato,
    PalmMute,
    MutedPick,
    NaturalHarmonic,
    PinchHarmonic,
    ArtificialHarmonic,
    Tap,
    SlideGuitar,
    Strum,
    NumTechniques
};

const char* getTechniqueName (Technique t) noexcept;

//==============================================================================
enum class PlayingMode
{
    Mono,             ///< One string at a time; legato between notes.
    Poly,             ///< Chord voicing engine assigns strings.
    GuitarController, ///< MIDI channel = string, or MPE.
    NumModes
};

enum class StrumDirection { Down, Up, Alternate, NumDirections };

//==============================================================================
struct NoteOnEvent
{
    int    stringIndex   = 0;
    int    midiNote      = 60;
    int    midiChannel   = 1;
    double fretPosition  = 0.0;
    double pitchHz       = 261.626;
    double velocity      = 0.8;
    Technique technique  = Technique::Pluck;
    int    harmonicPartial = 0;
    int    sampleOffset  = 0;
    double slideFromFret = -1.0;   ///< >= 0 means glide from here.
    double slideSeconds  = 0.0;

    /** strum-dynamics 6.1: how much of a chuck this strike is - 0 ordinary, 1
        the fretting hand flat across the strings. */
    double chuck         = 0.0;

    /** strum-dynamics 5: what struck the string, as an Excitation::Material
        index, or -1 for the player's own pick (the PICK group). */
    int    strikerMaterial = -1;

    /** bass-techniques 9 (MODEL-GAPS): the technique the bass step grid (or an
        imported BASS_TECH event) names for this note, as a BassStepType index;
        -1 lets the slap's own triggers decide. Inert on a guitar. */
    int    bassTechnique = -1;
};

struct NoteOffEvent
{
    int stringIndex  = 0;
    int midiNote     = 60;
    int sampleOffset = 0;
    bool letRing     = false;
};

struct BendEvent
{
    int    stringIndex  = -1;      ///< -1 = all strings.
    double cents        = 0.0;
    int    sampleOffset = 0;
};

struct PressureEvent
{
    int    stringIndex  = -1;
    double value        = 0.0;
    int    sampleOffset = 0;
};

struct ControlEvent
{
    int ccNumber     = 0;
    double value     = 0.0;
    int sampleOffset = 0;
};

//==============================================================================
/** Fixed-capacity, allocation-free event queue for one block. */
class PlayEventQueue
{
public:
    static constexpr int kCapacity = 192;

    void clear() noexcept
    {
        numNoteOns = 0;
        numNoteOffs = 0;
        numBends = 0;
        numPressures = 0;
        numControls = 0;
        overflowed = false;
    }

    bool addNoteOn (const NoteOnEvent& e) noexcept
    {
        if (numNoteOns >= kCapacity) { overflowed = true; return false; }
        noteOns[numNoteOns++] = e;
        return true;
    }

    bool addNoteOff (const NoteOffEvent& e) noexcept
    {
        if (numNoteOffs >= kCapacity) { overflowed = true; return false; }
        noteOffs[numNoteOffs++] = e;
        return true;
    }

    bool addBend (const BendEvent& e) noexcept
    {
        if (numBends >= kCapacity) { overflowed = true; return false; }
        bends[numBends++] = e;
        return true;
    }

    bool addPressure (const PressureEvent& e) noexcept
    {
        if (numPressures >= kCapacity) { overflowed = true; return false; }
        pressures[numPressures++] = e;
        return true;
    }

    bool addControl (const ControlEvent& e) noexcept
    {
        if (numControls >= kCapacity) { overflowed = true; return false; }
        controls[numControls++] = e;
        return true;
    }

    int getNumNoteOns() const noexcept   { return numNoteOns; }
    int getNumNoteOffs() const noexcept  { return numNoteOffs; }
    int getNumBends() const noexcept     { return numBends; }
    int getNumPressures() const noexcept { return numPressures; }
    int getNumControls() const noexcept  { return numControls; }

    const NoteOnEvent&   getNoteOn (int i) const noexcept   { return noteOns[juce::jlimit (0, kCapacity - 1, i)]; }
    const NoteOffEvent&  getNoteOff (int i) const noexcept  { return noteOffs[juce::jlimit (0, kCapacity - 1, i)]; }
    const BendEvent&     getBend (int i) const noexcept     { return bends[juce::jlimit (0, kCapacity - 1, i)]; }
    const PressureEvent& getPressure (int i) const noexcept { return pressures[juce::jlimit (0, kCapacity - 1, i)]; }
    const ControlEvent&  getControl (int i) const noexcept  { return controls[juce::jlimit (0, kCapacity - 1, i)]; }

    NoteOnEvent& getMutableNoteOn (int i) noexcept { return noteOns[juce::jlimit (0, kCapacity - 1, i)]; }

    /** True if the block produced more events than the queue can hold. Surfaces in
        the diagnostics panel rather than failing silently. */
    bool didOverflow() const noexcept { return overflowed; }

private:
    NoteOnEvent   noteOns[kCapacity];
    NoteOffEvent  noteOffs[kCapacity];
    BendEvent     bends[kCapacity];
    PressureEvent pressures[kCapacity];
    ControlEvent  controls[kCapacity];

    int numNoteOns = 0, numNoteOffs = 0, numBends = 0, numPressures = 0, numControls = 0;
    bool overflowed = false;
};

} // namespace luthier
