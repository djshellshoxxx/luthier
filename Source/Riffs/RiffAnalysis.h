#pragma once

/*  Fills in the metadata of a user riff (riff-library.md 4).

    Key: Krumhansl-Schmuckler over duration-weighted pitch classes.
    Techniques: from the notes (Riff::computeTechniques).
    Difficulty: clamp (1..5, round (1 + nps/3 + distinctTechniques/3
                                     + max (0, fretSpan - 4)/4)),
    where nps is note onsets per second at the riff's tempo. The generator
    (Tools/generate_factory_riffs.py, estimate_difficulty) uses the same
    formula to warn about hand-set factory values.
*/

#include "Riff.h"

namespace luthier
{

namespace RiffAnalysis
{
    struct Key
    {
        int root = 0;                         ///< pitch class
        juce::String scale { "ionian" };      ///< ionian or aeolian
        double correlation = 0.0;
    };

    /** The best-matching major or minor key of the notes. Chromatic ties go
        to the lower pitch class, then to major. */
    Key detectKey (const std::vector<ScoreNote>& notes);

    int estimateDifficulty (const Riff& riff);

    /** Fills key, techniques and difficulty from the notes. Tempo, metre and
        the rest are the caller's (from the capture). */
    void analyse (Riff& riff);
}

} // namespace luthier
