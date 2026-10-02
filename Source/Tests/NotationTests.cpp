/*  Notation export tests (notation-export.md section 6).

    The spec's tests name external fixtures - a MuseScore import, a Guitar Pro
    parser, fifty downloaded .gp files. Those belong to a manual pass and are
    recorded in docs/KNOWN_ISSUES.md. What is checked here is everything that can
    be checked against the plugin's own reader: that a score survives a round
    trip through each format it can both write and read, that the written XML is
    well formed, and that string and fret - the thing that makes this a guitar
    score rather than a list of pitches - survive.
*/

#include "TestFramework.h"

#include "../Notation/NotationExport.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** A short riff with a technique on most notes, built through the capture
        API the plugin itself uses. */
    PerformanceScore makeTestScore()
    {
        PerformanceScore score;

        score.getMeta().title = "Test Riff";
        score.getMeta().artist = "Luthier";

        score.beginCapture (120.0, 4, 4);

        struct Note { int string; int fret; double beat; double length; };

        const Note notes[] =
        {
            { 5, 0, 0.0,  0.5 }, { 5, 3, 0.5,  0.5 },
            { 4, 2, 1.0,  0.5 }, { 4, 0, 1.5,  0.5 },
            { 3, 2, 2.0,  1.0 }, { 2, 0, 3.0,  1.0 },
            { 1, 1, 4.0,  0.5 }, { 0, 12, 4.5, 0.5 },
            { 0, 15, 5.0, 1.0 }, { 5, 0, 6.0,  2.0 }
        };

        // Standard tuning, from the high E down.
        const int open[6] = { 64, 59, 55, 50, 45, 40 };

        int index = 0;

        for (const auto& note : notes)
        {
            const int midi = open[note.string] + note.fret;

            score.noteStarted (note.string, note.fret, midi, 440.0, 0.8, note.beat);

            // A different technique on most of them, so every writer's technique
            // path is exercised.
            switch (index % 5)
            {
                case 0:
                {
                    ScoreTechnique bend;
                    bend.type = ScoreTechnique::Type::bend;
                    bend.value = 1.0;
                    bend.curve = { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 1.0 } };
                    score.addTechnique (note.string, bend);
                    break;
                }

                case 1:
                    score.addTechnique (note.string, { ScoreTechnique::Type::hammerOn });
                    break;

                case 2:
                    score.addTechnique (note.string, { ScoreTechnique::Type::palmMute });
                    break;

                case 3:
                    score.addTechnique (note.string, { ScoreTechnique::Type::slideUp });
                    break;

                case 4:
                    score.addTechnique (note.string, { ScoreTechnique::Type::vibrato });
                    break;

                default:
                    break;
            }

            score.noteEnded (note.string, note.beat + note.length);

            ++index;
        }

        score.addChordSymbol (0.0, "Em");
        score.addChordSymbol (4.0, "G");

        score.endCapture (8.0);

        return score;
    }

    juce::File makeTempFile (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory)
                 .getChildFile ("LuthierNotationTests")
                 .getChildFile (name);
    }
}

//==============================================================================
/*  The capture API turns events into measures, and the score knows how many
    notes it holds. */
LUTHIER_TEST (Notation, captureBuildsMeasures)
{
    const auto score = makeTestScore();

    CHECK_MSG (score.getTotalNoteCount() == 10,
               juce::String (score.getTotalNoteCount()) + " notes captured, expected 10");

    const auto& track = score.getTrack (0);

    // Eight beats of 4/4 is two measures.
    CHECK_MSG (track.measures.size() == 2,
               juce::String ((int) track.measures.size()) + " measures, expected 2");

    // Notes landed in the measure they belong to, with the beat made relative.
    for (const auto& measure : track.measures)
    {
        for (const auto* note : measure.collectNotes())
        {
            CHECK_MSG (note->startBeat >= 0.0 && note->startBeat < 4.0,
                       "a note sits at beat " + juce::String (note->startBeat, 3)
                         + " inside a 4/4 measure");

            CHECK (note->durationBeats > 0.0);
            CHECK (note->stringIndex >= 0 && note->stringIndex < 6);
            CHECK (note->fret >= 0);
        }
    }

    // The chord symbols went in too.
    CHECK (! track.measures[0].chordSymbols.empty());
    CHECK (track.measures[0].chordSymbols[0].second == "Em");
}

//==============================================================================
/*  notation-export 0.2: the score is guitar-aware. The same pitch on two
    different strings is two different notes, and must stay that way. */
LUTHIER_TEST (Notation, stringAndFretAreNotDerivedFromPitch)
{
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    // E4 played two ways: string 2 fret 9, and string 1 fret 5. Same pitch.
    score.noteStarted (2, 9, 64, 440.0, 0.8, 0.0);
    score.noteEnded (2, 1.0);

    score.noteStarted (1, 5, 64, 440.0, 0.8, 1.0);
    score.noteEnded (1, 2.0);

    score.endCapture (4.0);

    const auto notes = score.getTrack (0).measures[0].collectNotes();

    CHECK (notes.size() == 2);

    if (notes.size() < 2)
        return;

    // Same pitch, different string and fret.
    CHECK (notes[0]->midiNote == notes[1]->midiNote);

    CHECK_MSG (notes[0]->stringIndex != notes[1]->stringIndex,
               "two different fingerings collapsed onto one string");

    CHECK (notes[0]->fret != notes[1]->fret);
}

//==============================================================================
/*  notation-export 2.1: the MusicXML is well formed, carries string and fret,
    and states the tuning. */
LUTHIER_TEST (Notation, musicXmlIsWellFormedAndGuitarAware)
{
    const auto score = makeTestScore();

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (score);

    CHECK (xml.isNotEmpty());

    // It parses. A string that looks like XML but does not parse is the failure
    // a notation program would hit, so it is checked by parsing rather than by
    // searching for tags.
    auto parsed = juce::parseXML (xml);

    CHECK_MSG (parsed != nullptr, "the MusicXML did not parse");

    if (parsed == nullptr)
        return;

    CHECK (parsed->hasTagName ("score-partwise"));
    CHECK (parsed->getStringAttribute ("version") == "4.0");

    auto* part = parsed->getChildByName ("part");
    CHECK (part != nullptr);

    if (part == nullptr)
        return;

    // Every note carries its string and fret.
    int notesFound = 0, withStringAndFret = 0;

    for (auto* measure : part->getChildWithTagNameIterator ("measure"))
    {
        for (auto* note : measure->getChildWithTagNameIterator ("note"))
        {
            if (note->getChildByName ("rest") != nullptr)
                continue;

            ++notesFound;

            if (auto* notations = note->getChildByName ("notations"))
                if (auto* technical = notations->getChildByName ("technical"))
                    if (technical->getChildByName ("string") != nullptr
                          && technical->getChildByName ("fret") != nullptr)
                        ++withStringAndFret;
        }
    }

    CHECK_MSG (notesFound >= 10,
               juce::String (notesFound) + " notes in the MusicXML, expected at least 10");

    CHECK_MSG (withStringAndFret == notesFound,
               juce::String (notesFound - withStringAndFret)
                 + " notes were written without a string and fret");

    // The tuning is declared, or the frets mean nothing on reimport.
    auto* firstMeasure = part->getChildByName ("measure");
    CHECK (firstMeasure != nullptr);

    if (firstMeasure != nullptr)
    {
        auto* attributes = firstMeasure->getChildByName ("attributes");
        CHECK (attributes != nullptr);

        if (attributes != nullptr)
        {
            auto* details = attributes->getChildByName ("staff-details");
            CHECK_MSG (details != nullptr, "no staff-details, so no tuning");

            if (details != nullptr)
            {
                int tunings = 0;

                for (auto* tuning : details->getChildWithTagNameIterator ("staff-tuning"))
                {
                    juce::ignoreUnused (tuning);
                    ++tunings;
                }

                CHECK_MSG (tunings == 6,
                           juce::String (tunings) + " staff-tuning entries, expected 6");
            }

            // A TAB clef, because this is tablature.
            auto* clef = attributes->getChildByName ("clef");
            CHECK (clef != nullptr);

            if (clef != nullptr)
                CHECK (clef->getChildElementAllSubText ("sign", "") == "TAB");
        }
    }
}

//==============================================================================
/*  Task X (notation-export.md 2.1): the standard staff, distinct from the
    ASCII/GP tab lane - real noteheads, no fret numbers. The fixture's ten
    notes are known in advance (makeTestScore's own table, standard tuning),
    so the written pitch and duration of every one of them is checked, not
    just that the writer produced *some* plausible XML. */
LUTHIER_TEST (Notation, musicXmlStandardStaffHasCorrectPitchesAndDurations)
{
    const auto score = makeTestScore();

    NotationExportOptions options;
    options.staffMode = NotationExportOptions::StaffMode::standardStaff;

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (score, options);

    auto parsed = juce::parseXML (xml);
    CHECK_MSG (parsed != nullptr, "the standard-staff MusicXML did not parse");

    if (parsed == nullptr)
        return;

    auto* part = parsed->getChildByName ("part");
    CHECK (part != nullptr);

    if (part == nullptr)
        return;

    // A real staff: treble clef sounding an octave below what is printed,
    // not a TAB clef, and no staff-details (that block only means something
    // when there is a fret number to make sense of).
    auto* firstMeasure = part->getChildByName ("measure");
    CHECK (firstMeasure != nullptr);

    if (firstMeasure != nullptr)
    {
        auto* attributes = firstMeasure->getChildByName ("attributes");
        CHECK (attributes != nullptr);

        if (attributes != nullptr)
        {
            auto* clef = attributes->getChildByName ("clef");
            CHECK (clef != nullptr);

            if (clef != nullptr)
            {
                CHECK (clef->getChildElementAllSubText ("sign", "") == "G");
                CHECK (clef->getChildElementAllSubText ("clef-octave-change", "") == "-1");
            }

            CHECK_MSG (attributes->getChildByName ("staff-details") == nullptr,
                       "standard staff wrote staff-details, which is tab-only");
        }
    }

    // The fixture's ten notes, in order, as written on a standard staff: the
    // sounding pitch (open string + fret) raised an octave to match the
    // clef's -1 octave-change, and the same duration as the source note.
    struct Expected { const char* step; int alter; int octave; int durationTicks; };

    const Expected expected[] =
    {
        { "E", 0, 3, 240 },   // string 5 fret 0  (E2 -> written E3)
        { "G", 0, 3, 240 },   // string 5 fret 3  (G2 -> written G3)
        { "B", 0, 3, 240 },   // string 4 fret 2  (B2 -> written B3)
        { "A", 0, 3, 240 },   // string 4 fret 0  (A2 -> written A3)
        { "E", 0, 4, 480 },   // string 3 fret 2  (E3 -> written E4)
        { "G", 0, 4, 480 },   // string 2 fret 0  (G3 -> written G4)
        { "C", 0, 5, 240 },   // string 1 fret 1  (C4 -> written C5)
        { "E", 0, 6, 240 },   // string 0 fret 12 (E5 -> written E6)
        { "G", 0, 6, 480 },   // string 0 fret 15 (G5 -> written G6)
        { "E", 0, 3, 960 },   // string 5 fret 0  (E2 -> written E3)
    };

    const int numExpected = (int) (sizeof (expected) / sizeof (expected[0]));
    int noteIndex = 0, technicalOnStandardStaff = 0;

    for (auto* measure : part->getChildWithTagNameIterator ("measure"))
    {
        for (auto* note : measure->getChildWithTagNameIterator ("note"))
        {
            if (note->getChildByName ("rest") != nullptr)
                continue;

            if (auto* notations = note->getChildByName ("notations"))
                if (notations->getChildByName ("technical") != nullptr)
                    ++technicalOnStandardStaff;

            if (! juce::isPositiveAndBelow (noteIndex, numExpected))
            {
                ++noteIndex;
                continue;
            }

            const auto& want = expected[(size_t) noteIndex];
            auto* pitch = note->getChildByName ("pitch");

            CHECK_MSG (pitch != nullptr, "note " + juce::String (noteIndex) + " has no <pitch>");

            if (pitch != nullptr)
            {
                CHECK_MSG (pitch->getChildElementAllSubText ("step", "") == want.step,
                           "note " + juce::String (noteIndex) + " step "
                             + pitch->getChildElementAllSubText ("step", "?") + ", expected " + want.step);

                CHECK_MSG (pitch->getChildElementAllSubText ("alter", "0").getIntValue() == want.alter,
                           "note " + juce::String (noteIndex) + " alter wrong");

                CHECK_MSG (pitch->getChildElementAllSubText ("octave", "").getIntValue() == want.octave,
                           "note " + juce::String (noteIndex) + " octave "
                             + pitch->getChildElementAllSubText ("octave", "?") + ", expected "
                             + juce::String (want.octave));
            }

            CHECK_MSG (note->getChildElementAllSubText ("duration", "").getIntValue() == want.durationTicks,
                       "note " + juce::String (noteIndex) + " duration "
                         + note->getChildElementAllSubText ("duration", "?") + ", expected "
                         + juce::String (want.durationTicks));

            ++noteIndex;
        }
    }

    CHECK_MSG (noteIndex == numExpected,
               juce::String (noteIndex) + " notes found, expected " + juce::String (numExpected));

    CHECK_MSG (technicalOnStandardStaff == 0,
               juce::String (technicalOnStandardStaff) + " notes carried tab <technical> on the standard staff");
}

//==============================================================================
/*  notation-export 6: MusicXML round trip - export, reimport, compare. */
LUTHIER_TEST (Notation, musicXmlRoundTrips)
{
    const auto original = makeTestScore();

    NotationExporter exporter;
    const auto xml = exporter.renderMusicXml (original);

    NotationImporter importer;
    PerformanceScore reimported;

    CHECK_MSG (importer.readMusicXml (xml, reimported),
               "the reimport failed: " + importer.getLastError());

    CHECK_MSG (reimported.getTotalNoteCount() == original.getTotalNoteCount(),
               "reimported " + juce::String (reimported.getTotalNoteCount())
                 + " notes, exported " + juce::String (original.getTotalNoteCount()));

    // The pitches, strings and frets all survived.
    const auto originalNotes = original.getTrack (0).measures[0].collectNotes();
    const auto reimportedNotes = reimported.getTrack (0).measures[0].collectNotes();

    CHECK (originalNotes.size() == reimportedNotes.size());

    for (size_t i = 0; i < juce::jmin (originalNotes.size(), reimportedNotes.size()); ++i)
    {
        CHECK_MSG (originalNotes[i]->midiNote == reimportedNotes[i]->midiNote,
                   "note " + juce::String ((int) i) + ": pitch changed");

        CHECK_MSG (originalNotes[i]->stringIndex == reimportedNotes[i]->stringIndex,
                   "note " + juce::String ((int) i) + ": string changed from "
                     + juce::String (originalNotes[i]->stringIndex) + " to "
                     + juce::String (reimportedNotes[i]->stringIndex));

        CHECK_MSG (originalNotes[i]->fret == reimportedNotes[i]->fret,
                   "note " + juce::String ((int) i) + ": fret changed");
    }

    // The techniques MusicXML can carry came back.
    int techniquesFound = 0;

    for (const auto* note : reimportedNotes)
        techniquesFound += (int) note->techniques.size();

    CHECK_MSG (techniquesFound > 0, "no techniques survived the MusicXML round trip");
}

//==============================================================================
/*  notation-export 6: ASCII tab columns line up for a fixed-tempo riff. That is
    the one thing ASCII tab has to get right. */
LUTHIER_TEST (Notation, asciiTabColumnsAlign)
{
    const auto score = makeTestScore();

    NotationExporter exporter;

    NotationExportOptions options;
    options.lineWidth = 80;

    const auto text = exporter.renderAsciiTab (score, options);

    CHECK (text.isNotEmpty());

    const auto lines = juce::StringArray::fromLines (text);

    // Find each block of six consecutive staff lines and check they are all the
    // same length. A block whose lines differ in length is misaligned, which is
    // exactly what makes tab unreadable.
    int blocksChecked = 0;

    for (int i = 0; i + 5 < lines.size(); ++i)
    {
        bool isBlock = true;

        for (int line = 0; line < 6; ++line)
            if (! lines[i + line].containsChar ('|') || ! lines[i + line].containsChar ('-'))
                isBlock = false;

        if (! isBlock)
            continue;

        const int expectedLength = lines[i].length();

        for (int line = 1; line < 6; ++line)
        {
            CHECK_MSG (lines[i + line].length() == expectedLength,
                       "staff line " + juce::String (line) + " of a block is "
                         + juce::String (lines[i + line].length())
                         + " characters, the first is " + juce::String (expectedLength));
        }

        // The bar lines line up vertically too.
        for (int column = 0; column < expectedLength; ++column)
        {
            if (lines[i][column] != '|')
                continue;

            for (int line = 1; line < 6; ++line)
                CHECK_MSG (lines[i + line][column] == '|',
                           "a bar line at column " + juce::String (column)
                             + " is missing from staff line " + juce::String (line));
        }

        ++blocksChecked;
        i += 5;
    }

    CHECK_MSG (blocksChecked > 0, "no tab blocks were found in the output");

    // The requested line width is respected.
    for (const auto& line : lines)
        CHECK_MSG (line.length() <= options.lineWidth + 4,
                   "a line is " + juce::String (line.length())
                     + " characters, the limit is " + juce::String (options.lineWidth));
}

//==============================================================================
/*  notation-export 2.3: the technique symbols the spec names all appear. */
LUTHIER_TEST (Notation, asciiTabUsesTheSpecifiedSymbols)
{
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    struct Entry { ScoreTechnique::Type type; const char* symbol; };

    const Entry entries[] =
    {
        { ScoreTechnique::Type::bend,        "b" },
        { ScoreTechnique::Type::bendRelease, "r" },
        { ScoreTechnique::Type::hammerOn,    "h" },
        { ScoreTechnique::Type::pullOff,     "p" },
        { ScoreTechnique::Type::slideUp,     "/" },
        { ScoreTechnique::Type::slideDown,   "\\" },
        { ScoreTechnique::Type::vibrato,     "~" }
    };

    double beat = 0.0;

    for (const auto& entry : entries)
    {
        score.noteStarted (0, 5, 69, 440.0, 0.8, beat);

        ScoreTechnique technique;
        technique.type = entry.type;
        technique.value = 1.0;
        score.addTechnique (0, technique);

        score.noteEnded (0, beat + 0.5);

        beat += 1.0;
    }

    score.endCapture (beat);

    NotationExporter exporter;

    NotationExportOptions options;
    options.density = NotationExportOptions::SymbolDensity::full;
    options.lineWidth = 200;

    const auto text = exporter.renderAsciiTab (score, options);

    for (const auto& entry : entries)
        CHECK_MSG (text.contains (entry.symbol),
                   juce::String (getTechniqueName (entry.type))
                     + " did not produce its symbol \"" + entry.symbol + "\"");

    // Harmonics get their own bracket forms.
    PerformanceScore harmonics;
    harmonics.beginCapture (120.0, 4, 4);

    harmonics.noteStarted (0, 12, 76, 440.0, 0.8, 0.0);
    harmonics.addTechnique (0, { ScoreTechnique::Type::naturalHarmonic });
    harmonics.noteEnded (0, 1.0);

    harmonics.noteStarted (1, 7, 66, 440.0, 0.8, 1.0);
    harmonics.addTechnique (1, { ScoreTechnique::Type::pinchHarmonic });
    harmonics.noteEnded (1, 2.0);

    harmonics.endCapture (4.0);

    const auto harmonicText = exporter.renderAsciiTab (harmonics, options);

    CHECK_MSG (harmonicText.contains ("<12>"), "a natural harmonic did not use <12>");
    CHECK_MSG (harmonicText.contains ("[7]"), "an artificial harmonic did not use [7]");

    // Notes-only density drops them all.
    options.density = NotationExportOptions::SymbolDensity::notesOnly;

    const auto plain = exporter.renderAsciiTab (score, options);

    CHECK_MSG (! plain.contains ("~"), "notes-only density still emitted a vibrato symbol");
}

//==============================================================================
/*  notation-export 2.4: MIDI export writes one track per string. */
LUTHIER_TEST (Notation, midiExportIsPerString)
{
    const auto score = makeTestScore();

    const auto file = makeTempFile ("test.mid");

    NotationExporter exporter;

    CHECK_MSG (exporter.write (score, NotationFormat::midi, file),
               "the MIDI export failed: " + exporter.getLastError());

    CHECK (file.existsAsFile());
    CHECK (file.getSize() > 0);

    // Read it back with JUCE's own parser.
    juce::MidiFile midiFile;

    {
        juce::FileInputStream stream (file);

        CHECK (stream.openedOk());
        CHECK (midiFile.readFrom (stream));
    }

    // A meta track plus one per string that has notes.
    CHECK_MSG (midiFile.getNumTracks() >= 2,
               juce::String (midiFile.getNumTracks()) + " tracks, expected at least 2");

    int totalNoteOns = 0;

    for (int track = 0; track < midiFile.getNumTracks(); ++track)
    {
        const auto* sequence = midiFile.getTrack (track);

        if (sequence == nullptr)
            continue;

        for (int i = 0; i < sequence->getNumEvents(); ++i)
            if (sequence->getEventPointer (i)->message.isNoteOn())
                ++totalNoteOns;
    }

    CHECK_MSG (totalNoteOns == score.getTotalNoteCount(),
               juce::String (totalNoteOns) + " note-ons in the MIDI file, the score has "
                 + juce::String (score.getTotalNoteCount()));

    file.deleteFile();
}

//==============================================================================
/*  notation-export 2.2: the Guitar Pro bundle is a real zip containing GPIF. */
LUTHIER_TEST (Notation, guitarProBundleIsAValidZip)
{
    const auto score = makeTestScore();

    const auto file = makeTempFile ("test.gp");

    NotationExporter exporter;

    CHECK_MSG (exporter.write (score, NotationFormat::guitarPro, file),
               "the Guitar Pro export failed: " + exporter.getLastError());

    CHECK (file.existsAsFile());
    CHECK (file.getSize() > 0);

    // It is a zip, and the GPIF is inside it.
    {
        juce::ZipFile zip (file);

        CHECK_MSG (zip.getNumEntries() > 0, "the .gp file is not a valid zip");

        bool foundScore = false;

        for (int i = 0; i < zip.getNumEntries(); ++i)
            if (const auto* entry = zip.getEntry (i))
                if (entry->filename.containsIgnoreCase ("gpif"))
                    foundScore = true;

        CHECK_MSG (foundScore, "the .gp bundle contains no GPIF score");
    }

    // And the XML inside is well formed, with string and fret on every note.
    const auto xml = exporter.renderGuitarProXml (score);

    auto parsed = juce::parseXML (xml);

    CHECK_MSG (parsed != nullptr, "the GPIF XML did not parse");

    if (parsed != nullptr)
    {
        auto* notes = parsed->getChildByName ("Notes");

        CHECK (notes != nullptr);

        if (notes != nullptr)
        {
            int count = 0;

            for (auto* note : notes->getChildWithTagNameIterator ("Note"))
            {
                juce::ignoreUnused (note);
                ++count;
            }

            CHECK_MSG (count == score.getTotalNoteCount(),
                       juce::String (count) + " notes in the GPIF, the score has "
                         + juce::String (score.getTotalNoteCount()));
        }

        // The tuning is declared.
        CHECK (xml.contains ("Tuning"));
    }

    file.deleteFile();
}

//==============================================================================
/*  notation-export 6: ASCII tab round trip. */
LUTHIER_TEST (Notation, asciiTabRoundTrips)
{
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    // A plain riff with no techniques: ASCII tab's documented loss is that bend
    // curves flatten, so a round trip is only exact without them.
    struct Note { int string; int fret; double beat; };

    const Note notes[] =
    {
        { 5, 0, 0.0 }, { 5, 3, 1.0 }, { 4, 2, 2.0 }, { 4, 0, 3.0 }
    };

    const int open[6] = { 64, 59, 55, 50, 45, 40 };

    for (const auto& note : notes)
    {
        score.noteStarted (note.string, note.fret, open[note.string] + note.fret,
                           440.0, 0.8, note.beat);
        score.noteEnded (note.string, note.beat + 0.5);
    }

    score.endCapture (4.0);

    NotationExporter exporter;

    NotationExportOptions options;
    options.lineWidth = 200;
    options.chordSymbols = false;

    const auto text = exporter.renderAsciiTab (score, options);

    NotationImporter importer;
    PerformanceScore reimported;

    CHECK_MSG (importer.readAsciiTab (text, reimported),
               "the ASCII reimport failed: " + importer.getLastError());

    CHECK_MSG (reimported.getTotalNoteCount() == score.getTotalNoteCount(),
               "reimported " + juce::String (reimported.getTotalNoteCount())
                 + " notes from ASCII, exported " + juce::String (score.getTotalNoteCount()));

    // The frets came back, which is what the format actually carries.
    const auto reimportedNotes = reimported.getTrack (0).measures[0].collectNotes();

    juce::Array<int> frets;

    for (const auto* note : reimportedNotes)
        frets.add (note->fret);

    for (const auto& note : notes)
        CHECK_MSG (frets.contains (note.fret),
                   "fret " + juce::String (note.fret) + " did not survive the ASCII round trip");
}

//==============================================================================
/*  notation-export 5 and 2: the formats describe themselves, including their
    own round-trip losses. */
LUTHIER_TEST (Notation, formatsDeclareTheirLosses)
{
    for (int f = 0; f < (int) NotationFormat::numFormats; ++f)
    {
        const auto format = (NotationFormat) f;

        CHECK (juce::String (getNotationFormatName (format)).isNotEmpty());
        CHECK (juce::String (getNotationFormatExtension (format)).startsWith ("."));
        CHECK (juce::String (getNotationFormatLoss (format)).isNotEmpty());
    }

    // The specific losses the spec documents.
    CHECK (juce::String (getNotationFormatLoss (NotationFormat::musicXml))
             .containsIgnoreCase ("whammy"));

    CHECK (juce::String (getNotationFormatLoss (NotationFormat::asciiTab))
             .containsIgnoreCase ("bend"));

    CHECK (juce::String (getNotationFormatLoss (NotationFormat::midi))
             .containsIgnoreCase ("technique"));

    CHECK (juce::String (getNotationFormatLoss (NotationFormat::guitarPro))
             .containsIgnoreCase ("no loss"));

    CHECK (juce::String (getNotationFormatExtension (NotationFormat::musicXml)) == ".musicxml");
    CHECK (juce::String (getNotationFormatExtension (NotationFormat::guitarPro)) == ".gp");
    CHECK (juce::String (getNotationFormatExtension (NotationFormat::asciiTab)) == ".txt");
    CHECK (juce::String (getNotationFormatExtension (NotationFormat::midi)) == ".mid");
}

//==============================================================================
/*  practice-tools 6: the importer says what it can and cannot read, rather than
    reporting a supported format as corrupt. */
LUTHIER_TEST (Notation, importerIsHonestAboutWhatItReads)
{
    // What it does read.
    CHECK (NotationImporter::canRead (juce::File ("riff.txt")));
    CHECK (NotationImporter::canRead (juce::File ("song.musicxml")));
    CHECK (NotationImporter::canRead (juce::File ("song.xml")));

    // What it does not.
    CHECK (! NotationImporter::canRead (juce::File ("song.gp5")));
    CHECK (! NotationImporter::canRead (juce::File ("song.gp")));
    CHECK (! NotationImporter::canRead (juce::File ("song.ptb")));

    // And the message says why, and what to do instead.
    const auto file = makeTempFile ("unsupported.gp5");
    file.getParentDirectory().createDirectory();
    file.replaceWithText ("not really a guitar pro file");

    NotationImporter importer;
    PerformanceScore score;

    CHECK (! importer.read (file, score));

    const auto error = importer.getLastError();

    CHECK_MSG (error.containsIgnoreCase ("musicxml"),
               "the error does not suggest MusicXML: " + error);

    CHECK_MSG (! error.containsIgnoreCase ("corrupt"),
               "an unsupported format was reported as corrupt: " + error);

    file.deleteFile();
}

//==============================================================================
/*  notation-export 3: the live view renders a window of measures. */
LUTHIER_TEST (Notation, liveTabWindowRendersASlice)
{
    const auto score = makeTestScore();

    NotationExporter exporter;

    // One measure at a time, which is what the live view scrolls through.
    const auto first = exporter.renderAsciiTabWindow (score, 0, 1);
    const auto second = exporter.renderAsciiTabWindow (score, 1, 1);

    CHECK (first.isNotEmpty());
    CHECK (second.isNotEmpty());

    CHECK_MSG (first != second, "two different measures rendered identically");

    // Six staff lines plus the ruler.
    const auto lines = juce::StringArray::fromLines (first);

    int staffLines = 0;

    for (const auto& line : lines)
        if (line.containsChar ('|') && line.containsChar ('-'))
            ++staffLines;

    CHECK_MSG (staffLines == 6,
               juce::String (staffLines) + " staff lines in a window, expected 6");

    // Asking beyond the end is clamped rather than crashing.
    const auto beyond = exporter.renderAsciiTabWindow (score, 99, 4);
    CHECK (beyond.isNotEmpty());
}

//==============================================================================
/*  An empty score exports nothing and says so, rather than writing a file with
    no notes in it. */
LUTHIER_TEST (Notation, anEmptyScoreIsRefusedWithAReason)
{
    PerformanceScore empty;

    NotationExporter exporter;

    const auto file = makeTempFile ("empty.musicxml");

    CHECK (! exporter.write (empty, NotationFormat::musicXml, file));
    CHECK (exporter.getLastError().isNotEmpty());

    CHECK_MSG (! file.existsAsFile(), "an empty score still wrote a file");
}

//==============================================================================
/*  The MusicXML pitch conversion, which every note in every export depends on. */
LUTHIER_TEST (Notation, musicXmlPitchConversion)
{
    struct Expected { int midi; const char* step; int alter; int octave; };

    const Expected cases[] =
    {
        { 60, "C", 0, 4 },      // middle C
        { 61, "C", 1, 4 },      // C#
        { 64, "E", 0, 4 },
        { 69, "A", 0, 4 },      // A440
        { 40, "E", 0, 2 },      // low E on a guitar
        { 70, "A", 1, 4 }
    };

    for (const auto& expected : cases)
    {
        juce::String step;
        int alter = 0, octave = 0;

        PerformanceScore::getMusicXmlPitch (expected.midi, step, alter, octave);

        CHECK_MSG (step == expected.step,
                   "MIDI " + juce::String (expected.midi) + ": step " + step
                     + ", expected " + expected.step);

        CHECK_MSG (alter == expected.alter,
                   "MIDI " + juce::String (expected.midi) + ": alter " + juce::String (alter));

        CHECK_MSG (octave == expected.octave,
                   "MIDI " + juce::String (expected.midi) + ": octave " + juce::String (octave));
    }

    CHECK (PerformanceScore::getNoteName (60) == "C4");
    CHECK (PerformanceScore::getNoteName (69) == "A4");
    CHECK (PerformanceScore::getNoteName (40) == "E2");
}
