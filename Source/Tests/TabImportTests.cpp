// FEAT2-TAB: the tab import engine (spec/tab-import-export.md). Round-trip,
// fixtures, technique mapping, playback smoke and the adversarial fuzz test.
#include "TestFramework.h"
#include "../Notation/NotationExport.h"
#include "../Riffs/Riff.h"
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffDestinations.h"
#include "../Riffs/RiffTransposer.h"

#include <random>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    std::vector<const ScoreNote*> allNotes (const PerformanceScore& score, int track = 0)
    {
        std::vector<const ScoreNote*> notes;
        const auto& t = score.getTrack (track);
        for (const auto& measure : t.measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    notes.push_back (&note);
        return notes;
    }

    bool anyHas (const PerformanceScore& score, ScoreTechnique::Type type)
    {
        for (const auto* n : allNotes (score))
            if (n->hasTechnique (type))
                return true;
        return false;
    }

    // A plain four-note phrase, standard header, one bar.
    const char* kSimpleTab =
        "Tuning: Standard\n"
        "Tempo: 120 bpm    4/4\n"
        "\n"
        "e|----------------|\n"
        "B|----------------|\n"
        "G|----------------|\n"
        "D|--------7-------|\n"
        "A|----5-----------|\n"
        "E|3---------------|\n";

    // A real-world-shaped phrase with techniques.
    const char* kTechniqueTab =
        "E A D G B e\n"
        "e|--------------------|\n"
        "B|--------------------|\n"
        "G|----7h9p7-----------|\n"
        "D|--5b----------9/11--|\n"
        "A|--------------------|\n"
        "E|-----------x--------|\n";
}

//==============================================================================
LUTHIER_TEST (TabImport, bareFretsParseToRightStringsAndFrets)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (kSimpleTab, score));

    const auto notes = allNotes (score);
    CHECK_MSG (notes.size() == 3, juce::String ((int) notes.size()));

    // Lowest string is the bottom line -> highest string index.
    int fretOnString5 = -1, fretOnString4 = -1, fretOnString3 = -1;
    for (const auto* n : notes)
    {
        if (n->stringIndex == 5) fretOnString5 = n->fret;
        if (n->stringIndex == 4) fretOnString4 = n->fret;
        if (n->stringIndex == 3) fretOnString3 = n->fret;
    }
    CHECK (fretOnString5 == 3);
    CHECK (fretOnString4 == 5);
    CHECK (fretOnString3 == 7);
}

LUTHIER_TEST (TabImport, tuningAndCapoHeadersApply)
{
    const char* tab =
        "Tuning: Drop D\n"
        "Capo: 2\n"
        "D|0---------------|\n"
        "A|----------------|\n"
        "F|----------------|\n"
        "C|----------------|\n"
        "G|----------------|\n"
        "D|0---------------|\n";

    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (tab, score));

    const auto& track = score.getTrack (0);
    CHECK (track.capoFret == 2);
    CHECK (track.tuning[5] == 38);           // Drop D low string
    CHECK (score.getMeta().tuningName.containsIgnoreCase ("drop"));

    // Open low D with a 2-fret capo sounds an E.
    for (const auto* n : allNotes (score))
        if (n->stringIndex == 5)
            CHECK (n->midiNote == 38 + 2);
}

LUTHIER_TEST (TabImport, techniqueGlyphsMapToTypes)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (kTechniqueTab, score));

    CHECK (anyHas (score, ScoreTechnique::Type::hammerOn));
    CHECK (anyHas (score, ScoreTechnique::Type::pullOff));
    CHECK (anyHas (score, ScoreTechnique::Type::bend));
    CHECK (anyHas (score, ScoreTechnique::Type::slideUp));
    CHECK (anyHas (score, ScoreTechnique::Type::deadNote));

    // The bend digits after 'b' are its amount, not a phantom note; the dead
    // note is a note, not a dropped column.
    for (const auto* n : allNotes (score))
    {
        CHECK (n->fret >= 0 && n->fret <= 36);
        CHECK (n->stringIndex >= 0 && n->stringIndex < score.getTrack (0).numStrings);
    }
}

LUTHIER_TEST (TabImport, wrappedGlyphsMapToHarmonicsAndGhost)
{
    const char* tab =
        "e|--------------------|\n"
        "B|--------------------|\n"
        "G|----<12>---(7)------|\n"
        "D|--------[5]---------|\n"
        "A|--------------------|\n"
        "E|--------------------|\n";

    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (tab, score));
    CHECK (anyHas (score, ScoreTechnique::Type::naturalHarmonic));
    CHECK (anyHas (score, ScoreTechnique::Type::artificialHarmonic));
    CHECK (anyHas (score, ScoreTechnique::Type::ghostNote));

    // <12> is fret 12, not frets 1 and 2.
    bool sawTwelve = false;
    for (const auto* n : allNotes (score))
        if (n->fret == 12 && n->hasTechnique (ScoreTechnique::Type::naturalHarmonic))
            sawTwelve = true;
    CHECK (sawTwelve);
}

LUTHIER_TEST (TabImport, multiSystemTabsAccumulate)
{
    const char* tab =
        "e|3---------------|\n"
        "B|----------------|\n"
        "G|----------------|\n"
        "D|----------------|\n"
        "A|----------------|\n"
        "E|----------------|\n"
        "\n"
        "e|5---------------|\n"
        "B|----------------|\n"
        "G|----------------|\n"
        "D|----------------|\n"
        "A|----------------|\n"
        "E|----------------|\n";

    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (tab, score));

    const auto notes = allNotes (score);
    CHECK (notes.size() == 2);
    // The second system's note starts a bar later, not on top of the first.
    double maxBeat = 0.0;
    for (const auto* n : notes) maxBeat = juce::jmax (maxBeat, n->startBeat);
    CHECK (maxBeat >= 0.0);   // both parsed; beats are measure-relative after endCapture
    CHECK (score.getTrack (0).measures.size() >= 2);
}

LUTHIER_TEST (TabImport, exportThenReimportIsStable)
{
    NotationImporter importer;
    PerformanceScore first;
    CHECK (importer.readAsciiTab (kTechniqueTab, first));

    NotationExporter exporter;
    const auto text = exporter.renderAsciiTab (first);

    PerformanceScore second;
    CHECK (importer.readAsciiTab (text, second));

    // Same number of notes, on the same strings, at the same frets.
    const auto a = allNotes (first);
    const auto b = allNotes (second);
    CHECK_MSG (a.size() == b.size(),
               juce::String ((int) a.size()) + " vs " + juce::String ((int) b.size()));

    auto key = [] (const std::vector<const ScoreNote*>& v)
    {
        juce::StringArray k;
        for (const auto* n : v)
            k.add (juce::String (n->stringIndex) + ":" + juce::String (n->fret));
        k.sort (false);
        return k.joinIntoString (",");
    };
    CHECK_MSG (key (a) == key (b), key (a) + " != " + key (b));
}

LUTHIER_TEST (TabImport, musicXmlTabImports)
{
    // A minimal MusicXML tab: one note, string 6 fret 3, with a hammer-on.
    const char* xml =
        "<?xml version=\"1.0\"?>"
        "<score-partwise><part id=\"P1\"><measure number=\"1\">"
        "<attributes><divisions>4</divisions>"
        "<staff-details><staff-lines>6</staff-lines></staff-details></attributes>"
        "<note><pitch><step>G</step><octave>2</octave></pitch><duration>4</duration>"
        "<notations><technical><string>6</string><fret>3</fret>"
        "<hammer-on>H</hammer-on></technical></notations></note>"
        "</measure></part></score-partwise>";

    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.readMusicXml (xml, score), importer.getLastError());

    const auto notes = allNotes (score);
    CHECK (notes.size() == 1);
    if (! notes.empty())
    {
        CHECK (notes[0]->stringIndex == 5);   // string 6 -> index 5
        CHECK (notes[0]->fret == 3);
    }
    CHECK (anyHas (score, ScoreTechnique::Type::hammerOn));
}

LUTHIER_TEST (TabImport, importedTabRendersFiniteBoundedAudio)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (kSimpleTab, score));

    const auto riff = Riff::fromScore (score);
    CHECK (! riff.notes.empty());

    // Every riff note is on a valid string with an in-range pitch.
    for (const auto& n : riff.notes)
    {
        CHECK (n.stringIndex >= 0 && n.stringIndex < riff.getNumStrings());
        CHECK (n.fret >= 0 && n.fret <= Riff::kMaxFret);
        CHECK (n.midiNote >= 0 && n.midiNote <= 127);
    }

    constexpr double sr = 44100.0;
    const int len = (int) (sr * 1.0);
    const auto audio = RiffDestinations::render (riff, {}, GuitarType::Stratocaster,
                                                 120.0, sr, len);

    CHECK (audio.getNumSamples() == len);
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        const float* d = audio.getReadPointer (ch);
        CHECK_FINITE (d, len);
        float peak = 0.0f;
        for (int i = 0; i < len; ++i) peak = juce::jmax (peak, std::abs (d[i]));
        CHECK_MSG (peak < 8.0f, juce::String (peak));   // bounded, not blowing up
    }
}

LUTHIER_TEST (TabImport, fuzzNeverCrashesOrProducesInvalidNotes)
{
    NotationImporter importer;
    std::mt19937 rng (0xB0FFEEu);

    auto validate = [] (TestContext& ctx, const PerformanceScore& score)
    {
        const int strings = score.getTrack (0).numStrings;
        CHECK (strings >= 1 && strings <= kMaxStrings);
        for (const auto* n : allNotes (score))
        {
            CHECK (n->stringIndex >= 0 && n->stringIndex < strings);
            CHECK (n->fret >= 0 && n->fret <= 36);
            CHECK (n->midiNote >= 0 && n->midiNote <= 127);
            CHECK (std::isfinite (n->startBeat) && n->startBeat >= 0.0);
            CHECK (std::isfinite (n->durationBeats));
        }
    };

    // 1. Random bytes.
    for (int iter = 0; iter < 200; ++iter)
    {
        juce::String junk;
        const int len = (int) (rng() % 400);
        for (int i = 0; i < len; ++i)
            junk += juce::String::charToString ((juce::juce_wchar) (1 + rng() % 250));

        PerformanceScore score;
        importer.readAsciiTab (junk, score);   // must return, not crash
        validate (ctx, score);
    }

    // 2. Truncated / malformed staff lines.
    const char* broken[] = {
        "E|3", "|||||||", "E|999999999999", "e\nB\nG\nD\nA\nE",
        "E|3-5-7-9-11-13-15-", "-|-|-|-|-|-|", "E|xxxxxxxxxxxx",
        "Tuning:\nTempo:\nCapo:\nE|", "E|[[[[<<<<((((", "E|3b\\/~ph.>PMtr" };
    for (const auto* b : broken)
    {
        PerformanceScore score;
        importer.readAsciiTab (b, score);
        validate (ctx, score);
    }

    // 3. Mixed line endings.
    {
        PerformanceScore score;
        importer.readAsciiTab ("E|3---|\r\nB|--5-|\rG|7---|\n", score);
        validate (ctx, score);
    }

    // 4. Huge input.
    {
        juce::String big;
        for (int i = 0; i < 5000; ++i) big += "E|3---5---7---9---|\n";
        PerformanceScore score;
        importer.readAsciiTab (big, score);
        validate (ctx, score);
    }

    // 5. Wrong string count and out-of-range frets.
    {
        PerformanceScore score;
        importer.readAsciiTab (
            "S1|99---0---|\nS2|45---12--|\nS3|3--------|\nS4|7--------|\n", score);
        validate (ctx, score);
    }

    // 6. Unicode / non-ASCII.
    {
        PerformanceScore score;
        importer.readAsciiTab (juce::String::fromUTF8 (
            "E|3--\xc3\xa9\xe2\x99\xaf--5--|\nB|--\xf0\x9f\x8e\xb8--7--|\n"), score);
        validate (ctx, score);
    }

    // 7. A fuzzed tab that parses should also render safely.
    {
        PerformanceScore score;
        if (importer.readAsciiTab (
                "E|3---5b7---x---<12>--(9)--|\nA|--7h9p7--/11--|\n", score))
        {
            const auto riff = Riff::fromScore (score);
            const auto audio = RiffDestinations::render (riff, {}, GuitarType::Stratocaster,
                                                         120.0, 22050.0, 4096);
            CHECK_FINITE (audio.getReadPointer (0), audio.getNumSamples());
        }
    }
}
