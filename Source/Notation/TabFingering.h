#pragma once

/*  Fingering a pitch-only part (tab-import-export.md section 8).

    A standard MIDI file knows pitches and nothing about strings, and a guitar
    score is not a list of pitches (notation-export 0, rule 2). So a MIDI import
    has to guess where each note was played. This is the guess, kept simple
    enough to explain:

      1. Notes are taken in time order; notes that start together are a chord
         and are fingered together.
      2. A single note goes where its fret is closest to where the hand is
         (the running average of recent fretted notes), with a preference for
         lower frets and for open strings, and a penalty for a string that is
         still sounding. One string, one note at a time.
      3. A chord is fingered lowest pitch first, lowest string first, each note
         on a free string that keeps the chord's fretted notes within a
         four-fret stretch where possible.
      4. A pitch the instrument cannot reach is clamped to the nearest string's
         open note or its highest fret, and counted, so the caller can say so.

    Deterministic, offline, message or worker thread.
*/

#include "PerformanceScore.h"

namespace luthier
{

namespace TabFingering
{
    struct Result
    {
        int notesFingered = 0;
        int notesClamped = 0;     ///< pitches outside the instrument, forced onto a string
        int stringsUsed = 0;
    };

    /** Re-fingers every note of `trackIndex` from its midiNote, using the
        track's tuning and capo. `maxFret` bounds the neck. */
    Result assign (PerformanceScore& score, int trackIndex = 0, int maxFret = 24);

    /** True when the track's existing string/fret choices already sound the
        pitches they claim (every fret within 0..maxFret of the string's open
        note): a Luthier-profile file or a per-string export needs no guess. */
    bool isPlausible (const PerformanceScore& score, int trackIndex = 0, int maxFret = 24);
}

} // namespace luthier
