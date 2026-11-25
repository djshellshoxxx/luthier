/*  What a host's re-activation costs at an unchanged configuration.

    clap-validator's param fuzzing activates the plugin 50 times at one rate
    and block size, processing five blocks each time; the macOS runner timed
    the test out at 45 s (Linux: ~17.5 s). Each juce::dsp::Convolution the
    plugin constructs starts JUCE's loader thread, destroying one waits for
    that thread to wake from its 10 ms idle sleep, and each response built
    waited on the same thread: a re-activation rebuilt the cabinet and body
    banks, three latency probes and the four TONE MATCH convolutions. An
    identical re-prepare now keeps them (LiveConvolution::prepare, IrSlot). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Support/ThreadProbe.h"

#include <iostream>

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (PrepareCost, anIdenticalReprepareStartsNoConvolutionAndBuildsNoResponse)
{
    LuthierAudioProcessor processor;

    // One activation as the validator makes it: prepare, five blocks, release.
    const auto activate = [&processor]
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        processor.prepareToPlay (48000.0, 512);
        const auto took = juce::Time::getMillisecondCounterHiRes() - start;

        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 5; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        processor.releaseResources();
        return took;
    };

    const double first = activate();
    const int created = ThreadProbe::convolutionsCreated.load();
    const int built = ThreadProbe::convolutionResponsesBuilt.load();
    const double second = activate();

    const int newConvolutions = ThreadProbe::convolutionsCreated.load() - created;
    const int newResponses = ThreadProbe::convolutionResponsesBuilt.load() - built;
    const auto timings = " (first prepare " + juce::String (first, 1) + " ms, identical second "
                           + juce::String (second, 1) + " ms)";

    // One trace line, so a slow runner's log shows the margin it had.
    std::cout << "[prepare-cost]" << timings << std::endl;

    CHECK_MSG (newConvolutions == 0, juce::String (newConvolutions)
                 + " convolutions constructed by an identical re-prepare" + timings);
    CHECK_MSG (newResponses == 0, juce::String (newResponses)
                 + " impulse responses rebuilt by an identical re-prepare" + timings);

    // Relative to the first, so the machine's speed cancels out; the margin
    // is generous because a shared runner's scheduler owns the worst case.
    CHECK_MSG (second < 0.5 * first + 50.0, "an identical re-prepare cost about as much as the first" + timings);
}

LUTHIER_TEST (PrepareCost, theFirstPrepareBuildsTheGuitarEveryLaterOneBuilds)
{
    /*  The engine builds its own guitar type at every prepare, before the
        processor applies the guitar it plays. In Free the default Stratocaster
        plays as the Classic T-Style (editions 5.1.3): an engine still on the
        Stratocaster at the first prepare loaded that guitar's body and cabinet
        IRs, and the T-Style's at the second, so an identical second prepare
        had a response to build. */
    LuthierAudioProcessor processor;
    const auto before = processor.getEngine().getGuitarType();
    processor.prepareToPlay (48000.0, 512);
    processor.releaseResources();
    const auto after = processor.getEngine().getGuitarType();
    CHECK_MSG (after == before, "the first prepare built guitar type " + juce::String ((int) before)
                                  + ", and the engine plays type " + juce::String ((int) after));
}
