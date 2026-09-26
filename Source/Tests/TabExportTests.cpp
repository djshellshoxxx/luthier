// FEAT2-TAB: regression fixtures for the shared notation export path.
#include "TestFramework.h"
#include "../Notation/NotationExport.h"
#include "../Export/MidiPerformance.h"
#include <algorithm>
#include <initializer_list>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PerformanceScore tabScore()
    {
        PerformanceScore score;
        score.getTrack (0).measures.resize (1);
        score.getTrack (0).measures[0].voices.resize (1);
        return score;
    }

    ScoreNote tabNote (int string, int fret, double beat,
                       std::initializer_list<ScoreTechnique> techniques = {})
    {
        ScoreNote note;
        note.stringIndex = string;
        note.fret = fret;
        note.startBeat = beat;
        note.durationBeats = 0.5;
        note.midiNote = 64 + fret;
        note.techniques = techniques;
        return note;
    }

    juce::String window (const PerformanceScore& score)
    {
        return NotationExporter{}.renderAsciiTabWindow (score, 0, 1);
    }
}

LUTHIER_TEST (TabExport, knownRiffHasExactStringsAndBars)
{
    auto score = tabScore();
    score.getTrack (0).measures[0].voices[0].notes = {
        tabNote (5, 0, 0), tabNote (5, 3, 1), tabNote (4, 2, 2), tabNote (4, 0, 3)
    };
    const juce::String expected =
        "   1---2---3---4--- \n"
        "E |----------------|\n"
        "B |----------------|\n"
        "G |----------------|\n"
        "D |----------------|\n"
        "A |--------2---0---|\n"
        "E |0---3-----------|\n";
    CHECK_MSG (window (score) == expected, window (score));
}

LUTHIER_TEST (TabExport, techniquesHaveExactAlignedOutput)
{
    auto score = tabScore();
    score.getTrack (0).measures[0].voices[0].notes = {
        tabNote (0, 5, 0, {{ScoreTechnique::Type::bend}}),
        tabNote (1, 7, 1, {{ScoreTechnique::Type::slideUp}}),
        tabNote (5, 3, 2, {{ScoreTechnique::Type::palmMute}})
    };
    const juce::String expected =
        "             PM--\n"
        "   1 ---2 ---3  ---4--- \n"
        "E |5b------------------|\n"
        "B |-----7/-------------|\n"
        "G |--------------------|\n"
        "D |--------------------|\n"
        "A |--------------------|\n"
        "E |----------3PM-------|\n";
    CHECK_MSG (window (score) == expected, window (score));
}

LUTHIER_TEST (TabExport, widenedCellsKeepBeatRulerAligned)
{
    auto score = tabScore();
    score.getTrack (0).measures[0].voices[0].notes = {
        tabNote (0, 12, 0, {{ ScoreTechnique::Type::bend }}),
        tabNote (0, 10, 1), tabNote (0, 9, 2), tabNote (0, 8, 3)
    };
    const auto lines = juce::StringArray::fromLines (window (score));
    CHECK (lines[0].indexOfChar ('2') == lines[1].indexOf ("10"));
    CHECK (lines[0].indexOfChar ('3') == lines[1].indexOfChar ('9'));
    CHECK (lines[0].indexOfChar ('4') == lines[1].indexOfChar ('8'));
    for (int s = 1; s <= 6; ++s)
        CHECK (lines[s].length() == lines[0].length());
}

LUTHIER_TEST (TabExport, denseOnsetsDoNotOverwriteOrConcatenateFrets)
{
    auto score = tabScore();
    score.getTrack (0).measures[0].voices[0].notes = {
        tabNote (0, 5, 0), tabNote (0, 7, 0.0625), tabNote (0, 9, 0.125),
        tabNote (1, 12, 0.0625)
    };
    const auto lines = juce::StringArray::fromLines (window (score));
    CHECK (lines[1].contains ("5-7-9"));
    CHECK (lines[1].indexOfChar ('7') == lines[2].indexOf ("12"));
}

LUTHIER_TEST (TabExport, combinedTechniquesDoNotEraseOneAnother)
{
    auto score = tabScore();
    auto& notes = score.getTrack (0).measures[0].voices[0].notes;
    notes = { tabNote (0, 12, 0, {{ ScoreTechnique::Type::vibrato },
                                { ScoreTechnique::Type::naturalHarmonic },
                                { ScoreTechnique::Type::bend }}) };
    const auto first = window (score);
    CHECK (first.contains ("<12>b~"));
    std::reverse (notes[0].techniques.begin(), notes[0].techniques.end());
    CHECK (window (score) == first);
}

LUTHIER_TEST (TabExport, allNoteTechniquesHaveReadableGlyphs)
{
    using T = ScoreTechnique::Type;
    struct Fixture { T type; const char* glyph; };
    const Fixture fixtures[] = {
        {T::bend, "5b"}, {T::bendRelease, "5r"}, {T::preBend, "5pb"},
        {T::slideUp, "5/"}, {T::slideDown, "5\\"}, {T::slideLegato, "5\\"},
        {T::slideShift, "5\\"}, {T::slideIn, "/5"}, {T::slideOut, "5/"},
        {T::hammerOn, "5h"}, {T::pullOff, "5p"}, {T::palmMute, "5PM"},
        {T::deadNote, "x"}, {T::naturalHarmonic, "<5>"},
        {T::pinchHarmonic, "[5]"}, {T::artificialHarmonic, "[5]"},
        {T::tapHarmonic, "[5]t"}, {T::tap, "5t"}, {T::vibrato, "5~"},
        {T::trill, "5tr"}, {T::whammy, "5w"}, {T::ghostNote, "(5)"},
        {T::accent, "5>"}, {T::staccato, "5."}, {T::letRing, "5LR"}
    };
    for (const auto& fixture : fixtures)
    {
        auto score = tabScore();
        score.getTrack (0).measures[0].voices[0].notes = {
            tabNote (0, 5, 0, {{fixture.type, 3.0}})
        };
        CHECK_MSG (window (score).contains (fixture.glyph), getTechniqueName (fixture.type));
    }
}

LUTHIER_TEST (TabExport, palmMuteSpanAndDensityAreExplicit)
{
    auto score = tabScore();
    auto& notes = score.getTrack (0).measures[0].voices[0].notes;
    notes = {tabNote (0, 5, 0, {{ScoreTechnique::Type::palmMute}})};
    notes[0].durationBeats = 2.0;
    CHECK (window (score).contains ("PM-----"));
    NotationExportOptions options;
    options.density = NotationExportOptions::SymbolDensity::notesOnly;
    const auto plain = NotationExporter{}.renderAsciiTabWindow (score, 0, 1, options);
    CHECK (! plain.contains ("PM"));
    CHECK (plain.contains ("5---"));
}

LUTHIER_TEST (TabExport, everySectionAndTrackIsExported)
{
    auto score = tabScore();
    score.getTrack (0).name = "Lead";
    score.getTrack (0).measures.resize (2);
    score.getTrack (0).measures[0].sectionName = "Verse";
    score.getTrack (0).measures[1].sectionName = "Chorus";
    auto& bass = score.addTrack ("Bass");
    bass.numStrings = 4;
    bass.measures.resize (1);
    bass.measures[0].voices.resize (1);
    bass.measures[0].voices[0].notes = { tabNote (3, 17, 0) };
    const auto text = NotationExporter{}.renderAsciiTab (score);
    CHECK (text.contains ("[Verse]"));
    CHECK (text.contains ("[Chorus]"));
    CHECK (text.contains ("Bass"));
    CHECK (text.contains ("17"));
}

LUTHIER_TEST (TabExport, rangeUsesEachMeasuresTimeSignature)
{
    auto score = tabScore();
    auto& measures = score.getTrack (0).measures;
    measures.resize (3);
    for (int m = 0; m < 3; ++m)
    {
        measures[(size_t) m].timeSignatureNumerator = m == 0 ? 3 : 4;
        measures[(size_t) m].voices.resize (1);
        measures[(size_t) m].voices[0].notes = { tabNote (0, 10 + m, 0) };
    }
    NotationExportOptions options;
    options.fromBeat = 3.0;
    options.lengthBeats = 4.0;
    const auto text = NotationExporter{}.renderAsciiTab (score, options);
    CHECK (text.contains ("11"));
    CHECK (! text.contains ("10"));
    CHECK (! text.contains ("12"));
}

LUTHIER_TEST (TabExport, denseBarWrapsWithoutDroppingTokens)
{
    auto score = tabScore();
    auto& notes = score.getTrack (0).measures[0].voices[0].notes;
    for (int i = 0; i < 32; ++i)
        notes.push_back (tabNote (0, 12, i / 8.0, {{ScoreTechnique::Type::bend}}));
    NotationExportOptions options;
    options.lineWidth = 40;
    const auto text = NotationExporter{}.renderAsciiTab (score, options);
    int count = 0;
    for (const auto& line : juce::StringArray::fromLines (text))
    {
        CHECK_MSG (line.length() <= 40, line);
        if (line.startsWith ("E "))
            for (int pos = 0; (pos = line.indexOf (pos, "12b")) >= 0; pos += 3)
                ++count;
    }
    CHECK (count == 32);
}

LUTHIER_TEST (TabExport, windowClampsToExistingMeasures)
{
    const auto score = tabScore();
    NotationExporter exporter;
    CHECK (exporter.renderAsciiTabWindow (score, 0, 0).isEmpty());
    CHECK (exporter.renderAsciiTabWindow (score, 4, 1) == window (score));
    CHECK (exporter.renderAsciiTabWindow (score, -1, 1) == window (score));
}

LUTHIER_TEST (TabExport, midiPerformanceTechniquesReachBothNotationWriters)
{
    auto score = tabScore();
    score.getTrack (0).measures[0].voices[0].notes = {
        tabNote (0, 5, 0, {{ScoreTechnique::Type::bend, 2.0},
                          {ScoreTechnique::Type::vibrato, 5.0},
                          {ScoreTechnique::Type::palmMute, 0.7}}),
        tabNote (1, 7, 1, {{ScoreTechnique::Type::hammerOn}}),
        tabNote (1, 5, 2, {{ScoreTechnique::Type::pullOff}})
    };
    auto performance = MidiPerformance::fromScore (score, 48000.0);
    PerformanceScore restored;
    performance.toScore (restored);
    const auto text = window (restored);
    CHECK (text.contains ("5b~PM"));
    CHECK (text.contains ("7h"));
    CHECK (text.contains ("5p"));
    const auto xml = NotationExporter{}.renderMusicXml (restored);
    CHECK (juce::parseXML (xml) != nullptr);
    CHECK (xml.contains ("<bend-alter>2.00</bend-alter>"));
    CHECK (xml.contains ("palm-mute"));
    CHECK (xml.contains ("<hammer-on"));
    CHECK (xml.contains ("<pull-off"));
    CHECK (xml.contains ("<string>2</string>"));
    CHECK (xml.contains ("<fret>7</fret>"));
}
