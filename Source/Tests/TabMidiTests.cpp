// tab-import-export 8: MIDI <-> tab. Round trips through the exporter's MIDI,
// the fingering guess on a generic file, range clamping, and technique
// coverage in the written file.
#include "TestFramework.h"
#include "../Notation/NotationExport.h"
#include "../Notation/TabFingering.h"
#include "../Riffs/Riff.h"

#include <set>
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

    std::multiset<int> pitches (const PerformanceScore& score)
    {
        std::multiset<int> p;
        for (const auto* n : allNotes (score)) p.insert (n->midiNote);
        return p;
    }

    juce::File tempFile (const char* name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    }

    // A generic format-1 file: tempo track, then one channel-1 track of notes.
    juce::MemoryBlock genericMidi (const std::vector<std::pair<int, int>>& notesAndBeats, int velocity = 100)
    {
        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);

        juce::MidiMessageSequence meta;
        meta.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
        meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        file.addTrack (meta);

        juce::MidiMessageSequence track;
        for (const auto& [note, beat] : notesAndBeats)
        {
            track.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), beat * 480.0);
            track.addEvent (juce::MidiMessage::noteOff (1, note), beat * 480.0 + 400.0);
        }
        track.updateMatchedPairs();
        file.addTrack (track);

        juce::MemoryOutputStream out;
        file.writeTo (out, 1);
        return out.getMemoryBlock();
    }

    const char* kSimpleTab =
        "e|----------------|\n"
        "B|----------------|\n"
        "G|----------------|\n"
        "D|--------7-------|\n"
        "A|----5-----------|\n"
        "E|3---------------|\n";
}

//==============================================================================
LUTHIER_TEST (TabMidi, scoreToMidiToScoreKeepsPitchesAndTiming)
{
    NotationImporter importer;
    PerformanceScore first;
    CHECK (importer.readAsciiTab (kSimpleTab, first));

    const auto file = tempFile ("luthier-tabmidi-roundtrip.mid");
    NotationExporter exporter;
    CHECK_MSG (exporter.writeMidi (first, file), exporter.getLastError());

    PerformanceScore second;
    CHECK_MSG (importer.readMidi (file, second), importer.getLastError());
    file.deleteFile();

    CHECK (pitches (first) == pitches (second));
    CHECK (second.getTotalNoteCount() == 3);

    // The per-string export put each note on its own channel, so the strings
    // survive without any guessing.
    for (const auto* n : allNotes (second))
    {
        const ScoreNote* match = nullptr;
        for (const auto* m : allNotes (first))
            if (m->midiNote == n->midiNote) match = m;
        CHECK (match != nullptr && match->stringIndex == n->stringIndex && match->fret == n->fret);
        CHECK (match != nullptr && std::abs (match->startBeat - n->startBeat) < 0.02);
    }

    // And it renders as tab again.
    CHECK (exporter.renderAsciiTab (second).containsChar ('|'));
}

LUTHIER_TEST (TabMidi, genericMidiIsFingeredSensibly)
{
    // An open-position walk up the strings, then an open C chord.
    const auto bytes = genericMidi ({ { 40, 0 }, { 45, 1 }, { 50, 2 }, { 55, 3 }, { 59, 4 }, { 64, 5 },
                                      { 48, 6 }, { 52, 6 }, { 55, 6 }, { 60, 6 } });

    NotationImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.readMidi (bytes.getData(), bytes.getSize(), score), importer.getLastError());

    const auto notes = allNotes (score);
    CHECK_MSG (notes.size() == 10, juce::String ((int) notes.size()));

    // Beats 0-5 are the scale (measure 0 and the first half of measure 1);
    // the chord is at beat 6 (measure 1, beat 2).
    std::set<int> chordStrings;
    const auto& measures = score.getTrack (0).measures;
    CHECK (measures.size() == 2);

    for (size_t m = 0; m < measures.size(); ++m)
    {
        for (const auto& voice : measures[m].voices)
        {
            for (const auto& n : voice.notes)
            {
                CHECK (n.fret >= 0 && n.fret <= 5);                     // nothing wandered up the neck
                CHECK (n.stringIndex >= 0 && n.stringIndex < 6);
                const int open = score.getTrack (0).tuning[(size_t) n.stringIndex];
                CHECK (open + n.fret == n.midiNote);                    // the fingering sounds the pitch

                const bool chord = m == 1 && n.startBeat > 1.5;
                if (chord)
                    chordStrings.insert (n.stringIndex);
                else
                    CHECK_MSG (n.fret == 0, juce::String (n.midiNote) + " on fret " + juce::String (n.fret));   // the open scale lands on open strings
            }
        }
    }

    // One string per chord note.
    CHECK (chordStrings.size() == 4);

    CHECK (! importer.getLastDiagnostics().warnings.isEmpty());
    CHECK (importer.getLastDiagnostics().summary().startsWith ("Loaded"));

    // Playable.
    const auto riff = Riff::fromScore (score);
    CHECK (riff.notes.size() == 10);
}

LUTHIER_TEST (TabMidi, handPositionFollowsThePhrase)
{
    // A run around fret 7-10 on the top strings should stay there, not jump
    // to open strings and back.
    const auto bytes = genericMidi ({ { 71, 0 }, { 72, 1 }, { 74, 2 }, { 76, 3 }, { 74, 4 }, { 72, 5 } });

    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readMidi (bytes.getData(), bytes.getSize(), score));

    int minFret = 99, maxFret = -1;
    for (const auto* n : allNotes (score))
    {
        minFret = juce::jmin (minFret, n->fret);
        maxFret = juce::jmax (maxFret, n->fret);
    }
    CHECK_MSG (maxFret - minFret <= 5, juce::String (minFret) + ".." + juce::String (maxFret));
}

LUTHIER_TEST (TabMidi, bassRangeAndOutOfRangePitches)
{
    // A low part becomes a four-string bass.
    {
        const auto bytes = genericMidi ({ { 28, 0 }, { 33, 1 }, { 38, 2 }, { 43, 3 } });
        NotationImporter importer;
        PerformanceScore score;
        CHECK (importer.readMidi (bytes.getData(), bytes.getSize(), score));
        CHECK (score.getTrack (0).numStrings == 4);
        for (const auto* n : allNotes (score))
            CHECK (n->fret == 0);
    }

    // Pitches the instrument cannot reach are moved onto it and reported.
    {
        const auto bytes = genericMidi ({ { 20, 0 }, { 60, 1 }, { 110, 2 } });
        NotationImporter importer;
        PerformanceScore score;
        CHECK (importer.readMidi (bytes.getData(), bytes.getSize(), score));
        CHECK (score.getTotalNoteCount() == 3);
        for (const auto* n : allNotes (score))
        {
            CHECK (n->fret >= 0 && n->fret <= 24);
            CHECK (n->midiNote >= 0 && n->midiNote <= 127);
        }
        bool clampedWarning = false;
        for (const auto& w : importer.getLastDiagnostics().warnings)
            if (w.contains ("range")) clampedWarning = true;
        CHECK (clampedWarning);
    }

    // Junk is refused, not crashed on.
    {
        NotationImporter importer;
        PerformanceScore score;
        const char junk[] = "this is not a midi file at all";
        CHECK (! importer.readMidi (junk, sizeof (junk), score));
        CHECK (importer.getLastError().isNotEmpty());
        CHECK (! importer.readMidi (nullptr, 0, score));
    }
}

LUTHIER_TEST (TabMidi, writtenMidiCarriesBendsSlidesAndVibrato)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (
        "e|--------------------|\nB|--------------------|\nG|--7b9---7~~~--5/7---|\n"
        "D|--------------------|\nA|--------------------|\nE|--------------------|\n", score));

    const auto file = tempFile ("luthier-tabmidi-techniques.mid");
    NotationExporter exporter;
    CHECK (exporter.writeMidi (score, file));

    juce::MidiFile midi;
    {
        juce::FileInputStream in (file);
        CHECK (in.openedOk() && midi.readFrom (in));
    }
    file.deleteFile();

    int wheels = 0, legatoCc = 0, noteOns = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        for (const auto* holder : *midi.getTrack (t))
        {
            if (holder->message.isPitchWheel()) ++wheels;
            if (holder->message.isController() && holder->message.getControllerNumber() == 68) ++legatoCc;
            if (holder->message.isNoteOn()) ++noteOns;
        }
    }

    CHECK (noteOns == 4);
    CHECK (legatoCc == 0);   // no hammer/pull in this phrase
    // A bend (2 points), a vibrato (many), a slide (5 + reset): well over a dozen.
    CHECK_MSG (wheels >= 12, juce::String (wheels));
}

LUTHIER_TEST (TabMidi, fingeringLeavesPlausibleScoresAlone)
{
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (kSimpleTab, score));
    CHECK (TabFingering::isPlausible (score));

    // Everything on one string with wrong frets is not plausible.
    PerformanceScore wrong = score;
    for (auto& measure : wrong.getTrack (0).measures)
        for (auto& voice : measure.voices)
            for (auto& note : voice.notes)
                note.stringIndex = 0;
    CHECK (! TabFingering::isPlausible (wrong));

    const auto r = TabFingering::assign (wrong);
    CHECK (r.notesFingered == 3);
    CHECK (r.notesClamped == 0);
    CHECK (TabFingering::isPlausible (wrong));
}
