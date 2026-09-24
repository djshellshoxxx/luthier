/*  Regression tests for bugs found in code review (docs/review/FINDINGS.md).
    Each test names the finding it pins down. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Amp/ToneStack.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  R-001: a host may hand processBlock more samples than it promised in
    prepareToPlay. The engine split such a block, but the processor's own
    scratch buffers (the click, the tune click, the backing track, the monitor)
    were sized from the promise and written for the whole block. */
LUTHIER_TEST (ReviewRegression, aBlockBiggerThanPreparedIsRenderedWhole)
{
    constexpr int kPrepared = 128;
    constexpr int kHostBlock = 1000;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kPrepared);
    processor.setPracticePanelOpen (true);
    processor.getMetronome().setEnabled (true);
    processor.setClickToMain (true);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), kHostBlock);
    bool heardLate = false;

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        // A note after the prepared size must still be played, at its own time.
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 700);

        processor.processBlock (buffer, midi);

        CHECK_FINITE (buffer.getReadPointer (0), kHostBlock);

        if (block == 0)
        {
            // Nothing from the note before its own sample: it was not moved earlier.
            heardLate = buffer.getMagnitude (0, kPrepared * 5, kHostBlock - kPrepared * 5) > 0.0f;
        }
    }

    CHECK (heardLate);
}

//==============================================================================
/*  R-006: the tone stack's b3 term had t*C1C2C3R1R2R4 where Yeh's derivation has
    t*l*C1C2C3R1R2R4. A passive network cannot have gain above unity; with the bass
    at zero the typo gave up to +28 dB of treble. */
namespace
{
    double toneStackGainAt (double bass, double mid, double treble, double hz)
    {
        constexpr double sr = 48000.0;
        luthier::ToneStack stack;
        stack.prepare (sr);
        stack.setControls (bass, mid, treble);

        double peak = 0.0;
        const int n = (int) sr;

        for (int i = 0; i < n; ++i)
        {
            const double y = stack.process (std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));

            if (i > n / 2)
                peak = std::max (peak, std::abs (y));
        }

        return peak;
    }
}

LUTHIER_TEST (ReviewRegression, theToneStackIsPassiveAtEverySetting)
{
    // ToneStack::process applies a fixed 6.5x insertion-loss makeup; the
    // network itself must stay at or below unity.
    constexpr double makeup = 6.5;

    for (double bass : { 0.0, 0.5, 1.0 })
        for (double treble : { 0.0, 0.5, 1.0 })
            for (double hz : { 100.0, 1000.0, 5000.0, 10000.0 })
                CHECK_MSG (toneStackGainAt (bass, 0.5, treble, hz) / makeup <= 1.05,
                           "bass " + juce::String (bass) + " treble " + juce::String (treble)
                               + " at " + juce::String (hz) + " Hz");
}
