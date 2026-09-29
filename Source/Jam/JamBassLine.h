#pragma once

/*  The bass line's pitch choice (jam-mode.md 3.1 and 6.2).

    Tokens resolve against the chord's ChordTemplate::intervalMask:
      R  the root (the bass note of a slash chord)
      3  the template's third, or R without one
      5  the fifth (b5 or #5 when the template has one)
      7  the template's seventh, or the octave
      8  the octave
      A  an approach note - a semitone below or a scale step above the next
         root - when the next chord is anticipated; otherwise R
      W  a walking step toward the next root
      m  a muted ghost on the note already sounding

    The register is E1-C3. Each note takes the octave nearest the previous
    note, with the root kept within E1-A2. Nothing here is random: the same
    chords and tokens give the same line.
*/

#include "../Rhythm/ChordDetector.h"

namespace luthier
{

class JamBassLine
{
public:
    static constexpr int kLowest = 28;      ///< E1
    static constexpr int kHighest = 48;     ///< C3
    static constexpr int kRootHighest = 45; ///< A2

    void reset() noexcept { previous = -1; walkIndex = 0; }

    /** Resolves a token to a MIDI note, or -1 for none. `next` is the next
        chord when it is known (anticipated), else an unknown symbol;
        `walkStepsLeft` counts the W / A tokens before the next bar line
        including this one. */
    int resolve (char token, const ChordSymbol& chord, const ChordSymbol& next, int walkStepsLeft) noexcept;

    /** The pitch class a chord-tone token names (R 3 5 7 8), or -1. */
    static int pitchClassFor (char token, const ChordSymbol& chord) noexcept;

    int getPrevious() const noexcept { return previous; }
    void setPrevious (int note) noexcept { previous = note; }

    /** The note of pitch class `pc` in [lo, hi] nearest `reference`. */
    static int nearest (int pc, int reference, int lo, int hi) noexcept;

private:
    int previous = -1;
    int walkIndex = 0;
};

} // namespace luthier
