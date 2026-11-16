#include "TestFramework.h"
#include "../Practice/Looper.h"
#include <atomic>
#include <thread>

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (SessionRecorderConcurrency, resetStartsANewAudioAndMidiTimeline)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (48000.0, 1.0));
    recorder.setEnabled (true);
    juce::AudioBuffer<float> audio (2, 512);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 125);
    recorder.captureMidi (midi, 512);
    recorder.processBlock (audio, 512);
    recorder.reset();
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (recorder.getNumMidiEvents() == 0);
    CHECK (recorder.isEnabled());
    recorder.captureMidi (midi, 512);
    recorder.processBlock (audio, 512);
    CHECK (recorder.getRecordedSamples() == 512);
    CHECK (recorder.getNumMidiEvents() == 1);
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("luthier-recorder-reset", "", false);
    CHECK (recorder.saveLastTake (directory));
    const auto files = directory.findChildFiles (juce::File::findFiles, false, "*.mid");
    CHECK (files.size() == 1);
    if (files.size() == 1)
    {
        juce::FileInputStream stream (files[0]);
        juce::MidiFile file;
        CHECK (file.readFrom (stream));
        CHECK (file.getNumTracks() == 1);
        if (file.getNumTracks() == 1)
        {
            const auto* track = file.getTrack (0);
            CHECK (track->getNumEvents() >= 1);
            if (track->getNumEvents() >= 1)
                CHECK (track->getEventPointer (0)->message.getTimeStamp() == 5.0);
        }
    }
    CHECK (directory.deleteRecursively());
}

LUTHIER_TEST (SessionRecorderConcurrency, resizingAndResettingWhileCapturingRemainsUsable)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (8000.0, 1.0));
    recorder.setEnabled (true);
    juce::AudioBuffer<float> audio (2, 64);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 32);
    std::atomic<bool> run { true };
    std::atomic<unsigned> blocks { 0 };
    std::thread producer ([&]
    {
        while (run.load())
        {
            if ((blocks.load() & 1u) == 0)
                recorder.captureBlock (audio, midi, 64);
            else
            {
                recorder.captureMidi (midi, 64);
                recorder.processBlock (audio, 64);
            }
            blocks.fetch_add (1);
        }
    });
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000.0;
    while (blocks.load() == 0 && juce::Time::getMillisecondCounterHiRes() < deadline)
        std::this_thread::yield();
    for (int i = 0; i < 100; ++i)
    {
        recorder.reset();
        if (i % 10 == 0)
            CHECK (recorder.prepare (i % 20 == 0 ? 8000.0 : 16000.0, 1.0));
        recorder.drainMidi();
    }
    run.store (false);
    producer.join();
    CHECK (blocks.load() > 0);
    recorder.reset();
    recorder.captureMidi (midi, 64);
    recorder.processBlock (audio, 64);
    CHECK (recorder.getRecordedSamples() == 64);
    CHECK (recorder.getNumMidiEvents() == 2);
}

LUTHIER_TEST (SessionRecorderConcurrency, savingDuringCaptureSnapshotsCoherentStereo)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (8000.0, 1.0));
    recorder.setEnabled (true);
    recorder.setRecordMidi (false);
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    std::atomic<bool> run { true };
    std::atomic<unsigned> blocks { 0 };
    std::thread producer ([&]
    {
        while (run.load())
        {
            const float value = (blocks.load() & 1u) == 0 ? 0.25f : 0.5f;
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                audio.setSample (0, i, value);
                audio.setSample (1, i, -value);
            }
            recorder.captureBlock (audio, midi, audio.getNumSamples());
            blocks.fetch_add (1);
        }
    });
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000.0;
    while (blocks.load() < 4 && juce::Time::getMillisecondCounterHiRes() < deadline)
        std::this_thread::yield();
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("luthier-recorder-snapshot", "", false);
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const auto take = directory.getChildFile (juce::String (attempt));
        CHECK (recorder.saveLastTake (take, 0.02));
        const auto files = take.findChildFiles (juce::File::findFiles, false, "*.wav");
        CHECK (files.size() == 1);
        if (files.size() != 1)
            continue;
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (files[0]));
        CHECK (reader != nullptr);
        if (reader == nullptr)
            continue;
        CHECK (reader->sampleRate == 8000.0);
        CHECK (reader->lengthInSamples == 160);
        juce::AudioBuffer<float> saved (2, 160);
        CHECK (reader->read (&saved, 0, 160, 0, true, true));
        bool coherent = true;
        for (int i = 0; i < 160; ++i)
            coherent = coherent && std::abs (saved.getSample (0, i) + saved.getSample (1, i)) < 1.0e-6f;
        CHECK (coherent);
    }
    run.store (false);
    producer.join();
    CHECK (blocks.load() >= 4);
    CHECK (directory.deleteRecursively());
}

LUTHIER_TEST (SessionRecorderConcurrency, combinedCapturePreservesRateAndMidiOnlyClock)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (44100.0, 1.0));
    recorder.setEnabled (true);
    juce::AudioBuffer<float> audio (2, 512);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    recorder.captureBlock (audio, midi, 512);
    recorder.prepareForHost (44100.0);
    CHECK (recorder.getRecordedSamples() == 512);
    CHECK (recorder.getNumMidiEvents() == 1);
    recorder.prepareForHost (48000.0);
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (recorder.getNumMidiEvents() == 0);
    CHECK (recorder.getSampleRate() == 48000.0);
    CHECK (std::abs (recorder.getCapacityMinutes() - 1.0) < 1.0e-6);
    recorder.setRecordAudio (false);
    recorder.captureBlock (audio, midi, 512);
    recorder.captureBlock (audio, midi, 512);
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (recorder.getNumMidiEvents() == 2);
    recorder.reset();
    CHECK (recorder.getNumMidiEvents() == 0);
}
