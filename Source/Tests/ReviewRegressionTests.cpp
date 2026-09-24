/*  Regression tests for bugs found in code review (docs/review/FINDINGS.md).
    Each test names the finding it pins down. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

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
