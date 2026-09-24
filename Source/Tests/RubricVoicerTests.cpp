/*  Rubric voicer tests (ambiguity-resolutions.md 4.7, and every term of 4.1 - 4.6).

    The 4.7 cases are here as the spec lists them, and so is a direct check of
    each constraint, weight, style bias and tie-break, because a voicer can pass
    "every chord gives something playable" with its weights all wrong.

    Fingerings are written the way a chord chart writes them, lowest string
    first: "x 3 2 0 1 0" is open C. The voicer's own arrays run the other way,
    string 0 the highest, and chart() does the turning round.
*/

#include "TestFramework.h"

#include "../Model/Playing/RubricVoicer.h"
#include "../Rhythm/ChordDetector.h"
#include "../Model/Guitar/GuitarLibrary.h"

#include <algorithm>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** Every imperfection off, so each fret is exactly a semitone. */
    void makeStandard (TuningEngine& t)
    {
        t.prepare (kSr);
        t.setTuningPreset (TuningPreset::Standard);
        t.randomiseRealismDetune (0.0, 1);

        for (int s = 0; s < 6; ++s)
            t.setIntonationSlope (s, 0.0);
    }

    /** 4.7's "J-bass tuned BEAD": four strings, B0 E1 A1 D2, twenty frets. */
    void makeBead (TuningEngine& t)
    {
        t.prepare (kSr);
        t.setTuningPreset (TuningPreset::BassStandard);
        t.randomiseRealismDetune (0.0, 1);

        const double open[] = { 73.416, 55.000, 41.203, 30.868 };   // string 0 = D2

        for (int s = 0; s < 4; ++s)
        {
            t.setOpenFrequency (s, open[s]);
            t.setIntonationSlope (s, 0.0);
            t.setMaxFrets (s, GuitarLibrary::get (GuitarType::JazzBass).maxFrets);
        }
    }

    struct Fixture
    {
        Fixture()
        {
            makeStandard (tuning);
            voicer.prepare (&tuning, 6);
            voicer.setPitchMode (RubricPitchMode::chordTones);
        }

        TuningEngine tuning;
        RubricVoicer voicer;
    };

    /** A chord chart, lowest string first, as a fret per string with string 0 the
        highest. "x" is muted. */
    std::array<int, kMaxStrings> chart (const char* lowToHigh)
    {
        std::array<int, kMaxStrings> frets;
        frets.fill (-1);

        const auto tokens = juce::StringArray::fromTokens (lowToHigh, " ", "");
        const int n = tokens.size();

        for (int i = 0; i < n; ++i)
            frets[(size_t) (n - 1 - i)] = (tokens[i] == "x") ? -1 : tokens[i].getIntValue();

        return frets;
    }

    /** The fingering a voicing uses, as a chart, for failure messages. */
    juce::String chartOf (const ChordVoicing& v, int numStrings)
    {
        std::array<int, kMaxStrings> frets;
        frets.fill (-1);

        for (int i = 0; i < v.numNotes; ++i)
            if (v.notes[(size_t) i].valid && juce::isPositiveAndBelow (v.notes[(size_t) i].stringIndex, kMaxStrings))
                frets[(size_t) v.notes[(size_t) i].stringIndex] = (int) v.notes[(size_t) i].fretPosition;

        juce::StringArray tokens;

        for (int s = numStrings - 1; s >= 0; --s)
            tokens.add (frets[(size_t) s] < 0 ? juce::String ("x") : juce::String (frets[(size_t) s]));

        return tokens.joinIntoString (" ");
    }

    /** A voicing for a fingering, to seed 4.4's previous chord. */
    ChordVoicing voicingFromChart (const TuningEngine& t, const char* lowToHigh)
    {
        const auto frets = chart (lowToHigh);
        ChordVoicing v;

        for (int s = 0; s < t.getNumStrings(); ++s)
        {
            if (frets[(size_t) s] < 0)
                continue;

            auto& note = v.notes[(size_t) v.numNotes++];
            note.stringIndex = s;
            note.fretPosition = (double) frets[(size_t) s];
            note.midiNote = (int) std::round (hzToMidi (t.computeFrequency (s, note.fretPosition), t.getConcertA()));
            note.valid = true;
        }

        v.playable = v.numNotes > 0;
        return v;
    }

    int buildChordNotes (int root, int templateIndex, int* dest, int maxNotes)
    {
        const auto& t = getChordTemplate (templateIndex);

        int count = 0;

        for (int interval = 0; interval < 12 && count < maxNotes; ++interval)
            if ((t.intervalMask & (uint16_t) (1u << interval)) != 0)
                dest[count++] = 48 + root + interval;

        return count;
    }

    uint16_t chordMaskOf (const int* notes, int count)
    {
        uint16_t mask = 0;

        for (int i = 0; i < count; ++i)
            mask = (uint16_t) (mask | (1u << (((notes[i] % 12) + 12) % 12)));

        return mask;
    }

    /** Where the fretting hand sits: the lowest fretted fret. An all-open chord
        puts it nowhere, which is reported as -1. */
    int handPositionOf (const ChordVoicing& v)
    {
        int lowest = -1;

        for (int i = 0; i < v.numNotes; ++i)
        {
            const int f = (int) v.notes[(size_t) i].fretPosition;

            if (v.notes[(size_t) i].valid && f > 0 && (lowest < 0 || f < lowest))
                lowest = f;
        }

        return lowest;
    }

    double centsApart (double a, double b)
    {
        return 1200.0 * std::log2 (a / b);
    }

    /** Everything about a voicing that is physics rather than taste, checked
        without the voicer's help: one note per string, every fret on the neck
        and within the span, every pitch the one that string and fret make, every
        note a chord tone, the lowest string on the pitch class it must be, and
        no adjacent crossing wider than 4.1 allows. Empty when it all holds.
        A 12-string's octave strings cross their partners by design, so the
        crossing check is optional. */
    juce::String physicalProblem (const TuningEngine& t, const ChordVoicing& v, int numStrings,
                                  int maxFret, int maxSpan, uint16_t chordMask, int lowestPcMustBe,
                                  bool checkCrossings = true)
    {
        std::array<int, kMaxStrings> pitchOn;
        pitchOn.fill (-1);

        int lo = 1000, hi = -1;

        for (int i = 0; i < v.numNotes; ++i)
        {
            const auto& note = v.notes[(size_t) i];

            if (! note.valid)
                continue;

            const int s = note.stringIndex;

            if (! juce::isPositiveAndBelow (s, numStrings))
                return "a note on string " + juce::String (s);

            if (pitchOn[(size_t) s] >= 0)
                return "two notes on string " + juce::String (s);

            const int f = (int) note.fretPosition;

            if (note.fretPosition != (double) f || f < 0 || f > maxFret)
                return "fret " + juce::String (note.fretPosition, 2) + " on string " + juce::String (s);

            const double hz = t.computeFrequency (s, (double) f, 0.0);

            if (std::abs (centsApart (hz, midiToHz ((double) note.midiNote, t.getConcertA()))) > 5.0)
                return "string " + juce::String (s) + " fret " + juce::String (f)
                         + " does not sound MIDI " + juce::String (note.midiNote);

            if (((chordMask >> (note.midiNote % 12)) & 1u) == 0)
                return "MIDI " + juce::String (note.midiNote) + " is not a chord tone";

            if (f > 0)
            {
                lo = juce::jmin (lo, f);
                hi = juce::jmax (hi, f);
            }

            pitchOn[(size_t) s] = note.midiNote;
        }

        if (hi >= 0 && hi - lo > maxSpan)
            return "a span of " + juce::String (hi - lo) + " frets";

        int below = -1;

        for (int s = numStrings - 1; s >= 0; --s)
        {
            const int pitch = pitchOn[(size_t) s];

            if (pitch < 0)
                continue;

            if (below < 0 && lowestPcMustBe >= 0 && pitch % 12 != lowestPcMustBe)
                return "the lowest string sounds pitch class " + juce::String (pitch % 12);

            if (checkCrossings && below >= 0 && pitch < below - RubricWeights::maxInversionSemitones)
                return "string " + juce::String (s) + " crosses below its neighbour";

            below = pitch;
        }

        return {};
    }

    juce::MemoryBlock serialise (const ChordVoicing& v)
    {
        juce::MemoryOutputStream out;

        out.writeInt (v.numNotes);

        for (int i = 0; i < v.numNotes; ++i)
        {
            const auto& note = v.notes[(size_t) i];
            out.writeInt (note.midiNote);
            out.writeInt (note.stringIndex);
            out.writeDouble (note.fretPosition);
            out.writeDouble (note.velocity);
            out.writeBool (note.valid);
        }

        out.writeInt (v.lowestFret);
        out.writeInt (v.highestFret);
        out.writeInt (v.fretSpan);
        out.writeBool (v.requiresBarre);
        out.writeBool (v.playable);
        out.writeInt (v.droppedNotes);

        return out.getMemoryBlock();
    }

    const int kCMajor[]    = { 48, 52, 55 };
    const int kFMajor[]    = { 53, 57, 60 };
    const int kC7[]        = { 48, 52, 55, 58 };
    const int kCMaj7[]     = { 48, 52, 55, 59 };
    const int kEMajor[]    = { 40, 47, 52, 56, 59, 64 };
}

//==============================================================================
/*  4.2 - 4.4 name their weights. These are them. */
LUTHIER_TEST (RubricVoicer, weightsAreTheSpecsWeights)
{
    using W = RubricWeights;

    CHECK (W::openBonus == 3.0);
    CHECK (W::handMovePenalty == 0.4);
    CHECK (W::barrePenalty == 2.0);
    CHECK (W::mutePenalty == 4.0);
    CHECK (W::dupNotePenalty == 1.0);
    CHECK (W::extensionDroppedPenalty == 3.0);

    CHECK (W::openStyleOpenString == 6.0);
    CHECK (W::openStyleRootLow == 3.0);
    CHECK (W::openStyleBarre == -2.0);
    CHECK (W::barreStyleBarre == 5.0);
    CHECK (W::barreStyleRootLowest == 2.0);
    CHECK (W::triadMatch == 6.0);
    CHECK (W::triadMiss == -3.0);
    CHECK (W::shellMatch == 7.0);
    CHECK (W::shellMiss == -5.0);
    CHECK (W::dropMatch == 5.0);
    CHECK (W::powerMatch == 8.0);
    CHECK (W::powerMiss == -6.0);
    CHECK (W::rootlessMatch == 6.0);
    CHECK (W::rootlessMiss == -6.0);
    CHECK (W::wideMatch == 4.0);
    CHECK (W::bassMatch == 8.0);
    CHECK (W::bassWalkingApproach == 2.0);
    CHECK (W::bassWiderThanOctave == -6.0);

    CHECK (W::commonNote == 2.0);
    CHECK (W::commonPosition == 1.0);
    CHECK (W::positionJump == -2.0);
    CHECK (W::positionJumpFrets == 5);

    CHECK (W::maxInversionSemitones == 4);
    CHECK (W::minBarreStrings == 3);

    // rhythm-engine 3: hand span default 5, user range 3-7.
    RubricVoicer v;
    CHECK (v.getMaxFretSpan() == 5);
    v.setMaxFretSpan (1);
    CHECK (v.getMaxFretSpan() == 3);
    v.setMaxFretSpan (9);
    CHECK (v.getMaxFretSpan() == 7);
}

//==============================================================================
/*  4.2, term by term, on fingerings worked out by hand. */
LUTHIER_TEST (RubricVoicer, eachScoreTermIsTheRubrics)
{
    Fixture f;

    // Open C, x32010, no previous chord, hand hint 0.
    {
        const auto frets = chart ("x 3 2 0 1 0");
        const auto s = f.voicer.evaluate (frets.data(), kCMajor, 3);

        CHECK_MSG (s.valid, "open C failed constraint " + juce::String (s.failedConstraint));
        CHECK (s.openStrings == 2);
        CHECK (s.mutedStrings == 1);
        CHECK (s.duplicates == 2);          // C3 + C4, E3 + E4
        CHECK (s.droppedExtensions == 0);
        CHECK (! s.usesBarre);
        CHECK (s.handPosition == 1);

        CHECK_NEAR (s.openTerm, 6.0, 1.0e-9);        // 3 x 2 open strings
        CHECK_NEAR (s.muteTerm, -4.0, 1.0e-9);       // 4 x 1 muted
        CHECK_NEAR (s.styleTerm, 9.0, 1.0e-9);       // Open: +6 open string, +3 root on string 5
        CHECK_NEAR (s.handMoveTerm, -0.4, 1.0e-9);   // |1 - 0| x 0.4
        CHECK_NEAR (s.barreTerm, 0.0, 1.0e-9);
        CHECK_NEAR (s.duplicateTerm, -2.0, 1.0e-9);
        CHECK_NEAR (s.droppedTerm, 0.0, 1.0e-9);
        CHECK_NEAR (s.transitionTerm, 0.0, 1.0e-9);
        CHECK_NEAR (s.total, 8.6, 1.0e-9);
        CHECK (s.totalTenths == 86);
    }

    // The hand hint moves only the hand term: |1 - 5| x 0.4.
    {
        f.voicer.setPreferredPosition (5);
        const auto frets = chart ("x 3 2 0 1 0");
        const auto s = f.voicer.evaluate (frets.data(), kCMajor, 3);

        CHECK_NEAR (s.handMoveTerm, -1.6, 1.0e-9);
        CHECK_NEAR (s.total, 7.4, 1.0e-9);
        f.voicer.setPreferredPosition (0);
    }

    // C3 and G3 only: the third is dropped (-3), the fifth never is.
    {
        const auto frets = chart ("x 3 x 0 x x");
        const auto s = f.voicer.evaluate (frets.data(), kCMajor, 3);

        CHECK (s.valid);
        CHECK (s.droppedExtensions == 1);
        CHECK_NEAR (s.droppedTerm, -3.0, 1.0e-9);
        CHECK_NEAR (s.muteTerm, -16.0, 1.0e-9);
        CHECK_NEAR (s.openTerm, 3.0, 1.0e-9);
        CHECK_NEAR (s.handMoveTerm, -1.2, 1.0e-9);
        CHECK_NEAR (s.total, 3.0 - 16.0 + 9.0 - 1.2 - 3.0, 1.0e-9);
    }

    {
        const auto frets = chart ("x 3 2 x 1 x");   // C3 E3 C4, no G
        const auto s = f.voicer.evaluate (frets.data(), kCMajor, 3);

        CHECK (s.valid);
        CHECK_MSG (s.droppedExtensions == 0, "leaving out the perfect fifth cost a dropped extension");
        CHECK (s.duplicates == 1);
    }

    // The E-shape F barre in the Barre style: -2 barre, +5 barre used, +2 root on string 6.
    {
        f.voicer.setStyle (RubricStyle::barre);
        const auto frets = chart ("1 3 3 2 1 1");
        const auto s = f.voicer.evaluate (frets.data(), kFMajor, 3);

        CHECK_MSG (s.valid, "the F barre failed constraint " + juce::String (s.failedConstraint));
        CHECK (s.usesBarre);
        CHECK (s.barreFret == 1);
        CHECK (s.handPosition == 1);
        CHECK_NEAR (s.barreTerm, -2.0, 1.0e-9);
        CHECK_NEAR (s.styleTerm, 7.0, 1.0e-9);
        CHECK_NEAR (s.handMoveTerm, -0.4, 1.0e-9);
        CHECK_NEAR (s.duplicateTerm, -3.0, 1.0e-9);
        CHECK_NEAR (s.total, -2.0 + 7.0 - 0.4 - 3.0, 1.0e-9);
        f.voicer.setStyle (RubricStyle::open);
    }
}

//==============================================================================
/*  4.1: each constraint, broken on its own, rejects the fingering, and says
    which one it was. */
LUTHIER_TEST (RubricVoicer, everyConstraintOfFourOneRejects)
{
    Fixture f;

    auto failure = [&f] (const char* lowToHigh, const int* notes, int count)
    {
        const auto frets = chart (lowToHigh);
        return f.voicer.evaluate (frets.data(), notes, count).failedConstraint;
    };

    CHECK (failure ("x 3 2 0 1 0", kCMajor, 3) == 0);

    // 1: a fret past guitar.max_fret.
    f.voicer.setMaxFret (12);
    CHECK (failure ("x 15 x 0 x x", kCMajor, 3) == 1);
    f.voicer.setMaxFret (22);

    // 2: C3 at A3 and C5 at e8 is five frets; the hand here spans four.
    f.voicer.setMaxFretSpan (4);
    CHECK (failure ("x 3 x x x 8", kCMajor, 3) == 2);
    f.voicer.setMaxFretSpan (5);
    CHECK (failure ("x 3 x x x 8", kCMajor, 3) == 0);

    // 3: G2 on the lowest string is neither the root nor the bass.
    CHECK (failure ("3 3 2 0 1 0", kCMajor, 3) == 3);

    // 4: C4 on the A string, then the open G3 above it - five semitones down.
    CHECK (failure ("x 15 x 0 x x", kCMajor, 3) == 4);

    // 5: five fretted notes need a barre, and fret 3 holds only one string.
    CHECK (failure ("x 3 5 5 5 8", kCMajor, 3) == 5);

    // 6: a barre at 8 across all six, and four notes above it for three fingers.
    CHECK (failure ("8 10 10 9 13 8", kCMajor, 3) == 6);

    // 6: the barre F with the A string open under the barre.
    CHECK (failure ("1 0 3 2 1 1", kFMajor, 3) == 6);

    // Not a voicing of the chord at all: the open B is not in C major.
    CHECK (failure ("x 3 2 0 0 0", kCMajor, 3) == RubricScore::kNotThisChord);
}

//==============================================================================
/*  4.3: each style's bias, on a fingering that earns it and one that does not. */
LUTHIER_TEST (RubricVoicer, styleBiasesAreFourThrees)
{
    Fixture f;

    auto bias = [&f] (RubricStyle style, const char* lowToHigh, const int* notes, int count, int root = -1)
    {
        f.voicer.setStyle (style);
        f.voicer.setRootPitchClass (root);

        const auto frets = chart (lowToHigh);
        const auto s = f.voicer.evaluate (frets.data(), notes, count);

        return s.valid ? s.styleTerm : -1000.0 - s.failedConstraint;
    };

    CHECK_NEAR (bias (RubricStyle::open, "x 3 2 0 1 0", kCMajor, 3), 9.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::open, "1 3 3 2 1 1", kFMajor, 3), 3.0 - 2.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::barre, "1 3 3 2 1 1", kFMajor, 3), 7.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::barre, "x 3 2 0 1 0", kCMajor, 3), 0.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::triad, "x x x 5 5 3", kCMajor, 3), 6.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::triad, "x 3 2 0 x x", kCMajor, 3), -3.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::shell, "x 3 2 3 x x", kC7, 4), 7.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::shell, "x 3 2 x x x", kC7, 4), -5.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::drop2, "x 3 5 4 5 x", kCMaj7, 4), 5.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::drop2, "x 3 2 0 0 x", kCMaj7, 4), 0.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::drop3, "8 x 9 9 8 x", kCMaj7, 4), 5.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::drop3, "x 3 5 4 5 x", kCMaj7, 4), 0.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::power, "x 3 5 5 x x", kCMajor, 3), 8.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::power, "x 3 x x x x", kCMajor, 3), -6.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::rootless, "x x 2 3 x x", kC7, 4, 0), 6.0, 1.0e-9);
    const int loneC[] = { 48 };
    CHECK_NEAR (bias (RubricStyle::rootless, "x 3 x x x x", loneC, 1, 0), -6.0, 1.0e-9);

    CHECK_NEAR (bias (RubricStyle::wide, "0 2 2 1 0 0", kEMajor, 6), 4.0, 1.0e-9);
    CHECK_NEAR (bias (RubricStyle::wide, "x 3 2 0 1 0", kCMajor, 3), 0.0, 1.0e-9);
}

//==============================================================================
/*  4.4: +2 a common note, +1 a common finger position, -2 a jump of more than
    five frets. */
LUTHIER_TEST (RubricVoicer, transitionBonusCountsNotesPositionsAndJumps)
{
    Fixture f;
    f.voicer.setPreviousVoicing (voicingFromChart (f.tuning, "x 3 2 0 1 0"));
    f.voicer.setPreferredPosition (1);

    {
        // F after C: C4 is common; the D string moves 2 -> 3 and the B string
        // stays at 1, both within a fret.
        const auto frets = chart ("1 0 3 2 1 x");
        const auto s = f.voicer.evaluate (frets.data(), kFMajor, 3);

        CHECK_MSG (s.valid, "failed constraint " + juce::String (s.failedConstraint));
        CHECK (s.commonNotes == 1);
        CHECK (s.commonPositions == 2);
        CHECK (s.positionJumps == 0);
        CHECK_NEAR (s.transitionTerm, 4.0, 1.0e-9);
    }

    {
        // C at the eighth fret: C4 and E4 are common, and the D and B strings
        // jump eight and seven frets.
        const auto frets = chart ("x x 10 9 8 8");
        const auto s = f.voicer.evaluate (frets.data(), kCMajor, 3);

        CHECK_MSG (s.valid, "failed constraint " + juce::String (s.failedConstraint));
        CHECK (s.commonNotes == 2);
        CHECK (s.commonPositions == 0);
        CHECK (s.positionJumps == 2);
        CHECK_NEAR (s.transitionTerm, 0.0, 1.0e-9);
    }

    // With no previous chord there is nothing to be close to.
    f.voicer.clearPreviousVoicing();

    const auto frets = chart ("1 0 3 2 1 x");
    CHECK_NEAR (f.voicer.evaluate (frets.data(), kFMajor, 3).transitionTerm, 0.0, 1.0e-9);

    // voice() records its own result as the next chord's previous voicing.
    f.voicer.voice (kCMajor, nullptr, 3);
    CHECK (f.voicer.hasPreviousVoicing());
}

//==============================================================================
/*  4.6: ties go to more strings, then the lower fret sum. */
LUTHIER_TEST (RubricVoicer, tiesBreakByStringCountThenFretSum)
{
    RubricVoicer::RankKey a, b;
    a.numSlots = b.numSlots = 6;

    a.scoreTenths = 10;  b.scoreTenths = 9;
    CHECK (RubricVoicer::ranksAbove (a, b));
    CHECK (! RubricVoicer::ranksAbove (b, a));

    // Equal scores: more strings wins, whatever the fret sums.
    b.scoreTenths = 10;
    a.strings = 5;  a.fretSum = 30;
    b.strings = 4;  b.fretSum = 3;
    CHECK (RubricVoicer::ranksAbove (a, b));
    CHECK (! RubricVoicer::ranksAbove (b, a));

    // Equal strings: the lower fret sum wins.
    b.strings = 5;  b.fretSum = 6;
    CHECK (RubricVoicer::ranksAbove (b, a));
    CHECK (! RubricVoicer::ranksAbove (a, b));

    // Identical keys rank neither way.
    CHECK (! RubricVoicer::ranksAbove (a, a));

    // And through the search: on BEAD with the hand at 2, a lone C at B-1 and at
    // A-3 both score -4.4 (one fret from the hint either way). B-1 has the
    // lower fret sum.
    TuningEngine t;
    makeBead (t);

    RubricVoicer v;
    v.prepare (&t, 4);
    v.setMaxFret (20);
    v.setPitchMode (RubricPitchMode::chordTones);
    v.setStyle (RubricStyle::bass);
    v.setBassPattern (RubricBassPattern::root);
    v.setRootPitchClass (0);
    v.setPreferredPosition (2);

    const auto low = chart ("1 x x x");
    const auto high = chart ("x x 3 x");
    const auto lowScore = v.evaluate (low.data(), kC7, 4);
    const auto highScore = v.evaluate (high.data(), kC7, 4);

    CHECK (lowScore.valid && highScore.valid);
    CHECK_MSG (lowScore.totalTenths == highScore.totalTenths,
               "expected a tie, got " + juce::String (lowScore.total, 1) + " and " + juce::String (highScore.total, 1));

    const auto voicing = v.voice (kC7, nullptr, 4);

    CHECK_MSG (chartOf (voicing, 4) == "1 x x x",
               "the tie went to " + chartOf (voicing, 4) + ", not the lower fret sum");
}

//==============================================================================
/*  4.7: "All 84 chord templates in every ship key, every style: valid voicing or
    explicit unplayable."

    The detector has fewer than 84 templates - ChordDetector.cpp explains why -
    so this runs every one it has. */
LUTHIER_TEST (RubricVoicer, everyTemplateInEveryKeyAndStyleIsVoicedOrUnplayable)
{
    Fixture f;

    struct Variant { RubricStyle style; RubricBassPattern pattern; int cap; };

    const Variant variants[] =
    {
        { RubricStyle::open,     RubricBassPattern::root,      6 },
        { RubricStyle::barre,    RubricBassPattern::root,      6 },
        { RubricStyle::triad,    RubricBassPattern::root,      3 },
        { RubricStyle::shell,    RubricBassPattern::root,      3 },
        { RubricStyle::drop2,    RubricBassPattern::root,      4 },
        { RubricStyle::drop3,    RubricBassPattern::root,      4 },
        { RubricStyle::power,    RubricBassPattern::root,      3 },
        { RubricStyle::rootless, RubricBassPattern::root,      6 },
        { RubricStyle::wide,     RubricBassPattern::root,      6 },
        { RubricStyle::bass,     RubricBassPattern::root,      2 },
        { RubricStyle::bass,     RubricBassPattern::rootFifth, 2 },
    };

    const int numTemplates = getNumChordTemplates();

    int cases = 0, voiced = 0, unplayable = 0, truncatedSimple = 0;
    juce::StringArray failures;

    for (const auto& variant : variants)
    {
        f.voicer.setStyle (variant.style);
        f.voicer.setBassPattern (variant.pattern);

        for (int t = 0; t < numTemplates; ++t)
        {
            for (int root = 0; root < 12; ++root)
            {
                int notes[12];
                const int count = buildChordNotes (root, t, notes, 12);

                f.voicer.reset();
                f.voicer.setRootPitchClass (root);

                const auto v = f.voicer.voice (notes, nullptr, count);
                const auto outcome = f.voicer.getLastOutcome();
                ++cases;

                const juce::String name = juce::String (getPitchClassName (root)) + getChordTemplate (t).suffix
                                            + " (" + juce::String ((int) variant.style) + ")";

                if (v.numNotes == 0)
                {
                    ++unplayable;

                    if (v.playable || outcome != RubricOutcome::unplayable)
                        failures.add (name + ": empty but not reported unplayable");

                    continue;
                }

                ++voiced;

                if (! v.playable || outcome != RubricOutcome::voiced || ! f.voicer.getLastScore().valid)
                    failures.add (name + ": voiced but not reported playable");

                if (v.numNotes > variant.cap)
                    failures.add (name + ": " + juce::String (v.numNotes) + " notes for a cap of "
                                    + juce::String (variant.cap));

                // Rootless may put any chord tone lowest, and must never sound the root.
                const bool rootless = variant.style == RubricStyle::rootless;
                const auto problem = physicalProblem (f.tuning, v, 6, 22, f.voicer.getMaxFretSpan(),
                                                      chordMaskOf (notes, count), rootless ? -1 : root);

                if (problem.isNotEmpty())
                    failures.add (name + " " + chartOf (v, 6) + ": " + problem);

                if (rootless)
                    for (int i = 0; i < v.numNotes; ++i)
                        if (v.notes[(size_t) i].midiNote % 12 == root && count > 1)
                            failures.add (name + " " + chartOf (v, 6) + ": rootless sounds the root");

                // Triads and sevenths are the everyday chords; they must never
                // need the node budget.
                if (getChordTemplate (t).noteCount <= 4 && f.voicer.wasLastSearchTruncated())
                    ++truncatedSimple;
            }
        }
    }

    CHECK_MSG (cases == numTemplates * 12 * (int) (sizeof (variants) / sizeof (variants[0])),
               "ran " + juce::String (cases) + " cases");

    CHECK_MSG (failures.isEmpty(),
               juce::String (failures.size()) + " bad results, first few: "
                 + failures.joinIntoString ("; ").substring (0, 600));

    // On a standard six-string the root alone is always playable, so nothing
    // should come back unplayable here.
    CHECK_MSG (unplayable == 0, juce::String (unplayable) + " of " + juce::String (cases) + " were unplayable");

    CHECK_MSG (truncatedSimple == 0,
               juce::String (truncatedSimple) + " searches for chords of four notes or fewer hit the node budget");
}

//==============================================================================
/*  rhythm-engine 10's version of the same promise, on every ship guitar and in
    both pitch modes: never a fingering no hand could make. */
LUTHIER_TEST (RubricVoicer, everyShipGuitarVoicesEveryTemplate)
{
    const int numTemplates = getNumChordTemplates();

    for (const auto mode : { RubricPitchMode::chordTones, RubricPitchMode::exact })
    {
        int cases = 0, voiced = 0;
        juce::StringArray failures;

        for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
        {
            const auto spec = GuitarLibrary::get ((GuitarType) g);

            TuningEngine tuning;
            tuning.prepare (kSr);
            tuning.setNumStrings (spec.numStrings);
            tuning.setTuningPreset (spec.tuning);

            for (int s = 0; s < spec.numStrings; ++s)
                tuning.setMaxFrets (s, spec.maxFrets);

            RubricVoicer voicer;
            voicer.prepare (&tuning, spec.numStrings);
            voicer.setMaxFret (spec.maxFrets);
            voicer.setPitchMode (mode);
            voicer.setStyle (spec.category == GuitarCategory::Bass ? RubricStyle::bass : RubricStyle::open);

            for (int t = 0; t < numTemplates; ++t)
            {
                for (int root = 0; root < 12; ++root)
                {
                    int notes[12];
                    const int count = buildChordNotes (root, t, notes, 12);

                    const auto v = voicer.voice (notes, nullptr, count);
                    ++cases;

                    if (v.numNotes > 0)
                        ++voiced;
                    else if (voicer.getLastOutcome() != RubricOutcome::unplayable)
                        failures.add (juce::String (spec.name) + ": empty and not reported unplayable");

                    uint32_t used = 0;

                    for (int i = 0; i < v.numNotes; ++i)
                    {
                        const auto& note = v.notes[(size_t) i];

                        if (! note.valid)
                            continue;

                        if (! juce::isPositiveAndBelow (note.stringIndex, spec.numStrings))
                        {
                            failures.add (juce::String (spec.name) + ": note on string " + juce::String (note.stringIndex));
                            continue;
                        }

                        if (note.fretPosition < 0.0 || note.fretPosition > (double) spec.maxFrets)
                            failures.add (juce::String (spec.name) + ": note at fret " + juce::String (note.fretPosition, 1));

                        const auto bit = 1u << note.stringIndex;

                        if ((used & bit) != 0)
                            failures.add (juce::String (spec.name) + ": two notes on string " + juce::String (note.stringIndex));

                        used |= bit;
                    }
                }
            }
        }

        const juce::String modeName = (mode == RubricPitchMode::exact) ? "exact" : "chord tones";

        CHECK_MSG (failures.isEmpty(),
                   modeName + ": " + juce::String (failures.size()) + " invalid voicings, first few: "
                     + failures.joinIntoString ("; ").substring (0, 400));

        const double fraction = (double) voiced / (double) juce::jmax (1, cases);

        CHECK_MSG (fraction > 0.9,
                   modeName + ": only " + juce::String (fraction * 100.0, 1) + "% of chords were voiced");
    }
}

//==============================================================================
/*  4.7: "I-IV-V-I in C on standard tuning open style, hand travels no more than
    3 frets between chords." */
LUTHIER_TEST (RubricVoicer, oneFourFiveOneTravelsThreeFretsAtMost)
{
    Fixture f;
    f.voicer.setStyle (RubricStyle::open);

    const int chords[4][3] = { { 48, 52, 55 }, { 53, 57, 60 }, { 55, 59, 62 }, { 48, 52, 55 } };
    const int roots[4] = { 0, 5, 7, 0 };
    const char* names[4] = { "C", "F", "G", "C" };

    int lastPosition = -1;

    for (int i = 0; i < 4; ++i)
    {
        f.voicer.setRootPitchClass (roots[i]);
        const auto v = f.voicer.voice (chords[i], nullptr, 3);

        CHECK_MSG (v.numNotes > 0, juce::String (names[i]) + " was not voiced");

        const auto problem = physicalProblem (f.tuning, v, 6, 22, 5, chordMaskOf (chords[i], 3), roots[i]);
        CHECK_MSG (problem.isEmpty(), juce::String (names[i]) + " " + chartOf (v, 6) + ": " + problem);

        // The first C is the rubric's own answer, pinned so a change to it is
        // seen: 8-3-5-0-5-0 mutes nothing, and 4.2's mute penalty (4) outweighs
        // what open C (x32010) gains elsewhere (DECISIONS "Rubric unisons").
        if (i == 0)
            CHECK_MSG (chartOf (v, 6) == "8 3 5 0 5 0", "the first C was " + chartOf (v, 6));

        const int position = handPositionOf (v);

        if (position >= 0)
        {
            if (lastPosition >= 0)
                CHECK_MSG (std::abs (position - lastPosition) <= 3,
                           juce::String (names[i - 1]) + " -> " + names[i] + " moved the hand from fret "
                             + juce::String (lastPosition) + " to " + juce::String (position)
                             + " (" + chartOf (v, 6) + ")");

            lastPosition = position;
        }
    }
}

//==============================================================================
/*  4.6 / 4.7: "fixed inputs produce byte-identical voicing across runs". */
LUTHIER_TEST (RubricVoicer, fixedInputsGiveByteIdenticalVoicings)
{
    auto run = []
    {
        Fixture f;
        juce::MemoryBlock all;

        const RubricStyle styles[] = { RubricStyle::open, RubricStyle::barre, RubricStyle::triad,
                                       RubricStyle::shell, RubricStyle::drop2, RubricStyle::drop3,
                                       RubricStyle::power, RubricStyle::rootless, RubricStyle::wide };

        const int numTemplates = getNumChordTemplates();

        for (const auto style : styles)
        {
            f.voicer.setStyle (style);

            // A progression rather than isolated chords, so the previous voicing
            // and the hand position carried between calls are part of the input.
            for (int step = 0; step < 24; ++step)
            {
                int notes[12];
                const int root = (step * 5) % 12;
                const int count = buildChordNotes (root, (step * 7) % numTemplates, notes, 12);

                f.voicer.setRootPitchClass (root);
                const auto block = serialise (f.voicer.voice (notes, nullptr, count));
                all.append (block.getData(), block.getSize());
            }
        }

        return all;
    };

    const auto first = run();
    const auto second = run();

    CHECK_MSG (first.getSize() > 0, "nothing was voiced");
    CHECK_MSG (first == second, "two identical runs produced different voicings");

    // reset() must return a voicer to exactly the state prepare() left it in.
    Fixture f;
    const auto before = serialise (f.voicer.voice (kC7, nullptr, 4));
    f.voicer.voice (kFMajor, nullptr, 3);
    f.voicer.reset();
    const auto after = serialise (f.voicer.voice (kC7, nullptr, 4));

    CHECK_MSG (before == after, "a reset voicer voiced C7 differently from a fresh one");
}

//==============================================================================
/*  4.7: "Bass mode: on a J-bass tuned BEAD, a C7 symbol produces C on E-2nd fret
    only in the Root style, root + fifth in the Root-Fifth style."

    The E string's second fret is F#1, not a C, on BEAD or any other tuning, so
    the spec's position cannot be what it meant. The rubric puts the lone C at
    B-1 (C1): every single C scores -12 for three muted strings and +8 for the
    shape, and B-1 is nearest the hand. Root-Fifth adds G1 at E-3. */
LUTHIER_TEST (RubricVoicer, bassGivesRootOrRootAndFifthOnBead)
{
    TuningEngine t;
    makeBead (t);

    RubricVoicer v;
    v.prepare (&t, 4);
    v.setMaxFret (20);
    v.setPitchMode (RubricPitchMode::chordTones);
    v.setStyle (RubricStyle::bass);
    v.setRootPitchClass (0);

    // ---- Root ---------------------------------------------------------------------
    v.setBassPattern (RubricBassPattern::root);
    const auto root = v.voice (kC7, nullptr, 4);

    CHECK_MSG (root.numNotes == 1, "Root voiced " + juce::String (root.numNotes) + " notes: " + chartOf (root, 4));

    if (root.numNotes == 1)
    {
        CHECK (root.notes[0].midiNote % 12 == 0);
        CHECK_MSG (root.notes[0].stringIndex == 3 && (int) root.notes[0].fretPosition == 1,
                   "the C was at " + chartOf (root, 4));
    }

    CHECK_NEAR (v.getLastScore().styleTerm, 8.0, 1.0e-9);

    // ---- Root-Fifth -------------------------------------------------------------------
    v.reset();
    v.setBassPattern (RubricBassPattern::rootFifth);
    const auto rootFifth = v.voice (kC7, nullptr, 4);

    CHECK_MSG (rootFifth.numNotes == 2,
               "Root-Fifth voiced " + juce::String (rootFifth.numNotes) + " notes: " + chartOf (rootFifth, 4));

    if (rootFifth.numNotes == 2)
    {
        int lowNote = rootFifth.notes[0].midiNote, highNote = rootFifth.notes[1].midiNote;

        if (lowNote > highNote)
            std::swap (lowNote, highNote);

        CHECK_MSG (lowNote % 12 == 0 && highNote % 12 == 7,
                   "expected C and G, got MIDI " + juce::String (lowNote) + " and " + juce::String (highNote));
        CHECK_MSG (highNote - lowNote <= 12, "the fifth is more than an octave above the root");
        CHECK_MSG (chartOf (rootFifth, 4) == "1 3 x x", "Root-Fifth was " + chartOf (rootFifth, 4));
    }

    CHECK_NEAR (v.getLastScore().styleTerm, 8.0, 1.0e-9);

    // ---- the other two Bass terms, directly -----------------------------------------------
    // Wider than an octave: C1 with G2 on the D string.
    v.reset();
    {
        const auto frets = chart ("1 x x 5");
        const auto s = v.evaluate (frets.data(), kC7, 4);
        CHECK_MSG (s.valid, "failed constraint " + juce::String (s.failedConstraint));
        CHECK_NEAR (s.styleTerm, 8.0 - 6.0, 1.0e-9);
    }

    // A walking approach: after C1, D1 at B-3 arrives by a whole step.
    v.setBassPattern (RubricBassPattern::root);
    v.setPreviousVoicing (voicingFromChart (t, "1 x x x"));
    {
        const int d7[] = { 38, 42, 45, 48 };
        v.setRootPitchClass (2);

        const auto frets = chart ("3 x x x");
        const auto s = v.evaluate (frets.data(), d7, 4);
        CHECK_MSG (s.valid, "failed constraint " + juce::String (s.failedConstraint));
        CHECK_NEAR (s.styleTerm, 8.0 + 2.0, 1.0e-9);
    }
}

//==============================================================================
/*  Exact mode is MidiInterpreter's: the player's pitches, placed playably. The
    same chords ChordVoicer::commonChordsAreVoicedPlayably holds it to. */
LUTHIER_TEST (RubricVoicer, exactModeKeepsThePlayersPitches)
{
    TuningEngine t;
    makeStandard (t);

    RubricVoicer v;
    v.prepare (&t, 6);
    v.setMaxFretSpan (4);
    v.setMaxFret (22);

    struct Chord { const char* name; std::vector<int> notes; };

    const std::vector<Chord> chords =
    {
        { "E major",   { 40, 47, 52, 56, 59, 64 } },
        { "A minor",   { 45, 52, 57, 60, 64 } },
        { "C major",   { 48, 52, 55, 60, 64 } },
        { "G major",   { 43, 47, 50, 55, 59, 67 } },
        { "D major",   { 50, 57, 62, 66 } },
        { "F major",   { 41, 48, 53, 57, 60, 65 } },
        { "B minor 7", { 47, 54, 57, 62, 66 } },
        { "E7",        { 40, 47, 52, 56, 59, 62 } },
        { "D minor",   { 50, 57, 62, 65 } },
        { "G7",        { 43, 47, 50, 55, 59, 65 } },
    };

    for (const auto& chord : chords)
    {
        v.reset();
        const auto voicing = v.voice (chord.notes.data(), nullptr, (int) chord.notes.size());

        CHECK_MSG (voicing.numNotes > 0, juce::String (chord.name) + " produced no notes");
        CHECK_MSG (voicing.fretSpan <= 4, juce::String (chord.name) + " spans " + juce::String (voicing.fretSpan));
        CHECK_MSG (voicing.droppedNotes <= 1,
                   juce::String (chord.name) + " dropped " + juce::String (voicing.droppedNotes) + " notes");

        const auto problem = physicalProblem (t, voicing, 6, 22, 4,
                                              chordMaskOf (chord.notes.data(), (int) chord.notes.size()),
                                              chord.notes.front() % 12);
        CHECK_MSG (problem.isEmpty(), juce::String (chord.name) + " " + chartOf (voicing, 6) + ": " + problem);

        for (int i = 0; i < voicing.numNotes; ++i)
        {
            const int pitch = voicing.notes[(size_t) i].midiNote;
            CHECK_MSG (std::find (chord.notes.begin(), chord.notes.end(), pitch) != chord.notes.end(),
                       juce::String (chord.name) + " sounded MIDI " + juce::String (pitch) + ", which was not played");
        }
    }

    // The open E major is placed exactly as a guitarist plays it.
    v.reset();
    const auto e = v.voice (kEMajor, nullptr, 6);
    CHECK_MSG (chartOf (e, 6) == "0 2 2 1 0 0", "E major was " + chartOf (e, 6));

    // A note below every string is left out; the rest of the chord still sounds.
    v.reset();
    const int withSubsonic[] = { 20, 48, 52, 55 };
    const auto partial = v.voice (withSubsonic, nullptr, 4);
    CHECK (partial.numNotes == 3);
    CHECK (partial.droppedNotes == 1);
}

//==============================================================================
/*  4.5: "Open strings are the capo'd notes", and nothing is fretted behind a
    capo, full or partial. */
LUTHIER_TEST (RubricVoicer, theCapoIsWhereTheNeckStarts)
{
    // Full capo at 5: the open strings are A D G C E A.
    {
        Fixture f;
        f.tuning.setCapoFret (5);
        f.voicer.setRootPitchClass (9);

        const int aMajor[] = { 57, 61, 64 };
        const auto v = f.voicer.voice (aMajor, nullptr, 3);

        CHECK_MSG (v.numNotes > 0, "A major under a capo at 5 was not voiced");

        const auto problem = physicalProblem (f.tuning, v, 6, 24 - 5, 5, chordMaskOf (aMajor, 3), 9);
        CHECK_MSG (problem.isEmpty(), chartOf (v, 6) + ": " + problem);

        int open = 0;

        for (int i = 0; i < v.numNotes; ++i)
            if (v.notes[(size_t) i].fretPosition == 0.0)
                ++open;

        CHECK_MSG (open > 0, "the capo'd strings were not used as open strings: " + chartOf (v, 6));
    }

    // Partial capo at 2 on the A, D and G strings.
    {
        Fixture f;
        f.tuning.setCapoFret (2);
        f.tuning.setCapoStringMask ((1u << 2) | (1u << 3) | (1u << 4));

        const auto v = f.voicer.voice (kEMajor, nullptr, 6);

        CHECK_MSG (v.numNotes > 0, "E major under a partial capo was not voiced");

        for (int i = 0; i < v.numNotes; ++i)
        {
            const auto& note = v.notes[(size_t) i];
            const bool clamped = f.tuning.getCapoFretFor (note.stringIndex) > 0;

            // An unclamped string's frets count from the nut; fret 1 or 2 there
            // would be behind the capo.
            if (! clamped && note.fretPosition > 0.0)
                CHECK_MSG (note.fretPosition > 2.0,
                           "string " + juce::String (note.stringIndex) + " fretted behind the capo: " + chartOf (v, 6));

            const double hz = f.tuning.computeFrequency (note.stringIndex, note.fretPosition, 0.0);
            CHECK_MSG (std::abs (centsApart (hz, midiToHz ((double) note.midiNote))) < 5.0,
                       "string " + juce::String (note.stringIndex) + " does not sound MIDI "
                         + juce::String (note.midiNote));
        }
    }
}

//==============================================================================
/*  4.7: when nothing passes, the answer is "unplayable", said out loud. */
LUTHIER_TEST (RubricVoicer, nothingPlayableIsReportedUnplayable)
{
    // Exact mode, a pitch below every string.
    {
        TuningEngine t;
        makeStandard (t);

        RubricVoicer v;
        v.prepare (&t, 6);

        const int tooLow[] = { 5 };
        const auto r = v.voice (tooLow, nullptr, 1);

        CHECK (r.numNotes == 0);
        CHECK (! r.playable);
        CHECK (v.getLastOutcome() == RubricOutcome::unplayable);

        // Nothing asked is not the same as nothing playable.
        v.voice (nullptr, nullptr, 0);
        CHECK (v.getLastOutcome() == RubricOutcome::none);
    }

    // A capo at the end of the neck leaves six open strings, E B G D A E, and
    // not one of them is in C# major.
    {
        Fixture f;
        f.tuning.setCapoFret (24);

        const int cSharp[] = { 49, 53, 56 };
        const auto r = f.voicer.voice (cSharp, nullptr, 3);

        CHECK (r.numNotes == 0);
        CHECK (! r.playable);
        CHECK (f.voicer.getLastOutcome() == RubricOutcome::unplayable);
        CHECK_MSG (! f.voicer.hasPreviousVoicing(), "an unplayable chord became the previous voicing");
    }
}

//==============================================================================
/*  A 12-string is six courses: a finger stops both strings of a pair, so every
    voiced string has its partner at the same fret. */
LUTHIER_TEST (RubricVoicer, twelveStringVoicesCourses)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.setNumStrings (12);
    t.randomiseRealismDetune (0.0, 1);

    double base[6];
    TuningEngine::getPresetFrequencies (TuningPreset::Standard, base, 6);

    for (int s = 0; s < 12; ++s)
    {
        t.setOpenFrequency (s, base[GuitarLibrary::courseForString (s)]
                                 * semitonesToRatio (GuitarLibrary::twelveStringOctaveOffset (s)));
        t.setIntonationSlope (s, 0.0);
        t.setMaxFrets (s, 20);
    }

    RubricVoicer v;
    v.prepare (&t, 12);
    v.setMaxFret (20);
    v.setPitchMode (RubricPitchMode::chordTones);

    const auto voicing = v.voice (kCMajor, nullptr, 3);

    CHECK_MSG (voicing.numNotes > 0 && voicing.numNotes % 2 == 0,
               juce::String (voicing.numNotes) + " notes on a 12-string");

    std::array<int, kMaxStrings> fretOn;
    fretOn.fill (-1);

    for (int i = 0; i < voicing.numNotes; ++i)
        fretOn[(size_t) voicing.notes[(size_t) i].stringIndex] = (int) voicing.notes[(size_t) i].fretPosition;

    for (int course = 0; course < 6; ++course)
        CHECK_MSG (fretOn[(size_t) (course * 2)] == fretOn[(size_t) (course * 2 + 1)],
                   "course " + juce::String (course) + " has its strings at frets "
                     + juce::String (fretOn[(size_t) (course * 2)]) + " and "
                     + juce::String (fretOn[(size_t) (course * 2 + 1)]));

    const auto problem = physicalProblem (t, voicing, 12, 20, 5, chordMaskOf (kCMajor, 3), -1, false);
    CHECK_MSG (problem.isEmpty(), problem);
}
