/*  Easter-egg: the Dueling Banjos motif detector.

    The detector must fire on the recognisable opening phrase played in any key
    (it matches relative intervals) and with a little human slop (an extra or a
    wrong note), yet must NOT fire on ordinary playing - a false trigger in the
    middle of a song would be a bad surprise. These tests pin both halves: the
    positive cases that have to trigger, and the negative cases that must not.
*/

#include "TestFramework.h"

#include "../Model/Playing/MotifDetector.h"

#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    // The reference phrase as absolute MIDI pitches from a root: the major
    // pentatonic climb to the octave, degrees 1 2 3 5 6 8.
    std::vector<int> motifFrom (int root)
    {
        return { root, root + 2, root + 4, root + 7, root + 9, root + 12 };
    }

    // Feeds a pitch sequence as one phrase (notes a comfortable 0.25 s apart, well
    // inside the reset gap) and reports whether the motif fired. `sampleRate`
    // matches what prepare() was given.
    bool playsAsMotif (MotifDetector& d, const std::vector<int>& notes,
                       double sampleRate = 48000.0)
    {
        const int64_t step = (int64_t) (0.25 * sampleRate);
        int64_t t = 1000;
        bool fired = false;
        for (int n : notes)
        {
            d.noteOn (n, t);
            fired = fired || d.isTriggered();
            t += step;
        }
        return fired;
    }

    MotifDetector makeDetector (double sampleRate = 48000.0)
    {
        MotifDetector d;
        d.prepare (sampleRate);
        return d;
    }
}

//==============================================================================
// Positive cases: these must trigger.

LUTHIER_TEST (MotifDetector, triggersOnExactPhraseInG)
{
    auto d = makeDetector();
    CHECK (playsAsMotif (d, motifFrom (55)));   // G3
    CHECK (d.consumeTrigger());                 // the latch is set
    CHECK (! d.consumeTrigger());               // and consumed exactly once
}

LUTHIER_TEST (MotifDetector, triggersInAnyKey)
{
    // Transposition invariance: the same intervals in several keys/registers.
    for (int root : { 36, 48, 50, 60, 67, 72 })
    {
        auto d = makeDetector();
        CHECK_MSG (playsAsMotif (d, motifFrom (root)),
                   juce::String ("motif should trigger from root ") + juce::String (root));
    }
}

LUTHIER_TEST (MotifDetector, toleratesAnExtraRepeatedNote)
{
    // A double-picked note (the 3rd degree played twice) is a natural slip and
    // should still trigger: it inserts a zero interval the matcher can absorb.
    auto d = makeDetector();
    std::vector<int> notes { 55, 57, 59, 59, 62, 64, 67 };
    CHECK (playsAsMotif (d, notes));
}

LUTHIER_TEST (MotifDetector, toleratesOneWrongNote)
{
    // The final note landed a semitone sharp: one wrong interval, still within
    // the one-edit tolerance.
    auto d = makeDetector();
    std::vector<int> notes { 55, 57, 59, 62, 64, 68 };   // last is +4 not +3
    CHECK (playsAsMotif (d, notes));
}

LUTHIER_TEST (MotifDetector, toleratesDroppedPickupNote)
{
    // Omitting the opening pick-up note still leaves the recognisable climb.
    auto d = makeDetector();
    std::vector<int> notes { 57, 59, 62, 64, 67 };   // motifFrom(55) without the first
    CHECK (playsAsMotif (d, notes));
}

//==============================================================================
// Negative cases: these must NOT trigger (false-positive resistance).

LUTHIER_TEST (MotifDetector, ignoresPlainMajorScale)
{
    // A diatonic run (whole whole half whole whole) differs from the pentatonic
    // phrase by two edits, so a player warming up on a scale is safe.
    auto d = makeDetector();
    std::vector<int> scale { 60, 62, 64, 65, 67, 69, 71, 72 };
    CHECK (! playsAsMotif (d, scale));
}

LUTHIER_TEST (MotifDetector, ignoresChromaticRun)
{
    auto d = makeDetector();
    std::vector<int> chromatic { 60, 61, 62, 63, 64, 65, 66, 67 };
    CHECK (! playsAsMotif (d, chromatic));
}

LUTHIER_TEST (MotifDetector, ignoresRepeatedChordStabs)
{
    // An arpeggiated C major chord, repeated: nothing like the phrase.
    auto d = makeDetector();
    std::vector<int> chord { 48, 52, 55, 48, 52, 55, 48, 52 };
    CHECK (! playsAsMotif (d, chord));
}

LUTHIER_TEST (MotifDetector, ignoresRandomMelody)
{
    auto d = makeDetector();
    std::vector<int> random { 60, 67, 61, 70, 55, 66, 72, 58, 63 };
    CHECK (! playsAsMotif (d, random));
}

//==============================================================================
// Timing / phrasing behaviour.

LUTHIER_TEST (MotifDetector, longGapBreaksThePhrase)
{
    // The phrase split by a long pause must not assemble into a trigger.
    auto d = makeDetector();
    const double sr = 48000.0;
    const int64_t step = (int64_t) (0.25 * sr);
    const int64_t bigGap = (int64_t) (3.0 * sr);   // well past the reset gap

    auto notes = motifFrom (55);
    int64_t t = 1000;
    bool fired = false;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        d.noteOn (notes[i], t);
        fired = fired || d.isTriggered();
        t += (i == 2 ? bigGap : step);   // drop a long silence mid-phrase
    }
    CHECK (! fired);
}

LUTHIER_TEST (MotifDetector, retriggersOnASecondPerformance)
{
    // After a reset the player can summon the banjo again.
    auto d = makeDetector();
    CHECK (playsAsMotif (d, motifFrom (55)));
    CHECK (d.consumeTrigger());

    d.reset();
    CHECK (playsAsMotif (d, motifFrom (60)));
    CHECK (d.consumeTrigger());
}
