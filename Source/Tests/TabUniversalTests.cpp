// Universal tab player: tuning / capo / key headers in every convention, key
// detection, chord names and strumming lines over a staff, chord charts with
// diagrams, strumming and picking patterns, Nashville numbers, and the strums
// the riff conversion derives for playback.
#include "TestFramework.h"
#include "../Notation/AsciiTabReader.h"
#include "../Notation/TabImportPipeline.h"
#include "../Notation/TabChordChart.h"
#include "../Notation/TabKeyDetector.h"
#include "../Riffs/Riff.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Flat
    {
        double beat = 0.0;
        int stringIndex = 0, fret = 0, midi = 0;
        std::vector<ScoreTechnique> techniques;
        bool has (ScoreTechnique::Type t) const
        {
            for (const auto& x : techniques) if (x.type == t) return true;
            return false;
        }
    };

    std::vector<Flat> flatten (const PerformanceScore& score)
    {
        std::vector<Flat> out;
        double barStart = 0.0;
        for (const auto& measure : score.getTrack (0).measures)
        {
            for (const auto& voice : measure.voices)
                for (const auto& n : voice.notes)
                    out.push_back ({ barStart + n.startBeat, n.stringIndex, n.fret, n.midiNote, n.techniques });
            barStart += measure.timeSignatureNumerator * 4.0 / juce::jmax (1, measure.timeSignatureDenominator);
        }
        std::stable_sort (out.begin(), out.end(), [] (const Flat& a, const Flat& b)
        {
            return a.beat < b.beat - 1.0e-9 || (std::abs (a.beat - b.beat) < 1.0e-9 && a.stringIndex < b.stringIndex);
        });
        return out;
    }

    /** Notes grouped by onset, in time order. */
    std::vector<std::vector<Flat>> groups (const PerformanceScore& score)
    {
        std::vector<std::vector<Flat>> out;
        for (const auto& f : flatten (score))
        {
            if (out.empty() || std::abs (out.back().front().beat - f.beat) > 1.0e-6)
                out.push_back ({});
            out.back().push_back (f);
        }
        return out;
    }

    juce::File sourceSibling (const char* relativeToThisFile)
    {
        const juce::String self (__FILE__);
        auto resolve = [&] (const juce::File& base) { return base.getChildFile (self).getSiblingFile (relativeToThisFile); };

        auto candidate = juce::File::isAbsolutePath (self) ? juce::File (self).getSiblingFile (relativeToThisFile)
                                                          : resolve (juce::File::getCurrentWorkingDirectory());
        if (candidate.exists())
            return candidate;

        for (auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
             dir != dir.getParentDirectory(); dir = dir.getParentDirectory())
        {
            candidate = resolve (dir);
            if (candidate.exists())
                return candidate;
        }
        return juce::File (self).getSiblingFile (relativeToThisFile);
    }

    juce::File corpus (const char* name)
    {
        return sourceSibling ((juce::String ("Fixtures/TabCorpus/") + name).toRawUTF8());
    }

    bool readFixture (const char* name, PerformanceScore& score, TabImportDiagnostics* d = nullptr, juce::String* error = nullptr)
    {
        TabImportPipeline pipeline;
        const bool ok = pipeline.read (corpus (name), score, d);
        if (error != nullptr) *error = pipeline.getLastError();
        return ok;
    }

    std::vector<int> tuningOf (const PerformanceScore& score)
    {
        const auto& t = score.getTrack (0);
        return std::vector<int> (t.tuning.begin(), t.tuning.begin() + juce::jlimit (0, kMaxStrings, t.numStrings));
    }

    juce::String chordsOf (const PerformanceScore& score)
    {
        juce::StringArray names;
        for (const auto& m : score.getTrack (0).measures)
            for (const auto& c : m.chordSymbols)
                names.add (c.second);
        return names.joinIntoString (" ");
    }
}

//==============================================================================
LUTHIER_TEST (TabUniversal, tuningStatementsInEveryConvention)
{
    struct Case { const char* line; std::vector<int> expected; const char* name; };
    const Case cases[] = {
        { "Tuning: Drop D",                          { 64, 59, 55, 50, 45, 38 }, "Drop D" },
        { "Tuning: Drop D, half step down",          { 63, 58, 54, 49, 44, 37 }, "Drop D, half step down" },
        { "Tuning: Drop D, 1 1/2 steps down",        { 61, 56, 52, 47, 42, 35 }, "" },
        { "Tuned down one and a half steps",         { 61, 56, 52, 47, 42, 37 }, "C# Standard" },
        { "Tuning: 2 steps down",                    { 60, 55, 51, 46, 41, 36 }, "C Standard" },
        { "Tuning: down 3 semitones",                { 61, 56, 52, 47, 42, 37 }, "C# Standard" },
        { "Tuning: Eb",                              { 63, 58, 54, 49, 44, 39 }, "Eb Standard" },
        { "Tuning: D",                               { 62, 57, 53, 48, 43, 38 }, "D Standard" },
        { "Stimmung: E A D G H E",                   { 64, 59, 55, 50, 45, 40 }, "" },
        { "Afinacion: Mi La Re Sol Si Mi",           {},                         "" },   // solfege lists are not read (documented)
        { "Tuning: E A D G B E (half step down)",    { 63, 58, 54, 49, 44, 39 }, "" },
        { "Tuning: Standard (E A D G B e)",          { 64, 59, 55, 50, 45, 40 }, "Standard" },
        { "Tuning: baritone",                        { 59, 54, 50, 45, 40, 35 }, "Baritone" },
        { "Tuning: DADF#AD",                         { 62, 57, 54, 50, 45, 38 }, "" },
        { "Tuning: Open G",                          { 62, 59, 55, 50, 43, 38 }, "Open G" },
        { "Tuning: 5 string bass",                   { 43, 38, 33, 28, 23 },     "5-string bass" },
        { "Tuning: 8-string, whole step down",       { 62, 57, 53, 48, 43, 38, 33, 28 }, "" },
        { "Tuning: E2 A2 D3 G3 B3 E4",               { 64, 59, 55, 50, 45, 40 }, "" },
        { "Tuning: Ukulele",                         { 69, 64, 60, 67 },         "Ukulele" },
    };

    for (const auto& c : cases)
    {
        std::vector<int> midi;
        juce::String name;
        const bool ok = AsciiTabReader::parseTuningStatement (c.line, midi, name);
        CHECK_MSG (ok == ! c.expected.empty(), c.line);
        if (ok)
        {
            CHECK_MSG (midi == c.expected, juce::String (c.line) + " -> " + name);
            if (c.name[0] != '\0')
                CHECK_MSG (name == c.name, juce::String (c.line) + " named " + name);

            // The canonical name re-parses to the same list (the normalizer re-feeds it).
            std::vector<int> again;
            juce::String nameAgain;
            CHECK_MSG (AsciiTabReader::parseTuningStatement ("Tuning: " + name, again, nameAgain) && again == midi,
                       juce::String ("re-parse of ") + name);
        }
    }

    // German H and a bare B in the same list: B is Bb.
    std::vector<int> midi;
    CHECK (AsciiTabReader::parseTuningNames ("E A D G H E", midi, true) && (midi == std::vector<int> { 64, 59, 55, 50, 45, 40 }));
    CHECK (AsciiTabReader::parseTuningNames ("Eb Ab Db Gb B Eb", midi, true) && midi[1] == 59);   // no H: B is B
}

LUTHIER_TEST (TabUniversal, keyStatementsAndNames)
{
    struct Case { const char* line; bool ok; int root; bool minor; };
    const Case cases[] = {
        { "Key: Am", true, 9, true },       { "Key of G", true, 7, false },     { "Key - F#m", true, 6, true },
        { "Key: Bb major", true, 10, false }, { "Tonart: A-Moll", true, 9, true }, { "Tonalidad: Sol menor", true, 7, true },
        { "KEY: Eb", true, 3, false },       { "Key = C#m", true, 1, true },     { "the key is in the drawer", false, 0, false },
        { "Keyboard part", false, 0, false }, { "Key: Everything", false, 0, false },
    };
    for (const auto& c : cases)
    {
        int root = -1; bool minor = false;
        const bool ok = AsciiTabReader::parseKeyStatement (c.line, root, minor);
        CHECK_MSG (ok == c.ok, c.line);
        if (ok && c.ok)
        {
            CHECK_MSG (root == c.root, juce::String (c.line) + " root " + juce::String (root));
            CHECK_MSG (minor == c.minor, c.line);
        }
    }
    CHECK (AsciiTabReader::keyName (9, true) == "Am");
    CHECK (AsciiTabReader::keyName (10, false) == "Bb");
    CHECK (AsciiTabReader::keyName (6, true) == "F#m");

    int root = -1; bool minor = false;
    CHECK (TabKeyDetector::parseKeyName ("F#m", root, minor) && root == 6 && minor);
}

LUTHIER_TEST (TabUniversal, keyIsEstimatedFromNotesAndChordsUnlessDeclared)
{
    // A C-major profile is C, an A-minor-shaped one (A emphasised) is Am.
    double cMajor[12] = { 5, 0, 2, 0, 3, 2, 0, 4, 0, 2, 0, 1 };
    CHECK (TabKeyDetector::estimateFromProfile (cMajor).name == "C");
    double aMinor[12] = { 3, 0, 2, 0, 4, 1, 0, 2, 1, 6, 0, 1 };
    CHECK (TabKeyDetector::estimateFromProfile (aMinor).name == "Am");
    double nothing[12] = {};
    CHECK (TabKeyDetector::estimateFromProfile (nothing).rootPitchClass < 0);

    PerformanceScore score;
    CHECK (readFixture ("key_estimate_am.txt", score));
    CHECK_MSG (score.getMeta().key == "Am", score.getMeta().key);

    // A declared key wins over the estimate, in any phrasing.
    CHECK (readFixture ("baritone_key_header.txt", score));
    CHECK_MSG (score.getMeta().key == "Am", score.getMeta().key);
    CHECK ((tuningOf (score) == std::vector<int> { 59, 54, 50, 45, 40, 35 }));
    CHECK (score.getMeta().tuningName == "Baritone");

    // Chord symbols vote: a G-C-D-G chart is in G.
    TabImportPipeline pipeline;
    CHECK (pipeline.read ("G  C  D  G\nG  C  D  G\n", score));
    CHECK_MSG (score.getMeta().key == "G", score.getMeta().key);

    // The riff carries the key root for the compiler.
    CHECK (Riff::fromScore (score).keyRoot == "G");
}

LUTHIER_TEST (TabUniversal, headersGermanHStepsDownRomanCapoAndNoteValueTempo)
{
    PerformanceScore score;
    TabImportDiagnostics d;
    CHECK (readFixture ("german_h_tuning.txt", score, &d));
    CHECK ((tuningOf (score) == std::vector<int> { 64, 59, 55, 50, 45, 40 }));
    CHECK (d.tuningFromHeader);
    const auto notes = flatten (score);
    CHECK (notes.size() == 5);
    bool bString = false;
    for (const auto& n : notes)
        if (n.stringIndex == 1) { bString = true; CHECK (n.fret == 1); CHECK (n.midi == 60); }
    CHECK (bString);

    CHECK (readFixture ("steps_down_dropd.txt", score, &d));
    CHECK_MSG ((tuningOf (score) == std::vector<int> { 61, 56, 52, 47, 42, 35 }), score.getMeta().tuningName);
    CHECK (score.getTrack (0).capoFret == 3);
    CHECK_MSG (score.getMeta().key == "Bm", score.getMeta().key);
    CHECK_NEAR (score.getMeta().tempoBpm, 92.0, 1.0e-9);
    CHECK (score.getMeta().timeSignatureNumerator == 4 && score.getMeta().timeSignatureDenominator == 4);
    for (const auto& n : flatten (score))
        if (n.stringIndex == 5) CHECK (n.midi == 35 + 3);
}

LUTHIER_TEST (TabUniversal, chordNamesStrumsAndAccentsOverAStaff)
{
    PerformanceScore score;
    TabImportDiagnostics d;
    CHECK (readFixture ("chords_over_staff_strum.txt", score, &d));
    CHECK_MSG (chordsOf (score) == "Am G", chordsOf (score));

    const auto g = groups (score);
    CHECK_MSG (g.size() == 8, juce::String ((int) g.size()));
    if (g.size() == 8)
    {
        // Columns: D D U U | D U D U, accents on the first stroke of each chord.
        const bool expectedUp[8] = { false, false, true, true, false, true, false, true };
        for (size_t i = 0; i < 8; ++i)
        {
            CHECK_MSG (g[i].size() == (i < 4 ? 5u : 6u), "group " + juce::String ((int) i));
            for (const auto& n : g[i])
            {
                CHECK_MSG (n.has (expectedUp[i] ? ScoreTechnique::Type::pickStrokeUp : ScoreTechnique::Type::pickStrokeDown),
                           "stroke " + juce::String ((int) i));
                CHECK (n.has (ScoreTechnique::Type::accent) == (i == 0 || i == 4));
            }
        }
        // The G chord sits under its name.
        const auto& m = score.getTrack (0).measures.front();
        CHECK (m.chordSymbols.size() == 2);
        if (m.chordSymbols.size() == 2)
        {
            CHECK_NEAR (m.chordSymbols[0].first, g[0].front().beat, 1.0e-9);
            CHECK_NEAR (m.chordSymbols[1].first, g[4].front().beat, 1.0e-9);
        }
    }

    // Neither the chord line (4) nor the strum (5) and accent (6) lines are reported as skipped.
    for (const auto& w : d.warnings)
        CHECK_MSG (! (w.startsWith ("line 4:") || w.startsWith ("line 5:") || w.startsWith ("line 6:")), w);

    // Playback: one strum per chord column, directions kept, chords carried.
    const auto riff = Riff::fromScore (score);
    CHECK_MSG (riff.strums.size() == 8, juce::String ((int) riff.strums.size()));
    if (riff.strums.size() == 8)
    {
        CHECK (riff.strums[0].down && ! riff.strums[2].down && riff.strums[4].down && ! riff.strums[7].down);
        CHECK (riff.strums[4].mask == 0x3f && riff.strums[0].mask == 0x1f);
    }
    CHECK (riff.chords.size() == 2 && riff.chords[1].symbol == "G");
    CHECK (riff.techniques.contains ("strum"));
}

LUTHIER_TEST (TabUniversal, chordChartWithDiagramsFollowsTheStrummingPattern)
{
    PerformanceScore score;
    TabImportDiagnostics d;
    CHECK (readFixture ("chart_diagram_strum.txt", score, &d));
    CHECK_MSG (chordsOf (score) == "Am G F G", chordsOf (score));
    CHECK (score.getTrack (0).capoFret == 2);
    CHECK_MSG (score.getMeta().key == "C", score.getMeta().key);
    CHECK (score.getTrack (0).measures.size() == 4);

    const auto diagrams = TabChordChart::extractDiagrams (corpus ("chart_diagram_strum.txt").loadFileAsString());
    CHECK (diagrams.size() == 3);
    if (diagrams.size() == 3)
        CHECK (diagrams[0].first == "Am" && (diagrams[0].second == std::vector<int> { -1, 0, 2, 2, 1, 0 }));

    // "D DU UDU": eight eighth-note cells, strokes on 1, 2, 2&, 3&, 4, 4&.
    const auto g = groups (score);
    CHECK_MSG (g.size() == 24, juce::String ((int) g.size()));
    if (g.size() >= 6)
    {
        const double onsets[6] = { 0.0, 1.0, 1.5, 2.5, 3.0, 3.5 };
        const bool up[6] = { false, false, true, true, false, true };
        for (size_t i = 0; i < 6; ++i)
        {
            CHECK_NEAR (g[i].front().beat, onsets[i], 1.0e-9);
            CHECK_MSG (g[i].size() == 5, "stroke " + juce::String ((int) i));   // x02210: five strings
            for (const auto& n : g[i])
                CHECK (n.has (up[i] ? ScoreTechnique::Type::pickStrokeUp : ScoreTechnique::Type::pickStrokeDown));
        }
        // The diagram's frets, highest string first: 0 1 2 2 0, with the capo in the pitch.
        const int frets[5] = { 0, 1, 2, 2, 0 };
        for (size_t s = 0; s < 5; ++s)
        {
            CHECK (g[0][s].stringIndex == (int) s && g[0][s].fret == frets[s]);
            CHECK (g[0][s].midi == score.getTrack (0).tuning[s] + 2 + frets[s]);
        }
    }

    // Mutes in a pattern become dead-note strums.
    std::vector<int> pattern;
    CHECK (TabChordChart::extractStrumPattern ("Strum: D x U x", pattern));
    CHECK ((pattern == std::vector<int> { 1, 2, -1, 2 }));
    CHECK (TabChordChart::extractStrumPattern (juce::String (juce::CharPointer_UTF8 ("Pattern: \xe2\x86\x93 \xe2\x86\x93\xe2\x86\x91 \xe2\x86\x91\xe2\x86\x93\xe2\x86\x91")), pattern));
    CHECK ((pattern == std::vector<int> { 1, 0, 1, -1, 0, -1, 1, -1 }));

    TabImportPipeline pipeline;
    CHECK (pipeline.read ("Strumming: D x D x\nE  A\n", score));
    const auto muted = groups (score);
    CHECK (muted.size() == 8);
    if (muted.size() == 8)
    {
        CHECK (! muted[0].front().has (ScoreTechnique::Type::deadNote));
        CHECK (muted[1].front().has (ScoreTechnique::Type::deadNote));
        const auto riff = Riff::fromScore (score);
        CHECK (riff.strums.size() == 8 && riff.strums[1].mute > 0.5 && riff.strums[0].mute < 0.5);
    }
}

LUTHIER_TEST (TabUniversal, nashvilleNumbersAndPickingPattern)
{
    PerformanceScore score;
    CHECK (readFixture ("chart_picking_nashville.txt", score));
    CHECK_MSG (chordsOf (score) == "G C D G", chordsOf (score));
    CHECK_MSG (score.getMeta().key == "G", score.getMeta().key);

    // p i m a: thumb on the shape's bass string, then G, B, e, one a beat.
    const auto g = groups (score);
    CHECK_MSG (g.size() == 16, juce::String ((int) g.size()));
    if (g.size() >= 4)
    {
        CHECK (g[0].size() == 1 && g[0][0].stringIndex == 5 && g[0][0].fret == 3);   // G: low E string, fret 3
        CHECK (g[1].size() == 1 && g[1][0].stringIndex == 2);
        CHECK (g[2].size() == 1 && g[2][0].stringIndex == 1);
        CHECK (g[3].size() == 1 && g[3][0].stringIndex == 0);
        CHECK_NEAR (g[3][0].beat, 3.0, 1.0e-9);
    }

    // Without a key, numbers are not chords.
    TabImportPipeline pipeline;
    CHECK (! pipeline.read ("1 4 5 1\n", score));
    CHECK (TabChordChart::extractChords ("6m 4 1 5", 0).size() == 4);
    CHECK (TabChordChart::extractChords ("6m 4 1 5", 0)[0].name == "Am");
}

LUTHIER_TEST (TabUniversal, chordVoicingsFollowTheTuning)
{
    // Drop D: a D chord's bass is the open sixth string, G keeps its root in the bass.
    PerformanceScore score;
    CHECK (readFixture ("chart_dropd_voicing.txt", score));
    CHECK ((tuningOf (score) == std::vector<int> { 64, 59, 55, 50, 45, 38 }));
    const auto g = groups (score);
    CHECK (g.size() >= 2);
    if (! g.empty())
    {
        bool openD = false;
        for (const auto& n : g[0]) if (n.stringIndex == 5) openD = n.fret == 0;
        CHECK (openD);
        for (const auto& n : g[0]) CHECK (n.midi % 12 == 2 || n.midi % 12 == 6 || n.midi % 12 == 9);
    }

    TabChordChart::Chord chord;
    CHECK (TabChordChart::parseChord ("Am7", chord));
    const auto v = TabChordChart::voicingFor (chord, { 64, 59, 55, 50, 45, 40 }, 0);
    CHECK (v.size() == 6);
    int sounding = 0;
    for (size_t s = 0; s < v.size(); ++s)
        if (v[s] >= 0)
        {
            ++sounding;
            const int pc = (std::vector<int> { 64, 59, 55, 50, 45, 40 }[s] + v[s]) % 12;
            CHECK (pc == 9 || pc == 0 || pc == 4 || pc == 7);
        }
    CHECK (sounding >= 4);

    // A bass chart: four strings, the shape's lowest notes.
    TabImportPipeline pipeline;
    CHECK (pipeline.read ("Tuning: bass\nE  A  D\n", score));
    CHECK (score.getTrack (0).numStrings == 4);
    for (const auto& n : flatten (score))
        CHECK (n.stringIndex < 4 && n.midi <= 60);
}

LUTHIER_TEST (TabUniversal, adversarialHeadersChartsAndStrumLinesNeverCrash)
{
    juce::Random random (0x7ab5);
    const char* const pieces[] = { "Tuning:", "tuned", "down", "half", "step", "steps", "1/2", "1 1/2", "Drop", "D", "H",
                                   "Key:", "Am", "x02210", "{define:", "frets", "Strumming:", "DU", "x", "^", "v",
                                   "Picking:", "p", "i", "m", "a", "1 4 5 1", "Capo", "III", "|", "e|--0--|", ":",
                                   "=", "(", ")", "\xe2\x86\x93", "\xe2\x86\x91", "99", "-", "N.C.", "\n" };

    for (int iteration = 0; iteration < 300; ++iteration)
    {
        juce::String text;
        const int pieceCount = random.nextInt (40);
        for (int i = 0; i < pieceCount; ++i)
        {
            if (random.nextInt (5) == 0)
                text += juce::String::charToString ((juce::juce_wchar) (random.nextInt (0x7e) + 1));
            else
                text += pieces[random.nextInt ((int) (sizeof (pieces) / sizeof (pieces[0])))];
            text += random.nextBool() ? " " : "";
        }

        std::vector<int> midi;
        juce::String name;
        AsciiTabReader::parseTuningStatement (text, midi, name);
        for (int m : midi) CHECK (m >= 0 && m <= 127);
        CHECK (midi.size() <= (size_t) kMaxStrings);

        int root = -1; bool minor = false;
        if (AsciiTabReader::parseKeyStatement (text, root, minor))
            CHECK (root >= 0 && root < 12);

        for (const auto& d : TabChordChart::extractDiagrams (text))
            CHECK (d.second.size() >= 4 && d.second.size() <= 8);
        std::vector<int> pattern;
        TabChordChart::extractStrumPattern (text, pattern);
        CHECK (pattern.size() <= 64);
        TabChordChart::extractPickingPattern (text, pattern);
        CHECK (pattern.size() <= 32);

        PerformanceScore score;
        TabImportPipeline pipeline;
        TabImportDiagnostics d;
        if (pipeline.read (text, score, &d))
        {
            for (const auto& n : flatten (score))
            {
                CHECK (n.stringIndex >= 0 && n.stringIndex < score.getTrack (0).numStrings);
                CHECK (n.midi >= 0 && n.midi <= 127 && n.fret >= 0 && n.fret <= 36);
            }
            const auto riff = Riff::fromScore (score, 0, Riff::kMaxImportedBeats);
            CHECK (riff.lengthBeats > 0.0 && riff.lengthBeats <= Riff::kMaxImportedBeats);
            for (const auto& s : riff.strums) CHECK (s.mask != 0);
        }
    }

    // Absurd but well-formed: a chart of thousands of chords, a strum line longer than the staff.
    juce::String huge;
    for (int i = 0; i < 2500; ++i) huge += "Am G\n";
    PerformanceScore score;
    TabImportPipeline pipeline;
    CHECK (pipeline.read ("Strumming: D DU UDU\n" + huge, score));
    CHECK ((int) score.getTrack (0).measures.size() <= TabChordChart::kMaxChords);

    juce::String longStrum = "    ";
    for (int i = 0; i < 3000; ++i) longStrum += (i % 2 == 0 ? "D " : "U ");
    CHECK (pipeline.read (longStrum + "\ne|--0--0--|\nB|--1--1--|\nG|--0--0--|\nD|--2--2--|\nA|--3--3--|\nE|--------|\n", score));
    CHECK (score.getTotalNoteCount() == 10);
}
