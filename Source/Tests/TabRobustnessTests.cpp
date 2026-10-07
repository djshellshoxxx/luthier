// Tab reader robustness: Guitar Pro 3/4/5 binary, compressed MusicXML, notation-only
// MusicXML, unsupported-container messages, and adversarial input to every reader.
#include "TestFramework.h"
#include "../Notation/NotationExport.h"
#include "../Notation/GuitarProLegacyReader.h"
#include "../Notation/TabImportPipeline.h"
#include "../Notation/TabChordChart.h"
#include "../Riffs/Riff.h"
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffTransposer.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <tuple>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Flat
    {
        double beat = 0.0;   // absolute, in quarter notes
        double length = 0.0;
        int stringIndex = 0, fret = 0, midi = 0;
        std::vector<ScoreTechnique> techniques;
        bool has (ScoreTechnique::Type t) const
        {
            for (const auto& x : techniques) if (x.type == t) return true;
            return false;
        }
        double valueOf (ScoreTechnique::Type t) const
        {
            for (const auto& x : techniques) if (x.type == t) return x.value;
            return -1.0;
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
                    out.push_back ({ barStart + n.startBeat, n.durationBeats, n.stringIndex, n.fret, n.midiNote, n.techniques });
            barStart += measure.timeSignatureNumerator * 4.0 / juce::jmax (1, measure.timeSignatureDenominator);
        }
        std::stable_sort (out.begin(), out.end(), [] (const Flat& a, const Flat& b)
        {
            return a.beat < b.beat - 1.0e-9 || (std::abs (a.beat - b.beat) < 1.0e-9 && a.stringIndex > b.stringIndex);
        });
        return out;
    }

    /** __FILE__ is relative to the build directory on some generators, so resolve it
        against the working directory first and then every ancestor of the executable. */
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

    juce::File fixture (const char* name)
    {
        return sourceSibling ((juce::String ("Fixtures/") + name).toRawUTF8());
    }

    juce::File tempFile (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTabRobustness");
        dir.createDirectory();
        return dir.getChildFile (name);
    }

    /** Playback compilation: the notes the riff player would actually place. */
    std::vector<std::pair<int, int>> playbackMidi (const PerformanceScore& score)
    {
        const auto riff = Riff::fromScore (score);
        const auto instrument = GuitarSpecSummary::forRiff (riff);
        const auto compiled = RiffCompiler::compile (riff, RiffPlaySettings{}, instrument);
        std::vector<std::pair<int, int>> out;
        if (compiled != nullptr)
            for (const auto& n : compiled->placement.notes)
                out.emplace_back (n.stringIndex, n.midiNote);
        return out;
    }

    // Expected content of the riff_gp*.gp* fixtures: bars 1-2 repeated once, then bar 3;
    // drop-D tuning, capo 2, 100 bpm.
    void checkRiff (TestContext& ctx, const PerformanceScore& score, bool gp4OrNewer)
    {
        const auto& track = score.getTrack (0);
        CHECK (track.numStrings == 6);
        CHECK (track.capoFret == 2);
        CHECK (track.tuning[5] == 38);
        CHECK (track.tuning[0] == 64);
        CHECK_NEAR (score.getMeta().tempoBpm, 100.0, 0.01);
        CHECK (score.getMeta().title == "Fixture Riff");

        const auto notes = flatten (score);
        CHECK_MSG (notes.size() == 20, "notes: " + juce::String ((int) notes.size()));
        if (notes.size() != 20)
            return;

        // bar 1: open low D (sounds E with the capo), then 5h7 on the A string
        CHECK (notes[0].stringIndex == 5 && notes[0].fret == 0 && notes[0].midi == 40);
        CHECK_NEAR (notes[0].beat, 0.0, 1e-6);
        CHECK_NEAR (notes[0].length, 1.0, 1e-6);
        CHECK (notes[1].stringIndex == 4 && notes[1].fret == 5 && notes[1].midi == 52);
        CHECK_NEAR (notes[1].beat, 1.0, 1e-6);
        CHECK_NEAR (notes[1].length, 0.5, 1e-6);
        CHECK (notes[2].fret == 7 && notes[2].has (ScoreTechnique::Type::hammerOn));
        CHECK_NEAR (notes[2].beat, 1.5, 1e-6);
        CHECK (notes[3].stringIndex == 3 && notes[3].fret == 7 && notes[3].midi == 59);
        if (gp4OrNewer)   // GP3 has no palm-mute flag
            CHECK (notes[3].has (ScoreTechnique::Type::palmMute));
        CHECK (notes[4].has (ScoreTechnique::Type::ghostNote) && notes[4].stringIndex == 2);

        // bar 2: a bent half note tied into a quarter (one 3-beat note), then a harmonic
        CHECK (notes[5].stringIndex == 1 && notes[5].fret == 5 && notes[5].has (ScoreTechnique::Type::bend));
        CHECK_NEAR (notes[5].beat, 4.0, 1e-6);
        CHECK_NEAR (notes[5].length, 3.0, 1e-6);
        CHECK (notes[5].valueOf (ScoreTechnique::Type::bend) > 0.9);
        CHECK (notes[6].stringIndex == 0 && notes[6].fret == 12);
        CHECK_NEAR (notes[6].beat, 7.0, 1e-6);
        CHECK (notes[6].has (ScoreTechnique::Type::naturalHarmonic));

        // the repeat: bar 1 again at beat 8, bar 2 again at 12, bar 3 at 16
        CHECK_NEAR (notes[7].beat, 8.0, 1e-6);
        CHECK_NEAR (notes[12].beat, 12.0, 1e-6);
        CHECK (notes[12].fret == 5 && notes[12].has (ScoreTechnique::Type::bend));

        // bar 3: a three-note chord, a slide 5 -> 7, the 7, a dead note
        CHECK_NEAR (notes[14].beat, 16.0, 1e-6);
        CHECK (notes[14].stringIndex == 5 && notes[15].stringIndex == 4 && notes[16].stringIndex == 3);
        CHECK_NEAR (notes[17].beat, 17.0, 1e-6);
        CHECK (notes[17].has (ScoreTechnique::Type::slideShift) || notes[17].has (ScoreTechnique::Type::slideLegato));
        CHECK_NEAR (notes[17].valueOf (notes[17].has (ScoreTechnique::Type::slideLegato)
                                         ? ScoreTechnique::Type::slideLegato : ScoreTechnique::Type::slideShift), 7.0, 1e-6);
        CHECK (notes[18].fret == 7 && notes[18].stringIndex == 2);
        CHECK (notes[19].has (ScoreTechnique::Type::deadNote));
    }
}

//==============================================================================
LUTHIER_TEST (TabRobustness, guitarPro3FileReads)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("riff_gp3.gp3"), score), importer.getLastError());
    checkRiff (ctx, score, false);
}

LUTHIER_TEST (TabRobustness, guitarPro4FileReads)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("riff_gp4.gp4"), score), importer.getLastError());
    checkRiff (ctx, score, true);
}

LUTHIER_TEST (TabRobustness, guitarPro5FileReads)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("riff_gp5.gp5"), score), importer.getLastError());
    checkRiff (ctx, score, true);
}

LUTHIER_TEST (TabRobustness, guitarPro510FileReads)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (fixture ("riff_gp5_v510.gp5"), score), importer.getLastError());
    checkRiff (ctx, score, true);
}

LUTHIER_TEST (TabRobustness, guitarProPlaybackCompilesToTheWrittenPitches)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.read (fixture ("riff_gp5.gp5"), score));

    const auto placed = playbackMidi (score);
    CHECK (! placed.empty());
    if (placed.empty())
        return;
    CHECK (placed.front().second == 40);   // drop-D low string + capo 2
    CHECK (placed.front().first == 5);

    for (const auto& [string, midi] : placed)
        CHECK (midi >= 36 && midi <= 80);
}

LUTHIER_TEST (TabRobustness, guitarProMultiTrackPicksFirstPitchedTrackAndHonoursPreference)
{
    for (const char* name : { "multitrack_band.gp5", "multitrack_band.gp4" })
    {
        // Default: skip the drum track, take the bass.
        {
            NotationImporter importer;
            PerformanceScore score;
            CHECK_MSG (importer.read (fixture (name), score), importer.getLastError());
            CHECK (score.getTrack (0).numStrings == 4);
            CHECK (score.getTrack (0).tuning[3] == 28);
            const auto notes = flatten (score);
            CHECK (notes.size() == 4);
            if (notes.size() == 4)
                CHECK (notes[0].midi == 28 && notes[1].midi == 35);
            CHECK (importer.getLastDiagnostics().warnings.joinIntoString (" ").contains ("Lead"));
        }

        // Asking for track 3 (index 2) gets the guitar.
        {
            NotationImporter importer;
            importer.setPreferredTrack (2);
            PerformanceScore score;
            CHECK (importer.read (fixture (name), score));
            CHECK (score.getTrack (0).numStrings == 6);
            const auto notes = flatten (score);
            CHECK (notes.size() == 8);
            if (notes.size() == 8)
                CHECK (notes[0].midi == 64 && notes[1].midi == 60 && notes[2].midi == 55 && notes[3].midi == 43);
        }

        // Asking for the drum track falls back to a pitched one.
        {
            NotationImporter importer;
            importer.setPreferredTrack (0);
            PerformanceScore score;
            CHECK (importer.read (fixture (name), score));
            CHECK (score.getTrack (0).numStrings == 4);
        }
    }
}

LUTHIER_TEST (TabRobustness, guitarProLegacyIsSniffedByContentNotExtension)
{
    const auto bytes = fixture ("riff_gp5.gp5").loadFileAsString();   // text view only to check it exists
    CHECK (bytes.isNotEmpty());

    auto renamed = tempFile ("renamed.txt");
    CHECK (fixture ("riff_gp4.gp4").copyFileTo (renamed));

    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (renamed, score), importer.getLastError());
    CHECK (score.getTotalNoteCount() == 20);
    renamed.deleteFile();
}

LUTHIER_TEST (TabRobustness, truncatedGuitarProKeepsWhatItCouldRead)
{
    juce::MemoryBlock bytes;
    CHECK (fixture ("riff_gp5.gp5").loadFileAsData (bytes));

    // Cut in the middle of the measure data.
    for (const double fraction : { 0.55, 0.7, 0.85, 0.95 })
    {
        const size_t cut = (size_t) ((double) bytes.getSize() * fraction);
        NotationImporter importer;
        PerformanceScore score;
        const bool ok = importer.readGuitarProLegacy (bytes.getData(), cut, score);

        if (ok)
        {
            CHECK (score.getTotalNoteCount() >= 1);
            CHECK (importer.getLastDiagnostics().isPartial() || ! importer.getLastDiagnostics().warnings.isEmpty());
        }
        else
            CHECK (importer.getLastError().isNotEmpty());
    }

    // Truncated to nothing useful: a message, never a crash.
    for (const size_t cut : { (size_t) 0, (size_t) 5, (size_t) 31, (size_t) 100, (size_t) 700 })
    {
        NotationImporter importer;
        PerformanceScore score;
        const bool ok = importer.readGuitarProLegacy (bytes.getData(), juce::jmin (cut, bytes.getSize()), score);
        if (! ok)
            CHECK (importer.getLastError().isNotEmpty());
    }
}

LUTHIER_TEST (TabRobustness, corruptedGuitarProNeverCrashes)
{
    juce::MemoryBlock original;
    CHECK (fixture ("riff_gp5.gp5").loadFileAsData (original));

    juce::Random rng (12345);
    for (int round = 0; round < 400; ++round)
    {
        juce::MemoryBlock bytes (original);
        const int flips = 1 + rng.nextInt (6);
        for (int i = 0; i < flips; ++i)
            bytes[(size_t) (31 + rng.nextInt ((int) bytes.getSize() - 31))] = (char) rng.nextInt (256);

        NotationImporter importer;
        PerformanceScore score;
        const bool ok = importer.readGuitarProLegacy (bytes.getData(), bytes.getSize(), score);
        if (! ok)
            CHECK (importer.getLastError().isNotEmpty());
    }
}

LUTHIER_TEST (TabRobustness, hugeDeclaredCountsAreRefused)
{
    juce::MemoryBlock bytes;
    CHECK (fixture ("riff_gp3.gp3").loadFileAsData (bytes));

    // GP3: the measure count sits right after the 64 MIDI channels. Find it by
    // scanning for the declared counts (3 measures, 1 track) and replace with giants.
    bool patched = false;
    for (size_t i = 31; i + 8 < bytes.getSize() && ! patched; ++i)
    {
        const auto* p = (const juce::uint8*) bytes.getData() + i;
        if (p[0] == 3 && p[1] == 0 && p[2] == 0 && p[3] == 0 && p[4] == 1 && p[5] == 0 && p[6] == 0 && p[7] == 0
            && i > 64 * 12)
        {
            bytes[i + 3] = (char) 0x7f;
            bytes[i + 2] = (char) 0xff;
            bytes[i + 1] = (char) 0xff;
            patched = true;
        }
    }
    CHECK (patched);

    NotationImporter importer;
    PerformanceScore score;
    const juce::Time start = juce::Time::getCurrentTime();
    const bool ok = importer.readGuitarProLegacy (bytes.getData(), bytes.getSize(), score);
    CHECK (! ok);
    CHECK (importer.getLastError().isNotEmpty());
    CHECK ((juce::Time::getCurrentTime() - start).inSeconds() < 5.0);
}

LUTHIER_TEST (TabRobustness, gpxAndPowerTabGetFriendlyErrors)
{
    for (const auto& [name, magic, mention] : std::vector<std::tuple<const char*, const char*, const char*>> {
            { "x.gpx", "BCFZ-and-some-payload-bytes", "gp" },
            { "x.ptb", "ptab-and-some-payload-bytes", "MusicXML" } })
    {
        auto f = tempFile (name);
        f.replaceWithText (magic);
        NotationImporter importer;
        PerformanceScore score;
        CHECK (! importer.read (f, score));
        const auto e = importer.getLastError();
        CHECK_MSG (e.containsIgnoreCase (mention), e);
        CHECK (! e.containsIgnoreCase ("corrupt"));
        f.deleteFile();
    }
}

LUTHIER_TEST (TabRobustness, notGuitarProWithGuitarProExtensionExplainsItself)
{
    for (const char* name : { "n.gp3", "n.gp4", "n.gp5" })
    {
        auto f = tempFile (name);
        f.replaceWithText ("this is not a guitar pro file at all");
        NotationImporter importer;
        PerformanceScore score;
        CHECK (! importer.read (f, score));
        CHECK (importer.getLastError().containsIgnoreCase ("musicxml"));
        f.deleteFile();
    }
}

//==============================================================================
namespace
{
    PerformanceScore buildScore()
    {
        PerformanceScore s;
        s.beginCapture (96.0, 4, 4);
        auto& t = s.getTrack (0);
        t.numStrings = 6;
        t.tuning = { { 62, 57, 53, 48, 43, 38, 0, 0, 0, 0, 0, 0 } };   // whole step down
        t.capoFret = 0;
        s.noteStarted (5, 3, 41, 87.3, 0.8, 0.0);  s.noteEnded (5, 1.0);
        s.noteStarted (3, 2, 50, 146.8, 0.8, 1.0); s.noteEnded (3, 2.0);
        s.noteStarted (0, 5, 67, 392.0, 0.8, 2.0); s.noteEnded (0, 3.5);
        s.endCapture (4.0);
        return s;
    }

    juce::File zipMusicXml (TestContext& ctx, const juce::File& xml, const juce::String& zipName, bool withContainer)
    {
        juce::ZipFile::Builder builder;
        builder.addFile (xml, 9, "score.musicxml");

        if (withContainer)
        {
            auto container = tempFile ("container.xml");
            container.replaceWithText ("<?xml version=\"1.0\"?><container><rootfiles>"
                                       "<rootfile full-path=\"score.musicxml\" media-type=\"application/vnd.recordare.musicxml+xml\"/>"
                                       "</rootfiles></container>");
            builder.addFile (container, 9, "META-INF/container.xml");
        }

        auto out = tempFile (zipName);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        CHECK (stream.openedOk());
        double progress = 0.0;
        CHECK (builder.writeToStream (stream, &progress));
        return out;
    }
}

LUTHIER_TEST (TabRobustness, compressedMusicXmlRoundTrips)
{
    const auto score = buildScore();
    auto xml = tempFile ("rt.musicxml");
    NotationExporter exporter;
    CHECK_MSG (exporter.write (score, NotationFormat::musicXml, xml), exporter.getLastError());

    for (const bool withContainer : { true, false })
    {
        const auto mxl = zipMusicXml (ctx, xml, withContainer ? "rt_c.mxl" : "rt_n.mxl", withContainer);
        NotationImporter importer;
        PerformanceScore back;
        CHECK_MSG (importer.read (mxl, back), importer.getLastError());

        const auto notes = flatten (back);
        CHECK (notes.size() == 3);
        if (notes.size() == 3)
        {
            CHECK (notes[0].stringIndex == 5 && notes[0].fret == 3 && notes[0].midi == 41);
            CHECK (notes[1].stringIndex == 3 && notes[1].fret == 2);
            CHECK (notes[2].stringIndex == 0 && notes[2].fret == 5);
            CHECK_NEAR (notes[2].length, 1.5, 1e-6);
        }
        CHECK (back.getTrack (0).tuning[5] == 38);
        CHECK_NEAR (back.getMeta().tempoBpm, 96.0, 0.1);
        mxl.deleteFile();
    }
}

LUTHIER_TEST (TabRobustness, mxlWithoutAScoreAndNonZipAreFriendly)
{
    auto junk = tempFile ("junk.mxl");
    junk.replaceWithText ("this is not a zip file");
    NotationImporter importer;
    PerformanceScore score;
    CHECK (! importer.read (junk, score));
    CHECK (importer.getLastError().isNotEmpty());
    junk.deleteFile();

    auto empty = tempFile ("empty.mxl");
    {
        juce::ZipFile::Builder builder;
        auto readme = tempFile ("readme.txt");
        readme.replaceWithText ("hello");
        builder.addFile (readme, 9, "readme.txt");
        empty.deleteFile();
        juce::FileOutputStream out (empty);
        double p = 0.0;
        builder.writeToStream (out, &p);
    }
    CHECK (! importer.read (empty, score));
    CHECK (importer.getLastError().isNotEmpty());
    empty.deleteFile();
}

LUTHIER_TEST (TabRobustness, notationOnlyMusicXmlIsFingeredAndTied)
{
    const juce::String xml =
        "<?xml version=\"1.0\"?><score-partwise version=\"3.1\"><part-list>"
        "<score-part id=\"P1\"><part-name>Guitar</part-name></score-part></part-list>"
        "<part id=\"P1\"><measure number=\"1\"><attributes><divisions>2</divisions>"
        "<time><beats>4</beats><beat-type>4</beat-type></time></attributes>"
        "<direction><sound tempo=\"90\"/></direction>"
        "<note><pitch><step>E</step><octave>2</octave></pitch><duration>2</duration><type>quarter</type></note>"
        "<note><pitch><step>A</step><octave>2</octave></pitch><duration>2</duration><tie type=\"start\"/></note>"
        "<note><pitch><step>A</step><octave>2</octave></pitch><duration>2</duration><tie type=\"stop\"/></note>"
        "<forward><duration>2</duration></forward></measure>"
        "<measure number=\"2\"><note><pitch><step>G</step><octave>2</octave></pitch><duration>4</duration></note>"
        "</measure></part></score-partwise>";

    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.readMusicXml (xml, score), importer.getLastError());

    const auto notes = flatten (score);
    CHECK_MSG (notes.size() == 3, "notes " + juce::String ((int) notes.size()));
    if (notes.size() == 3)
    {
        CHECK (notes[0].midi == 40 && notes[0].stringIndex == 5 && notes[0].fret == 0);
        CHECK (notes[1].midi == 45 && notes[1].stringIndex == 4 && notes[1].fret == 0);
        CHECK_NEAR (notes[1].beat, 1.0, 1e-6);
        CHECK_NEAR (notes[1].length, 2.0, 1e-6);   // the tie lengthened it
        CHECK (notes[2].midi == 43 && notes[2].stringIndex == 5 && notes[2].fret == 3);
        CHECK_NEAR (notes[2].beat, 4.0, 1e-6);     // after the bar, <forward> having used the rest of bar 1
    }
    CHECK (importer.getLastDiagnostics().warnings.joinIntoString (" ").containsIgnoreCase ("fingered"));
}

LUTHIER_TEST (TabRobustness, timewiseMusicXmlIsRefusedPolitely)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (! importer.readMusicXml ("<?xml version=\"1.0\"?><score-timewise><measure number=\"1\"/></score-timewise>", score));
    CHECK (importer.getLastError().containsIgnoreCase ("part-wise"));
}

//==============================================================================
//  ASCII corpus: realistic, original, short pages in every dialect people paste.
//==============================================================================
namespace
{
    juce::File corpus (const char* name)
    {
        return sourceSibling ((juce::String ("Fixtures/TabCorpus/") + name).toRawUTF8());
    }

    struct Loaded
    {
        bool ok = false;
        juce::String error;
        PerformanceScore score;
        TabImportDiagnostics diagnostics;
        std::vector<Flat> notes;
    };

    std::unique_ptr<Loaded> load (const char* name)
    {
        auto out = std::make_unique<Loaded>();
        NotationImporter importer;
        out->ok = importer.read (corpus (name), out->score);
        out->error = importer.getLastError();
        out->diagnostics = importer.getLastDiagnostics();
        if (out->ok)
            out->notes = flatten (out->score);
        return out;
    }

    /** Notes that start at (almost) `beat`, as sorted MIDI values. */
    std::vector<int> midiAt (const std::vector<Flat>& notes, double beat)
    {
        std::vector<int> v;
        for (const auto& n : notes)
            if (std::abs (n.beat - beat) < 0.02)
                v.push_back (n.midi);
        std::sort (v.begin(), v.end());
        return v;
    }

    struct Expect
    {
        const char* file;
        int numStrings, lowTuning, capo, measures, notes;
    };
}

LUTHIER_TEST (TabCorpus, everyDialectLoadsWithTheExpectedShape)
{
    static const Expect table[] =
    {
        { "ug_dropd_capo.txt",         6, 38, 2, 1, 14 },
        { "olga_classic.txt",          6, 40, 0, 1, 9  },
        { "numeric_labels.txt",        6, 40, 0, 1, 11 },
        { "low_to_high.txt",           6, 40, 0, 1, 12 },
        { "bass_four.txt",             4, 28, 0, 1, 4  },
        { "bass_five_low_b.txt",       5, 23, 0, 1, 4  },
        { "seven_string.txt",          7, 35, 0, 1, 14 },
        { "unicode_box.txt",           6, 40, 0, 1, 9  },
        { "endash_fullwidth.txt",      6, 40, 0, 1, 13 },
        { "markdown_fence.md",         6, 40, 0, 1, 9  },
        { "html_pre.html",             6, 39, 3, 1, 9  },
        { "crlf_tabs.txt",             6, 38, 0, 1, 12 },
        { "repeat_x3.txt",             6, 40, 0, 3, 27 },
        { "rhythm_letters.txt",        6, 40, 0, 1, 5  },
        { "count_line.txt",            6, 40, 0, 1, 12 },
        { "wrapped_lyrics_systems.txt",6, 40, 0, 3, 30 },
        { "dash_label_nobars.txt",     6, 40, 0, 1, 13 },
        { "gp_ascii_export.txt",       6, 40, 0, 1, 5  },
        { "bom_double_bar.txt",        6, 40, 0, 2, 9  },
        { "mismatched_strings.txt",    6, 40, 0, 2, 9  },
        { "two_digit_frets.txt",       6, 40, 0, 1, 4  },
        { "tuning_footer_eb.txt",      6, 39, 0, 1, 6  },
    };

    for (const auto& e : table)
    {
        const auto r = load (e.file);
        CHECK_MSG (r->ok, juce::String (e.file) + ": " + r->error);
        if (! r->ok)
            continue;

        const auto& track = r->score.getTrack (0);
        CHECK_MSG (track.numStrings == e.numStrings, juce::String (e.file) + " strings " + juce::String (track.numStrings));
        CHECK_MSG (track.tuning[(size_t) track.numStrings - 1] == e.lowTuning,
                   juce::String (e.file) + " low string " + juce::String (track.tuning[(size_t) track.numStrings - 1]));
        CHECK_MSG (track.capoFret == e.capo, juce::String (e.file) + " capo " + juce::String (track.capoFret));
        CHECK_MSG ((int) track.measures.size() == e.measures, juce::String (e.file) + " measures " + juce::String ((int) track.measures.size()));
        CHECK_MSG ((int) r->notes.size() == e.notes, juce::String (e.file) + " notes " + juce::String ((int) r->notes.size()));

        // Every note sounds the pitch its string, fret and the capo say.
        for (const auto& n : r->notes)
            CHECK_MSG (n.midi == track.tuning[(size_t) n.stringIndex] + track.capoFret + n.fret,
                       juce::String (e.file) + " pitch of s" + juce::String (n.stringIndex) + " f" + juce::String (n.fret));
    }
}

LUTHIER_TEST (TabCorpus, tuningNamedInTheHeaderOrFooterIsApplied)
{
    {
        const auto r = load ("crlf_tabs.txt");     // DADGAD, tab-indented, CRLF
        CHECK (r->ok);
        const auto& t = r->score.getTrack (0).tuning;
        CHECK (t[0] == 62 && t[1] == 57 && t[2] == 55 && t[3] == 50 && t[4] == 45 && t[5] == 38);
    }
    {
        const auto r = load ("tuning_footer_eb.txt");   // all strings a semitone down
        CHECK (r->ok);
        CHECK (r->score.getTrack (0).tuning[0] == 63 && r->score.getTrack (0).tuning[5] == 39);
        CHECK (! r->notes.empty() && r->notes.front().midi == 39);
    }
    {
        const auto r = load ("html_pre.html");     // "Capo&nbsp;3 &amp; Tuning: Half step down"
        CHECK (r->ok);
        CHECK (r->score.getTrack (0).capoFret == 3);
        CHECK (r->score.getTrack (0).tuning[5] == 39);
    }
}

LUTHIER_TEST (TabCorpus, ultimateGuitarPageKeepsTabAndDropsChordMarkup)
{
    const auto r = load ("ug_dropd_capo.txt");
    CHECK (r->ok);
    if (! r->ok) return;

    // Drop D + capo 2: the open low string sounds E2 (38 + 2).
    const auto first = midiAt (r->notes, r->notes.front().beat);
    CHECK (first.size() == 6);                         // all six strings in the first column
    CHECK (std::find (first.begin(), first.end(), 40) != first.end());

    CHECK (r->notes.front().stringIndex >= 0);
    int lowStringNotes = 0;
    for (const auto& n : r->notes)
        if (n.stringIndex == 5) { ++lowStringNotes; CHECK (n.fret == 0 && n.midi == 40); }
    CHECK (lowStringNotes == 1);
}

LUTHIER_TEST (TabCorpus, techniquesAreRecognised)
{
    const auto r = load ("olga_classic.txt");
    CHECK (r->ok);
    if (r->notes.size() != 9) { CHECK (false); return; }

    const int frets[] = { 5, 7, 5, 7, 9, 7, 5, 7, 0 };
    for (size_t i = 0; i < 9; ++i)
        CHECK_MSG (r->notes[i].fret == frets[i], "fret " + juce::String ((int) i) + " = " + juce::String (r->notes[i].fret));

    using T = ScoreTechnique::Type;
    CHECK (r->notes[1].has (T::hammerOn));
    CHECK (r->notes[2].has (T::pullOff));
    CHECK (r->notes[3].has (T::slideLegato));
    CHECK (r->notes[4].has (T::slideLegato) || r->notes[4].fret == 9);
    CHECK (r->notes[6].has (T::bendRelease) || r->notes[6].has (T::bend));
    CHECK (r->notes[7].has (T::vibrato));
    CHECK (r->notes[8].has (T::deadNote));

    // Timing order is strictly increasing in column order.
    for (size_t i = 1; i < r->notes.size(); ++i)
        CHECK (r->notes[i].beat > r->notes[i - 1].beat);
}

LUTHIER_TEST (TabCorpus, systemOrderLabelsAndGlyphsDoNotChangeThePitches)
{
    // The same two chords written four ways must give the same pitches.
    const auto numbered = load ("numeric_labels.txt");
    const auto lowFirst = load ("low_to_high.txt");
    CHECK (numbered->ok && lowFirst->ok);
    if (! (numbered->ok && lowFirst->ok)) return;

    // numbered: high-e 0 / B 1 / G 0 / D 2 / A 3 (an open C-ish shape), then a different one
    const auto first = midiAt (numbered->notes, numbered->notes.front().beat);
    CHECK ((first == std::vector<int> { 48, 52, 55, 60, 64 }));

    // low_to_high: E3 A2 D2 G1 B0 e0 read from the bottom up -> same strings as written.
    CHECK (lowFirst->notes.front().stringIndex == 5 && lowFirst->notes.front().fret == 3);
    const auto chord = midiAt (lowFirst->notes, lowFirst->notes.front().beat);
    CHECK ((chord == std::vector<int> { 43, 47, 52, 56, 59, 64 }));
    CHECK (lowFirst->diagnostics.systemsReversed == 1);
    CHECK (numbered->diagnostics.stringLabelsRewritten == 6);

    const auto box = load ("unicode_box.txt");
    const auto dashes = load ("endash_fullwidth.txt");
    CHECK (box->ok && dashes->ok);
    CHECK (box->diagnostics.unicodeGlyphsMapped > 0);
    CHECK (dashes->diagnostics.unicodeGlyphsMapped > 0);
    if (box->ok)
        CHECK ((midiAt (box->notes, box->notes.front().beat) == std::vector<int> { 48, 52, 55, 60, 64 }));
}

LUTHIER_TEST (TabCorpus, repeatsAreUnrolled)
{
    const auto r = load ("repeat_x3.txt");
    CHECK (r->ok);
    if (! r->ok) return;
    const auto& m = r->score.getTrack (0).measures;
    CHECK (m.size() == 3);
    // Three identical bars: the same notes at the same offsets.
    const auto bar = [&] (size_t i)
    {
        std::vector<std::tuple<double, int, int>> v;
        for (const auto& voice : m[i].voices)
            for (const auto& n : voice.notes)
                v.emplace_back ((double) std::llround (n.startBeat * 1000.0), n.stringIndex, n.fret);
        std::sort (v.begin(), v.end());
        return v;
    };
    CHECK (bar (0).size() == 9);
    CHECK (bar (1) == bar (0));
    CHECK (bar (2) == bar (0));
}

LUTHIER_TEST (TabCorpus, rhythmAndCountLinesPlaceTheNotesInTime)
{
    {
        const auto r = load ("rhythm_letters.txt");     // q q e e q
        CHECK (r->ok && r->notes.size() == 5);
        if (r->notes.size() == 5)
        {
            const double expect[] = { 0.0, 1.0, 2.0, 2.5, 3.0 };
            for (size_t i = 0; i < 5; ++i)
                CHECK_NEAR (r->notes[i].beat, expect[i], 1e-6);
            CHECK_NEAR (r->notes[0].length, 1.0, 1e-6);
            CHECK_NEAR (r->notes[2].length, 0.5, 1e-6);
        }
    }
    {
        const auto r = load ("count_line.txt");          // 1 e & a 2 e & a 3 e & a 4
        CHECK (r->ok && r->notes.size() == 12);
        if (r->notes.size() == 12)
        {
            const double expect[] = { 0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 2.25, 2.5, 2.75, 3.0 };
            const int frets[] = { 0, 2, 3, 5, 7, 8, 10, 12, 0, 0, 0, 3 };
            for (size_t i = 0; i < 12; ++i)
            {
                CHECK_NEAR (r->notes[i].beat, expect[i], 1e-6);
                CHECK (r->notes[i].fret == frets[i]);
            }
        }
    }
}

LUTHIER_TEST (TabCorpus, chordOnlyChartsBecomeStrummedBars)
{
    {
        const auto r = load ("chords_ug.txt");          // UG [ch] markup, "Capo 1"
        CHECK_MSG (r->ok, r->error);
        if (! r->ok) return;
        const auto& track = r->score.getTrack (0);
        CHECK (track.measures.size() == 8);
        CHECK (track.capoFret == 1);
        CHECK (r->diagnostics.chordChartBars == 8);

        const char* names[] = { "Am", "C", "G", "D/F#", "F", "G", "Am7", "Em" };
        for (size_t i = 0; i < 8 && i < track.measures.size(); ++i)
        {
            CHECK (track.measures[i].chordSymbols.size() == 1);
            if (! track.measures[i].chordSymbols.empty())
                CHECK (track.measures[i].chordSymbols[0].second == names[i]);
        }

        // Am shape x02210 + capo 1 on beat 0 of bar 1.
        CHECK ((midiAt (r->notes, 0.0) == std::vector<int> { 46, 53, 58, 61, 65 }));
        // and again on beat 3 (a second strum in the bar)
        CHECK ((midiAt (r->notes, 2.0) == std::vector<int> { 46, 53, 58, 61, 65 }));
    }
    {
        const auto r = load ("chords_chordpro.txt");    // ChordPro {capo: 2} and [Am]inline
        CHECK (r->ok);
        if (r->ok)
        {
            CHECK (r->score.getTrack (0).measures.size() == 4);
            CHECK (r->score.getTrack (0).capoFret == 2);
            CHECK_NEAR (r->score.getMeta().tempoBpm, 90.0, 0.01);
        }
    }
    {
        const auto r = load ("chords_plain.txt");       // "Intro: Am F C G", "x2"
        CHECK (r->ok);
        if (r->ok)
        {
            const auto& m = r->score.getTrack (0).measures;
            CHECK (m.size() == 10);
            const char* names[] = { "Am", "F", "C", "G", "Am", "F", "C", "G", "C", "G" };
            for (size_t i = 0; i < m.size() && i < 10; ++i)
                if (! m[i].chordSymbols.empty())
                    CHECK (m[i].chordSymbols[0].second == names[i]);
        }
    }
}

LUTHIER_TEST (TabCorpus, chordShapesAreFrettableAndSoundTheRoot)
{
    const char* symbols[] = { "C", "D", "E", "G", "A", "F", "B", "Bb", "F#m", "C#m7", "Abmaj7", "Dsus4", "G7", "E5",
                              "Cadd9", "D/F#", "Am7", "Ebm", "A9", "G6", "Bdim", "Faug", "Gmaj9", "Em11", "C7sus4" };

    for (const char* symbol : symbols)
    {
        TabChordChart::Chord chord;
        CHECK_MSG (TabChordChart::parseChord (symbol, chord), symbol);

        const auto frets = TabChordChart::shapeFor (chord, 6);
        CHECK (frets.size() == 6);
        int played = 0, lowestFret = 99, highestFret = 0;
        for (const int f : frets)
            if (f >= 0) { ++played; lowestFret = juce::jmin (lowestFret, f); highestFret = juce::jmax (highestFret, f); }
        CHECK_MSG (played >= 3, juce::String (symbol) + " plays " + juce::String (played) + " strings");
        CHECK_MSG (highestFret <= 15, juce::String (symbol) + " runs to fret " + juce::String (highestFret));

        // The lowest sounding string carries the bass (the root, or the slash bass).
        static const int open[6] = { 64, 59, 55, 50, 45, 40 };
        int lowest = -1;
        for (int s = 5; s >= 0 && lowest < 0; --s)
            if (frets[(size_t) s] >= 0) lowest = open[s] + frets[(size_t) s];
        const int expectedBass = chord.bassPitchClass >= 0 ? chord.bassPitchClass : chord.rootPitchClass;
        CHECK_MSG (lowest % 12 == expectedBass, juce::String (symbol) + " bass " + juce::String (lowest % 12)
                                                + " expected " + juce::String (expectedBass));
    }

    // Things that are not chords.
    for (const char* word : { "Hello", "Bad", "Add", "Verse", "x2", "I", "H", "Cat", "" })
    {
        TabChordChart::Chord chord;
        CHECK_MSG (! TabChordChart::parseChord (word, chord), juce::String ("not a chord: ") + word);
    }
}

LUTHIER_TEST (TabCorpus, drumTabAndEmptyInputAreRefusedWithAnExplanation)
{
    const auto drums = load ("drums_only.txt");
    CHECK (! drums->ok);
    CHECK (drums->error.containsIgnoreCase ("drum"));

    for (const char* name : { "empty.txt", "whitespace_only.txt" })
    {
        const auto r = load (name);
        CHECK (! r->ok);
        CHECK (r->error.containsIgnoreCase ("empty") || r->error.containsIgnoreCase ("no tablature"));
    }
}

LUTHIER_TEST (TabCorpus, mismatchedStringCountsDegradeGracefully)
{
    const auto r = load ("mismatched_strings.txt");
    CHECK (r->ok);
    if (! r->ok) return;
    CHECK (r->score.getTrack (0).numStrings == 6);
    CHECK (r->notes.size() == 9);                      // 6 + the 3 strings the short system has
    CHECK (r->diagnostics.warnings.joinIntoString (" ").containsIgnoreCase ("3 strings"));
}

LUTHIER_TEST (TabCorpus, playbackCompilationPlacesTheImportedNotes)
{
    for (const char* name : { "ug_dropd_capo.txt", "numeric_labels.txt", "low_to_high.txt", "unicode_box.txt", "bass_four.txt" })
    {
        const auto r = load (name);
        CHECK_MSG (r->ok, name);
        if (! r->ok) continue;

        const auto riff = Riff::fromScore (r->score);
        const auto instrument = GuitarSpecSummary::forRiff (riff);
        const auto compiled = RiffCompiler::compile (riff, RiffPlaySettings{}, instrument);
        CHECK_MSG (compiled != nullptr, name);
        if (compiled == nullptr) continue;

        // Everything written is played, on the string it was written on, at its pitch.
        CHECK_MSG (compiled->placement.dropped == 0, name);
        CHECK_MSG (compiled->placement.notes.size() == r->notes.size(), name);
        std::multiset<int> written, placed;
        for (const auto& n : r->notes) written.insert (n.midi);
        for (const auto& n : compiled->placement.notes) placed.insert (n.midiNote);
        CHECK_MSG (written == placed, name);
    }
}

//==============================================================================
//  Adversarial input: nothing may crash, hang, or allocate without bound.
//==============================================================================
namespace
{
    struct Timed
    {
        bool ok = false;
        double seconds = 0.0;
        juce::String error;
    };

    Timed readText (const juce::String& text)
    {
        Timed t;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        NotationImporter importer;
        PerformanceScore score;
        t.ok = importer.readAsciiTab (text, score);
        t.error = importer.getLastError();
        t.seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        return t;
    }
}

LUTHIER_TEST (TabAdversarial, tenMegabytesOfDashesIsRefusedQuickly)
{
    const auto t = readText (juce::String::repeatedString ("-", 10 * 1024 * 1024));
    CHECK (! t.ok);
    CHECK (t.error.isNotEmpty());
    CHECK_MSG (t.seconds < 5.0, "took " + juce::String (t.seconds) + "s");

    const auto lines = readText (juce::String::repeatedString (juce::String::repeatedString ("-", 79) + "\n", 130000));
    CHECK (! lines.ok);
    CHECK_MSG (lines.seconds < 5.0, "took " + juce::String (lines.seconds) + "s");
}

LUTHIER_TEST (TabAdversarial, oneEnormousLineCostsLinearTimeNotQuadratic)
{
    for (const char* unit : { "-", "|", "(", "[ch][", "e|--0--" })
    {
        const auto t = readText (juce::String::repeatedString (unit, 1900000 / (int) std::strlen (unit)));
        CHECK (! t.ok);
        CHECK_MSG (t.seconds < 8.0, juce::String (unit) + " took " + juce::String (t.seconds) + "s");
    }
}

LUTHIER_TEST (TabAdversarial, binaryAndRandomTextAreHandled)
{
    juce::Random rng (99);

    juce::String binary;
    for (int i = 0; i < 200000; ++i)
        binary += juce::String::charToString ((juce::juce_wchar) (rng.nextInt (255) + 1));
    const auto b = readText (binary);
    CHECK (! b.ok);
    CHECK_MSG (b.seconds < 8.0, "binary took " + juce::String (b.seconds));

    juce::String tabby;
    static const char alphabet[] = "-|0123456789ehpbrx/\\~()[]<>\n :.";
    for (int i = 0; i < 400000; ++i)
        tabby += alphabet[rng.nextInt ((int) sizeof (alphabet) - 1)];
    const auto t = readText (tabby);
    CHECK_MSG (t.seconds < 8.0, "random tab-ish text took " + juce::String (t.seconds));

    for (const char* s : { "", " ", "\n\n\n", "|", "e|", "e|-", "e|--|", "|||||||", "e|:::|", "e|--0--|x99999999999",
                           "e|--\x7f--|", "[ch][/ch][ch]", "[[[[]]]]", "((((", "\t\t\t", "e|--0--|\nB|--1--|" })
        readText (s);   // must simply return
}

LUTHIER_TEST (TabAdversarial, hugeRepeatCountsAndManySystemsAreBounded)
{
    const auto r = readText ("e|:--0--:|x999999999\nB|:-----:|\nG|:-----:|\nD|:-----:|\nA|:-----:|\nE|:-----:|\n");
    CHECK (r.ok);
    CHECK (r.seconds < 5.0);

    const auto many = readText (juce::String::repeatedString (
        "e|:--0--:|x99\nB|:-----:|\nG|:-----:|\nD|:-----:|\nA|:-----:|\nE|:-----:|\n", 2000));
    CHECK (many.seconds < 8.0);
}

LUTHIER_TEST (TabAdversarial, stringCountMismatchesNeverCrash)
{
    juce::String text;
    for (int rows = 1; rows <= 14; ++rows)
    {
        for (int i = 0; i < rows; ++i)
            text << "e|--" << (i % 10) << "--|\n";
        text << "\n";
    }
    const auto t = readText (text);
    CHECK (t.seconds < 5.0);
}

LUTHIER_TEST (TabAdversarial, importerFilesWithWrongContentAreRefusedNotCrashed)
{
    juce::Random rng (5);
    for (const char* ext : { ".mid", ".midi", ".gp", ".gp3", ".gp4", ".gp5", ".gpx", ".ptb", ".xml", ".musicxml", ".mxl", ".txt", ".tab" })
    {
        auto f = tempFile (juce::String ("garbage") + ext);
        juce::MemoryBlock junk (4096);
        for (size_t i = 0; i < junk.getSize(); ++i)
            junk[i] = (char) rng.nextInt (256);
        f.replaceWithData (junk.getData(), junk.getSize());

        NotationImporter importer;
        PerformanceScore score;
        const bool ok = importer.read (f, score);
        if (! ok)
            CHECK_MSG (importer.getLastError().isNotEmpty(), ext);
        f.deleteFile();

        // And a zero-byte file.
        f.replaceWithText ("");
        const bool emptyOk = importer.read (f, score);
        if (! emptyOk)
            CHECK_MSG (importer.getLastError().isNotEmpty(), juce::String ("empty ") + ext);
        f.deleteFile();
    }
}

LUTHIER_TEST (TabAdversarial, truncatedAndMutatedMusicXmlAndMidiAreSurvived)
{
    NotationExporter exporter;
    auto xml = tempFile ("mut.musicxml");
    auto mid = tempFile ("mut.mid");
    const auto score = buildScore();
    CHECK (exporter.write (score, NotationFormat::musicXml, xml));
    CHECK (exporter.write (score, NotationFormat::midi, mid));

    const auto text = xml.loadFileAsString();
    juce::MemoryBlock midiBytes;
    CHECK (mid.loadFileAsData (midiBytes));

    juce::Random rng (3);
    for (int round = 0; round < 60; ++round)
    {
        NotationImporter importer;
        PerformanceScore out;

        importer.readMusicXml (text.substring (0, rng.nextInt (text.length())), out);

        juce::MemoryBlock m (midiBytes);
        const int cut = rng.nextInt ((int) m.getSize());
        if (round % 2 == 0)
            m.setSize ((size_t) cut);
        else
            m[(size_t) cut] = (char) rng.nextInt (256);
        importer.readMidi (m.getData(), m.getSize(), out);
    }

    xml.deleteFile();
    mid.deleteFile();
}

//==============================================================================
//  MIDI: band files (type 1 multi-track, type 0 multi-channel) give one guitar part.
//==============================================================================
namespace
{
    void addNotes (juce::MidiMessageSequence& seq, int channel, const std::vector<int>& notes)
    {
        double tick = 0.0;
        for (const int n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (channel, n, (juce::uint8) 96), tick);
            seq.addEvent (juce::MidiMessage::noteOff (channel, n), tick + 400.0);
            tick += 480.0;
        }
    }

    juce::MemoryBlock midiBytes (juce::MidiFile& f, int type)
    {
        juce::MemoryOutputStream out;
        f.writeTo (out, type);
        return out.getMemoryBlock();
    }
}

LUTHIER_TEST (TabRobustness, multiTrackMidiPicksTheGuitarAndIgnoresDrums)
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);

    juce::MidiMessageSequence conductor, drums, bass, guitar;
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent (400000), 0.0);        // 150 bpm
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0.0);
    addNotes (drums, 10, { 36, 38, 36, 38, 36, 38, 36, 38, 36, 38, 36, 38 });   // the busiest part
    bass.addEvent (juce::MidiMessage::programChange (2, 33), 0.0);
    addNotes (bass, 2, { 28, 33, 28, 33, 28, 33, 28, 33 });
    guitar.addEvent (juce::MidiMessage::programChange (3, 29), 0.0);
    addNotes (guitar, 3, { 52, 55, 59 });
    for (auto* s : { &conductor, &drums, &bass, &guitar }) { s->updateMatchedPairs(); file.addTrack (*s); }

    const auto bytes = midiBytes (file, 1);

    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.readMidi (bytes.getData(), bytes.getSize(), score), importer.getLastError());

    const auto notes = flatten (score);
    CHECK_MSG (notes.size() == 3, "notes " + juce::String ((int) notes.size()));
    if (notes.size() == 3)
    {
        CHECK (notes[0].midi == 52 && notes[1].midi == 55 && notes[2].midi == 59);
        CHECK_NEAR (notes[1].beat, 1.0, 0.01);
        CHECK (notes[0].midi == score.getTrack (0).tuning[(size_t) notes[0].stringIndex] + notes[0].fret);
    }
    CHECK (score.getTrack (0).numStrings == 6);
    CHECK_NEAR (score.getMeta().tempoBpm, 150.0, 0.5);
    CHECK (score.getTrack (0).measures[0].timeSignatureNumerator == 3);
    CHECK (importer.getLastDiagnostics().warnings.joinIntoString (" ").containsIgnoreCase ("drums ignored"));
}

LUTHIER_TEST (TabRobustness, singleTrackMidiWithDrumChannelDropsTheDrums)
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);

    juce::MidiMessageSequence all;
    all.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
    all.addEvent (juce::MidiMessage::programChange (1, 25), 0.0);
    addNotes (all, 1, { 45, 52, 57, 64 });
    addNotes (all, 10, { 42, 42, 42, 42, 42, 42, 42, 42 });
    all.updateMatchedPairs();
    file.addTrack (all);

    const auto bytes = midiBytes (file, 0);
    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.readMidi (bytes.getData(), bytes.getSize(), score), importer.getLastError());
    const auto notes = flatten (score);
    CHECK (notes.size() == 4);
    for (const auto& n : notes)
        CHECK (n.midi == 45 || n.midi == 52 || n.midi == 57 || n.midi == 64);
}

LUTHIER_TEST (TabRobustness, perStringExportStillRoundTripsThroughMidi)
{
    PerformanceScore score;
    score.beginCapture (96.0, 4, 4);
    score.noteStarted (5, 3, 43, 98.0, 0.8, 0.0);  score.noteEnded (5, 1.0);
    score.noteStarted (3, 2, 52, 164.8, 0.8, 1.0); score.noteEnded (3, 2.0);
    score.noteStarted (0, 5, 69, 440.0, 0.8, 2.0); score.noteEnded (0, 3.5);
    score.endCapture (4.0);

    auto mid = tempFile ("perstring.mid");
    NotationExporter exporter;
    CHECK (exporter.write (score, NotationFormat::midi, mid));

    NotationImporter importer;
    PerformanceScore back;
    CHECK_MSG (importer.read (mid, back), importer.getLastError());

    const auto notes = flatten (back);
    CHECK (notes.size() == 3);
    if (notes.size() == 3)
    {
        CHECK (notes[0].stringIndex == 5 && notes[0].fret == 3);   // the exporter's strings survive
        CHECK (notes[1].stringIndex == 3 && notes[1].fret == 2);
        CHECK (notes[2].stringIndex == 0 && notes[2].fret == 5);
    }
    mid.deleteFile();
}
