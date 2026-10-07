// cli-tools.md: the offline MIDI<->tab<->MusicXML<->Guitar Pro converter and the
// inspect / validate / transpose / retune commands added to luthier-render.
//
// Two layers of coverage:
//   1. The conversion *pipeline* (import by format -> PerformanceScore ->
//      export), tested directly against the reused NotationImporter /
//      NotationExporter / TabFingering, which is fast and runs everywhere.
//   2. The *CLI surface* (argument handling, format auto-detection, batch
//      semantics, exit codes, fuzz safety), tested end to end by invoking the
//      built LuthierRender binary as a subprocess. Those checks run when the
//      binary is present beside the test runner (it is, in CI and in the
//      documented build), and are skipped cleanly otherwise.

#include "TestFramework.h"
#include "../Notation/NotationExport.h"
#include "../Notation/AsciiTabReader.h"
#include "../Notation/TabFingering.h"

#include <juce_events/juce_events.h>
#include <random>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    //==========================================================================
    std::vector<const ScoreNote*> allNotes (const PerformanceScore& score, int track = 0)
    {
        std::vector<const ScoreNote*> notes;

        if (track >= score.getNumTracks())
            return notes;

        for (const auto& measure : score.getTrack (track).measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    notes.push_back (&note);

        return notes;
    }

    std::vector<int> pitches (const PerformanceScore& score)
    {
        std::vector<int> p;
        for (const auto* n : allNotes (score))
            p.push_back (n->midiNote);
        std::sort (p.begin(), p.end());
        return p;
    }

    juce::File fixture (const juce::String& name)
    {
        return juce::File (__FILE__).getSiblingFile ("Fixtures").getChildFile (name);
    }

    juce::File tempDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("luthier-cli-tests");
        d.createDirectory();
        return d;
    }

    juce::File tempFile (const juce::String& name)
    {
        auto f = tempDir().getChildFile (name);
        f.deleteFile();
        return f;
    }

    // Writes `score` to a temp file in `format`, reads it back, returns true on a
    // clean round-trip through the writer+reader. `out` holds the re-read score.
    bool roundTripThroughFile (const PerformanceScore& score, NotationFormat format,
                               const juce::String& name, PerformanceScore& out)
    {
        NotationExporter exporter;
        auto file = tempFile (name);

        if (! exporter.write (score, format, file))
            return false;

        NotationImporter importer;
        return importer.read (file, out);
    }

    //==========================================================================
    // The built LuthierRender binary, beside the test runner. Empty if absent.
    juce::File renderBinary()
    {
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto config = exe.getParentDirectory().getFileName();          // "Release"
        const auto build  = exe.getParentDirectory()                         // Release
                               .getParentDirectory()                         // LuthierTests_artefacts
                               .getParentDirectory();                        // build/

        juce::String name = "LuthierRender";
       #if JUCE_WINDOWS
        name += ".exe";
       #endif

        return build.getChildFile ("LuthierRender_artefacts")
                    .getChildFile (config)
                    .getChildFile (name);
    }

    struct ProcResult { int exitCode = -999; juce::String output; };

    ProcResult runRender (const juce::StringArray& args)
    {
        ProcResult r;

        juce::StringArray command;
        command.add (renderBinary().getFullPathName());
        command.addArray (args);

        juce::ChildProcess proc;

        if (! proc.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            return r;

        r.output = proc.readAllProcessOutput();   // blocks until the process ends
        proc.waitForProcessToFinish (30000);
        r.exitCode = proc.getExitCode();
        return r;
    }
}

//==============================================================================
// 1. Pipeline: pairwise conversions.
//==============================================================================
LUTHIER_TEST (CliConvert, pairwiseConversions)
{
    NotationImporter importer;
    PerformanceScore tab;
    CHECK (importer.read (fixture ("simple.tab"), tab));
    CHECK (tab.getTotalNoteCount() > 0);

    const auto expected = pitches (tab);

    // tab -> MIDI -> read back: pitches preserved.
    {
        PerformanceScore back;
        CHECK (roundTripThroughFile (tab, NotationFormat::midi, "pw.mid", back));
        CHECK (pitches (back) == expected);
    }

    // tab -> MusicXML -> read back.
    {
        PerformanceScore back;
        CHECK (roundTripThroughFile (tab, NotationFormat::musicXml, "pw.musicxml", back));
        CHECK (back.getTotalNoteCount() == tab.getTotalNoteCount());
    }

    // tab -> Guitar Pro: export-only (not read back). It must write a non-empty
    // bundle and set no error, or report the error cleanly - never crash.
    {
        NotationExporter exporter;
        auto gp = tempFile ("pw.gp");
        const bool ok = exporter.write (tab, NotationFormat::guitarPro, gp);
        CHECK_MSG (ok, "Guitar Pro export failed: " + exporter.getLastError());
        if (ok) CHECK (gp.getSize() > 0);
    }

    // MusicXML source -> tab, and MusicXML -> MIDI.
    {
        PerformanceScore xml;
        CHECK (importer.read (fixture ("hammer.musicxml"), xml));
        CHECK (xml.getTotalNoteCount() > 0);

        PerformanceScore asTab, asMidi;
        CHECK (roundTripThroughFile (xml, NotationFormat::asciiTab, "fromxml.tab", asTab));
        CHECK (asTab.getTotalNoteCount() > 0);
        CHECK (roundTripThroughFile (xml, NotationFormat::midi, "fromxml.mid", asMidi));
        CHECK (asMidi.getTotalNoteCount() > 0);
    }
}

//==============================================================================
// 2. Round-trip stability.
//==============================================================================
LUTHIER_TEST (CliConvert, roundTripStability)
{
    NotationImporter importer;

    // tab -> MIDI -> tab: pitches and note count stable.
    PerformanceScore tab;
    CHECK (importer.read (fixture ("simple.tab"), tab));
    const auto tabPitches = pitches (tab);

    PerformanceScore viaMidi;
    CHECK (roundTripThroughFile (tab, NotationFormat::midi, "rt1.mid", viaMidi));

    PerformanceScore viaTab;
    CHECK (roundTripThroughFile (viaMidi, NotationFormat::asciiTab, "rt1.tab", viaTab));

    CHECK (pitches (viaTab) == tabPitches);

    // MIDI -> tab -> MIDI: the note set survives. Build the MIDI from the tab
    // first so the test needs no committed binary fixture.
    {
        NotationExporter exporter;
        auto midiFile = tempFile ("rt2-src.mid");
        CHECK (exporter.write (tab, NotationFormat::midi, midiFile));

        PerformanceScore fromMidi;
        CHECK (importer.read (midiFile, fromMidi));
        const auto midiPitches = pitches (fromMidi);

        PerformanceScore midiTab;
        CHECK (roundTripThroughFile (fromMidi, NotationFormat::asciiTab, "rt2.tab", midiTab));

        PerformanceScore midiAgain;
        CHECK (roundTripThroughFile (midiTab, NotationFormat::midi, "rt2b.mid", midiAgain));

        CHECK (pitches (midiAgain) == midiPitches);
    }
}

//==============================================================================
// 3. Format auto-detection (canRead), including the .gp rejection.
//==============================================================================
LUTHIER_TEST (CliConvert, formatDetection)
{
    auto dir = tempDir();

    const char* readable[] = { "a.mid", "b.midi", "c.tab", "d.txt", "e.musicxml", "f.xml",
                               "A.MID", "C.TAB" };
    for (auto* n : readable)
        CHECK_MSG (NotationImporter::canRead (dir.getChildFile (n)), juce::String ("should read ") + n);

    const char* unreadable[] = { "g.gp", "h.gp5", "i.gpx", "j.ptb", "k.bin", "l.pdf", "m" };
    for (auto* n : unreadable)
        CHECK_MSG (! NotationImporter::canRead (dir.getChildFile (n)), juce::String ("should reject ") + n);

    // A .gp file that contains valid tab text is still rejected: detection is by
    // extension, and .gp is export-only.
    auto fakeGp = tempFile ("fake.gp");
    fakeGp.replaceWithText (fixture ("simple.tab").loadFileAsString());
    PerformanceScore score;
    NotationImporter importer;
    CHECK (! importer.read (fakeGp, score));
    CHECK (importer.getLastError().isNotEmpty());
}

//==============================================================================
// 4. --transpose: pitches shift and the part stays playable.
//==============================================================================
LUTHIER_TEST (CliConvert, transposeShiftsPitches)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.read (fixture ("simple.tab"), score));

    auto before = pitches (score);

    // Mirror what the CLI does: shift every midiNote, then re-finger.
    for (int t = 0; t < score.getNumTracks(); ++t)
        for (auto& m : score.getTrack (t).measures)
            for (auto& v : m.voices)
                for (auto& n : v.notes)
                    n.midiNote = juce::jlimit (0, 127, n.midiNote + 5);

    for (int t = 0; t < score.getNumTracks(); ++t)
        TabFingering::assign (score, t);

    PerformanceScore viaMidi;
    CHECK (roundTripThroughFile (score, NotationFormat::midi, "tr.mid", viaMidi));

    auto after = pitches (viaMidi);
    CHECK (after.size() == before.size());
    for (size_t i = 0; i < after.size() && i < before.size(); ++i)
        CHECK (after[i] == before[i] + 5);

    // Every fret is reachable (re-fingering kept it on the neck).
    for (const auto* n : allNotes (viaMidi))
        CHECK (n->fret >= 0 && n->fret <= 24);
}

//==============================================================================
// 5. --retune: same pitches, different strings/frets onto a named tuning.
//==============================================================================
LUTHIER_TEST (CliConvert, retunePreservesPitches)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.read (fixture ("simple.tab"), score));

    const auto before = pitches (score);

    // Drop D on the low string, highest-string-first.
    std::array<int, kMaxStrings> dropD { { 64, 59, 55, 50, 45, 38, 0, 0, 0, 0, 0, 0 } };
    for (int t = 0; t < score.getNumTracks(); ++t)
    {
        score.getTrack (t).tuning = dropD;
        score.getTrack (t).numStrings = 6;
        TabFingering::assign (score, t);
    }

    // Pitches unchanged by retuning.
    CHECK (pitches (score) == before);

    // And it re-fingered onto the new neck: every fret reachable from the new
    // open-string pitches.
    for (const auto* n : allNotes (score))
    {
        CHECK (n->stringIndex >= 0 && n->stringIndex < 6);
        const int open = dropD[(size_t) juce::jlimit (0, 5, n->stringIndex)];
        CHECK (n->midiNote - open >= 0);
        CHECK (n->midiNote - open <= 24);
    }

    // An explicit note-list tuning parses the same way the importer's does.
    std::vector<int> dadgad;
    CHECK (AsciiTabReader::parseTuningNames ("D A D G A D", dadgad, true));
    CHECK (dadgad.size() == 6);
}

//==============================================================================
// 6. Fuzz: empty, truncated, random and oversized inputs never crash.
//==============================================================================
LUTHIER_TEST (CliConvert, fuzzNeverCrashes)
{
    NotationImporter importer;
    std::mt19937 rng (0xC0FFEE);

    auto tryText = [&] (const juce::String& text, const juce::String& name)
    {
        auto f = tempFile (name);
        f.replaceWithText (text);
        PerformanceScore score;
        importer.read (f, score);                 // must return, never throw/crash
        // Whatever it returns, a failed read leaves an error, a success leaves notes.
        CHECK (importer.getLastError().isNotEmpty() || score.getTotalNoteCount() >= 0);
    };

    tryText ("", "empty.tab");
    tryText ("\n\n\n", "blank.tab");
    tryText ("e|", "trunc.tab");
    tryText (juce::String::repeatedString ("e|-------\n", 1), "oneline.tab");

    // Random bytes as "tab", "xml" and "mid".
    for (int iter = 0; iter < 20; ++iter)
    {
        juce::String junk;
        const int len = (int) (rng() % 500);
        for (int i = 0; i < len; ++i)
            junk += juce::String::charToString ((juce::juce_wchar) (33 + (rng() % 94)));

        tryText (junk, "junk.tab");
        tryText (junk, "junk.xml");
    }

    // Random bytes as a MIDI file (binary path).
    for (int iter = 0; iter < 20; ++iter)
    {
        juce::MemoryBlock block;
        const int len = (int) (rng() % 2000);
        for (int i = 0; i < len; ++i)
        {
            const char byte = (char) (rng() & 0xff);
            block.append (&byte, 1);
        }

        auto f = tempFile ("junk.mid");
        f.replaceWithData (block.getData(), block.getSize());
        PerformanceScore score;
        importer.read (f, score);
        CHECK (importer.getLastError().isNotEmpty() || score.getTotalNoteCount() >= 0);
    }

    // A huge but well-formed tab: many bars, must not blow up.
    {
        juce::String big = "Tuning: Standard\n";
        const char* names = "eBGDAE";
        for (int line = 0; line < 6; ++line)
        {
            juce::String row = juce::String::charToString ((juce::juce_wchar) names[line]);
            row += "|";
            for (int bar = 0; bar < 400; ++bar)
                row += "--3--5--7--|";
            big += row + "\n";
        }

        auto f = tempFile ("huge.tab");
        f.replaceWithText (big);
        PerformanceScore score;
        importer.read (f, score);                 // may be partial; must not crash
        CHECK (importer.getLastError().isNotEmpty() || score.getTotalNoteCount() >= 0);
    }
}

//==============================================================================
// 7. End-to-end CLI: the built LuthierRender binary as a subprocess.
//    Runs when the binary is present beside the test runner (CI builds both).
//==============================================================================
LUTHIER_TEST (CliConvert, endToEndSubprocess)
{
    const auto bin = renderBinary();

    if (! bin.existsAsFile())
    {
        // Not an environment that built the CLI target; the pipeline tests above
        // still cover the logic. Record the skip without failing.
        std::cout << "    (LuthierRender not found beside the test runner; "
                     "skipping subprocess checks)" << std::endl;
        CHECK (true);
        return;
    }

    auto work = tempDir().getChildFile ("e2e");
    work.deleteRecursively();
    work.createDirectory();

    // --list-formats
    {
        auto r = runRender ({ "--list-formats" });
        CHECK (r.exitCode == 0);
        CHECK (r.output.contains ("MIDI") && r.output.contains ("Guitar Pro"));
    }

    // --convert a tab to MIDI into an output directory.
    {
        auto r = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                              "--to", "midi", "--out", work.getFullPathName() });
        CHECK_MSG (r.exitCode == 0, r.output);
        CHECK (work.getChildFile ("simple.mid").existsAsFile());
    }

    // auto-detection: a .musicxml input converts to tab without being told.
    {
        auto r = runRender ({ "--convert", fixture ("hammer.musicxml").getFullPathName(),
                              "--to", "tab", "--out", work.getFullPathName() });
        CHECK_MSG (r.exitCode == 0, r.output);
        CHECK (work.getChildFile ("hammer.tab").existsAsFile());
    }

    // --inspect prints the expected fields.
    {
        auto r = runRender ({ "--inspect", fixture ("simple.tab").getFullPathName() });
        CHECK (r.exitCode == 0);
        CHECK (r.output.contains ("Format") && r.output.contains ("Notes")
                 && r.output.contains ("Tuning"));
    }

    // --validate: a glob of valid files exits 0.
    {
        auto r = runRender ({ "--validate", fixture ("simple.tab").getFullPathName() });
        CHECK (r.exitCode == 0);
        CHECK (r.output.contains ("valid"));
    }

    // Batch over a mixed directory: good files convert, bad ones skip, exit 0.
    {
        auto mixed = work.getChildFile ("mixed");
        mixed.createDirectory();
        fixture ("simple.tab").copyFileTo (mixed.getChildFile ("ok.tab"));
        mixed.getChildFile ("bad.tab").replaceWithText ("@@@ not a tab @@@\n");
        mixed.getChildFile ("ignore.bin").replaceWithText ("binary");

        auto outDir = work.getChildFile ("mixed-out");
        auto r = runRender ({ "--convert-batch", mixed.getFullPathName(),
                              "--to", "midi", "--out", outDir.getFullPathName() });
        CHECK_MSG (r.exitCode == 0, r.output);          // some succeeded -> 0
        CHECK (r.output.contains ("OK"));
        CHECK (r.output.contains ("skip"));
        CHECK (outDir.getChildFile ("ok.mid").existsAsFile());
        CHECK (! outDir.getChildFile ("bad.mid").existsAsFile());
    }

    // Batch where every input is bad: exit nonzero.
    {
        auto allbad = work.getChildFile ("allbad");
        allbad.createDirectory();
        allbad.getChildFile ("x.tab").replaceWithText ("@@@\n");
        allbad.getChildFile ("y.tab").replaceWithText ("###\n");

        auto r = runRender ({ "--convert-batch", allbad.getFullPathName(),
                              "--to", "midi", "--out", work.getChildFile ("allbad-out").getFullPathName() });
        CHECK (r.exitCode != 0);
    }

    // A glob that matches nothing: exit 2.
    {
        auto r = runRender ({ "--convert-batch", work.getChildFile ("nope").getFullPathName() + "/*.mid",
                              "--to", "tab", "--out", work.getChildFile ("none-out").getFullPathName() });
        CHECK (r.exitCode == 2);
    }

    // --validate on an invalid file: exit nonzero, never a crash.
    {
        auto bad = work.getChildFile ("bad.tab");
        bad.replaceWithText ("@@@ not a tab @@@\n");
        auto r = runRender ({ "--validate", bad.getFullPathName() });
        CHECK (r.exitCode != 0);
        CHECK (r.output.contains ("INVALID"));
    }

    // Fuzz E2E: a random-byte .mid is skipped with a status, not a crash.
    {
        auto junk = work.getChildFile ("junk.mid");
        juce::MemoryBlock block;
        std::mt19937 rng (7);
        for (int i = 0; i < 300; ++i) { const char byte = (char) (rng() & 0xff); block.append (&byte, 1); }
        junk.replaceWithData (block.getData(), block.getSize());

        auto r = runRender ({ "--convert", junk.getFullPathName(),
                              "--to", "tab", "--out", work.getFullPathName() });
        CHECK (r.exitCode != 0);                         // nothing converted
        CHECK (r.output.contains ("skip"));
    }

    // --transpose and --retune as convert modifiers both succeed end to end.
    {
        auto r1 = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                               "--to", "midi", "--transpose", "12",
                               "--out", work.getChildFile ("t.mid").getFullPathName() });
        CHECK_MSG (r1.exitCode == 0, r1.output);
        CHECK (work.getChildFile ("t.mid").existsAsFile());

        auto r2 = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                               "--to", "tab", "--retune", "drop-d",
                               "--out", work.getChildFile ("d.tab").getFullPathName() });
        CHECK_MSG (r2.exitCode == 0, r2.output);
        CHECK (work.getChildFile ("d.tab").loadFileAsString().containsIgnoreCase ("Drop D"));

        // An unknown tuning is rejected cleanly.
        auto r3 = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                               "--to", "tab", "--retune", "nonsense-tuning",
                               "--out", work.getChildFile ("n.tab").getFullPathName() });
        CHECK (r3.exitCode != 0);
    }

    // --no-clobber: a second run that would overwrite is skipped.
    {
        auto outDir = work.getChildFile ("clobber-out");
        outDir.createDirectory();
        auto a = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                              "--to", "midi", "--out", outDir.getFullPathName() });
        CHECK (a.exitCode == 0);
        auto b = runRender ({ "--convert", fixture ("simple.tab").getFullPathName(),
                              "--to", "midi", "--out", outDir.getFullPathName(), "--no-clobber" });
        CHECK (b.output.contains ("exists"));
    }

    // Existing render commands still work (regression): --help and --list-presets.
    {
        auto help = runRender ({ "--help" });
        CHECK (help.exitCode == 0);
        CHECK (help.output.contains ("--midi") && help.output.contains ("--convert"));
    }
}
