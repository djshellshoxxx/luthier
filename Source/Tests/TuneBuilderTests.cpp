/*  The Tune Builder's model: tune-builder.md 1, 4, 5, 6, 8, 9.2, 10, 11 and the
    model-level tests of section 15, with file-formats.md 13, 14 and 16 for the
    `.luthiertune` file.

    What needs the TUNE tab, a render or a microphone (15's export-audio null,
    Luthier-profile re-render, sung capture and standalone relaunch) is not
    here: those are for the panel and the audio path once they are wired.
*/

#include "TestFramework.h"

#include "../Tune/TuneTemplates.h"
#include "../Tune/TuneMidi.h"
#include "../Rhythm/GenreKit.h"
#include "../Rhythm/Patterns.h"

#include <algorithm>
#include <map>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    ChordCell chord (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    TuneSection makeSection (const char* name, int bars, std::vector<ChordCell> chords)
    {
        TuneSection s;
        s.name = name;
        s.lengthBars = bars;
        s.chords = std::move (chords);
        s.rhythmPatternId = "Folk Down Up";
        s.genreKitId = "Folk Fingerstyle";
        return s;
    }

    /** Verse (2 bars: Am, F) and Chorus (1 bar: G), played Verse x2, Chorus. */
    Tune makeSongTune()
    {
        Tune t;
        t.meta.title = "Test Song";
        t.meta.tempoBpm = 120.0;

        auto verse = makeSection ("Verse", 2, { chord ("Am"), chord ("F") });

        MelodyTrack melody;
        melody.notes.push_back (MelodyNote::make (0.0, 1.0, 72, 100));

        auto clipped = MelodyNote::make (1.5, 0.5, 74, 90);
        clipped.articulation = NoteArticulation::staccato;
        melody.notes.push_back (clipped);

        verse.melody = melody;
        verse.bass.mode = BassMode::root;

        t.addSection (verse);
        t.addSection (makeSection ("Chorus", 1, { chord ("G") }));

        TuneSetlistEntry a;
        a.section = "Verse";
        a.repeats = 2;

        TuneSetlistEntry b;
        b.section = "Chorus";

        t.setSetlist ({ a, b });
        return t;
    }

    double pick (RtRandom& rng, std::initializer_list<double> values)
    {
        return *(values.begin() + rng.nextInt ((int) values.size()));
    }

    const char* const kQualities[] = { "", "m", "7", "maj7", "m7", "dim", "aug", "sus2", "sus4", "m7b5",
                                       "9", "13", "6", "6/9", "add9", "5", "7#9", "mMaj7", "dim7", "m9" };
    constexpr int kNumQualities = (int) (sizeof (kQualities) / sizeof (kQualities[0]));

    const char* const kExtensions[] = { "9", "b9", "#11", "13", "add9", "b13" };

    MelodyNote randomNote (RtRandom& rng, double lengthBeats)
    {
        MelodyNote n;
        n.startBeat = tunetheory::canonical ((double) rng.nextInt (juce::jmax (1, (int) (lengthBeats * 3.0))) / 3.0);
        n.durationBeats = tunetheory::canonical (pick (rng, { 0.25, 0.5, 1.0, 1.0 / 3.0, 2.0 }));

        switch (rng.nextInt (3))
        {
            case 0:  n.pitch = MelodyPitch::absolute (36 + rng.nextInt (60)); break;
            case 1:  n.pitch = MelodyPitch::rootOffset (rng.nextInt (25) - 12); break;
            default: n.pitch = MelodyPitch::chordTone (1 + rng.nextInt (6)); break;
        }

        n.velocity = 1 + rng.nextInt (127);
        n.articulation = (NoteArticulation) rng.nextInt ((int) NoteArticulation::numArticulations);
        n.technique = (NoteTechnique) rng.nextInt ((int) NoteTechnique::numTechniques);
        n.locked = rng.nextInt (2) == 1;

        if (rng.nextInt (10) == 0)
            n.extra.set ("future_note_field", "x");

        return n;
    }

    /** A tune that uses every field, with values on the grids the model keeps. */
    Tune makeRandomTune (RtRandom& rng)
    {
        Tune t;
        t.meta.title = "Random " + juce::String (rng.nextInt (1000));
        t.meta.artist = rng.nextInt (2) == 0 ? juce::String ("Someone") : juce::String();
        t.meta.tempoBpm = tunetheory::canonical (60.0 + 0.5 * (double) rng.nextInt (280));

        static const int signatures[5][2] = { { 4, 4 }, { 3, 4 }, { 6, 8 }, { 5, 4 }, { 7, 8 } };
        const int sig = rng.nextInt (5);
        t.meta.timeSigNumerator = signatures[sig][0];
        t.meta.timeSigDenominator = signatures[sig][1];

        t.meta.keyTonic = rng.nextInt (12);
        t.meta.mode = (TuneMode) rng.nextInt (7);
        t.meta.swingPercent = tunetheory::canonical ((double) rng.nextInt (101));
        t.meta.feelPercent = tunetheory::canonical (0.5 * (double) rng.nextInt (201));
        t.meta.tags.add ("random");

        const double beatsPerBar = t.getBeatsPerBar();
        const int numSections = 1 + rng.nextInt (5);

        for (int i = 0; i < numSections; ++i)
        {
            TuneSection s;
            s.name = "Section " + juce::String (i + 1);
            s.lengthBars = 1 + rng.nextInt (16);
            s.rhythmPatternId = rng.nextInt (2) == 0 ? "Folk Down Up" : "Bossa Comp";
            s.genreKitId = rng.nextInt (2) == 0 ? "Folk Fingerstyle" : "Bossa Nova";

            const int numChords = rng.nextInt (9);

            for (int c = 0; c < numChords; ++c)
            {
                auto cell = ChordCell::make (rng.nextInt (12), kQualities[rng.nextInt (kNumQualities)],
                                             pick (rng, { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0 }));

                if (rng.nextInt (4) == 0)
                    cell.bass = (cell.root + 1 + rng.nextInt (11)) % 12;

                if (rng.nextInt (4) == 0)
                    cell.extensions.add (kExtensions[rng.nextInt (6)]);

                if (rng.nextInt (6) == 0)
                    cell.strumOverride = "Classic Strum";

                cell.emphasis = (ChordEmphasis) rng.nextInt (3);
                s.chords.push_back (cell);
            }

            if (numChords > 0 && rng.nextInt (4) == 0)
                s.chords.back().durationBeats = 0.0;

            const double length = (double) s.lengthBars * beatsPerBar;

            if (rng.nextInt (2) == 0)
            {
                MelodyTrack m;
                m.stringHint = rng.nextInt (7);
                m.articulationDefault = (NoteArticulation) (1 + rng.nextInt (5));
                m.source = (MelodySource) rng.nextInt ((int) MelodySource::numSources);
                m.seed = rng.nextInt (100000);
                m.density = tunetheory::canonical (0.25 * (double) (1 + rng.nextInt (24)));
                m.rangeLow = 40 + rng.nextInt (20);
                m.rangeHigh = m.rangeLow + 12 + rng.nextInt (24);
                m.followChords = rng.nextInt (2) == 1;

                for (int n = rng.nextInt (20); --n >= 0;)
                    m.notes.push_back (randomNote (rng, length));

                s.melody = m;
            }

            s.bass.mode = (BassMode) rng.nextInt ((int) BassMode::numModes);

            if (s.bass.mode == BassMode::manual)
                for (int n = 1 + rng.nextInt (6); --n >= 0;)
                    s.bass.notes.push_back (randomNote (rng, length));

            for (int type = 0; type < (int) LayerType::numTypes; ++type)
            {
                if (rng.nextInt (3) != 0)
                    continue;

                TuneLayer layer;
                layer.type = (LayerType) type;
                layer.enabled = rng.nextInt (2) == 1;
                layer.volume = tunetheory::canonical ((double) rng.nextInt (101) / 100.0);
                layer.pan = tunetheory::canonical ((double) (rng.nextInt (201) - 100) / 100.0);
                layer.patternId = layer.type == LayerType::arpeggio ? "Travis Picking" : "";
                layer.seed = rng.nextInt (1000);

                if (layer.type == LayerType::countermelody)
                    for (int n = rng.nextInt (8); --n >= 0;)
                        layer.notes.push_back (randomNote (rng, length));

                s.layers.push_back (layer);
            }

            s.role = (SectionRole) rng.nextInt ((int) SectionRole::numRoles);
            s.rhythmOn = rng.nextInt (4) != 0;
            s.rhythmLinkedTo = (i > 0 && rng.nextInt (4) == 0) ? "Section 1" : "";
            s.feel = tunetheory::canonical ((double) rng.nextInt (11) / 10.0);
            s.strum = tunetheory::canonical ((double) rng.nextInt (11) / 10.0);
            s.stateBoundary = rng.nextInt (3) == 0;
            s.style = (MelodyStyle) rng.nextInt ((int) MelodyStyle::numStyles);

            t.arrangement.sections.push_back (s);
        }

        if (rng.nextInt (2) == 0)
        {
            for (int e = 1 + rng.nextInt (6); --e >= 0;)
            {
                TuneSetlistEntry entry;
                entry.section = "Section " + juce::String (1 + rng.nextInt (numSections));
                entry.repeats = 1 + rng.nextInt (4);
                t.arrangement.setlist.push_back (entry);
            }
        }

        if (rng.nextInt (4) == 0)
        {
            TuneVariation v;
            v.name = "B idea";
            v.arrangement = t.arrangement;
            t.variations.push_back (v);
        }

        if (rng.nextInt (3) == 0)
            t.extra.set ("future_top_level", 7);

        if (rng.nextInt (3) == 0)
            t.arrangement.sections[0].extra.set ("future_section", juce::JSON::parse ("{ \"a\": [1, 2], \"b\": \"c\" }"));

        return t;
    }

    bool sameNotes (const std::vector<MelodyNote>& a, const std::vector<MelodyNote>& b)
    {
        return a == b;
    }

    juce::String notesAsJson (const std::vector<MelodyNote>& notes)
    {
        Tune t;
        auto s = makeSection ("S", 1, {});
        MelodyTrack m;
        m.notes = notes;
        s.melody = m;
        t.addSection (s);
        return TuneFile::toJson (t);
    }
}

//==============================================================================
// Theory and chords
//==============================================================================

LUTHIER_TEST (TuneBuilder, everyQualityAliasResolvesToATemplateChordDetectorKnows)
{
    for (const char* alias : { "maj", "major", "M", "min", "minor", "-", "M7", "Maj7", "ma7", "min7", "-7",
                               "M9", "min9", "-9", "min6", "-6", "mM7", "mmaj7", "o", "o7", "+", "+7",
                               "sus", "dom7" })
    {
        juce::String canonical;
        const bool resolved = tunetheory::resolveQuality (alias, canonical);

        CHECK_MSG (resolved && tunetheory::getQualityMask (canonical) != 0,
                   juce::String ("alias \"") + alias + "\" does not resolve to a template");
    }

    for (int i = 0; i < kNumQualities; ++i)
        CHECK_MSG (tunetheory::isKnownQuality (kQualities[i]), juce::String ("unknown quality ") + kQualities[i]);
}

LUTHIER_TEST (TuneBuilder, chordTonesComeInChordToneOrderWithTensionsAbove)
{
    CHECK ((getChordToneIntervals (chord ("C9")) == std::vector<int> { 0, 4, 7, 10, 14 }));
    CHECK ((getChordToneIntervals (chord ("Cm7")) == std::vector<int> { 0, 3, 7, 10 }));
    CHECK ((getChordToneIntervals (chord ("C6/9")) == std::vector<int> { 0, 4, 7, 9, 14 }));
    CHECK ((getChordToneIntervals (chord ("Csus4")) == std::vector<int> { 0, 5, 7 }));
    CHECK ((getChordToneIntervals (chord ("C7(#11)")) == std::vector<int> { 0, 4, 7, 10, 18 }));
    CHECK ((getChordToneIntervals (chord ("Cdim7")) == std::vector<int> { 0, 3, 6, 9 }));

    // A slash chord's bass goes underneath, from the lowest note up.
    CHECK ((voiceChord (chord ("C/E"), 40) == std::vector<int> { 40, 48, 55 }));

    // Am on a guitar: A2 upward.
    CHECK ((voiceChord (chord ("Am"), 40) == std::vector<int> { 45, 48, 52 }));
}

LUTHIER_TEST (TuneBuilder, keysSpellAndParseTheWayMusiciansWriteThem)
{
    CHECK (tunetheory::formatKey (9, TuneMode::aeolian) == "Am");
    CHECK (tunetheory::formatKey (3, TuneMode::ionian) == "Eb");
    CHECK (tunetheory::formatKey (2, TuneMode::aeolian) == "Dm");
    CHECK (tunetheory::formatKey (6, TuneMode::ionian) == "F#");

    int tonic = -1;
    TuneMode mode = TuneMode::ionian;

    CHECK (tunetheory::parseKey ("F#m", tonic, mode) && tonic == 6 && mode == TuneMode::aeolian);
    CHECK (tunetheory::parseKey ("D dorian", tonic, mode) && tonic == 2 && mode == TuneMode::dorian);
    CHECK (tunetheory::parseKey ("Bb", tonic, mode) && tonic == 10 && mode == TuneMode::ionian);
    CHECK (! tunetheory::parseKey ("H", tonic, mode));

    CHECK (tunetheory::snapToScale (61, 0, TuneMode::ionian) == 60);   // C# -> C, the lower of a tie
    CHECK (tunetheory::moveByScaleSteps (64, 1, 0, TuneMode::ionian) == 65);
    CHECK (tunetheory::moveByScaleSteps (64, -2, 0, TuneMode::ionian) == 60);
}

LUTHIER_TEST (TuneBuilder, chordSpansRepeatAShortProgressionAndHoldAnUnderspecifiedLastCell)
{
    // 1.1: built from cells until the section is full...
    const auto cycling = resolveChordSpans (makeSection ("S", 8, { chord ("Am"), chord ("F") }), 4.0);

    CHECK (cycling.size() == 8);
    CHECK (cycling[2].cellIndex == 0 && cycling[2].startBeat == 8.0);
    CHECK (cycling.back().endBeat == 32.0);

    // ...unless the last cell's duration is left open, when it holds to fill.
    auto holding = makeSection ("S", 4, { chord ("Am"), chord ("F", 0.0) });
    const auto held = resolveChordSpans (holding, 4.0);

    CHECK (held.size() == 2);
    CHECK (held[1].startBeat == 4.0 && held[1].endBeat == 16.0);

    // A cell longer than the section stops at the section's end.
    const auto clipped = resolveChordSpans (makeSection ("S", 1, { chord ("C", 12.0) }), 4.0);
    CHECK (clipped.size() == 1 && clipped[0].endBeat == 4.0);

    // No chords, no spans.
    CHECK (resolveChordSpans (makeSection ("S", 4, {}), 4.0).empty());
}

//==============================================================================
// Progression shorthand (2.2; 15: "100 shorthand strings parse to expected
// ChordCell arrays; malformed strings produce named errors")
//==============================================================================

LUTHIER_TEST (TuneBuilder, shorthandParsesToTheExpectedCells)
{
    {
        const auto r = parseProgression ("Am F C G", 4.0);
        CHECK_MSG (r.ok(), r.describe());
        CHECK (r.sections.size() == 1 && r.sections[0].name.isEmpty() && r.sections[0].bars == 4);

        const auto& c = r.sections[0].cells;
        CHECK (c.size() == 4);
        CHECK (c[0].root == 9 && c[0].quality == "m" && c[0].durationBeats == 4.0);
        CHECK (c[1].root == 5 && c[1].quality.isEmpty());
        CHECK (c[3].root == 7 && c[3].quality.isEmpty());
    }

    {
        const auto r = parseProgression ("| Am7 | D7 | Gmaj7 | Cmaj7 |", 4.0);
        CHECK_MSG (r.ok(), r.describe());
        const auto& c = r.sections[0].cells;
        CHECK (c.size() == 4);
        CHECK (c[0].quality == "m7" && c[1].quality == "7" && c[2].quality == "maj7" && c[3].root == 0);
    }

    {
        const auto r = parseProgression ("[Verse] Am F C G [Chorus] F C G Am", 4.0);
        CHECK_MSG (r.ok(), r.describe());
        CHECK (r.sections.size() == 2);
        CHECK (r.sections[0].name == "Verse" && r.sections[1].name == "Chorus");
        CHECK (r.sections[1].cells.size() == 4 && r.sections[1].cells[3].root == 9);
    }

    {
        // "Am*2 doubles the duration, Am*0.5 halves it."
        const auto r = parseProgression ("Am*2 F*0.5 C*0.5", 4.0);
        CHECK_MSG (r.ok(), r.describe());
        const auto& c = r.sections[0].cells;
        CHECK (c[0].durationBeats == 8.0 && c[1].durationBeats == 2.0 && c[2].durationBeats == 2.0);
        CHECK (r.sections[0].bars == 3);
    }

    {
        // Inside pipes a bar's chords share it.
        const auto r = parseProgression ("| Am F | C |", 4.0);
        CHECK_MSG (r.ok(), r.describe());
        const auto& c = r.sections[0].cells;
        CHECK (c.size() == 3 && c[0].durationBeats == 2.0 && c[1].durationBeats == 2.0 && c[2].durationBeats == 4.0);
    }

    {
        // In 3/4 a bar is three beats.
        const auto r = parseProgression ("G C", 3.0);
        CHECK (r.ok() && r.sections[0].cells[0].durationBeats == 3.0);
    }

    struct Case { const char* text; int root; const char* quality; int bass; };

    const Case cases[] =
    {
        { "C/E", 0, "", 4 },        { "Cmaj", 0, "", -1 },      { "Cmin7", 0, "m7", -1 },
        { "C-7", 0, "m7", -1 },     { "CM7", 0, "maj7", -1 },   { "Co7", 0, "dim7", -1 },
        { "C+", 0, "aug", -1 },     { "Csus", 0, "sus4", -1 },  { "Bb7", 10, "7", -1 },
        { "F#m7b5", 6, "m7b5", -1 }, { "Eb6/9", 3, "6/9", -1 }, { "C6/9/E", 0, "6/9", 4 },
        { "am", 9, "m", -1 },       { "C/C", 0, "", -1 },       { "Dbmaj9", 1, "maj9", -1 },
        { "G13", 7, "13", -1 },     { "E5", 4, "5", -1 },       { "Asus2/E", 9, "sus2", 4 }
    };

    for (const auto& c : cases)
    {
        const auto r = parseProgression (c.text, 4.0);
        const bool ok = r.ok() && r.sections.size() == 1 && r.sections[0].cells.size() == 1;
        CHECK_MSG (ok, juce::String (c.text) + ": " + r.describe());

        if (ok)
        {
            const auto& cell = r.sections[0].cells[0];
            CHECK_MSG (cell.root == c.root && cell.quality == c.quality && cell.bass == c.bass,
                       juce::String (c.text) + " parsed as " + getChordSymbol (cell, false));
        }
    }

    {
        ChordCell cell;
        ProgressionError error;
        CHECK (parseChordSymbol ("C7(b9,#11)", cell, error));
        CHECK ((cell.quality == "7" && cell.extensions == juce::StringArray { "b9", "#11" }));
    }

    {
        const auto r = parseProgression ("C G*fill", 4.0);
        CHECK (r.ok() && r.sections[0].cells[1].holdsToFill());
    }
}

LUTHIER_TEST (TuneBuilder, aHundredGeneratedProgressionsRoundTripThroughShorthand)
{
    RtRandom rng (0x70726F67ull);

    for (int run = 0; run < 100; ++run)
    {
        const bool flats = rng.nextInt (2) == 1;
        std::vector<ChordCell> cells;

        for (int i = 1 + rng.nextInt (8); --i >= 0;)
        {
            auto c = ChordCell::make (rng.nextInt (12), kQualities[rng.nextInt (kNumQualities)],
                                      pick (rng, { 4.0, 8.0, 2.0, 1.0, 6.0, 3.0, 12.0 }));

            if (rng.nextInt (4) == 0)
                c.bass = (c.root + 1 + rng.nextInt (11)) % 12;

            if (rng.nextInt (4) == 0)
                c.extensions.add (kExtensions[rng.nextInt (6)]);

            cells.push_back (c);
        }

        if (rng.nextInt (5) == 0)
            cells.back().durationBeats = 0.0;

        const auto text = formatProgression (cells, 4.0, flats);
        const auto r = parseProgression (text, 4.0);

        const bool same = r.ok() && r.sections.size() == 1 && r.sections[0].cells == cells;
        CHECK_MSG (same, "\"" + text + "\" -> " + (r.ok() ? formatProgression (r.sections[0].cells, 4.0, flats)
                                                          : r.describe()));
    }
}

LUTHIER_TEST (TuneBuilder, malformedShorthandIsRefusedWithANamedError)
{
    struct Case { const char* text; ProgressionError error; };

    const Case cases[] =
    {
        { "H7",              ProgressionError::unknownRoot },
        { "Am 7 F",          ProgressionError::unknownRoot },
        { "Cfoo",            ProgressionError::unknownQuality },
        { "C7(x)",           ProgressionError::unknownQuality },
        { "Cmaj7x",          ProgressionError::unknownQuality },
        { "C/H",             ProgressionError::badSlashBass },
        { "C/",              ProgressionError::badSlashBass },
        { "Am*0",            ProgressionError::badDuration },
        { "Am*x",            ProgressionError::badDuration },
        { "Am*99",           ProgressionError::badDuration },
        { "Am*1.2.3",        ProgressionError::badDuration },
        { "[Verse Am F",     ProgressionError::unclosedSection },
        { "[] Am",           ProgressionError::emptySectionName },
        { "Am ] F",          ProgressionError::unexpectedCharacter },
        { "Am *2",           ProgressionError::unexpectedCharacter },
        { "| Am | | F |",    ProgressionError::emptyBar }
    };

    for (const auto& c : cases)
    {
        const auto r = parseProgression (c.text, 4.0);
        CHECK_MSG (r.error == c.error,
                   juce::String (c.text) + ": expected " + getProgressionErrorName (c.error)
                     + ", got " + getProgressionErrorName (r.error));
        CHECK (juce::String (getProgressionErrorName (r.error)).isNotEmpty());
    }

    // The position points at the offending token.
    const auto r = parseProgression ("Am Cfoo", 4.0);
    CHECK (r.position == 3 && r.token == "Cfoo");

    // A refused edit changes nothing.
    auto tune = makeSongTune();
    const auto before = tune;
    CHECK (! applyProgressionText (tune, 0, "Am Cfoo").ok());
    CHECK (tune == before);
}

LUTHIER_TEST (TuneBuilder, typedShorthandReplacesTheActiveSectionOrTheNamedOnes)
{
    auto tune = makeSongTune();

    CHECK (applyProgressionText (tune, 0, "Dm G C").ok());
    CHECK (tune.arrangement.sections[0].chords.size() == 3);
    CHECK (tune.arrangement.sections[0].lengthBars == 3);   // grown to fit, never shrunk

    CHECK (applyProgressionText (tune, 0, "[Chorus] F G C [Bridge] Em Am").ok());
    CHECK (tune.findSection ("Bridge") >= 0);
    CHECK (tune.arrangement.sections[(size_t) tune.findSection ("Chorus")].chords.size() == 3);

    // The setlist existed, so the new section joins it.
    CHECK (tune.arrangement.setlist.back().section == "Bridge");
}

//==============================================================================
// Section 5 tools
//==============================================================================

LUTHIER_TEST (TuneBuilder, diatonicPaletteAndFunctionsFollowTheKey)
{
    CHECK (makeDiatonicChord (0, TuneMode::ionian, 2, false, 4.0).quality == "m");
    CHECK (makeDiatonicChord (0, TuneMode::ionian, 5, true, 4.0).quality == "7");
    CHECK (makeDiatonicChord (0, TuneMode::ionian, 7, true, 4.0).quality == "m7b5");
    CHECK (makeDiatonicChord (9, TuneMode::aeolian, 1, false, 4.0).root == 9);

    CHECK (getDiatonicDegree (chord ("G7"), 0, TuneMode::ionian) == 5);
    CHECK (getDiatonicDegree (chord ("Bb"), 0, TuneMode::ionian) == 0);
    CHECK (getRomanNumeral (chord ("Dm7"), 0, TuneMode::ionian) == "ii7");
    CHECK (getRomanNumeral (chord ("Bdim"), 0, TuneMode::ionian) == "viio");
}

LUTHIER_TEST (TuneBuilder, suggestNextChordOffersThreeDistinctCommonMoves)
{
    const auto g = chord ("G");
    const auto fromV = suggestNextChords (&g, 0, TuneMode::ionian, "", 4.0);

    CHECK (fromV.size() == 3);
    CHECK (fromV[0].root == 0 && fromV[0].quality.isEmpty());   // V -> I first
    CHECK (fromV[1].root == 9 && fromV[1].quality == "m");      // then vi

    const auto fresh = suggestNextChords (nullptr, 0, TuneMode::ionian, "", 4.0);
    CHECK (fresh.size() == 3 && fresh[0].root == 0 && fresh[1].root == 5 && fresh[2].root == 7);

    // Blues suggests dominant sevenths; a C7 out of the key resolves to F7.
    const auto c7 = chord ("C7");
    const auto blues = suggestNextChords (&c7, 0, TuneMode::ionian, "Chicago Blues", 4.0);
    CHECK (blues.size() == 3 && blues[0].root == 5 && blues[0].quality == "7");

    // Substitutions for a dominant include the tritone sub and a ii-V.
    const std::vector<ChordCell> progression { chord ("Dm7"), chord ("G7"), chord ("Cmaj7") };
    const auto offers = suggestSubstitutions (progression, 1, 0, TuneMode::ionian);

    bool tritone = false, twoFive = false;

    for (const auto& o : offers)
    {
        tritone = tritone || (o.name == "Tritone substitution" && o.cells.size() == 1 && o.cells[0].root == 1);
        twoFive = twoFive || (o.name == "ii-V into next chord" && o.cells.size() == 2);
    }

    CHECK (tritone);
    CHECK (twoFive);
}

LUTHIER_TEST (TuneBuilder, reharmonizeSubstitutesAndKeepsTheLength)
{
    auto total = [] (const std::vector<ChordCell>& cells)
    {
        double sum = 0.0;
        for (const auto& c : cells) sum += c.durationBeats;
        return sum;
    };

    ReharmonizeOptions tritoneOnly;
    tritoneOnly.secondaryDominants = false;
    tritoneOnly.modalInterchange = false;

    const std::vector<ChordCell> twoFiveOne { chord ("Dm7"), chord ("G7"), chord ("Cmaj7") };
    const auto subbed = reharmonize (twoFiveOne, 0, TuneMode::ionian, tritoneOnly);

    CHECK (subbed.size() == 3 && subbed[1].root == 1 && subbed[1].quality == "7");   // G7 -> Db7
    CHECK (total (subbed) == total (twoFiveOne));

    ReharmonizeOptions dominantsOnly;
    dominantsOnly.tritoneSubstitutions = false;
    dominantsOnly.modalInterchange = false;

    const std::vector<ChordCell> popChords { chord ("C"), chord ("Am"), chord ("Dm"), chord ("G") };
    const auto withDominants = reharmonize (popChords, 0, TuneMode::ionian, dominantsOnly);

    CHECK (withDominants.size() == 7);
    CHECK (withDominants[1].root == 4 && withDominants[1].quality == "7");   // E7 before Am
    CHECK (total (withDominants) == total (popChords));

    ReharmonizeOptions borrowOnly;
    borrowOnly.tritoneSubstitutions = false;
    borrowOnly.secondaryDominants = false;

    const std::vector<ChordCell> plagal { chord ("F"), chord ("C") };
    CHECK (reharmonize (plagal, 0, TuneMode::ionian, borrowOnly)[0].quality == "m");   // IV -> iv
}

LUTHIER_TEST (TuneBuilder, transposeMovesChordsKeyAndAbsoluteMelodyAndIsReversible)
{
    auto tune = makeSongTune();
    tune.arrangement.sections[0].melody->notes.push_back (MelodyNote::make (3.0, 1.0, 60, 90));
    tune.arrangement.sections[0].melody->notes.back().pitch = MelodyPitch::rootOffset (7);
    tune.storeVariation ("B");

    const auto original = tune;

    CHECK (transposeTune (tune, 2));
    CHECK (tune.meta.keyTonic == 2);
    CHECK (tune.arrangement.sections[0].chords[0].root == 11);          // Am -> Bm
    CHECK (tune.arrangement.sections[0].melody->notes[0].pitch.value == 74);
    CHECK (tune.arrangement.sections[0].melody->notes[2].pitch == MelodyPitch::rootOffset (7));
    CHECK (tune.variations[0].arrangement.sections[0].chords[0].root == 11);

    CHECK (transposeTune (tune, -2));
    CHECK (tune == original);
    CHECK (! transposeTune (tune, 0));
}

LUTHIER_TEST (TuneBuilder, modalShiftMovesDiatonicChordsAndOptionallyTheMelody)
{
    Tune tune;
    auto s = makeSection ("S", 4, { chord ("C"), chord ("F"), chord ("G"), chord ("Dm7") });
    MelodyTrack m;
    m.notes.push_back (MelodyNote::make (0.0, 1.0, 64, 90));   // E, the major third
    m.notes.push_back (MelodyNote::make (1.0, 1.0, 62, 90));   // D, the same in both modes
    s.melody = m;
    tune.addSection (s);

    auto kept = tune;
    CHECK (shiftMode (kept, TuneMode::aeolian, false));
    CHECK (kept.meta.mode == TuneMode::aeolian);
    CHECK (kept.arrangement.sections[0].chords[0].quality == "m");      // I -> i
    CHECK (kept.arrangement.sections[0].chords[3].quality == "m7b5");   // ii7 -> half-diminished ii
    CHECK (kept.arrangement.sections[0].melody->notes[0].pitch.value == 64);

    auto followed = tune;
    CHECK (shiftMode (followed, TuneMode::aeolian, true));
    CHECK (followed.arrangement.sections[0].melody->notes[0].pitch.value == 63);
    CHECK (followed.arrangement.sections[0].melody->notes[1].pitch.value == 62);

    CHECK (! shiftMode (followed, TuneMode::aeolian, true));
}

//==============================================================================
// Melody (4) and bass (6)
//==============================================================================

LUTHIER_TEST (TuneBuilder, autoMelodyIsByteIdenticalForASeedAcrossAThousandRuns)
{
    auto tune = makeSongTune();
    tune.arrangement.sections[0].lengthBars = 8;
    tune.arrangement.sections[0].melody->notes.clear();

    const auto first = generateAutoMelody (tune, 0, 42);
    const auto firstText = notesAsJson (first);

    CHECK (! first.empty());

    bool allSame = true;

    for (int run = 0; run < 1000 && allSame; ++run)
        allSame = notesAsJson (generateAutoMelody (tune, 0, 42)) == firstText;

    CHECK (allSame);

    // A different seed is a different melody.
    CHECK (notesAsJson (generateAutoMelody (tune, 0, 43)) != firstText);
}

LUTHIER_TEST (TuneBuilder, autoMelodyStaysInRangeRestsAtCadencesAndStartsOnAChordTone)
{
    auto tune = makeSongTune();
    auto& verse = tune.arrangement.sections[0];
    verse.lengthBars = 8;
    verse.melody->notes.clear();

    for (int seed = 1; seed <= 50; ++seed)
    {
        const auto notes = generateAutoMelody (tune, 0, seed);
        CHECK (! notes.empty());

        for (const auto& n : notes)
        {
            const int pitch = n.pitch.value;
            CHECK (pitch >= 48 && pitch <= 79);                        // C3..G5

            // 4.1: the last beat of each bar and the last bar rest.
            const double inBar = std::fmod (n.startBeat, 4.0);
            CHECK (inBar < 3.0);
            CHECK (n.getEndBeat() <= 28.0 + 1.0e-9);
        }

        // "Start on a chord tone of the first chord (default third)": C over Am.
        CHECK (tunetheory::wrapPitchClass (notes.front().pitch.value) == 0);
    }
}

LUTHIER_TEST (TuneBuilder, regenerateLeavesLockedNotesByteIdentical)
{
    auto tune = makeSongTune();
    auto& verse = tune.arrangement.sections[0];
    verse.lengthBars = 8;
    verse.melody->notes.clear();

    auto lockedA = MelodyNote::make (2.0, 1.5, 67, 111);
    lockedA.locked = true;
    lockedA.technique = NoteTechnique::bend;
    lockedA.extra.set ("future_note_field", 5);

    auto lockedB = MelodyNote::make (9.0, 0.5, 60, 50);
    lockedB.locked = true;
    lockedB.pitch = MelodyPitch::chordTone (3);

    verse.melody->notes = { lockedA, lockedB };

    for (int round = 0; round < 50; ++round)
    {
        CHECK (regenerateMelody (tune, 0));

        const auto& notes = tune.arrangement.sections[0].melody->notes;
        int foundA = 0, foundB = 0;

        for (const auto& n : notes)
        {
            foundA += n == lockedA ? 1 : 0;
            foundB += n == lockedB ? 1 : 0;

            // Nothing generated sits on top of a locked note.
            if (! n.locked)
                for (const auto& l : { lockedA, lockedB })
                    CHECK (n.getEndBeat() <= l.startBeat + 1.0e-9 || n.startBeat >= l.getEndBeat() - 1.0e-9);
        }

        CHECK (foundA == 1 && foundB == 1);
    }

    CHECK (tune.arrangement.sections[0].melody->seed == 51);
}

LUTHIER_TEST (TuneBuilder, improviseVariesEachPassAndFreezeWritesItDown)
{
    auto tune = makeSongTune();
    auto& verse = tune.arrangement.sections[0];
    verse.lengthBars = 8;
    verse.melody->source = MelodySource::improvise;
    verse.melody->notes.clear();

    auto locked = MelodyNote::make (4.0, 1.0, 69, 100);
    locked.locked = true;
    verse.melody->notes.push_back (locked);

    const auto pass0 = generateImprovisedPass (tune, 0, 0);
    const auto pass1 = generateImprovisedPass (tune, 0, 1);

    CHECK (! sameNotes (pass0, pass1));
    CHECK (sameNotes (pass0, generateImprovisedPass (tune, 0, 0)));
    CHECK (std::count (pass0.begin(), pass0.end(), locked) == 1);
    CHECK (std::count (pass1.begin(), pass1.end(), locked) == 1);

    CHECK (freezeImprovisedPass (tune, 0, 1));
    CHECK (sameNotes (tune.arrangement.sections[0].melody->notes, pass1));
    CHECK (tune.arrangement.sections[0].melody->source == MelodySource::draw);

    // The timeline asks to be rebuilt per pass only while Improvise is on.
    auto improvising = makeSongTune();
    improvising.arrangement.sections[0].melody->source = MelodySource::improvise;
    CHECK (TuneTimeline::build (improvising).needsRebuildEachPass());
    CHECK (! TuneTimeline::build (tune).needsRebuildEachPass());
}

LUTHIER_TEST (TuneBuilder, relativePitchesFollowTheChordTheyLandOn)
{
    Tune tune;
    tune.addSection (makeSection ("S", 2, { chord ("C"), chord ("G") }));

    auto fifth = MelodyNote::make (0.0, 1.0, 60, 100);
    fifth.pitch = MelodyPitch::rootOffset (7);

    auto third = fifth;
    third.pitch = MelodyPitch::chordTone (2);

    CHECK (resolveNotePitch (tune, 0, fifth) == 67);    // G over C
    CHECK (resolveNotePitch (tune, 0, third) == 64);    // E over C

    fifth.startBeat = 4.0;
    third.startBeat = 4.0;
    CHECK (resolveNotePitch (tune, 0, fifth) == 74);    // D over G
    CHECK (resolveNotePitch (tune, 0, third) == 71);    // B over G

    // Past the chord's tones the count carries on an octave up.
    auto high = third;
    high.pitch = MelodyPitch::chordTone (4);
    CHECK (resolveNotePitch (tune, 0, high) == 79);     // G5, the root again
}

LUTHIER_TEST (TuneBuilder, recordQuantiseKeepsVelocityAndRefitsHeldNotesToTheNewChord)
{
    Tune tune;
    tune.addSection (makeSection ("S", 2, { chord ("C"), chord ("F") }));

    const std::vector<RecordedNote> played
    {
        { 0.1, 0.9, 61, 77 },    // C#: snapped to key
        { 3.4, 5.1, 64, 90 }     // E held across the change to F
    };

    const auto notes = quantiseRecording (played, QuantiseGrid::eighth, tune, 0, true, true);

    CHECK (notes.size() == 3);
    CHECK (notes[0].startBeat == 0.0 && notes[0].durationBeats == 1.0);
    CHECK (notes[0].pitch.value == 60 && notes[0].velocity == 77);

    CHECK (notes[1].startBeat == 3.5 && notes[1].getEndBeat() == 4.0 && notes[1].pitch.value == 64);
    CHECK (notes[2].startBeat == 4.0 && notes[2].pitch.value == 65);   // E is not in F; F is
    CHECK (notes[2].velocity == 90);

    for (const auto& n : notes)
        CHECK (n.locked);

    // Triplet grids land on thirds of a beat.
    const auto triplet = quantiseRecording ({ { 0.3, 0.7, 60, 100 } }, QuantiseGrid::eighthTriplet, tune, 0, false, false);
    CHECK (triplet.size() == 1 && triplet[0].startBeat == tunetheory::canonical (1.0 / 3.0));
    CHECK_NEAR (triplet[0].getEndBeat(), 2.0 / 3.0, 1.0e-5);
}

LUTHIER_TEST (TuneBuilder, styleTransferChangesPhrasingButNeverPitchOrCount)
{
    RtRandom rng (0x5354594Cull);
    std::vector<MelodyNote> notes;

    for (int i = 0; i < 40; ++i)
    {
        auto n = randomNote (rng, 32.0);
        n.pitch = MelodyPitch::absolute (50 + rng.nextInt (30));
        notes.push_back (n);
    }

    for (int s = 0; s < (int) MelodyStyle::numStyles; ++s)
    {
        const auto styled = applyMelodyStyle (notes, (MelodyStyle) s);

        CHECK (styled.size() == notes.size());

        for (size_t i = 0; i < notes.size() && i < styled.size(); ++i)
            CHECK (styled[i].pitch == notes[i].pitch);
    }

    // Jazz sax swings an offbeat eighth to the triplet.
    const auto swung = applyMelodyStyle ({ MelodyNote::make (0.5, 0.5, 60, 90) }, MelodyStyle::jazzSax);
    CHECK_NEAR (swung[0].startBeat, 0.5 + 1.0 / 6.0, 1.0e-6);
}

LUTHIER_TEST (TuneBuilder, bassLinesFollowTheChordsAndWalkIntoTheNextRoot)
{
    Tune tune;
    tune.addSection (makeSection ("S", 2, { chord ("C"), chord ("G") }));

    tune.setBassMode (0, BassMode::root);
    const auto roots = generateBassLine (tune, 0);

    CHECK (roots.size() == 2);
    CHECK (roots[0].startBeat == 0.0 && roots[0].pitch.value == 36 && roots[0].durationBeats == 4.0);
    CHECK (roots[1].startBeat == 4.0 && roots[1].pitch.value == 31);

    tune.setBassMode (0, BassMode::walking);
    const auto walk = generateBassLine (tune, 0);

    CHECK (walk.size() == 8);
    CHECK (walk[0].pitch.value == 36);
    CHECK (std::abs (walk[3].pitch.value - 31) == 1);   // a half step into G
    CHECK (walk[4].pitch.value == 31);
    CHECK (std::abs (walk[7].pitch.value - 36) == 1);   // and back into C

    tune.setBassMode (0, BassMode::off);
    CHECK (generateBassLine (tune, 0).empty());

    // Genre follows the kit: country is root-fifth.
    tune.setSectionRhythm (0, "Country Boom Chick", "Nashville Country");
    tune.setBassMode (0, BassMode::genre);
    const auto country = generateBassLine (tune, 0);
    CHECK (country.size() == 4 && country[1].pitch.value == 36 + 7);

    // A 3/4 waltz alternates root and fifth bar by bar.
    auto waltz = TuneTemplateLibrary::createBlank();
    waltz.setTimeSignature (3, 4);
    waltz.addSection (makeSection ("W", 2, { chord ("G", 6.0) }));
    waltz.setBassMode (0, BassMode::rootFifth);
    const auto waltzBass = generateBassLine (waltz, 0);
    CHECK (waltzBass.size() == 2 && waltzBass[1].startBeat == 3.0 && waltzBass[1].pitch.value == waltzBass[0].pitch.value + 7);
}

LUTHIER_TEST (TuneBuilder, countermelodyStaysUnderTheMelodyOnChordTones)
{
    auto tune = makeSongTune();
    auto& verse = tune.arrangement.sections[0];
    verse.lengthBars = 8;
    verse.melody->notes = generateAutoMelody (tune, 0, 7);

    const auto counter = generateCountermelody (tune, 0, 3);
    CHECK (! counter.empty());
    CHECK (notesAsJson (counter) == notesAsJson (generateCountermelody (tune, 0, 3)));

    for (const auto& n : counter)
    {
        CHECK (n.pitch.value >= verse.melody->rangeLow - 5 && n.pitch.value <= verse.melody->rangeLow + 19);
        CHECK (n.getEndBeat() <= tune.getSectionLengthBeats (0) + 1.0e-9);
    }
}

//==============================================================================
// Sections, setlist and edits
//==============================================================================

LUTHIER_TEST (TuneBuilder, editsKeepNamesUniqueAndTheSetlistInStep)
{
    auto tune = makeSongTune();

    CHECK (tune.duplicateSection (0) == 1);
    CHECK (tune.arrangement.sections[1].name == "Verse 2");

    CHECK (tune.renameSection (0, "Intro"));
    CHECK (tune.arrangement.setlist[0].section == "Intro");
    CHECK (! tune.renameSection (0, "Chorus"));     // taken
    CHECK (! tune.renameSection (0, "Intro"));      // no change

    CHECK (tune.removeSection (tune.findSection ("Chorus")));
    CHECK (tune.arrangement.setlist.size() == 1);

    CHECK (! tune.setSectionLength (0, tune.arrangement.sections[0].lengthBars));
    CHECK (tune.setSectionLength (0, 4));

    // Repeat count on a tune without a setlist writes one that keeps the order.
    Tune plain;
    plain.addSection (makeSection ("A", 2, {}));
    plain.addSection (makeSection ("B", 2, {}));
    const double before = plain.getTotalBeats();
    CHECK (plain.setRepeatCount (1, 3));
    CHECK (plain.arrangement.setlist.size() == 2 && plain.arrangement.setlist[1].repeats == 3);
    CHECK (plain.getTotalBeats() == before + 2.0 * 8.0);

    // Drawn notes are manual edits, so they lock.
    const int index = plain.addMelodyNote (0, MelodyNote::make (0.0, 1.0, 60, 100));
    CHECK (index == 0 && plain.arrangement.sections[0].melody->notes[0].locked);
    CHECK (plain.setMelodyNoteLocked (0, 0, false));
    CHECK (plain.nudgeMelodyNotes (0, { 0 }, 0.5, 2));
    CHECK (plain.arrangement.sections[0].melody->notes[0].pitch.value == 62);
    CHECK (plain.arrangement.sections[0].melody->notes[0].locked);

    // A/B.
    const auto a = plain.arrangement;
    CHECK (plain.storeVariation ("A") == 0);
    plain.removeSection (0);
    CHECK (plain.swapWithVariation (0));
    CHECK (plain.arrangement == a);

    CHECK (juce::String (getTuneEditClassName (TuneEditClass::melodyGenerate)) == "tune-melody-generate");
}

LUTHIER_TEST (TuneBuilder, aThousandSectionReordersKeepTheLengthAndEveryNotesPosition)
{
    RtRandom rng (0x52454F52ull);

    for (int trial = 0; trial < 4; ++trial)
    {
        auto tune = makeRandomTune (rng);

        const double total = tune.getTotalBeats();
        std::map<juce::String, TuneSection> byName;

        for (const auto& s : tune.arrangement.sections)
            byName[s.name] = s;

        for (int move = 0; move < 250; ++move)
        {
            const int n = tune.getNumSections();
            tune.moveSection (rng.nextInt (n), rng.nextInt (n));

            CHECK (tune.getTotalBeats() == total);
        }

        for (const auto& s : tune.arrangement.sections)
            CHECK (s == byName[s.name]);
    }
}

//==============================================================================
// .luthiertune (11; file-formats 13, 14, 16)
//==============================================================================

LUTHIER_TEST (TuneBuilder, aHundredRandomTunesRoundTripByteIdentical)
{
    RtRandom rng (0x524F554Eull);

    for (int run = 0; run < 100; ++run)
    {
        const auto tune = makeRandomTune (rng);
        const auto first = TuneFile::toJson (tune);

        Tune loaded;
        const auto result = TuneFile::fromJson (first, loaded);

        CHECK_MSG (result.ok(), juce::String (getTuneLoadErrorName (result.error)) + " " + result.message);
        CHECK_MSG (TuneFile::toJson (loaded) == first, "run " + juce::String (run) + " saved differently");
        CHECK_MSG (loaded == tune, "run " + juce::String (run) + " loaded as a different tune");
    }
}

LUTHIER_TEST (TuneBuilder, unknownFieldsAreKeptAndWrittenBack)
{
    const juce::String text = R"({
  "schema": 1,
  "magic": "luthier.tune",
  "meta": { "title": "Future", "key": "Am", "future_meta": true },
  "sections": [
    {
      "name": "Verse",
      "length_bars": 2,
      "chords": [ { "root": "A", "quality": "min", "beats": 4, "voicing_hint": "open" } ],
      "melody": [ { "start": 0.0, "dur": 1.0, "pitch": 69, "vel": 90, "ornament": { "kind": "mordent" } } ],
      "future_section": [1, 2, 3]
    }
  ],
  "setlist": [ { "section": "Verse", "repeats": 2, "fade": 0.5 } ],
  "future_top_level": "kept"
})";

    Tune tune;
    const auto result = TuneFile::fromJson (text, tune);
    CHECK_MSG (result.ok(), result.message);

    CHECK (tune.meta.keyTonic == 9 && tune.meta.mode == TuneMode::aeolian);
    CHECK (tune.meta.extra.contains ("future_meta"));
    CHECK (tune.arrangement.sections[0].chords[0].extra.contains ("voicing_hint"));
    CHECK (tune.arrangement.sections[0].melody->notes[0].extra.contains ("ornament"));
    CHECK (tune.arrangement.sections[0].extra.contains ("future_section"));
    CHECK (tune.arrangement.setlist[0].extra.contains ("fade"));
    CHECK (tune.extra.contains ("future_top_level"));

    // Written back, and read again to the same tune.
    const auto saved = TuneFile::toJson (tune);
    CHECK (saved.contains ("\"voicing_hint\": \"open\""));
    CHECK (saved.contains ("\"mordent\""));
    CHECK (saved.contains ("\"future_top_level\": \"kept\""));

    Tune again;
    CHECK (TuneFile::fromJson (saved, again).ok());
    CHECK (again == tune);
    CHECK (TuneFile::toJson (again) == saved);
}

LUTHIER_TEST (TuneBuilder, loadErrorsAreNamedAndLeaveTheTuneAlone)
{
    const auto sentinel = makeSongTune();

    struct Case { const char* text; TuneLoadError error; };

    const Case cases[] =
    {
        { "not json at all",                                                            TuneLoadError::malformedJson },
        { "[1, 2, 3]",                                                                  TuneLoadError::badMagic },
        { "{ \"schema\": 1 }",                                                          TuneLoadError::badMagic },
        { "{ \"magic\": \"luthier.preset\" }",                                          TuneLoadError::badMagic },
        { "{ \"magic\": \"luthier.tune\", \"schema\": 99 }",                            TuneLoadError::unsupportedSchema },
        { "{ \"magic\": \"luthier.tune\", \"sections\": \"x\" }",                       TuneLoadError::invalidField },
        { "{ \"magic\": \"luthier.tune\", \"sections\": [ { \"length_bars\": 4 } ] }",  TuneLoadError::missingField },
        { "{ \"magic\": \"luthier.tune\", \"sections\": [ { \"name\": \"V\", \"chords\": [ { \"root\": \"H\" } ] } ] }",
                                                                                        TuneLoadError::invalidField },
        { "{ \"magic\": \"luthier.tune\", \"meta\": { \"time_sig\": \"4/5\" } }",        TuneLoadError::invalidField },
        { "{ \"magic\": \"luthier.tune\", \"sections\": [ { \"name\": \"V\", \"melody\": [ { \"start\": 0, \"pitch\": \"loud\" } ] } ] }",
                                                                                        TuneLoadError::invalidField }
    };

    for (const auto& c : cases)
    {
        auto tune = sentinel;
        const auto result = TuneFile::fromJson (c.text, tune);

        CHECK_MSG (result.error == c.error,
                   juce::String (c.text) + ": expected " + getTuneLoadErrorName (c.error)
                     + ", got " + getTuneLoadErrorName (result.error) + " (" + result.message + ")");
        CHECK (result.message.isNotEmpty());
        CHECK (tune == sentinel);
    }

    // Bytes that are not UTF-8, and a file that is not there.
    {
        const char bad[] = { '{', '"', (char) 0xC3, (char) 0x28, '"', '}' };
        auto tune = sentinel;
        CHECK (TuneFile::fromBytes (bad, sizeof (bad), tune).error == TuneLoadError::notUtf8);
        CHECK (tune == sentinel);
    }

    {
        auto tune = sentinel;
        const auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("luthier-no-such-tune.luthiertune");
        CHECK (TuneFile::load (missing, tune).error == TuneLoadError::fileNotFound);
        CHECK (tune == sentinel);
    }

    // A tempo out of range loads, clamped, with a warning rather than a refusal.
    {
        Tune tune;
        const auto result = TuneFile::fromJson ("{ \"magic\": \"luthier.tune\", \"meta\": { \"tempo_bpm\": 900 } }", tune);
        CHECK (result.ok() && tune.meta.tempoBpm == Tune::kMaxTempo && result.warnings.size() == 1);
    }

    // A missing schema is schema 1 (file-formats 0.2).
    {
        Tune tune;
        CHECK (TuneFile::fromJson ("{ \"magic\": \"luthier.tune\" }", tune).ok());
    }
}

LUTHIER_TEST (TuneBuilder, tenThousandCorruptedFilesLoadOrRefuseCleanly)
{
    const auto base = TuneFile::toJson (makeSongTune());
    const auto* raw = base.toRawUTF8();
    const auto size = (size_t) base.getNumBytesAsUTF8();

    const auto sentinel = makeSongTune();
    RtRandom rng (0x46555A5Aull);

    int refused = 0;

    for (int run = 0; run < 10000; ++run)
    {
        juce::MemoryBlock bytes (raw, size);
        auto* data = static_cast<char*> (bytes.getData());

        for (int flips = 1 + rng.nextInt (3); --flips >= 0;)
            data[(size_t) rng.nextInt ((int) size)] = (char) rng.nextInt (256);

        auto tune = sentinel;
        const auto result = TuneFile::fromBytes (bytes.getData(), bytes.getSize(), tune);

        if (! result.ok())
        {
            ++refused;

            if (result.message.isEmpty() || ! (tune == sentinel))
            {
                CHECK_MSG (false, "run " + juce::String (run) + " refused without a message or changed the tune");
                break;
            }
        }
    }

    // Almost every mutation breaks the JSON, the magic or a field.
    CHECK (refused > 1000);
}

LUTHIER_TEST (TuneBuilder, saveIsAtomicAndKeepsADatedBackup)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTuneSaveTest");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto target = folder.getChildFile (juce::String ("Sketch") + TuneFile::kFileExtension);
    auto tune = makeSongTune();

    juce::String error;
    CHECK_MSG (TuneFile::save (tune, target, error), error);
    CHECK (! target.getSiblingFile (target.getFileName() + ".tmp").exists());
    CHECK (target.loadFileAsString() == TuneFile::toJson (tune));

    // The second save files the first away.
    tune.setTempo (96.0);
    CHECK_MSG (TuneFile::save (tune, target, error), error);

    const auto backups = TuneFile::getBackupFolder (target);
    CHECK (backups.getChildFile (target.getFileName()).existsAsFile());

    Tune loaded;
    CHECK (TuneFile::load (target, loaded).ok());
    CHECK (loaded == tune);

    // A typical tune is well inside 0.2's "< 100 KB".
    CHECK (target.getSize() < 100 * 1024);

    folder.deleteRecursively();
}

//==============================================================================
// Templates (10)
//==============================================================================

LUTHIER_TEST (TuneBuilder, theTenTemplatesLoadInOrderAndAreValid)
{
    juce::StringArray errors;
    const auto templates = TuneTemplateLibrary::loadFactory (&errors);

    CHECK_MSG ((int) templates.size() == TuneTemplateLibrary::kNumFactoryTemplates,
               juce::String ((int) templates.size()) + " templates; " + errors.joinIntoString ("; "));

    const char* const names[] =
    {
        "Blank", "Verse/Chorus", "12-bar blues in E", "AABA jazz standard", "Reggae one-drop in A",
        "Country waltz", "Rock ballad", "Bossa Nova", "Punk two-chord", "Instrumental fingerstyle"
    };

    if ((int) templates.size() != TuneTemplateLibrary::kNumFactoryTemplates)
        return;

    GenreKitLibrary kits;
    PatternLibrary patterns;

    for (size_t i = 0; i < templates.size(); ++i)
    {
        const auto& t = templates[i];

        CHECK_MSG (t.name == names[i], t.name + " where " + names[i] + " was expected");
        CHECK_MSG (t.tune.validate().isEmpty(), t.name + ": " + t.tune.validate().joinIntoString ("; "));
        CHECK (t.tune.meta.author == "Factory");
        CHECK (t.name.length() < 32);   // factory-content 12

        for (const auto& s : t.tune.arrangement.sections)
        {
            CHECK_MSG (s.genreKitId.isEmpty() || kits.indexOf (s.genreKitId) >= 0, t.name + ": kit " + s.genreKitId);
            CHECK_MSG (s.rhythmPatternId.isEmpty() || patterns.indexOf (s.rhythmPatternId) >= 0,
                       t.name + ": pattern " + s.rhythmPatternId);

            for (const auto& c : s.chords)
                CHECK (c.isValid());
        }

        // Every template plays: it builds a timeline, and anything with chords has notes.
        const auto timeline = TuneTimeline::build (t.tune);
        CHECK (timeline.getLengthPpq() == t.tune.getTotalBeats());
    }

    const auto& blank = templates[0].tune;
    CHECK (blank.getNumSections() == 0);

    const auto& verseChorus = templates[1].tune;
    CHECK (verseChorus.getNumSections() == 2);
    CHECK (verseChorus.arrangement.sections[0].lengthBars == 8 && verseChorus.arrangement.sections[0].chords.empty());

    const auto& blues = templates[2].tune;
    CHECK (blues.meta.keyTonic == 4 && blues.getNumSections() == 3);
    CHECK (blues.findSection ("Head") >= 0 && blues.findSection ("Middle") >= 0 && blues.findSection ("Out") >= 0);

    for (const auto& s : blues.arrangement.sections)
        CHECK (s.lengthBars == 12 && s.chords.size() == 12);

    const auto& aaba = templates[3].tune;
    CHECK (aaba.getTotalBeats() == 32.0 * 4.0);
    CHECK (aaba.arrangement.setlist.size() == 3);

    const auto& reggae = templates[4].tune;
    CHECK (reggae.meta.keyTonic == 9 && reggae.arrangement.sections[0].rhythmPatternId == "Reggae Skank");

    const auto& waltz = templates[5].tune;
    CHECK (waltz.meta.timeSigNumerator == 3 && waltz.meta.timeSigDenominator == 4);
    CHECK (waltz.arrangement.sections[0].rhythmPatternId == "Country Boom Chick");

    const auto& ballad = templates[6].tune;
    CHECK (formatProgression (ballad.arrangement.sections[0].chords, 4.0, false) == "Am F C G");
    CHECK (formatProgression (ballad.arrangement.sections[1].chords, 4.0, false) == "F G Am Am");

    const auto& bossa = templates[7].tune;
    CHECK (bossa.meta.keyTonic == 3 && bossa.meta.mode == TuneMode::ionian);
    CHECK (formatProgression (bossa.arrangement.sections[0].chords, 4.0, true) == "Ebmaj7 Cm7 Fm7 Bb7");

    const auto& punk = templates[8].tune;
    const auto& punkChords = punk.arrangement.sections[0].chords;
    CHECK (punkChords.size() == 2 && getDiatonicDegree (punkChords[0], punk.meta.keyTonic, punk.meta.mode) == 1
             && getDiatonicDegree (punkChords[1], punk.meta.keyTonic, punk.meta.mode) == 4);

    const auto& fingerstyle = templates[9].tune;
    CHECK (fingerstyle.getNumSections() == 1 && fingerstyle.arrangement.sections[0].chords.empty());
    CHECK (fingerstyle.arrangement.sections[0].melody.has_value()
             && fingerstyle.arrangement.sections[0].melody->notes.empty());
}

LUTHIER_TEST (TuneBuilder, templateFilesAreInCanonicalFormAndBlankMatchesTheBuiltIn)
{
    const auto templates = TuneTemplateLibrary::loadFactory();

    // file-formats 16: every factory file loads and saves byte-identical.
    for (const auto& t : templates)
        CHECK_MSG (t.file.loadFileAsString() == TuneFile::toJson (t.tune), t.file.getFileName() + " is not canonical");

    if (! templates.empty())
        CHECK (templates[0].tune == TuneTemplateLibrary::createBlank());

    const auto fresh = TuneTemplateLibrary::instantiate (TuneTemplateLibrary::createBlank());
    CHECK (fresh.meta.title == "Untitled Tune" && fresh.meta.author.isEmpty());
    CHECK (! fresh.meta.tags.contains ("template"));
    CHECK (fresh.meta.created.isNotEmpty());
}

//==============================================================================
// MIDI (8, 9.2)
//==============================================================================

LUTHIER_TEST (TuneBuilder, timelinePutsChordsMelodyBassAndSectionsOnTheBeat)
{
    const auto tune = makeSongTune();
    const auto timeline = TuneTimeline::build (tune);

    CHECK (timeline.getLengthPpq() == 20.0);   // Verse x2 (8 beats each) + Chorus (4)

    auto find = [&] (TunePart part, int pitch, bool on, double ppq)
    {
        for (const auto& e : timeline.getEvents())
            if (e.part == part && e.message.getNoteNumber() == pitch && std::abs (e.ppq - ppq) < 1.0e-9
                  && (on ? e.message.isNoteOn() : e.message.isNoteOff()))
                return true;

        return false;
    };

    // Am (A2 = 45 at the bottom) at the top of each verse, F after it, G in the chorus.
    CHECK (find (TunePart::chords, 45, true, 0.0));
    CHECK (find (TunePart::chords, 45, false, 4.0));
    CHECK (find (TunePart::chords, 45, true, 8.0));
    CHECK (find (TunePart::chords, 41, true, 12.0));
    CHECK (find (TunePart::chords, 43, true, 16.0));

    // Melody, including the staccato note played at half length.
    CHECK (find (TunePart::melody, 72, true, 0.0));
    CHECK (find (TunePart::melody, 72, true, 8.0));
    CHECK (find (TunePart::melody, 74, true, 9.5));
    CHECK (find (TunePart::melody, 74, false, 9.75));

    // Bass: whole notes on the roots, A1 then F1.
    CHECK (find (TunePart::bass, 33, true, 0.0));
    CHECK (find (TunePart::bass, 29, true, 4.0));

    // Channels as configured.
    for (const auto& e : timeline.getEvents())
    {
        if (e.part == TunePart::chords && e.message.isNoteOnOrOff()) CHECK (e.message.getChannel() == 1);
        if (e.part == TunePart::melody && e.message.isNoteOnOrOff()) CHECK (e.message.getChannel() == 2);
        if (e.part == TunePart::bass && e.message.isNoteOnOrOff())   CHECK (e.message.getChannel() == 3);
    }

    // Sorted, and a note ending never comes after one starting at the same time.
    const auto& events = timeline.getEvents();

    for (size_t i = 1; i < events.size(); ++i)
    {
        CHECK (events[i - 1].ppq <= events[i].ppq);

        if (events[i - 1].ppq == events[i].ppq && events[i - 1].message.isNoteOn())
            CHECK (! events[i].message.isNoteOff());
    }

    // Sections and rhythm changes in setlist order.
    CHECK (timeline.getSections().size() == 3);
    CHECK (timeline.getSections()[2].ppq == 16.0 && timeline.getSections()[2].name == "Chorus");
    CHECK (timeline.getRhythmChanges().size() == 3);
    CHECK (timeline.findSpanAt (17.0, false) == 2);
    CHECK (timeline.findSpanAt (21.0, true) == 0);

    // Plain note-on/off when realism is off: a bent note brings no pitch wheel.
    auto bent = tune;
    bent.arrangement.sections[0].melody->notes[0].technique = NoteTechnique::bend;

    TuneMidiOptions plain;
    plain.includeRealism = false;

    int wheels = 0, wheelsPlain = 0;

    // Held in variables: a range-for over build (...).getEvents() would iterate a
    // reference into a temporary that is already gone.
    const auto realistic = TuneTimeline::build (bent);
    const auto bare = TuneTimeline::build (bent, plain);

    for (const auto& e : realistic.getEvents())
        wheels += e.message.isPitchWheel() ? 1 : 0;

    for (const auto& e : bare.getEvents())
        wheelsPlain += e.message.isPitchWheel() ? 1 : 0;

    CHECK (wheels > 0);
    CHECK (wheelsPlain == 0);
}

LUTHIER_TEST (TuneBuilder, aLoopedTuneRunsSixtySecondsWithoutDrift)
{
    // tune-builder 15: "a 32-bar tune loops for 60 s without drift; final loop
    // ends at the same sample offset as the first within 1 sample". At 257 bpm
    // 32 bars take 29.9 s, so a minute is two full passes and the start of a
    // third, on a samples-per-beat figure that is nowhere near whole.
    Tune tune;
    tune.meta.tempoBpm = 257.0;
    tune.addSection (makeSection ("Loop", 32, { chord ("C"), chord ("G") }));

    const auto timeline = TuneTimeline::build (tune);
    CHECK (timeline.getLengthPpq() == 128.0);

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    const double samplesPerQuarter = sampleRate * 60.0 / tune.meta.tempoBpm;

    std::vector<int64_t> passStarts;
    std::map<int, int> held;
    bool neverStuck = true;
    int noteOns = 0;

    juce::MidiBuffer buffer;
    buffer.ensureSize (4096);

    const int64_t totalSamples = (int64_t) (60.0 * sampleRate);

    for (int64_t s = 0; s < totalSamples; s += blockSize)
    {
        // Each block's `from` is the previous block's `to`, computed the same
        // way, as TuneTimeline asks.
        const double from = (double) s / samplesPerQuarter;
        const double to = (double) (s + blockSize) / samplesPerQuarter;

        // A pass starts where the tune's first note-on (C3 of the C chord at
        // position 0) comes round again.
        timeline.forEachEventInRange (from, to, true, [&] (const TuneEvent& e, double at)
        {
            if (e.ppq == 0.0 && e.message.isNoteOn() && e.message.getNoteNumber() == 48)
                passStarts.push_back (s + (int64_t) std::llround ((at - from) * samplesPerQuarter));
        });

        // The audio-thread path: every note that starts also stops, and the
        // note-offs at the end of one pass land before the next pass's
        // note-ons, so no note is ever held twice.
        buffer.clear();
        timeline.renderBlock (from, to, samplesPerQuarter, blockSize, true, buffer);

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
            {
                ++held[message.getNoteNumber()];
                ++noteOns;
            }
            else if (message.isNoteOff())
            {
                --held[message.getNoteNumber()];
            }

            const int count = held[message.getNoteNumber()];
            neverStuck = neverStuck && count >= 0 && count <= 1;
        }
    }

    CHECK_MSG (passStarts.size() == 3, juce::String ((int) passStarts.size()) + " passes");
    CHECK (neverStuck);
    CHECK (noteOns > 0);

    for (size_t k = 0; k < passStarts.size(); ++k)
    {
        const auto expected = (int64_t) std::llround ((double) k * 128.0 * samplesPerQuarter);
        CHECK_MSG (std::abs (passStarts[k] - expected) <= 1,
                   "pass " + juce::String ((int) k) + " at " + juce::String (passStarts[k])
                     + ", expected " + juce::String (expected));
    }

    if (passStarts.size() == 3)
        CHECK (std::abs ((passStarts[2] - passStarts[1]) - (passStarts[1] - passStarts[0])) <= 1);
}

LUTHIER_TEST (TuneBuilder, theMidiFileHasAMetaTrackInstrumentTracksAndBeatAccurateTicks)
{
    const auto tune = makeSongTune();

    TuneMidiFileOptions options;
    options.split = TuneMidiFileOptions::TrackSplit::perInstrument;

    const auto file = buildTuneMidiFile (tune, options);
    CHECK (file.getNumTracks() == 3);   // meta, Guitar, Bass

    juce::MemoryOutputStream out;
    CHECK (file.writeTo (out));

    juce::MidiFile readBack;
    juce::MemoryInputStream in (out.getData(), out.getDataSize(), false);
    CHECK (readBack.readFrom (in));
    CHECK (readBack.getNumTracks() == 3);

    if (readBack.getNumTracks() != 3)
        return;

    int markers = 0;
    double secondsPerQuarter = 0.0;

    const auto* meta = readBack.getTrack (0);

    for (int i = 0; i < meta->getNumEvents(); ++i)
    {
        const auto& m = meta->getEventPointer (i)->message;

        if (m.isTextMetaEvent() && m.getMetaEventType() == 6)
            ++markers;

        if (m.isTempoMetaEvent())
            secondsPerQuarter = m.getTempoSecondsPerQuarterNote();
    }

    CHECK (markers == 3);
    CHECK_NEAR (secondsPerQuarter, 0.5, 1.0e-6);

    // The second melody note starts 1.5 beats in: 1440 ticks at 960 PPQ.
    const auto* guitar = readBack.getTrack (1);
    bool found = false;

    for (int i = 0; i < guitar->getNumEvents(); ++i)
    {
        const auto& m = guitar->getEventPointer (i)->message;
        found = found || (m.isNoteOn() && m.getNoteNumber() == 74 && std::abs (m.getTimeStamp() - 1440.0) < 0.5);
    }

    CHECK (found);

    // Per section: one track each for Verse and Chorus.
    options.split = TuneMidiFileOptions::TrackSplit::perSection;
    CHECK (buildTuneMidiFile (tune, options).getNumTracks() == 3);
}

LUTHIER_TEST (TuneBuilder, thePerformanceScoreKeepsSectionsChordSymbolsAndFrettedNotes)
{
    const auto tune = makeSongTune();

    PerformanceScore score;
    buildTuneScore (tune, score);

    // Two melody notes per verse, two verses.
    CHECK (score.getTotalNoteCount() == 4);

    const auto& track = score.getTrack (0);
    CHECK (track.measures.size() == 5);
    CHECK (track.measures[0].sectionName == "Verse");
    CHECK (track.measures[4].sectionName == "Chorus");

    bool sawAm = false;

    for (const auto& symbol : track.measures[0].chordSymbols)
        sawAm = sawAm || symbol.second == "Am";

    CHECK (sawAm);

    for (const auto& measure : track.measures)
        for (const auto* note : measure.collectNotes())
            CHECK (note->midiNote == track.tuning[(size_t) note->stringIndex] + note->fret);
}
