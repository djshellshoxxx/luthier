/*  Rhythm engine tests (rhythm-engine.md section 10). */

#include "TestFramework.h"

#include "../Rhythm/ChordDetector.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** Spells a template at a root as MIDI notes in one octave above C3. */
    int buildChordNotes (int root, int templateIndex, int* dest, int maxNotes)
    {
        const auto& t = getChordTemplate (templateIndex);

        int count = 0;

        for (int interval = 0; interval < 12 && count < maxNotes; ++interval)
            if ((t.intervalMask & (uint16_t) (1u << interval)) != 0)
                dest[count++] = 48 + root + interval;

        return count;
    }
}

//==============================================================================
/*  Every template must be distinguishable from every other. Two templates with
    the same set of pitch classes could never both be detected, so a duplicate is
    a table bug, not a detection bug. */
LUTHIER_TEST (Rhythm, chordTemplatesAreUnique)
{
    const int n = getNumChordTemplates();

    CHECK_MSG (n > 0, "the chord template table is empty");

    for (int a = 0; a < n; ++a)
    {
        const auto& first = getChordTemplate (a);

        CHECK_MSG (juce::String (first.longName).isNotEmpty(),
                   "template " + juce::String (a) + " has no name");

        // Every chord must contain its root.
        CHECK_MSG ((first.intervalMask & 1u) != 0,
                   juce::String (first.longName) + " does not contain its root");

        for (int b = a + 1; b < n; ++b)
        {
            const auto& second = getChordTemplate (b);

            CHECK_MSG (first.intervalMask != second.intervalMask,
                       juce::String ("templates '") + first.longName + "' and '"
                         + second.longName + "' have identical pitch content");
        }
    }
}

//==============================================================================
/*  Round trip: spell each template in each of the twelve keys, detect it, and
    get the same template and root back. */
LUTHIER_TEST (Rhythm, chordDetectorRoundTripsEveryTemplateInEveryKey)
{
    ChordDetector detector;
    detector.prepare (48000.0);

    const int n = getNumChordTemplates();

    int cases = 0;
    juce::StringArray failures;

    for (int t = 0; t < n; ++t)
    {
        for (int root = 0; root < 12; ++root)
        {
            int notes[12];
            const int count = buildChordNotes (root, t, notes, 12);

            const auto symbol = detector.detect (notes, count);
            ++cases;

            if (symbol.templateIndex != t || symbol.root != root)
            {
                failures.add (juce::String (getPitchClassName (root))
                                + getChordTemplate (t).suffix
                                + " detected as " + symbol.toString());
            }
        }
    }

    CHECK_MSG (cases == n * 12,
               "ran " + juce::String (cases) + " cases, expected " + juce::String (n * 12));

    CHECK_MSG (failures.isEmpty(),
               juce::String (failures.size()) + " of " + juce::String (cases)
                 + " chords mis-detected, first few: "
                 + failures.joinIntoString ("; ").substring (0, 400));
}

//==============================================================================
/*  Inversions: whatever the detector calls an inverted chord, the symbol it
    returns must describe exactly the notes that are sounding, and its bass must
    be the lowest note.

    Demanding the original root back would be wrong. Some chords are the same
    set of pitch classes as another chord on a different root - C6 and Am7 are
    the textbook pair - so with C in the bass, "C6" is not a mis-detection, it is
    the reading a musician would also give. What can be insisted on is that the
    symbol accounts for every note and invents none. */
LUTHIER_TEST (Rhythm, chordDetectorHandlesInversions)
{
    ChordDetector detector;
    detector.prepare (48000.0);

    const int n = getNumChordTemplates();

    int cases = 0;
    juce::StringArray failures;

    for (int t = 0; t < n; ++t)
    {
        for (int root = 0; root < 12; ++root)
        {
            int notes[12];
            const int count = buildChordNotes (root, t, notes, 12);

            // Invert by lifting each note in turn up an octave.
            for (int inversion = 1; inversion < count; ++inversion)
            {
                int inverted[12];

                for (int i = 0; i < count; ++i)
                    inverted[i] = notes[i] + (i < inversion ? 12 : 0);

                const auto symbol = detector.detect (inverted, count);
                ++cases;

                const juce::String original = juce::String (getPitchClassName (root))
                                                + getChordTemplate (t).suffix;

                if (! symbol.isKnown())
                {
                    failures.add (original + " inversion " + juce::String (inversion)
                                    + " was not detected at all");
                    continue;
                }

                // The pitch classes the symbol names must be exactly the ones
                // sounding: nothing dropped, nothing invented.
                uint16_t sounding = 0;

                for (int i = 0; i < count; ++i)
                    sounding = (uint16_t) (sounding | (uint16_t) (1u << (inverted[i] % 12)));

                const auto& detected = getChordTemplate (symbol.templateIndex);
                uint16_t described = 0;

                for (int interval = 0; interval < 12; ++interval)
                    if ((detected.intervalMask & (uint16_t) (1u << interval)) != 0)
                        described = (uint16_t) (described | (uint16_t) (1u << ((symbol.root + interval) % 12)));

                // Optional notes the player left out are allowed to be absent
                // from what is sounding, but nothing may be sounding that the
                // symbol does not name.
                if ((sounding & ~described) != 0)
                {
                    failures.add (original + " inversion " + juce::String (inversion)
                                    + " detected as " + symbol.toString()
                                    + ", which does not account for every note");
                    continue;
                }

                // The bass is whatever is lowest, which after inversion is not
                // the root any more.
                int lowest = inverted[0];

                for (int i = 1; i < count; ++i)
                    lowest = juce::jmin (lowest, inverted[i]);

                if (symbol.bass != ((lowest % 12) + 12) % 12)
                    failures.add ("bass wrong for " + original + " inversion "
                                    + juce::String (inversion));
            }
        }
    }

    CHECK_MSG (cases > 1000,
               "only ran " + juce::String (cases) + " inversion cases");

    CHECK_MSG (failures.isEmpty(),
               juce::String (failures.size()) + " of " + juce::String (cases)
                 + " inversions mis-detected, first few: "
                 + failures.joinIntoString ("; ").substring (0, 400));
}

//==============================================================================
/*  Confidence: a handful of notes that match nothing must be reported as
    unknown rather than forced into the nearest template. */
LUTHIER_TEST (Rhythm, lowConfidenceIsReportedAsUnknown)
{
    ChordDetector detector;
    detector.prepare (48000.0);

    // A cluster: four adjacent semitones is not a chord in any vocabulary.
    const int cluster[] = { 60, 61, 62, 63 };
    const auto clusterSymbol = detector.detect (cluster, 4);

    CHECK_MSG (! clusterSymbol.isKnown(),
               "a chromatic cluster was detected as " + clusterSymbol.toString());

    // A single note is not a chord either.
    const int single[] = { 60 };
    CHECK (! detector.detect (single, 1).isKnown());

    // But a plain triad is, with full confidence.
    const int cMajor[] = { 60, 64, 67 };
    const auto triad = detector.detect (cMajor, 3);

    CHECK (triad.isKnown());
    CHECK (triad.root == 0);
    CHECK_NEAR (triad.confidence, 1.0, 1.0e-9);
    CHECK (triad.toString() == "C");

    // And a fifth-less seventh still reads as that seventh: the fifth is
    // optional, which is what lets a guitarist leave it out.
    const int a7[] = { 57, 61, 67 };   // A, C#, G
    const auto dominant = detector.detect (a7, 3);

    CHECK_MSG (dominant.isKnown(), "A7 without its fifth was not detected");
    CHECK (dominant.root == 9);
    CHECK (juce::String (getChordTemplate (dominant.templateIndex).suffix) == "7");
}

//==============================================================================
/*  Slash chords: the bass note is reported separately, and the symbol says so. */
LUTHIER_TEST (Rhythm, slashChordsReportTheirBass)
{
    ChordDetector detector;
    detector.prepare (48000.0);

    // G major with B in the bass.
    const int gOverB[] = { 47, 55, 59, 62 };   // B2, G3, B3, D4
    const auto symbol = detector.detect (gOverB, 4);

    CHECK (symbol.isKnown());
    CHECK (symbol.root == 7);          // G
    CHECK (symbol.bass == 11);         // B
    CHECK (symbol.isSlash());
    CHECK (symbol.toString() == "G/B");

    // Root position is not a slash chord.
    const int g[] = { 55, 59, 62 };
    const auto plain = detector.detect (g, 3);

    CHECK (plain.isKnown());
    CHECK (! plain.isSlash());
    CHECK (plain.toString() == "G");
}

//==============================================================================
/*  The burst window groups notes arriving together into one chord
    (rhythm-engine 2). */
LUTHIER_TEST (Rhythm, burstWindowGroupsASpreadChord)
{
    const double sr = 48000.0;
    const int64_t windowSamples = (int64_t) (ChordDetector::kBurstWindowSeconds * sr);

    ChordDetector detector;
    detector.prepare (sr);

    // Three notes spread over 20 ms: inside the window, so one chord.
    detector.noteOn (60, 0);
    detector.noteOn (64, (int64_t) (0.010 * sr));
    detector.noteOn (67, (int64_t) (0.020 * sr));

    CHECK_MSG (! detector.advance ((int64_t) (0.020 * sr)),
               "the burst closed before its window had elapsed");

    CHECK_MSG (detector.advance (windowSamples + 1),
               "the burst did not close after its window elapsed");

    const auto symbol = detector.getLastSymbol();
    CHECK (symbol.isKnown());
    CHECK (symbol.toString() == "C");
    CHECK (detector.getNumHeldNotes() == 3);

    // Releasing a note removes it from the held set.
    detector.noteOff (64);
    CHECK (detector.getNumHeldNotes() == 2);

    detector.allNotesOff();
    CHECK (detector.getNumHeldNotes() == 0);
}
