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

//==============================================================================
//  notation-export 4 / 6.1: the chord, bar and bass-technique tracks are fed
//  from where the engine knows them - the detector, the slide engine and the
//  string-activity queue - so the NOTATION tab's chord history fills in use.
//==============================================================================
#include "../Capture/PerformanceCapture.h"
#include "../DSP/Slap/SlapEngine.h"
#include "../DSP/Slide/SlideEngine.h"
#include "../Rhythm/ChordDetector.h"
#include "../Routing/MidiOutRouter.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr double kCaptureSr = 48000.0;
    constexpr int kCaptureBlock = 256;

    CaptureClock captureClockAt (juce::int64 blockStart)
    {
        CaptureClock clock;
        clock.blockStartSample = blockStart;
        clock.sampleRate = kCaptureSr;
        clock.transportPlaying = true;
        clock.bpm = 120.0;
        clock.blockStartPpq = (double) blockStart * 120.0 / (60.0 * kCaptureSr);
        return clock;
    }
}

LUTHIER_TEST (Notation, theChordTrackComesFromTheDetectorOnceAStrumHasSettled)
{
    PerformanceCapture capture;
    capture.prepare (kCaptureSr);

    ChordDetector detector;
    detector.prepare (kCaptureSr);

    juce::int64 at = 0;

    auto block = [&]
    {
        capture.beginBlock (captureClockAt (at));
        capture.captureChord (detector);
        at += kCaptureBlock;
    };

    const int windowBlocks = (int) std::ceil (ChordDetector::kBurstWindowSeconds * kCaptureSr / kCaptureBlock) + 1;

    block();   // nothing held: nothing written

    // A strummed C, one note a block: inside the burst window it is one chord.
    detector.noteOn (48, at); block();
    detector.noteOn (52, at); block();
    const juce::int64 lastChange = at;
    detector.noteOn (55, at); block();

    capture.drain();
    CHECK_MSG (capture.getChords().empty(), "a chord was written before the strum had settled");

    for (int i = 0; i < windowBlocks; ++i)
        block();

    capture.drain();
    CHECK_MSG (capture.getChords().size() == 1, juce::String (capture.getChords().size()) + " chords, expected the one");

    if (! capture.getChords().empty())
    {
        CHECK_MSG (capture.getChords()[0].name == "C", "the chord was " + capture.getChords()[0].name);
        CHECK_MSG (capture.getChords()[0].sample == lastChange,
                   "the chord is at sample " + juce::String (capture.getChords()[0].sample)
                     + ", not where the strum settled, " + juce::String (lastChange));
        CHECK (capture.getChords()[0].musical);
    }

    // Holding on writes nothing more; letting go is a gap, not a symbol.
    for (int i = 0; i < 2 * windowBlocks; ++i)
        block();

    detector.allNotesOff();

    for (int i = 0; i < 2 * windowBlocks; ++i)
        block();

    capture.drain();
    CHECK (capture.getChords().size() == 1);

    // A different chord is a new symbol...
    for (int note : { 45, 48, 52 })
        detector.noteOn (note, at);

    for (int i = 0; i < 2 * windowBlocks; ++i)
        block();

    capture.drain();
    CHECK (capture.getChords().size() == 2);

    if (capture.getChords().size() == 2)
        CHECK_MSG (capture.getChords()[1].name == "Am", "the second chord was " + capture.getChords()[1].name);

    // ...and the same one struck again is not.
    detector.allNotesOff();
    block();

    for (int note : { 45, 48, 52 })
        detector.noteOn (note, at);

    for (int i = 0; i < 2 * windowBlocks; ++i)
        block();

    capture.drain();
    CHECK (capture.getChords().size() == 2);

    // Off costs nothing and re-reads on resume.
    capture.setState (CaptureState::off);
    const auto written = capture.getRecordsWritten();
    detector.allNotesOff();

    for (int note : { 50, 54, 57 })
        detector.noteOn (note, at);

    for (int i = 0; i < 2 * windowBlocks; ++i)
        block();

    CHECK (capture.getRecordsWritten() == written);
}

LUTHIER_TEST (Notation, chordSymbolsFormatWithoutAString)
{
    ChordDetector detector;
    detector.prepare (kCaptureSr);

    const int cMajor[] = { 48, 52, 55 };
    const int gOverB[] = { 47, 50, 55, 59 };   // G/B: B D G B

    char text[16];

    auto symbol = detector.detect (cMajor, 3);
    CHECK (symbol.format (text, (int) sizeof (text)) == 1);
    CHECK (juce::String (text) == symbol.toString());

    symbol = detector.detect (gOverB, 4);
    symbol.format (text, (int) sizeof (text));
    CHECK_MSG (juce::String (text) == symbol.toString(), juce::String (text) + " vs " + symbol.toString());
    CHECK (symbol.isSlash());

    // Unknown is empty; a tiny buffer is truncated and terminated.
    CHECK (ChordSymbol().format (text, (int) sizeof (text)) == 0 && text[0] == 0);
    char tiny[3];
    CHECK (symbol.format (tiny, 3) == 2 && tiny[2] == 0);
    CHECK (symbol.format (nullptr, 3) == 0);
}

LUTHIER_TEST (Notation, theBarIsWrittenWhereItLandsMovesAndLifts)
{
    PerformanceCapture capture;
    capture.prepare (kCaptureSr);

    SlideEngine slide;
    slide.prepare (kCaptureSr);

    SlideSettings settings;
    settings.enabled = true;
    settings.pressure = 0.55;
    slide.setSettings (settings);

    juce::int64 at = 0;

    auto block = [&]
    {
        capture.beginBlock (captureClockAt (at));
        capture.captureSlideBar (slide);
        at += kCaptureBlock;
    };

    block();                                   // off the strings: nothing yet
    slide.setOverlayFret (5.0); block();       // lands, full, at 5
    slide.setOverlayFret (5.1); block();       // a wobble under a quarter fret: nothing
    slide.setOverlayFret (7.0); block();       // moved
    settings.pressure = 0.2;
    slide.setSettings (settings); block();     // eased off: light
    block();                                   // no change: nothing
    slide.setOverlayFret (-1.0); block();      // lifted
    block();                                   // still off: nothing

    capture.drain();
    const auto& events = capture.getEvents();
    CHECK_MSG (events.size() == 4, juce::String (events.size()) + " bar events, expected 4");

    if (events.size() == 4)
    {
        struct Expected { double pos; const char* pressure; juce::int64 sample; };
        const Expected expected[] = { { 5.0, "full", 1 * kCaptureBlock }, { 7.0, "full", 3 * kCaptureBlock },
                                      { 7.0, "light", 4 * kCaptureBlock }, { 7.0, "lift", 6 * kCaptureBlock } };

        for (size_t i = 0; i < 4; ++i)
        {
            CHECK (events[i].event.eventClass == LuthierEventClass::slideBar);
            CHECK_NEAR (events[i].event.getReal ("pos"), expected[i].pos, 1.0e-4);
            CHECK_MSG (events[i].event.get ("pressure") == expected[i].pressure,
                       "event " + juce::String ((int) i) + " pressure " + events[i].event.get ("pressure"));
            CHECK (events[i].sample == expected[i].sample);
        }
    }

    // Slide Mode off: the bar is not there whatever the overlay says.
    settings.enabled = false;
    slide.setSettings (settings);
    slide.setOverlayFret (9.0);
    block();
    capture.drain();
    CHECK (capture.getEvents().size() == 4);
}

LUTHIER_TEST (Notation, aBassTechniqueOnTheActivityQueueBecomesABassTechEvent)
{
    PerformanceCapture capture;
    capture.prepare (kCaptureSr);
    capture.beginBlock (captureClockAt (0));

    StringActivityQueue queue;
    queue.push ({ 10, 3, 40, 0.9f, true });   // the note itself

    StringActivityEvent pop { 10, 3, 40, 0.9f, true };
    pop.kind = StringActivityEvent::Kind::bassTechnique;
    pop.code = (juce::uint8) SlapType::pop;
    pop.position = 0.2f;
    queue.push (pop);

    auto ghost = pop;
    ghost.sampleOffset = 20;
    ghost.code = (juce::uint8) SlapType::thumb;
    ghost.flags = StringActivityEvent::kGhost;
    queue.push (ghost);

    auto thump = ghost;
    thump.sampleOffset = 30;
    thump.flags = StringActivityEvent::kRebound;
    queue.push (thump);

    capture.captureStringActivity (queue);
    capture.drain();

    CHECK_MSG (capture.getNotes().size() == 1, "a technique report became a note");

    const auto& events = capture.getEvents();
    CHECK_MSG (events.size() == 3, juce::String (events.size()) + " events, expected 3");

    if (events.size() == 3)
    {
        const char* techs[] = { "pop", "ghost", "thump" };

        for (size_t i = 0; i < 3; ++i)
        {
            CHECK (events[i].event.eventClass == LuthierEventClass::bassTech);
            CHECK_MSG (events[i].event.get ("tech") == techs[i], events[i].event.get ("tech") + " where " + techs[i] + " was expected");
            CHECK (events[i].event.getInt ("str") == 3);
            CHECK_NEAR (events[i].event.getReal ("pos"), 0.2, 1.0e-4);
            CHECK (events[i].event.part == 1);
        }

        CHECK (events[0].sample == 10 && events[1].sample == 20 && events[2].sample == 30);
    }

    CHECK (juce::String (PerformanceCapture::bassTechniqueName ((int) SlapType::thumb, 0)) == "slap");
    CHECK (juce::String (PerformanceCapture::bassTechniqueName ((int) SlapType::pop, StringActivityEvent::kGhost)) == "ghost");

    // MIDI out plays the note and skips the report.
    MidiOutRouter router;
    router.prepare (kCaptureSr, kCaptureBlock);
    MidiOutConfig config;
    config.enabled = true;
    config.stringActivity = true;
    juce::MidiBuffer out;
    router.emit (out, config, queue, kCaptureBlock);

    int noteOns = 0;

    for (const auto metadata : out)
        noteOns += metadata.getMessage().isNoteOn() ? 1 : 0;

    CHECK_MSG (noteOns == 1, juce::String (noteOns) + " note-ons went to MIDI out for one note");
}

LUTHIER_TEST (Notation, playingAChordThroughThePluginFillsTheChordHistory)
{
    LuthierAudioProcessor processor;

    if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
        p->setValueNotifyingHost (0.0f);

    processor.prepareToPlay (kCaptureSr, kCaptureBlock);
    CHECK (processor.getPerformanceCapture().getState() == CaptureState::rolling);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kCaptureBlock);

    for (int b = 0; b < 80; ++b)   // 430 ms
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (b == 2)
            for (int note : { 48, 52, 55 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        if (b == 30)
            for (int note : { 48, 52, 55 })
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        if (b == 40)
            for (int note : { 45, 48, 52 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);
    }

    processor.drainPerformanceCapture();

    const auto& chords = processor.getPerformanceCapture().getChords();
    juce::StringArray names;

    for (const auto& chord : chords)
        names.add (chord.name);

    CHECK_MSG (chords.size() == 2, juce::String ((int) chords.size()) + " chords in the history: " + names.joinIntoString (" "));

    if (chords.size() == 2)
    {
        CHECK_MSG (chords[0].name == "C", "first chord " + chords[0].name);
        CHECK_MSG (chords[1].name == "Am", "second chord " + chords[1].name);
        CHECK (chords[0].sample < chords[1].sample);
    }
}

//==============================================================================
//  notation-export 0.1: export runs on a worker thread, with progress and a
//  cancel, and writes what the synchronous exporter writes.
//==============================================================================
#include "../Notation/NotationExportTask.h"

namespace
{
    /** Runs the message loop for a moment so posted callbacks arrive. */
    void pumpMessages (int ms)
    {
       #if JUCE_MODAL_LOOPS_PERMITTED
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
       #else
        juce::Thread::sleep (ms);
       #endif
    }
}

LUTHIER_TEST (Notation, exportRunsOnAWorkerThreadAndReportsProgress)
{
    NotationExportTask task;

    NotationExportTask::Request request;
    request.format = NotationFormat::musicXml;
    request.destination = makeTempFile ("async.musicxml");
    request.destination.deleteFile();
    request.score = makeTestScore();

    std::vector<double> progress;
    bool done = false;
    NotationExportTask::Result reported;

    CHECK (task.start (request,
                       [&] (double fraction) { progress.push_back (fraction); },
                       [&] (const NotationExportTask::Result& r) { done = true; reported = r; }));
    CHECK_MSG (! task.start (request), "a second export started while one was running");

    const auto result = task.waitForCompletion (20000);
    CHECK_MSG (result.ok, result.error);
    CHECK (! result.cancelled);
    CHECK (! task.isRunning());
    CHECK_NEAR (task.getProgress(), 1.0, 1.0e-9);
    CHECK (result.destination == request.destination);
    CHECK_MSG (request.destination.existsAsFile(), "the worker wrote nothing");

    // The file is what the synchronous exporter writes (its line endings
    // included: both go through File::replaceWithText), and reads back.
    NotationExporter sync;
    const auto syncFile = makeTempFile ("sync.musicxml");
    CHECK (sync.write (request.score, NotationFormat::musicXml, syncFile, request.options));
    CHECK (request.destination.loadFileAsString() == syncFile.loadFileAsString());
    syncFile.deleteFile();

    NotationImporter importer;
    PerformanceScore back;
    CHECK_MSG (importer.read (request.destination, back), importer.getLastError());
    CHECK (back.getTotalNoteCount() == request.score.getTotalNoteCount());

   #if JUCE_MODAL_LOOPS_PERMITTED
    // The callbacks arrive on the message thread, in order, ending at 1.
    pumpMessages (200);
    CHECK_MSG (done, "the done callback never reached the message thread");
    CHECK (reported.ok);
    CHECK (! progress.empty() && std::abs (progress.back() - 1.0) < 1.0e-9);

    for (size_t i = 1; i < progress.size(); ++i)
        CHECK (progress[i] >= progress[i - 1]);
   #else
    juce::ignoreUnused (done, reported);
   #endif

    request.destination.deleteFile();
}

LUTHIER_TEST (Notation, exportCanBeCancelledBeforeItWrites)
{
    NotationExportTask task;

    NotationExportTask::Request request;
    request.format = NotationFormat::asciiTab;
    request.destination = makeTempFile ("cancelled.txt");
    request.destination.deleteFile();
    request.score = makeTestScore();
    request.minimumStageMs = 400;

    CHECK (task.start (request));
    CHECK (task.isRunning());
    task.cancel();

    const auto result = task.waitForCompletion (20000);
    CHECK_MSG (result.cancelled && ! result.ok, "a cancelled export reported " + juce::String (result.ok ? "success" : result.error));
    CHECK_MSG (! request.destination.existsAsFile(), "a cancelled export left a file behind");
    CHECK (! task.isRunning());

    // The task is reusable once done: MIDI goes through the MIDI OUT profile writer.
    NotationExportTask::Request midi;
    midi.format = NotationFormat::midi;
    midi.destination = makeTempFile ("async.mid");
    midi.destination.deleteFile();
    midi.performance = MidiPerformance::fromScore (makeTestScore(), 48000.0);

    CHECK (task.start (midi));
    const auto midiResult = task.waitForCompletion (20000);
    CHECK_MSG (midiResult.ok, midiResult.error);
    CHECK (midi.destination.existsAsFile() && midi.destination.getSize() > 0);
    midi.destination.deleteFile();

    // An export with nothing in it fails with a reason rather than a file.
    NotationExportTask::Request empty = request;
    empty.minimumStageMs = 0;
    empty.score = PerformanceScore();
    empty.destination = makeTempFile ("empty.txt");
    CHECK (task.start (empty));
    const auto emptyResult = task.waitForCompletion (20000);
    CHECK (! emptyResult.ok && emptyResult.error.isNotEmpty());
    CHECK (! empty.destination.existsAsFile());

    // A task destroyed mid-export stops its worker and calls nobody back.
    bool called = false;

    {
        NotationExportTask doomed;
        auto slow = request;
        slow.destination = makeTempFile ("doomed.txt");
        slow.minimumStageMs = 2000;
        CHECK (doomed.start (slow, {}, [&] (const NotationExportTask::Result&) { called = true; }));
    }

    pumpMessages (50);
    CHECK (! called);
    CHECK (! makeTempFile ("doomed.txt").existsAsFile());
}
