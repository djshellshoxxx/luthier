#pragma once

/*  Transposition and re-fretting (riff-library.md 5.2).

    Target key: a root plus an optional scale. The shift d is the smallest in
    -6..+6 that takes the riff's root to the target root (a tritone goes
    down). Scale remapping runs only when asked ("Map scale"), when both
    scales have seven notes or both are pentatonic; chromatic riffs never
    remap.

    Placement, always against the loaded guitar and by pitch:
      1. for each candidate shift in {d, d-12, d+12}, smallest |shift| first,
         move every note along its own string; accept the first where every
         fret is in 0..maxFret;
      2. otherwise, at shift d, move whole legato chains (hammer, pull,
         legato slide, trill) that do not fit to the neighbouring string
         that keeps their pitch and puts them in range, the smaller fret
         winning;
      3. drop what still does not fit, and say how many.

    A bass line on a guitar goes up an octave onto strings 3-6; a guitar part
    on a bass goes down an octave. Pure and deterministic.
*/

#include "Riff.h"

#include <array>

namespace luthier
{

/** What placement needs to know about the loaded instrument. */
struct GuitarSpecSummary
{
    int numStrings = 6;
    std::array<int, kMaxStrings> tuning { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } };   ///< highest first
    int capo = 0;
    int maxFret = 24;
    bool isBass = false;
    bool hasWhammy = true;

    int openPitch (int stringIndex) const noexcept
    {
        return tuning[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)] + capo;
    }

    bool operator== (const GuitarSpecSummary& o) const noexcept;

    /** The instrument the riff was written for, for tests and previews. */
    static GuitarSpecSummary forRiff (const Riff& riff);
};

/** Audition settings that change what is compiled (riff-library 5.1). */
struct RiffPlaySettings
{
    int targetRoot = -1;              ///< pitch class, or -1 for the riff's own key
    juce::String targetScale;         ///< empty keeps the riff's scale
    bool mapScale = false;            ///< "Map scale" (default off)

    /** The tempo seconds-based figures (trill rate, slide-in time) are laid
        out at, in bpm; <= 0 uses the riff's own tempo. */
    double nominalBpm = 0.0;

    double levelDb = 0.0;             ///< audition trim, -24..0, scales velocity

    bool operator== (const RiffPlaySettings& o) const noexcept;
};

struct RiffPlacement
{
    std::vector<ScoreNote> notes;     ///< placed: string, fret, midiNote set; startBeat absolute
    std::vector<int> sourceIndex;     ///< the riff note each placed note came from
    int shift = 0;                    ///< semitones actually applied, octave included
    int keyShift = 0;                 ///< d
    int familyOctave = 0;             ///< +12 bass on guitar, -12 guitar on bass
    int dropped = 0;
    bool movedChains = false;
    bool retuned = false;             ///< a string's fret changed to keep a pitch
    juce::StringArray notices;
    int targetRoot = 0;
    juce::String targetScale;
};

namespace RiffTransposer
{
    /** The smallest shift in -6..+6 from one pitch class to another. */
    int keyShift (int fromRoot, int toRoot) noexcept;

    /** True when a scale can be remapped into the other (both heptatonic or
        both pentatonic, neither chromatic). */
    bool canMapScales (const juce::String& from, const juce::String& to);

    /** One pitch moved by d and, when the scales map, re-degreed: a scale
        note keeps its degree; any other keeps its offset from the nearest
        degree below it. `rootPitchClass` is the riff's root before the shift. */
    int mapPitch (int pitch, int rootPitchClass, int d, const juce::String& fromScale,
                  const juce::String& toScale, bool mapScale);

    RiffPlacement place (const Riff& riff, const RiffPlaySettings& settings, const GuitarSpecSummary& guitar);
}

} // namespace luthier
