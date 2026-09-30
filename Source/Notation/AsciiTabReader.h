#pragma once

/*  The ASCII tab reader (tab-import-export.md sections 1, 4 and 7).

    ASCII tab has no grammar: every site, every teacher and every export tool
    spells it a little differently, and a real file is a mixture of a title,
    chord names over lyrics, a tuning line, and staff systems in one of several
    dialects. The reader's job is to take the best guess a guitarist would
    take, never to reject a page because one line of it was odd.

    What comes out of it besides the score is a TabImportDiagnostics: how much
    of the page was read, what was skipped and why, so the tab reader panel can
    say "loaded 6 of 8 bars, 3 lines skipped" instead of "ok" or "failed".

    Worker or message thread. Nothing here is for the audio thread.
*/

#include "PerformanceScore.h"

namespace luthier
{

//==============================================================================
/** What the reader did with the page (tab-import-export 7). */
struct TabImportDiagnostics
{
    int totalLines = 0;        ///< non-empty lines in the input
    int staffLines = 0;        ///< lines read as strings of a system
    int headerLines = 0;       ///< tuning / capo / tempo / time lines used
    int annotationLines = 0;   ///< PM / let-ring / bend-amount lines above a staff (used or skipped)
    int skippedLines = 0;      ///< non-empty lines that were neither (titles, chords, lyrics, junk)
    int systems = 0;           ///< staff blocks read
    int measures = 0;          ///< measures produced, after unrolling repeats
    int notes = 0;
    int numStrings = 0;
    int repeatsUnrolled = 0;   ///< repeat sections / systems that were expanded
    int ignoredGlyphs = 0;     ///< characters inside a staff that no dialect explains
    int splitFrets = 0;        ///< digit pairs above fret 24 read as two notes

    bool tuningFromHeader = false;      ///< a "Tuning:" line named or listed it
    bool tuningFromStringNames = false; ///< inferred from the note names in front of the strings
    bool tempoFromHeader = false;
    bool timeSignatureFromHeader = false;

    /** Human-readable notes, one per event, capped so a huge file does not
        produce a huge list. "line 12: skipped (chord names)". */
    juce::StringArray warnings;

    /** True when anything was skipped, ignored or guessed beyond the tuning. */
    bool isPartial() const noexcept;

    /** One line for a status label: "Loaded 4 bars, 17 notes (6 strings); 2 lines skipped". */
    juce::String summary() const;
};

//==============================================================================
class AsciiTabReader
{
public:
    /** Parses the text into `destination` (cleared first). Returns true when
        at least one note was read; the diagnostics say how complete that was.
        Never throws, never crashes on any input. */
    bool read (const juce::String& text, PerformanceScore& destination,
               TabImportDiagnostics* diagnostics = nullptr);

    juce::String getLastError() const { return lastError; }

    /** The MIDI notes of a tuning written as note names ("E A D G B E",
        "DADGAD", "Eb Ab Db Gb Bb Eb", "E2 A2 D3 G3 B3 E4"), highest string
        first, using the octave guess tab-import-export 7.2 describes. False
        when the text is not a note list. `lowToHigh` says which way the list
        reads; the reader guesses from the case of the first and last names. */
    static bool parseTuningNames (const juce::String& text, std::vector<int>& midiHighFirst,
                                  bool lowToHigh = true);

private:
    juce::String lastError;
};

} // namespace luthier
