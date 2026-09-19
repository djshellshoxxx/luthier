/*  Freeze and E-Bow, per ambiguity-resolutions.md section 2.

    Section 2 resolves the "freeze / sustain infinite" ambiguity as a hybrid:
    Freeze is a captured-loop overlay (2.1), E-Bow drives the string through the
    feedback path (2.2). These are the 2.4 tests for the Freeze half.
*/

#include "TestFramework.h"

#include "../DSP/Master/FreezeOverlay.h"
#include "../DSP/Common/DspCommon.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** Fills a stereo buffer with a decaying plucked-ish tone plus a little
        noise, which is the kind of thing a freeze is actually taken from. */
    void fillWithSignal (juce::AudioBuffer<float>& buffer, double& phase,
                         RtRandom& random, double startSample)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double t = (startSample + (double) i) / kSr;
            const double envelope = std::exp (-t * 0.8);

            const double tone = std::sin (phase) * 0.6
                                  + std::sin (phase * 2.01) * 0.25
                                  + std::sin (phase * 3.02) * 0.1;

            const double noise = (random.nextDouble() * 2.0 - 1.0) * 0.02;
            const double value = (tone + noise) * envelope * 0.5;

            buffer.setSample (0, i, (float) value);
            buffer.setSample (1, i, (float) (value * 0.92));

            phase += juce::MathConstants<double>::twoPi * 220.0 / kSr;

            if (phase > juce::MathConstants<double>::twoPi * 1024.0)
                phase -= juce::MathConstants<double>::twoPi * 1024.0;
        }
    }

    /** Runs the overlay over silence and returns the layer it produces. */
    void renderLayer (FreezeOverlay& freeze, juce::AudioBuffer<float>& destination)
    {
        int written = 0;

        while (written < destination.getNumSamples())
        {
            const int count = juce::jmin (kBlock, destination.getNumSamples() - written);

            juce::AudioBuffer<float> block (2, count);
            block.clear();

            freeze.process (block);

            for (int channel = 0; channel < 2; ++channel)
                destination.copyFrom (channel, written, block, channel, 0, count);

            written += count;
        }
    }

    /** Captures a window, then leaves the layer holding. */
    void captureInto (FreezeOverlay& freeze)
    {
        juce::AudioBuffer<float> block (2, kBlock);

        double phase = 0.0;
        RtRandom random (0x51de);

        freeze.setEnabled (true);

        // Long enough to cover the largest window the spec allows.
        for (int i = 0; i < (int) (kSr * 1.5) / kBlock; ++i)
        {
            fillWithSignal (block, phase, random, (double) (i * kBlock));
            freeze.process (block);
        }
    }
}

//==============================================================================
/*  ambiguity-resolutions 2.4: "Freeze layer: capture then hold 60 s, RMS varies
    less than 0.5 dB." */
LUTHIER_TEST (Sustain, freezeLayerHoldsItsLevelForASixtySecondHold)
{
    FreezeOverlay freeze;
    freeze.prepare (kSr, 2);

    freeze.setCaptureMs (400.0);
    freeze.setLevelDb (-6.0);
    freeze.setAttackMs (20.0);

    captureInto (freeze);

    CHECK (freeze.isHolding());
    CHECK (freeze.getLoopLength() > 0);

    // Skip the first second so the attack ramp is not measured as drift.
    juce::AudioBuffer<float> warmup (2, (int) kSr);
    renderLayer (freeze, warmup);

    double quietest = 1.0e9, loudest = 0.0;

    juce::AudioBuffer<float> second (2, (int) kSr);

    for (int i = 0; i < 60; ++i)
    {
        renderLayer (freeze, second);

        CHECK_FINITE (second.getReadPointer (0), second.getNumSamples());

        double sum = 0.0;

        for (int channel = 0; channel < 2; ++channel)
            for (int s = 0; s < second.getNumSamples(); ++s)
            {
                const double value = (double) second.getSample (channel, s);
                sum += value * value;
            }

        const double rms = std::sqrt (sum / (double) (second.getNumSamples() * 2));

        quietest = juce::jmin (quietest, rms);
        loudest  = juce::jmax (loudest, rms);
    }

    CHECK_MSG (quietest > 1.0e-6, "the frozen layer fell silent during the hold");

    const double spreadDb = 20.0 * std::log10 (loudest / juce::jmax (1.0e-12, quietest));

    CHECK_MSG (spreadDb < 0.5,
               "the frozen layer's RMS moved by " + juce::String (spreadDb, 3)
                 + " dB across a 60 s hold, which is more than the half a decibel "
                   "ambiguity-resolutions 2.4 allows");
}

//==============================================================================
/*  The property the level test rests on: the seam is crossfaded once, at capture,
    so every cycle of the loop is identical to the one before it. */
LUTHIER_TEST (Sustain, freezeLoopRepeatsExactly)
{
    FreezeOverlay freeze;
    freeze.prepare (kSr, 2);

    freeze.setCaptureMs (300.0);
    freeze.setLevelDb (0.0);
    freeze.setAttackMs (5.0);

    captureInto (freeze);

    const int period = freeze.getLoopLength();

    CHECK (period > 0);

    // Past the attack, then two whole cycles.
    juce::AudioBuffer<float> settle (2, (int) kSr);
    renderLayer (freeze, settle);

    juce::AudioBuffer<float> twoCycles (2, period * 2);
    renderLayer (freeze, twoCycles);

    double worst = 0.0;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < period; ++i)
            worst = juce::jmax (worst,
                                std::abs ((double) twoCycles.getSample (channel, i)
                                            - (double) twoCycles.getSample (channel, i + period)));

    CHECK_MSG (worst < 1.0e-6,
               "consecutive cycles of the frozen loop differ by "
                 + juce::String (worst, 9) + ", so the layer is not periodic");
}

//==============================================================================
/*  2.1: "A new freeze replaces the layer." */
LUTHIER_TEST (Sustain, aSecondFreezeReplacesTheFirst)
{
    FreezeOverlay freeze;
    freeze.prepare (kSr, 2);
    freeze.setCaptureMs (250.0);
    freeze.setLevelDb (0.0);

    captureInto (freeze);

    juce::AudioBuffer<float> first (2, freeze.getLoopLength());
    renderLayer (freeze, first);

    // Re-arming mid-hold must capture again rather than be ignored.
    freeze.setEnabled (false);
    freeze.setEnabled (true);

    CHECK (! freeze.isHolding());

    juce::AudioBuffer<float> block (2, kBlock);

    double phase = 1.7;
    RtRandom random (0x9a1);

    for (int i = 0; i < (int) (kSr * 1.0) / kBlock; ++i)
    {
        // A different signal, so a replaced layer is distinguishable.
        fillWithSignal (block, phase, random, (double) (i * kBlock) + kSr);
        block.applyGain (0.35f);
        freeze.process (block);
    }

    CHECK (freeze.isHolding());

    juce::AudioBuffer<float> second (2, freeze.getLoopLength());
    renderLayer (freeze, second);

    const double firstRms  = first.getRMSLevel (0, 0, first.getNumSamples());
    const double secondRms = second.getRMSLevel (0, 0, second.getNumSamples());

    CHECK_MSG (std::abs (firstRms - secondRms) > 1.0e-4,
               "the second freeze produced the same layer as the first, so it was "
               "not replaced");
}

//==============================================================================
/*  Disabling releases rather than cutting, and the module goes fully idle. */
LUTHIER_TEST (Sustain, releaseFadesOutAndStops)
{
    FreezeOverlay freeze;
    freeze.prepare (kSr, 2);
    freeze.setCaptureMs (200.0);
    freeze.setLevelDb (0.0);
    freeze.setReleaseMs (100.0);

    captureInto (freeze);

    juce::AudioBuffer<float> settle (2, (int) kSr);
    renderLayer (freeze, settle);

    freeze.setEnabled (false);

    // Well past the release time.
    juce::AudioBuffer<float> tail (2, (int) (kSr * 0.5));
    renderLayer (freeze, tail);

    CHECK (! freeze.isHolding());

    juce::AudioBuffer<float> afterwards (2, kBlock);
    afterwards.clear();
    freeze.process (afterwards);

    CHECK_MSG (afterwards.getMagnitude (0, afterwards.getNumSamples()) == 0.0f,
               "the freeze kept sounding after its release finished");
}

//==============================================================================
/*  An idle freeze is not merely quiet, it is bit-transparent: the spec's
    zero-cost bypass. */
LUTHIER_TEST (Sustain, anIdleFreezeLeavesTheAudioAlone)
{
    FreezeOverlay freeze;
    freeze.prepare (kSr, 2);

    juce::AudioBuffer<float> block (2, kBlock);
    juce::AudioBuffer<float> reference (2, kBlock);

    double phase = 0.0;
    RtRandom random (0x2b2);

    fillWithSignal (block, phase, random, 0.0);

    for (int channel = 0; channel < 2; ++channel)
        reference.copyFrom (channel, 0, block, channel, 0, kBlock);

    freeze.process (block);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlock; ++i)
            CHECK (block.getSample (channel, i) == reference.getSample (channel, i));
}
