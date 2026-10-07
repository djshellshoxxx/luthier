// Universal tab player, panel side: tap tempo changes the playback rate in
// place, the view follows the music (highlighted column, chord sounding),
// tuning / key overrides, MIDI export round trip and the experimental
// MIDI-as-tab import label.
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../UI/PracticePanel.h"
#include "../Notation/NotationExport.h"

#include <cmath>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

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

    juce::File scratchDir()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-tabplayer");
        dir.createDirectory();
        return dir;
    }

    /** Runs the processor for `blocks` blocks with no MIDI, so the player's own clock advances. */
    void run (LuthierAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
        }
    }

    struct Flat { double beat; int stringIndex, fret; };

    std::vector<Flat> flatten (const PerformanceScore& score)
    {
        std::vector<Flat> out;
        double barStart = 0.0;
        for (const auto& measure : score.getTrack (0).measures)
        {
            for (const auto& voice : measure.voices)
                for (const auto& n : voice.notes)
                    out.push_back ({ barStart + n.startBeat, n.stringIndex, n.fret });
            barStart += measure.timeSignatureNumerator * 4.0 / juce::jmax (1, measure.timeSignatureDenominator);
        }
        std::stable_sort (out.begin(), out.end(), [] (const Flat& a, const Flat& b)
        {
            return a.beat < b.beat - 1.0e-6 || (std::abs (a.beat - b.beat) < 1.0e-6 && a.stringIndex < b.stringIndex);
        });
        return out;
    }
}

//==============================================================================
LUTHIER_TEST (TabPlayer, viewFollowsTheMusicAndTapTempoChangesTheRateInPlace)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    TabReaderTab reader (processor);

    CHECK_MSG (reader.openTab (corpus ("chords_over_staff_strum.txt"), scratchDir().getChildFile ("library.json")),
               reader.getStatusText());
    CHECK_MSG (reader.getInfoText().contains ("Key"), reader.getInfoText());
    CHECK_NEAR (reader.getPlaybackBpm(), 0.0, 1.0e-9);                  // the tab's own tempo until tapped
    CHECK (reader.getHighlightedColumn() < 0);

    CHECK (reader.startPlayback());
    auto& player = processor.getEngine().getRiffPlayer();
    run (processor, 2);                                                 // the audio thread publishes the state
    CHECK (reader.isPlaying());
    CHECK_NEAR (player.getAbsoluteBpm(), 100.0, 1.0e-9);                // "Tempo: 100"

    // Half a beat in: the first column is sounding, the chord above it is shown.
    run (processor, 40);
    reader.refresh();
    const int firstColumn = reader.getHighlightedColumn();
    const double firstBeat = reader.getPlayheadBeat();
    CHECK_MSG (firstColumn >= 0, juce::String (firstColumn));
    CHECK (firstBeat >= 0.0);
    CHECK_MSG (reader.getNowChordText() == "Am", reader.getNowChordText());
    CHECK (reader.getTabViewText().startsWith (juce::String::repeatedString (" ", juce::jmax (0, firstColumn)) + "v"));
    CHECK (reader.getTabViewText().contains ("Am"));                      // the chord row is drawn

    // Two beats later (still inside the one looping bar) the highlight has moved
    // right, and the chord has changed to G.
    run (processor, 260);
    reader.refresh();
    const int laterColumn = reader.getHighlightedColumn();
    const double laterBeat = reader.getPlayheadBeat();
    CHECK_MSG (laterBeat > firstBeat, juce::String (laterBeat) + " <= " + juce::String (firstBeat));
    CHECK_MSG (laterColumn > firstColumn, juce::String (laterColumn) + " <= " + juce::String (firstColumn));
    CHECK_MSG (reader.getNowChordText() == "G", reader.getNowChordText());

    // Tap tempo at 150 bpm (0.4 s apart): the rate changes while playing, nothing restarts.
    const double beatBeforeTap = player.getBeatPosition();
    bool changed = false;
    for (int i = 0; i < 4; ++i)
        changed = reader.tap (10.0 + 0.4 * i) || changed;
    CHECK (changed);
    CHECK_NEAR (reader.getPlaybackBpm(), 150.0, 0.5);
    CHECK_NEAR (player.getAbsoluteBpm(), 150.0, 0.5);
    CHECK (reader.isPlaying());
    run (processor, 10);
    CHECK (player.getBeatPosition() >= beatBeforeTap - 1.0e-6 || player.getLoopCount() > 0);

    // Faster: the same number of blocks now covers more beats than at 100 bpm.
    const double a = player.getBeatPosition();
    run (processor, 200);
    const double fast = player.getBeatPosition() - a;
    reader.setPlaybackBpm (60.0);
    CHECK_NEAR (player.getAbsoluteBpm(), 60.0, 1.0e-9);
    const double b = player.getBeatPosition();
    run (processor, 200);
    const double slow = player.getBeatPosition() - b;
    if (fast > 0.0 && slow > 0.0)
        CHECK_MSG (fast > slow * 1.8, juce::String (fast) + " vs " + juce::String (slow));

    reader.stopPlayback();
    run (processor, 4);
    CHECK (! reader.isPlaying());
    reader.refresh();
    CHECK (reader.getHighlightedColumn() < 0);
    CHECK (reader.getNowChordText().isEmpty());
    CHECK (reader.getTabViewText().contains ("e |") || reader.getTabViewText().contains ("E |"));
}

LUTHIER_TEST (TabPlayer, tuningAndKeyOverridesKeepStringsAndFrets)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);
    CHECK (reader.openTab (corpus ("steps_down_dropd.txt"), scratchDir().getChildFile ("library.json")));
    CHECK_MSG (reader.getInfoText().contains ("Capo 3") && reader.getInfoText().contains ("Key Bm"), reader.getInfoText());

    const auto written = flatten (reader.getScore());
    CHECK (reader.setTuningOverride ("Drop D"));
    CHECK (reader.getScore().getTrack (0).tuning[5] == 38);
    CHECK (reader.getScore().getMeta().tuningName == "Drop D");
    const auto overridden = flatten (reader.getScore());
    CHECK (overridden.size() == written.size());
    for (size_t i = 0; i < juce::jmin (written.size(), overridden.size()); ++i)
        CHECK (overridden[i].stringIndex == written[i].stringIndex && overridden[i].fret == written[i].fret);
    for (const auto& m : reader.getScore().getTrack (0).measures)
        for (const auto& v : m.voices)
            for (const auto& n : v.notes)
                if (n.stringIndex == 5)
                    CHECK (n.midiNote == 38 + 3 + n.fret);
    CHECK (reader.getTuningBox().getText() == "Drop D");
    CHECK (! reader.setTuningOverride ("not a tuning"));

    CHECK (reader.setTuningOverride ("As written"));
    CHECK (reader.getScore().getTrack (0).tuning[5] == 35);

    reader.setKeyOverride ("Em");
    CHECK (reader.getScore().getMeta().key == "Em");
    CHECK (reader.getKeyBox().getText() == "Em");
    CHECK_MSG (reader.getInfoText().contains ("Key Em"), reader.getInfoText());
    reader.setKeyOverride ("As read");
    CHECK (reader.getScore().getMeta().key == "Bm");

    // Playback compiles at the chosen tuning.
    processor.prepareToPlay (kSr, kBlock);
    CHECK (reader.setTuningOverride ("Eb Standard"));
    CHECK (reader.startPlayback());
    reader.stopPlayback();
}

LUTHIER_TEST (TabPlayer, midiExportRoundTripsNotesAndTempo)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);
    CHECK (reader.openTab (corpus ("chords_over_staff_strum.txt"), scratchDir().getChildFile ("library.json")));

    const auto midi = scratchDir().getChildFile ("roundtrip.mid");
    CHECK (reader.exportMidi (midi));
    CHECK_MSG (reader.getStatusText().startsWith ("Exported MIDI"), reader.getStatusText());

    NotationImporter importer;
    PerformanceScore back;
    CHECK_MSG (importer.readMidi (midi, back), importer.getLastError());
    CHECK_NEAR (back.getMeta().tempoBpm, 100.0, 0.5);

    const auto a = flatten (reader.getScore()), b = flatten (back);
    CHECK_MSG (a.size() == b.size(), juce::String ((int) a.size()) + " vs " + juce::String ((int) b.size()));
    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
    {
        CHECK_MSG (a[i].stringIndex == b[i].stringIndex && a[i].fret == b[i].fret,
                   "note " + juce::String ((int) i));
        CHECK_NEAR (b[i].beat, a[i].beat, 0.02);
    }
    midi.deleteFile();

    // Experimental: the same file opens back as tab, labelled as guessed.
    CHECK (reader.exportMidi (midi));
    CHECK (reader.importMidiAsTab (midi, scratchDir().getChildFile ("library.json")));
    CHECK_MSG (reader.getStatusText().contains ("Experimental MIDI-to-tab"), reader.getStatusText());
    CHECK (reader.getScore().getTotalNoteCount() == (int) a.size());
    midi.deleteFile();

    // Nothing loaded: a clear message, no file.
    TabReaderTab empty (processor);
    const auto none = scratchDir().getChildFile ("none.mid");
    CHECK (! empty.exportMidi (none));
    CHECK (! none.existsAsFile());
}
