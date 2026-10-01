/*  performance-budget.md 5 (PB-17): launch to audible.

    "Standalone launch to audible <= 1.5 s cold." The window and the audio device
    are the OS's; what the plug-in owns is everything from constructing the
    processor to the first sample of a played note, which is what this measures:
    construction, prepareToPlay, a note-on, and blocks rendered until one is not
    silent. Best of three, cold (the resource search forgotten) each time, as the
    Boot test does; the default run holds 2x the spec for a shared runner and
    LUTHIER_PERF=1 asserts it as written. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (Boot, aPlayedNoteIsAudibleWithinTheLaunchBudget)
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 128;

    auto launchToAudible = [&] (bool& heard)
    {
        IrLibrary::forgetResourcesFolder();
        const double start = juce::Time::getMillisecondCounterHiRes();

        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);

        heard = false;

        // Two seconds of audio is far more than a note needs to speak.
        for (int b = 0; b < (int) (2.0 * kSr / kBlock) && ! heard; ++b)
        {
            buffer.clear();
            p->processBlock (buffer, midi);
            midi.clear();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                if (buffer.getMagnitude (ch, 0, kBlock) > 1.0e-4f)
                    heard = true;
        }

        return juce::Time::getMillisecondCounterHiRes() - start;
    };

    double best = 1.0e9;
    bool everHeard = false;

    for (int i = 0; i < 3; ++i)
    {
        bool heard = false;
        best = juce::jmin (best, launchToAudible (heard));
        everHeard = everHeard || heard;
    }

    CHECK_MSG (everHeard, "a played note never became audible in two seconds");

    const double slack = perfRunRequested() ? 1.0 : 2.0;
    CHECK_MSG (best <= 1500.0 * slack,
               "launch to audible over budget: " + juce::String (best, 1) + " ms, budget 1500 ms x "
                   + juce::String (slack, 1));
}
