// tab-import-export 7: the dialect-tolerant ASCII reader. Real-world-shaped
// pages, every technique glyph, tuning inference, repeats, timing heuristics
// and graceful partial reads with diagnostics.
#include "TestFramework.h"
#include "../Notation/NotationExport.h"

#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    std::vector<const ScoreNote*> allNotes (const PerformanceScore& score)
    {
        std::vector<const ScoreNote*> notes;
        for (const auto& measure : score.getTrack (0).measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    notes.push_back (&note);
        return notes;
    }

    const ScoreNote* findNote (const PerformanceScore& score, int stringIndex, int fret, int skip = 0)
    {
        for (const auto* n : allNotes (score))
            if (n->stringIndex == stringIndex && n->fret == fret && skip-- == 0)
                return n;
        return nullptr;
    }

    bool anyHas (const PerformanceScore& score, ScoreTechnique::Type type)
    {
        for (const auto* n : allNotes (score))
            if (n->hasTechnique (type))
                return true;
        return false;
    }

    juce::File fixture (const char* name)
    {
        return juce::File (__FILE__).getSiblingFile ("Fixtures").getChildFile (name);
    }

    // Six standard string lines around one G string phrase.
    juce::String sixLines (const juce::String& g)
    {
        return "e|" + juce::String::repeatedString ("-", g.length()) + "|\n"
             + "B|" + juce::String::repeatedString ("-", g.length()) + "|\n"
             + "G|" + g + "|\n"
             + "D|" + juce::String::repeatedString ("-", g.length()) + "|\n"
             + "A|" + juce::String::repeatedString ("-", g.length()) + "|\n"
             + "E|" + juce::String::repeatedString ("-", g.length()) + "|\n";
    }
}

//==============================================================================
LUTHIER_TEST (TabDialect, messyRealWorldPageParsesAndReportsSkips)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("messy.tab"), score), importer.getLastError());

    const auto& d = importer.getLastDiagnostics();
    CHECK (d.tuningFromHeader);
    CHECK (d.tempoFromHeader);
    CHECK (score.getTrack (0).tuning[5] == 38);          // Drop D
    CHECK (score.getTrack (0).capoFret == 2);
    CHECK_NEAR (score.getMeta().tempoBpm, 96.0, 0.01);
    CHECK (d.systems == 2);
    CHECK (d.notes > 10);
    // Title, artist, tabbed-by, section, chords, lyric, comment, fade: skipped, not fatal.
    CHECK_MSG (d.skippedLines >= 6, juce::String (d.skippedLines));
    CHECK (d.isPartial());
    CHECK (d.summary().contains ("skipped"));

    // The first system has two bars, the second one bar played twice: four in all.
    CHECK_MSG (d.measures == 4, juce::String (d.measures));
    CHECK (d.repeatsUnrolled == 1);

    // Techniques from the page.
    CHECK (anyHas (score, ScoreTechnique::Type::hammerOn));
    CHECK (anyHas (score, ScoreTechnique::Type::pullOff));
    CHECK (anyHas (score, ScoreTechnique::Type::bend));
    CHECK (anyHas (score, ScoreTechnique::Type::slideLegato));
    CHECK (anyHas (score, ScoreTechnique::Type::naturalHarmonic));
    CHECK (anyHas (score, ScoreTechnique::Type::ghostNote));
    CHECK (anyHas (score, ScoreTechnique::Type::vibrato));
    CHECK (anyHas (score, ScoreTechnique::Type::deadNote));
    CHECK (anyHas (score, ScoreTechnique::Type::tap));

    // The PM span above the first bar palm-mutes the low D pulses under it.
    const auto* pulse = findNote (score, 5, 0);
    CHECK (pulse != nullptr && pulse->hasTechnique (ScoreTechnique::Type::palmMute));
}

LUTHIER_TEST (TabDialect, bassTabInfersStringsAndSlapPop)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("bass.tab"), score), importer.getLastError());

    const auto& t = score.getTrack (0);
    CHECK (t.numStrings == 4);
    CHECK (importer.getLastDiagnostics().tuningFromStringNames);
    CHECK_MSG (t.tuning[0] == 43 && t.tuning[3] == 28, juce::String (t.tuning[0]) + "/" + juce::String (t.tuning[3]));
    CHECK (anyHas (score, ScoreTechnique::Type::slap));
    CHECK (anyHas (score, ScoreTechnique::Type::pop));
}

LUTHIER_TEST (TabDialect, sevenStringInferredFromNames)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.read (fixture ("sevenstring.tab"), score));

    const auto& t = score.getTrack (0);
    CHECK (t.numStrings == 7);
    CHECK_MSG (t.tuning[6] == 35, juce::String (t.tuning[6]));   // low B
    CHECK (t.tuning[0] == 64);
    CHECK (allNotes (score).size() == 8);
}

LUTHIER_TEST (TabDialect, partialFileReadsTheGoodSystemAndReportsTheRest)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.read (fixture ("partial.tab"), score));

    const auto& d = importer.getLastDiagnostics();
    CHECK (d.notes >= 6);
    CHECK (d.skippedLines >= 3);
    CHECK (! d.warnings.isEmpty());
    CHECK (d.summary().startsWith ("Loaded"));

    // The G chord landed on the right strings.
    CHECK (findNote (score, 0, 3) != nullptr);
    CHECK (findNote (score, 4, 2) != nullptr);
    CHECK (findNote (score, 5, 3) != nullptr);
}

LUTHIER_TEST (TabDialect, emptyAndJunkInputFailCleanly)
{
    NotationImporter importer;

    for (const char* junk : { "", "\n\n\n", "just some words\nand more words", "|||", "-----" })
    {
        PerformanceScore score;
        CHECK_MSG (! importer.readAsciiTab (junk, score), juce::String (junk));
        CHECK (importer.getLastError().contains ("No tablature"));
        CHECK (importer.getLastDiagnostics().notes == 0);
    }

    // A page of chord names and nothing else is no longer junk: each chord becomes
    // a strummed bar (TabChordChart); see TabRobustnessTests.
    PerformanceScore chords;
    CHECK (importer.readAsciiTab ("Am G C D", chords));
    CHECK (chords.getTrack (0).measures.size() == 4);
}

//==============================================================================
LUTHIER_TEST (TabDialect, legatoGlyphsLandOnTheArrivingNote)
{
    NotationImporter importer;
    PerformanceScore score;

    // "7h9p7": the 9 is hammered, the second 7 is pulled; the first 7 is picked.
    CHECK (importer.readAsciiTab (sixLines ("--7h9p7---------"), score));
    const auto* first = findNote (score, 2, 7, 0);
    const auto* nine  = findNote (score, 2, 9);
    const auto* last  = findNote (score, 2, 7, 1);
    CHECK (first != nullptr && ! first->hasTechnique (ScoreTechnique::Type::hammerOn));
    CHECK (nine != nullptr && nine->hasTechnique (ScoreTechnique::Type::hammerOn));
    CHECK (last != nullptr && last->hasTechnique (ScoreTechnique::Type::pullOff));

    // The writer's spelling ("7-h9") and the caret dialect ("7^9", "9^7").
    CHECK (importer.readAsciiTab (sixLines ("--7-h9-----------"), score));
    CHECK (findNote (score, 2, 9) != nullptr && findNote (score, 2, 9)->hasTechnique (ScoreTechnique::Type::hammerOn));

    CHECK (importer.readAsciiTab (sixLines ("--7^9^7----------"), score));
    CHECK (findNote (score, 2, 9)->hasTechnique (ScoreTechnique::Type::hammerOn));
    CHECK (findNote (score, 2, 7, 1)->hasTechnique (ScoreTechnique::Type::pullOff));

    // Tap before the note, either way round.
    CHECK (importer.readAsciiTab (sixLines ("--5h8t12---------"), score));
    CHECK (findNote (score, 2, 12) != nullptr && findNote (score, 2, 12)->hasTechnique (ScoreTechnique::Type::tap));
    CHECK (importer.readAsciiTab (sixLines ("--T12------------"), score));
    CHECK (findNote (score, 2, 12) != nullptr && findNote (score, 2, 12)->hasTechnique (ScoreTechnique::Type::tap));
}

LUTHIER_TEST (TabDialect, bendAmountsReleasesAndPreBends)
{
    NotationImporter importer;
    PerformanceScore score;

    CHECK (importer.readAsciiTab (sixLines ("--7b9---7bfull--7b1/2---7b---"), score));
    const auto notes = allNotes (score);
    CHECK_MSG (notes.size() == 4, juce::String ((int) notes.size()));
    if (notes.size() == 4)
    {
        CHECK_NEAR (notes[0]->findTechnique (ScoreTechnique::Type::bend)->value, 2.0, 1.0e-9);
        CHECK_NEAR (notes[1]->findTechnique (ScoreTechnique::Type::bend)->value, 2.0, 1.0e-9);
        CHECK_NEAR (notes[2]->findTechnique (ScoreTechnique::Type::bend)->value, 1.0, 1.0e-9);
        CHECK_NEAR (notes[3]->findTechnique (ScoreTechnique::Type::bend)->value, 2.0, 1.0e-9);
    }

    CHECK (importer.readAsciiTab (sixLines ("--7b9r7---7pb9r7---7^---"), score));
    const auto n2 = allNotes (score);
    CHECK_MSG (n2.size() == 3, juce::String ((int) n2.size()));
    if (n2.size() == 3)
    {
        const auto* rel = n2[0]->findTechnique (ScoreTechnique::Type::bendRelease);
        CHECK (rel != nullptr && rel->value == 2.0 && rel->secondValue == 0.0);
        const auto* pre = n2[1]->findTechnique (ScoreTechnique::Type::preBend);
        CHECK (pre != nullptr && pre->value == 2.0);
        CHECK (n2[2]->hasTechnique (ScoreTechnique::Type::bend));
    }
}

LUTHIER_TEST (TabDialect, slidesVibratoHarmonicsMutesAndTies)
{
    NotationImporter importer;
    PerformanceScore score;

    CHECK (importer.readAsciiTab (sixLines ("--5/7--7\\5--5s7--/9--9\\--"), score));
    const auto n = allNotes (score);
    CHECK_MSG (n.size() == 8, juce::String ((int) n.size()));
    if (n.size() == 8)
    {
        // Slides between frets are legato slides to the target fret, which is
        // what the compiler plays as an unpicked arrival.
        CHECK (n[0]->hasTechnique (ScoreTechnique::Type::slideLegato) && n[0]->findTechnique (ScoreTechnique::Type::slideLegato)->value == 7.0);
        CHECK (n[2]->hasTechnique (ScoreTechnique::Type::slideLegato) && n[2]->findTechnique (ScoreTechnique::Type::slideLegato)->value == 5.0);
        CHECK (n[4]->hasTechnique (ScoreTechnique::Type::slideLegato));   // 's' between frets
        CHECK (n[6]->hasTechnique (ScoreTechnique::Type::slideIn));       // "/9"
        CHECK (n[7]->hasTechnique (ScoreTechnique::Type::slideOut));      // "9\"
        // The departing note lasts exactly until the arrival, so the compiler links them.
        CHECK_NEAR (n[0]->startBeat + n[0]->durationBeats, n[1]->startBeat, 1.0e-9);
    }

    CHECK (importer.readAsciiTab (sixLines ("--7~~--7v--12*--<12>--[7]--7PM--7.--7>--"), score));
    const auto m = allNotes (score);
    CHECK_MSG (m.size() == 8, juce::String ((int) m.size()));
    if (m.size() == 8)
    {
        CHECK (m[0]->hasTechnique (ScoreTechnique::Type::vibrato));
        CHECK (m[1]->hasTechnique (ScoreTechnique::Type::vibrato));
        CHECK (m[2]->hasTechnique (ScoreTechnique::Type::naturalHarmonic) && m[2]->fret == 12);
        CHECK (m[3]->hasTechnique (ScoreTechnique::Type::naturalHarmonic) && m[3]->fret == 12);
        CHECK (m[4]->hasTechnique (ScoreTechnique::Type::artificialHarmonic));
        CHECK (m[5]->hasTechnique (ScoreTechnique::Type::palmMute));
        CHECK (m[6]->hasTechnique (ScoreTechnique::Type::staccato));
        CHECK (m[7]->hasTechnique (ScoreTechnique::Type::accent));
    }

    // A tie extends the note before it instead of striking again.
    CHECK (importer.readAsciiTab (sixLines ("--7-------=7----"), score));
    const auto t = allNotes (score);
    CHECK_MSG (t.size() == 1, juce::String ((int) t.size()));
    if (t.size() == 1)
        CHECK_MSG (t[0]->durationBeats >= 2.0, juce::String (t[0]->durationBeats));

    // Dead notes and a whole-strum mute.
    CHECK (importer.readAsciiTab ("e|--x--|\nB|--x--|\nG|--x--|\nD|--3--|\nA|--x--|\nE|--x--|\n", score));
    CHECK (allNotes (score).size() == 6);
    CHECK (anyHas (score, ScoreTechnique::Type::deadNote));
}

//==============================================================================
LUTHIER_TEST (TabDialect, repeatsUnroll)
{
    NotationImporter importer;
    PerformanceScore score;

    // |: two bars :| = played twice.
    CHECK (importer.readAsciiTab (
        "e|:----|----:|\nB|-----|-----|\nG|-----|-----|\nD|-----|-----|\nA|-----|-----|\nE|3----|5----|\n", score));
    CHECK_MSG (importer.getLastDiagnostics().measures == 4, juce::String (importer.getLastDiagnostics().measures));
    CHECK (allNotes (score).size() == 4);

    // A trailing count on the staff, and one on its own line.
    CHECK (importer.readAsciiTab (
        "e|----|\nB|----|\nG|----|\nD|----|\nA|----|\nE|3---| x3\n", score));
    CHECK_MSG (importer.getLastDiagnostics().measures == 3, juce::String (importer.getLastDiagnostics().measures));
    CHECK (allNotes (score).size() == 3);

    CHECK (importer.readAsciiTab (
        "e|----|\nB|----|\nG|----|\nD|----|\nA|----|\nE|3---|\n(play 4 times)\n", score));
    CHECK (importer.getLastDiagnostics().measures == 4);
    CHECK (importer.getLastDiagnostics().skippedLines == 0);

    // A repeat only ever expands so far: an absurd count is clamped.
    CHECK (importer.readAsciiTab (
        "e|----|\nB|----|\nG|----|\nD|----|\nA|----|\nE|3---| x99\n", score));
    CHECK (importer.getLastDiagnostics().measures <= 16);
}

LUTHIER_TEST (TabDialect, timingFollowsTheGridOfTheBar)
{
    NotationImporter importer;
    PerformanceScore score;

    // Twelve columns in a 4/4 bar: three to a beat.
    CHECK (importer.readAsciiTab (sixLines ("3--5--7--9--"), score));
    auto n = allNotes (score);
    CHECK (n.size() == 4);
    if (n.size() == 4)
    {
        CHECK_NEAR (n[1]->startBeat, 1.0, 1.0e-9);
        CHECK_NEAR (n[3]->startBeat, 3.0, 1.0e-9);
    }

    // Ten columns: no clean grid, so the bar is stretched proportionally.
    CHECK (importer.readAsciiTab (sixLines ("3----5----"), score));
    n = allNotes (score);
    CHECK (n.size() == 2);
    if (n.size() == 2)
        CHECK_NEAR (n[1]->startBeat, 2.0, 1.0e-9);

    // Without a closing bar: four columns to a beat, spilling into later bars.
    CHECK (importer.readAsciiTab ("E|3---------------5---\n", score));
    n = allNotes (score);
    CHECK (n.size() == 2);
    CHECK (score.getTrack (0).measures.size() >= 2);

    // Durations run to the next note on the string.
    CHECK (importer.readAsciiTab (sixLines ("3-------5-------"), score));
    n = allNotes (score);
    if (n.size() == 2)
        CHECK_NEAR (n[0]->durationBeats, 2.0, 1.0e-9);
}

LUTHIER_TEST (TabDialect, tuningHeadersInEveryStyle)
{
    NotationImporter importer;
    PerformanceScore score;
    const auto staff = sixLines ("--3--");

    struct Case { const char* header; int top; int bottom; };
    const Case cases[] = {
        { "Tuning: D A D G A D\n",            62, 38 },
        { "Tuning: DADGAD\n",                 62, 38 },
        { "Tuning: Eb Ab Db Gb Bb Eb\n",      63, 39 },
        { "Tuning: E2 A2 D3 G3 B3 E4\n",      64, 40 },
        { "Tuned down 1/2 step\n",            63, 39 },
        { "Tuning: Drop C\n",                 62, 36 },
        { "Tuning: Open G\n",                 62, 38 },
        { "tuning = e B G D A E\n",           64, 40 },
        { "Standard tuning (EADGBE)\n",       64, 40 },
    };

    for (const auto& c : cases)
    {
        CHECK_MSG (importer.readAsciiTab (juce::String (c.header) + staff, score), c.header);
        const auto& t = score.getTrack (0);
        CHECK_MSG (t.tuning[0] == c.top && t.tuning[5] == c.bottom,
                   juce::String (c.header).trim() + " -> " + juce::String (t.tuning[0]) + "/" + juce::String (t.tuning[5]));
        CHECK (importer.getLastDiagnostics().tuningFromHeader);
    }

    // Capo and metre spellings.
    CHECK (importer.readAsciiTab ("Capo on 3rd fret\nTime: 3/4\n" + staff, score));
    CHECK (score.getTrack (0).capoFret == 3);
    CHECK (score.getMeta().timeSignatureNumerator == 3);

    // No header at all: standard assumed and said so.
    CHECK (importer.readAsciiTab (staff, score));
    CHECK (! importer.getLastDiagnostics().tuningFromHeader);
    CHECK (importer.getLastDiagnostics().summary().contains ("Standard") || importer.getLastDiagnostics().tuningFromStringNames);
}

LUTHIER_TEST (TabDialect, unlabelledAndRaggedStaffsStillRead)
{
    NotationImporter importer;
    PerformanceScore score;

    // No string names, bars at both ends, ragged lengths.
    CHECK (importer.readAsciiTab (
        "|--3---|\n|--0-----|\n|--0--|\n|--0---|\n|--2---|\n|--3---|\n", score));
    CHECK (allNotes (score).size() == 6);
    CHECK (score.getTrack (0).numStrings == 6);

    // A five-line guitar tab: read as the top five strings, with a warning.
    CHECK (importer.readAsciiTab ("e|--0--|\nB|--0--|\nG|--1--|\nD|--2--|\nA|--2--|\n", score));
    CHECK (allNotes (score).size() == 5);
    CHECK (score.getTrack (0).numStrings == 5);

    // Two-digit frets past 24 are two notes, and the reader says so.
    CHECK (importer.readAsciiTab (sixLines ("--35--"), score));
    CHECK (allNotes (score).size() == 2);
    CHECK (importer.getLastDiagnostics().splitFrets == 1);

    // Spaces as fill.
    CHECK (importer.readAsciiTab ("E|3   5   7   9   |\n", score));
    CHECK (allNotes (score).size() == 4);
}
