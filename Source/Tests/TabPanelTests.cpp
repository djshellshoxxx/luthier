// tab-import-export 9: the TAB reader panel. A partial page reports what was
// skipped, a MIDI file opens as tab, and what the player just played becomes
// a score that exports in every format.
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../UI/PracticePanel.h"
#include "../Notation/NotationExport.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    juce::File fixture (const char* name)
    {
        return juce::File (__FILE__).getSiblingFile ("Fixtures").getChildFile (name);
    }

    juce::File scratchDir()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-tabpanel");
        dir.createDirectory();
        return dir;
    }

    /** Plays four notes through the whole plugin, then drains the capture. */
    void playPhrase (LuthierAudioProcessor& processor)
    {
        if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
            p->setValueNotifyingHost (0.0f);

        processor.prepareToPlay (kSr, kBlock);

        const int notes[] = { 52, 55, 57, 59 };
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

        for (int b = 0; b < 200; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b % 40 == 0 && b / 40 < 4)
                midi.addEvent (juce::MidiMessage::noteOn (1, notes[b / 40], (juce::uint8) 100), 10);

            if (b % 40 == 30 && b / 40 < 4)
                midi.addEvent (juce::MidiMessage::noteOff (1, notes[b / 40]), 0);

            processor.processBlock (buffer, midi);
        }

        processor.drainPerformanceCapture();
    }
}

//==============================================================================
LUTHIER_TEST (TabPanel, partialTabOpensWithASpecificStatus)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);

    const auto library = scratchDir().getChildFile ("library.json");
    CHECK (reader.openTab (fixture ("messy.tab"), library));
    CHECK (reader.getScore().getTotalNoteCount() > 10);

    const auto status = reader.getStatusText();
    CHECK_MSG (status.startsWith ("Loaded"), status);
    CHECK_MSG (status.contains ("bars") && status.contains ("skipped"), status);
    CHECK_MSG (status.contains ("messy.tab"), status);
}

LUTHIER_TEST (TabPanel, midiFileOpensAsTabAndPlays)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);

    // Make a MIDI file from a tab with the exporter, then open it as a file.
    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab ("e|--0--|\nB|--1--|\nG|--0--|\nD|--2--|\nA|--3--|\nE|-----|\n", score));

    const auto midi = scratchDir().getChildFile ("chord.mid");
    NotationExporter exporter;
    CHECK (exporter.writeMidi (score, midi));

    CHECK (NotationImporter::canRead (midi));
    CHECK_MSG (reader.openTab (midi, scratchDir().getChildFile ("library.json")), reader.getStatusText());
    CHECK (reader.getScore().getTotalNoteCount() == 5);
    CHECK_MSG (reader.getStatusText().startsWith ("Loaded"), reader.getStatusText());

    // The reader's title comes from the file and the score can be compiled to play.
    CHECK (reader.getScoreTitle() == "chord");
    CHECK (! Riff::fromScore (reader.getScore()).notes.empty());
    midi.deleteFile();
}

LUTHIER_TEST (TabPanel, longTabScrollsPastTheFirstWindow)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);

    // Twelve bars, one distinct fret each: more than the 8-bar window can show.
    juce::String e = "e|", b = "B|", g = "G|", d = "D|", a = "A|", low = "E|";

    for (int bar = 0; bar < 12; ++bar)
    {
        e   += "--" + juce::String (bar) + "--|";
        b   += "-----|";
        g   += "-----|";
        d   += "-----|";
        a   += "-----|";
        low += "-----|";
    }

    NotationImporter importer;
    PerformanceScore score;
    CHECK (importer.readAsciiTab (e + "\n" + b + "\n" + g + "\n" + d + "\n" + a + "\n" + low + "\n", score));
    reader.openScore (score, "long");

    auto& from = reader.getFromBarSlider();
    const int bars = (int) score.getTrack (0).measures.size();
    CHECK (bars > 8);
    CHECK_MSG (from.getMaximum() == (double) bars, juce::String (from.getMaximum()));
    CHECK (from.isEnabled());

    const auto first = reader.getTabViewText();
    from.setValue (from.getMaximum(), juce::sendNotificationSync);
    CHECK (reader.getTabViewText() != first);
    CHECK (reader.getTabViewText().isNotEmpty());

    // A shorter score pulls the scroller back into range.
    PerformanceScore tiny;
    CHECK (importer.readAsciiTab ("e|--0--|\nB|-----|\nG|-----|\nD|-----|\nA|-----|\nE|-----|\n", tiny));
    reader.openScore (tiny, "tiny");
    CHECK (from.getValue() == 1.0);
    CHECK (! from.isEnabled());
}

LUTHIER_TEST (TabPanel, livePerformanceBecomesAnExportableScore)
{
    LuthierAudioProcessor processor;
    TabReaderTab reader (processor);

    // Nothing played yet: says so, opens nothing.
    CHECK (! reader.openLivePerformance());
    CHECK (reader.getStatusText().contains ("Nothing"));

    playPhrase (processor);

    CHECK_MSG (reader.openLivePerformance(), reader.getStatusText());
    CHECK_MSG (reader.getScore().getTotalNoteCount() == 4, juce::String (reader.getScore().getTotalNoteCount()));
    CHECK (reader.getScoreTitle() == "Live performance");

    // Every string/fret sounds its pitch: the take is a guitar score, not a pitch list.
    const auto& track = reader.getScore().getTrack (0);
    for (const auto& measure : track.measures)
        for (const auto& voice : measure.voices)
            for (const auto& note : voice.notes)
                CHECK (note.stringIndex >= 0 && note.stringIndex < track.numStrings
                        && track.tuning[(size_t) note.stringIndex] + track.capoFret + note.fret == note.midiNote);

    // And it writes in every format the box lists.
    NotationExporter exporter;
    for (int f = 0; f < (int) NotationFormat::numFormats; ++f)
    {
        const auto format = (NotationFormat) f;
        const auto out = scratchDir().getChildFile (juce::String ("live") + getNotationFormatExtension (format));
        CHECK_MSG (exporter.write (reader.getScore(), format, out), exporter.getLastError());
        CHECK (out.existsAsFile() && out.getSize() > 0);
        out.deleteFile();
    }

    // The MIDI it wrote reads back as the same four pitches.
    const auto midi = scratchDir().getChildFile ("live.mid");
    CHECK (exporter.writeMidi (reader.getScore(), midi));
    PerformanceScore back;
    NotationImporter importer;
    CHECK (importer.readMidi (midi, back));
    CHECK (back.getTotalNoteCount() == 4);
    midi.deleteFile();
}
