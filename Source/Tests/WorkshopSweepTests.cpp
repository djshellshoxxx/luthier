/*  SPEC-SWEEP ui-wiring UW-T7 (ui-wiring 23): auditions never commit. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

/*  UW-T7: a hundred seeded random auditions across every slot leave the
    committed guitar byte-for-byte as it was, and the undo stack untouched. */
LUTHIER_TEST (WorkshopBench, aHundredRandomAuditionsNeverCommit)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& bench = processor.getBench();
    auto& library = processor.getPartLibrary();

    const auto before = juce::JSON::toString (processor.getCurrentGuitar().toEmbeddedVar());
    const int undoBefore = processor.getNumUndoSteps();

    juce::Random random (0x5eed7);
    int auditioned = 0;

    for (int i = 0; i < 100; ++i)
    {
        const auto slot = (GuitarSlot) random.nextInt (kNumGuitarSlots);
        const auto parts = library.getParts (getSlotPartType (slot));

        if (parts.isEmpty())
            continue;

        bench.beginAudition (slot, parts[random.nextInt (parts.size())]);
        auditioned += bench.isAuditioning() ? 1 : 0;

        // Some auditions render a block, as a player listening would.
        if (i % 10 == 0)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        bench.endAudition();
    }

    CHECK (auditioned >= 50);
    CHECK_MSG (juce::JSON::toString (processor.getCurrentGuitar().toEmbeddedVar()) == before,
               "an audition changed the committed guitar");
    CHECK (processor.getNumUndoSteps() == undoBefore);
    CHECK (! processor.isGuitarEdited());
}
