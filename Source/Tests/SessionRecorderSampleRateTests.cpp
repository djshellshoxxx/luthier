/*  F-D1: the session recorder must know the sample rate of the audio it holds.

    The ring used to be sized (and its rate learned) only when the PRACTICE tab
    asked for a different length, so a host rate change left it recording at
    the new rate while saveLastTake stamped the WAV with the old one: the take
    played back at the wrong speed and pitch. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Practice/Looper.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kBlock = 512;

    juce::File scratch (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("luthier-recorder-rate").getChildFile (name);
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }

    /** Plays A4 through the plugin for `seconds` of audio at the prepared rate. */
    void playA4 (LuthierAudioProcessor& processor, double sampleRate, double seconds)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);
        const int blocks = (int) std::ceil (seconds * sampleRate / kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 110), 0);

            processor.processBlock (buffer, midi);
        }
    }

    struct Take
    {
        double headerRate = 0.0;
        int length = 0;
        juce::AudioBuffer<float> audio;
    };

    bool readTake (const juce::File& dir, Take& take)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))
            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f)); reader != nullptr)
            {
                take.headerRate = reader->sampleRate;
                take.length = (int) reader->lengthInSamples;
                take.audio.setSize (2, take.length);
                reader->read (&take.audio, 0, take.length, 0, true, true);
                return true;
            }

        return false;
    }

    /** The fundamental, in Hz as the file's own header would play it: the
        strongest autocorrelation lag between 50 and 400 samples. */
    double fundamentalHz (const Take& take)
    {
        const int start = take.length / 2;
        const int count = juce::jmin (8192, take.length - start - 400);

        if (count <= 0)
            return 0.0;

        const auto* x = take.audio.getReadPointer (0) + start;
        int bestLag = 0;
        double best = -1.0;

        for (int lag = 50; lag <= 400; ++lag)
        {
            double sum = 0.0, e1 = 0.0, e2 = 0.0;

            for (int i = 0; i < count; ++i)
            {
                sum += (double) x[i] * x[i + lag];
                e1 += (double) x[i] * x[i];
                e2 += (double) x[i + lag] * x[i + lag];
            }

            const double r = sum / std::sqrt (juce::jmax (1.0e-30, e1 * e2));

            if (r > best)
            {
                best = r;
                bestLag = lag;
            }
        }

        return bestLag > 0 ? take.headerRate / (double) bestLag : 0.0;
    }
}

//==============================================================================
LUTHIER_TEST (SessionRecorderRate, aTakeAfterAHostRateChangeIsSavedAtTheNewRate)
{
    LuthierAudioProcessor processor;
    processor.setPracticePanelOpen (true);
    processor.prepareToPlay (44100.0, kBlock);

    auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.prepare (44100.0, 1.0));
    recorder.setEnabled (true);

    processor.prepareToPlay (48000.0, kBlock);
    playA4 (processor, 48000.0, 1.0);

    const auto dir = scratch ("rate-change");
    CHECK (recorder.saveLastTake (dir));

    Take take;
    CHECK (readTake (dir, take));

    CHECK_MSG (take.headerRate == 48000.0, "the WAV header says " + juce::String (take.headerRate));
    CHECK_MSG (take.length > 40000, "the take holds " + juce::String (take.length) + " samples");

    const double hz = fundamentalHz (take);
    CHECK_MSG (std::abs (hz - 440.0) < 440.0 * 0.03,
               "A4 plays back at " + juce::String (hz, 1) + " Hz");
}

LUTHIER_TEST (SessionRecorderRate, capacityFollowsTheRateAndAnOldRateTakeIsDiscarded)
{
    LuthierAudioProcessor processor;
    processor.setPracticePanelOpen (true);
    processor.prepareToPlay (44100.0, kBlock);

    auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.prepare (44100.0, 1.0));
    recorder.setEnabled (true);
    CHECK (std::abs (recorder.getCapacityMinutes() - 1.0) < 1.0e-6);

    playA4 (processor, 44100.0, 0.5);
    CHECK (recorder.getRecordedSamples() > 0);

    // The same rate again (a host restarting its transport) keeps the take.
    const int kept = recorder.getRecordedSamples();
    processor.prepareToPlay (44100.0, kBlock);
    CHECK (recorder.getRecordedSamples() == kept);

    // A new rate: the ring keeps its length in minutes, and holds nothing of
    // the old rate (a take is never a mix of two rates).
    processor.prepareToPlay (96000.0, kBlock);
    CHECK_MSG (std::abs (recorder.getCapacityMinutes() - 1.0) < 1.0e-6,
               "capacity reads " + juce::String (recorder.getCapacityMinutes(), 4) + " minutes");
    CHECK (recorder.getSampleRate() == 96000.0);
    CHECK (recorder.getRecordedSamples() == 0);
    CHECK (recorder.isEnabled());
}

LUTHIER_TEST (SessionRecorderRate, aRecorderThatWasNeverSizedStaysUnallocatedAcrossARateChange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (44100.0, kBlock);
    processor.prepareToPlay (48000.0, kBlock);

    auto& recorder = processor.getSessionRecorder();
    CHECK (recorder.getCapacityMinutes() == 0.0);   // off by default: no ring, no memory
    CHECK (recorder.getSampleRate() == 48000.0);
}
