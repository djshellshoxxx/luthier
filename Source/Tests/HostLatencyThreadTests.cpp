#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Support/ThreadProbe.h"
#include <thread>

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (HostLatencyThread, audioPublishesButOnlyControlNotifiesHost)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 64);
    struct Listener final : juce::AudioProcessorListener
    {
        std::atomic<int> audioNotifications {0}, controlNotifications {0};
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& detail) override
        {
            if (detail.latencyChanged)
                (ThreadProbe::isMarkedAudioThread ? audioNotifications : controlNotifications).fetch_add (1);
        }
    } listener;
    processor.addListener (&listener);
    int changed = 0;
    for (int factor : {1, 2, 4, 8})
    {
        const int before = processor.getLatencySamples();
        processor.getEngine().getAmpEngine().setOversamplingFactor (factor);
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, 64);
            juce::MidiBuffer midi;
            buffer.clear();
            ThreadProbe::markAsAudioThread();
            processor.processBlock (buffer, midi);
            ThreadProbe::markAsAudioThread (false);
        });
        audio.join();
        const int expected = processor.getEngine().getLatencySamples();
        if (expected != before)
        {
            ++changed;
            CHECK (processor.getLatencySamples() == before);
        }
        CHECK (listener.audioNotifications.load() == 0);
        // Synchronously service the existing message-thread timer after it is
        // due; no custom notification path or private callback seam is used.
        for (int attempt = 0; attempt < 20 && processor.getLatencySamples() != expected; ++attempt)
        {
            juce::Thread::sleep (25);
            juce::Timer::callPendingTimersSynchronously();
        }
        CHECK (processor.getLatencySamples() == expected);
        CHECK (processor.getRouting().getLatencyReport().mainOut == expected);
    }
    processor.removeListener (&listener);
    CHECK (changed >= 2);
    CHECK (listener.controlNotifications.load() >= 2);
}
