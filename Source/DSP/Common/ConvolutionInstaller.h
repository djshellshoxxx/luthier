/*  Making juce::dsp::Convolution's impulse-response swap observable.

    juce::dsp::Convolution loads a response on its own background thread and then
    installs it the next time process() is called. Until that call happens the
    convolution is still a unit impulse, so a caller that starts using it the
    moment loadImpulseResponse() returns passes the signal through completely dry
    for the first block or two: on a guitar plugin that is the amp with no speaker
    on it at all, which is both the loudest and the harshest thing the signal path
    can produce, arriving exactly on the transient a preset change makes.

    It also makes the plugin non-deterministic. Whether the response is in yet
    depends on how many blocks have been processed, so the first render after a
    prepare does not match the second - which would make offline rendering and
    regression testing meaningless.

    These helpers drive the swap to completion on the thread that asked for it, so
    that once the load call returns the response really is in place. The caller
    holds its own lock while doing so; see CabinetEngine and BodyEngine, where the
    audio thread try-locks and falls back to its analytic model for the one block
    a swap can overlap.
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace luthier::ConvolutionInstaller
{
    /** Pushes silent blocks through until the convolution reports a response of a
        different length from `sizeBefore`, which is how a completed swap shows up
        from the outside. Returns true if it installed within the deadline.

        Never call this from the audio thread: it sleeps.
    */
    inline bool pumpUntilInstalled (juce::dsp::Convolution& convolution,
                                    int numChannels,
                                    int blockSize,
                                    int sizeBefore,
                                    int timeoutMs = 4000,
                                    int settleSamples = 0)
    {
        juce::AudioBuffer<float> silence (juce::jmax (1, numChannels),
                                          juce::jlimit (1, 4096, blockSize));   // never past the prepared block: a 16-sample pump overran a convolution prepared for 1

        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

        for (;;)
        {
            silence.clear();

            juce::dsp::AudioBlock<float> block (silence);
            juce::dsp::ProcessContextReplacing<float> context (block);
            convolution.process (context);

            if ((int) convolution.getCurrentIRSize() != sizeBefore)
            {
                /*  Installed - but juce::dsp::Convolution then crossfades from the
                    old response to the new one over its first ~50 ms of audio.
                    Pump that through too, so the first real block hears only the
                    new response and two renders of the same guitar match. */
                for (int done = 0; done < settleSamples; done += silence.getNumSamples())
                {
                    silence.clear();
                    juce::dsp::AudioBlock<float> settleBlock (silence);
                    juce::dsp::ProcessContextReplacing<float> settleContext (settleBlock);
                    convolution.process (settleContext);
                }

                return true;
            }

            if (juce::Time::getMillisecondCounter() >= deadline)
                return false;

            juce::Thread::sleep (1);
        }
    }

    /** Puts the convolution back to a one-sample unit impulse and waits for that
        to take effect.

        This is what makes the swap detectable at all. Two different cabinet
        responses are usually the same length as each other, so "the length
        changed" only means something if the length is first driven to a value no
        real response has. One sample is that value.
    */
    inline void installUnitImpulse (juce::dsp::Convolution& convolution,
                                    double sampleRate,
                                    int numChannels,
                                    int blockSize)
    {
        if ((int) convolution.getCurrentIRSize() == 1)
            return;

        juce::AudioBuffer<float> dirac (1, 1);
        dirac.setSample (0, 0, 1.0f);

        const int sizeBefore = (int) convolution.getCurrentIRSize();

        convolution.loadImpulseResponse (std::move (dirac),
                                         sampleRate,
                                         juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);

        pumpUntilInstalled (convolution, numChannels, blockSize, sizeBefore);
    }
}
