/*  TODO 11 remainder (MODEL-GAPS): the session recorder's record-audio /
    record-MIDI switches and auto-save, its MIDI captured without allocating,
    the drag from its own Save button (midi-export 4.2, TODO 10), the looper's
    default length, the trainers' note range and question count, and the tab
    reader's recent list, all through the tools practice-tools 11.2 sets up. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Practice/Looper.h"
#include "../Practice/Trainers.h"
#include "../Practice/PracticeRoutineSetup.h"
#include "../UI/PracticePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    juce::File scratch (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-practice-gaps").getChildFile (name);
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }

    /** A second of a tone and a note-on / note-off into the recorder. */
    void feed (SessionRecorder& recorder)
    {
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < (int) (kSr / kBlock); ++b)
        {
            for (int i = 0; i < kBlock; ++i)
                block.setSample (0, i, 0.25f * std::sin ((float) (b * kBlock + i) * 0.05f)), block.setSample (1, i, block.getSample (0, i));

            juce::MidiBuffer midi;

            if (b == 3)  midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 17);
            if (b == 90) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 5);

            recorder.captureMidi (midi, kBlock);
            recorder.processBlock (block, kBlock);
        }
    }
}

//==============================================================================
LUTHIER_TEST (PracticeGaps, theSessionRecorderRecordsWhatItIsToldTo)
{
    struct Case { bool audio, midi; bool expectWav, expectMid; };

    for (const auto& c : { Case { true, true, true, true }, Case { true, false, true, false }, Case { false, true, false, true } })
    {
        SessionRecorderSetup setup;
        setup.ringMinutes = 1.0;
        setup.recordAudio = c.audio;
        setup.recordMidi = c.midi;

        SessionRecorder recorder;
        CHECK (setup.applyTo (recorder, kSr));
        CHECK (recorder.isRecordingAudio() == c.audio && recorder.isRecordingMidi() == c.midi);

        recorder.setEnabled (true);
        feed (recorder);

        CHECK_MSG ((recorder.getRecordedSamples() > 0) == c.audio,
                   "audio " + juce::String ((int) c.audio) + ": " + juce::String (recorder.getRecordedSamples()) + " samples held");
        CHECK ((recorder.getNumMidiEvents() == 2) == c.midi);

        const auto dir = scratch ("record");
        CHECK (recorder.saveLastTake (dir));

        bool wav = false, mid = false;

        for (const auto& f : recorder.getLastSavedFiles())
        {
            wav = wav || (f.hasFileExtension ("wav") && f.existsAsFile());
            mid = mid || (f.hasFileExtension ("mid") && f.existsAsFile());
        }

        CHECK_MSG (wav == c.expectWav && mid == c.expectMid,
                   juce::String ("audio ") + (c.audio ? "on" : "off") + ", MIDI " + (c.midi ? "on" : "off")
                     + ": wrote wav " + (wav ? "yes" : "no") + ", mid " + (mid ? "yes" : "no"));
    }
}

LUTHIER_TEST (PracticeGaps, theSessionRecorderTakesMidiWithoutAllocating)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (kSr, 1.0));
    recorder.setEnabled (true);

    juce::MidiBuffer midi;
    midi.ensureSize (4096);

    for (int i = 0; i < 64; ++i)
        midi.addEvent (juce::MidiMessage::noteOn (1, 40 + i % 20, (juce::uint8) 90), i * 3);

    // The FIFO takes them; the sequence is filled on the message thread's drain.
    for (int b = 0; b < 100; ++b)
        recorder.captureMidi (midi, kBlock);

    CHECK (recorder.getNumMidiEvents() == 6400);
}

LUTHIER_TEST (PracticeGaps, stoppingTheRecorderSavesWhenAutoSaveIsOn)
{
    for (bool autoSave : { true, false })
    {
        SessionRecorderSetup setup;
        setup.ringMinutes = 1.0;
        setup.autoSaveOnStop = autoSave;

        SessionRecorder recorder;
        CHECK (setup.applyTo (recorder, kSr));
        recorder.setEnabled (true);
        feed (recorder);

        const auto dir = scratch ("autosave");
        const bool saved = recorder.stop (dir);

        CHECK (! recorder.isEnabled());
        CHECK (saved == autoSave);
        CHECK ((juce::RangedDirectoryIterator (dir, false, "session-*") != juce::RangedDirectoryIterator()) == autoSave);
    }
}

/*  midi-export 4.2 / TODO 10: the session recorder's own Save button is a drag source. */
LUTHIER_TEST (PracticeGaps, theSaveButtonDragsTheSavedTakeOut)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.prepare (kSr, 1.0));
    recorder.setEnabled (true);
    feed (recorder);

    SessionTab tab (processor);

    // Nothing saved yet: a drag saves first, then hands out the MIDI first.
    const auto files = tab.getSaveButton().filesToDrag();
    CHECK (files.size() == 2);

    if (files.size() == 2)
    {
        CHECK (juce::File (files[0]).hasFileExtension ("mid"));
        CHECK (juce::File (files[1]).hasFileExtension ("wav"));
        CHECK (juce::File (files[0]).existsAsFile() && juce::File (files[1]).existsAsFile());
    }

    for (const auto& f : files)
        juce::File (f).deleteFile();
}

//==============================================================================
/*  practice-tools 11.2: "default length". The first recording closes itself on the sample. */
LUTHIER_TEST (PracticeGaps, theLooperClosesItsFirstLoopAtTheDefaultLength)
{
    Looper looper;
    looper.prepare (kSr, 10.0);

    PracticeDefaults defaults;
    defaults.loopLengthSeconds = 2.0;

    PracticeTargets targets;
    targets.looper = &looper;
    defaults.applyTo (targets);

    CHECK (looper.getDefaultLengthSamples() == (int) (2.0 * kSr));

    looper.press();   // record
    juce::AudioBuffer<float> block (2, kBlock);

    for (int b = 0; b < (int) (3.0 * kSr / kBlock); ++b)
    {
        block.clear();
        looper.processBlock (block, kBlock);
    }

    CHECK (looper.getState() == Looper::State::playing);
    CHECK (looper.getLoopLengthSamples() == (int) (2.0 * kSr));

    // 0 leaves the close to the player.
    Looper free;
    free.prepare (kSr, 10.0);
    defaults.loopLengthSeconds = 0.0;
    targets.looper = &free;
    defaults.applyTo (targets);
    free.press();

    for (int b = 0; b < (int) (3.0 * kSr / kBlock); ++b)
    {
        block.clear();
        free.processBlock (block, kBlock);
    }

    CHECK (free.getState() == Looper::State::recordingFirst);
}

/*  practice-tools 11.2: the trainers' "note range" and "question count". */
LUTHIER_TEST (PracticeGaps, theTrainersKeepToTheirRangeAndSessionLength)
{
    PracticeDefaults defaults;
    defaults.rangeLowNote = 48;
    defaults.rangeHighNote = 72;
    defaults.questionCount = 5;

    ScaleTrainer scale;
    EarTrainer ear;
    PracticeTargets targets;
    targets.scaleTrainer = &scale;
    targets.earTrainer = &ear;
    defaults.applyTo (targets);

    CHECK (scale.getLowNote() == 48 && scale.getHighNote() == 72 && scale.getQuestionCount() == 5);
    CHECK (ear.getLowNote() == 48 && ear.getHighNote() == 72 && ear.getQuestionCount() == 5);

    // The scale trainer: a right pitch class outside the range is not an answer.
    juce::Random random (7);
    scale.setScale (ScaleType::ionian);
    scale.setMode (ScaleTrainer::Mode::quiz);
    scale.nextQuestion (random);
    const int pc = scale.getExpectedPitchClass();
    CHECK (pc >= 0);
    CHECK (! scale.answer (24 + pc));             // right note, two octaves too low
    CHECK (scale.getScore() == 0);
    CHECK (scale.answer (60 + pc));
    CHECK (scale.getScore() == 1);

    for (int i = 0; i < 10; ++i)
        scale.nextQuestion (random);

    CHECK (scale.isSessionComplete());
    CHECK (scale.getAsked() == 5);
    CHECK (scale.getExpectedPitchClass() < 0);

    // The ear trainer: every note it plays is in the range, and it stops at five.
    int notes[EarTrainer::kMaxNotesInQuestion];
    double beats[EarTrainer::kMaxNotesInQuestion];
    int questions = 0;

    for (int i = 0; i < 12; ++i)
    {
        const int n = ear.nextQuestion (random, notes, beats, EarTrainer::kMaxNotesInQuestion);

        if (n > 0)
            ++questions;

        for (int k = 0; k < n; ++k)
            CHECK_MSG (notes[k] >= 48 && notes[k] <= 72, "note " + juce::String (notes[k]) + " outside 48-72");
    }

    CHECK (questions == 5);
    CHECK (ear.isSessionComplete());
}

//==============================================================================
/*  TODO 11: "a test through the tab reader's recent list". Opening a tab in the
    drawer's TAB reader puts it at the top of the PRACTICE tab's recent list. */
LUTHIER_TEST (PracticeGaps, openingATabInTheReaderListsItAsRecent)
{
    LuthierAudioProcessor processor;
    const auto dir = scratch ("tabs");

    const auto tab = dir.getChildFile ("Riff.txt");
    CHECK (tab.replaceWithText ("e|-----0-----|\nB|---1---1---|\nG|-2-------2-|\nD|-----------|\nA|-----------|\nE|-----------|\n"));

    const auto libraryFile = dir.getChildFile ("library.json");

    TabReaderTab reader (processor);
    CHECK (reader.openTab (tab, libraryFile));
    CHECK (reader.getScore().getTotalNoteCount() > 0);

    PracticeLibrary library;
    library.load (libraryFile);
    CHECK (library.getRecentTabs().size() == 1);
    CHECK (library.getRecentTabs().size() == 1 && library.getRecentTabs().getFirst() == tab);

    // A second file goes on top.
    const auto other = dir.getChildFile ("Other.txt");
    CHECK (other.replaceWithText ("e|-----3-----|\nB|---1---1---|\nG|-0-------0-|\nD|-----------|\nA|-----------|\nE|-----------|\n"));
    CHECK (reader.openTab (other, libraryFile));

    library = PracticeLibrary();
    library.load (libraryFile);
    CHECK (library.getRecentTabs().size() == 2 && library.getRecentTabs().getFirst() == other);

    // A file the reader cannot read is not listed.
    const auto broken = dir.getChildFile ("Broken.musicxml");
    CHECK (broken.replaceWithText ("not xml"));
    CHECK (! reader.openTab (broken, libraryFile));

    library = PracticeLibrary();
    library.load (libraryFile);
    CHECK (library.getRecentTabs().size() == 2);

    dir.deleteRecursively();
}
