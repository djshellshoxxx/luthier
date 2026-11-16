/* Host sample-rate transitions must not relabel an existing recorded take. */
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Practice/Looper.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    void captureBlock (SessionRecorder& recorder)
    {
        juce::AudioBuffer<float> audio (2, 512);
        audio.clear();
        for (int i = 0; i < 512; ++i)
            audio.setSample (0, i, (float) i / 1024.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        recorder.captureMidi (midi, 512);
        recorder.processBlock (audio, 512);
    }
}

LUTHIER_TEST (SessionRecorderRate, hostRateChangesDiscardOldAudioAndMidi)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);
    auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.prepare (44100.0, 1.0));
    recorder.setEnabled (true);
    captureBlock (recorder);
    CHECK (recorder.getRecordedSamples() == 512);
    CHECK (recorder.getNumMidiEvents() == 1);

    processor.prepareToPlay (44100.0, 1024);
    CHECK (recorder.getRecordedSamples() == 512);
    CHECK (recorder.getNumMidiEvents() == 1);

    processor.prepareToPlay (96000.0, 512);
    CHECK (recorder.getSampleRate() == 96000.0);
    CHECK (std::abs (recorder.getCapacityMinutes() - 1.0) < 1.0e-6);
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (recorder.getNumMidiEvents() == 0);
    CHECK (recorder.isEnabled());
}

LUTHIER_TEST (SessionRecorderRate, unusedRecorderRemainsUnallocated)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);
    processor.prepareToPlay (48000.0, 512);
    const auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.getSampleRate() == 48000.0);
    CHECK (recorder.getCapacityMinutes() == 0.0);
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (! recorder.isEnabled());
}

LUTHIER_TEST (SessionRecorderRate, exportedWavUsesNewRateAndOnlyNewSamples)
{
    SessionRecorder recorder;
    CHECK (recorder.prepare (44100.0, 1.0));
    recorder.setEnabled (true);
    recorder.setRecordMidi (false);
    captureBlock (recorder);
    recorder.prepareForHost (48000.0);
    captureBlock (recorder);

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile ("luthier-recorder-rate", "", false);
    CHECK (recorder.saveLastTake (dir));
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
    CHECK (files.size() == 1);
    if (files.size() == 1)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (files[0]));
        CHECK (reader != nullptr);
        if (reader != nullptr)
        {
            CHECK (reader->sampleRate == 48000.0);
            CHECK (reader->lengthInSamples == 512);
            juce::AudioBuffer<float> audio (2, 512);
            CHECK (reader->read (&audio, 0, 512, 0, true, true));
            CHECK (std::abs (audio.getSample (0, 511) - 511.0f / 1024.0f) < 1.0e-5f);
        }
    }
    CHECK (dir.deleteRecursively());
}
