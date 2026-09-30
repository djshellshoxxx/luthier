/* USER_MANUAL.md MIDI Learn / midi-learn.md LEARN-03:
   exhaust the CC space, with the engine's four intentional exclusions.
   The spec's "all 128" discrepancy is recorded in CODEX_AUDIT_FINDINGS.md. */
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (MidiLearnCoverage, everyNonReservedCcCanBeLearnedAndApplied)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& learn = processor->getMidiLearn();
    auto* parameter = processor->getState().getParameter (ParamIDs::macroDrive);
    CHECK (parameter != nullptr);
    if (parameter == nullptr)
        return;

    for (int cc = 0; cc < 128; ++cc)
    {
        if (cc == 64 || cc == 66 || cc == 120 || cc == 123)
            continue;
        learn.clearAllMappings();
        learn.startLearning (ParamIDs::macroDrive);
        juce::MidiBuffer input;
        input.addEvent (juce::MidiMessage::controllerEvent (1, cc, 63), 0);
        learn.processMidi (input);
        learn.servicePendingLearn();
        CHECK_MSG (learn.getCcForParameter (ParamIDs::macroDrive) == cc,
                   "failed to learn CC " + juce::String (cc));
        CHECK (! learn.isLearning());
        CHECK (learn.getNumMappings() == 1);

        for (int value : { 0, 127 })
        {
            // Ensure a missing lookup cannot pass on the previous CC's value.
            parameter->setValueNotifyingHost (value == 0 ? 1.0f : 0.0f);
            input.clear();
            input.addEvent (juce::MidiMessage::controllerEvent (1, cc, value), 0);
            learn.processMidi (input);
            CHECK_NEAR (parameter->getValue(), value / 127.0f, 1.0e-6f);
        }
    }
}

LUTHIER_TEST (MidiLearnCoverage, performancePedalsAndPanicDoNotStealTheLearnTarget)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& learn = processor->getMidiLearn();
    for (int cc : { 64, 66, 120, 123 })
    {
        learn.clearAllMappings();
        learn.startLearning (ParamIDs::macroDrive);
        juce::MidiBuffer input;
        input.addEvent (juce::MidiMessage::controllerEvent (1, cc, 127), 0);
        learn.processMidi (input);
        learn.servicePendingLearn();
        CHECK (learn.isLearning());
        CHECK (learn.getNumMappings() == 0);
        // A valid control arriving after the excluded message still learns.
        input.clear();
        input.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0);
        learn.processMidi (input);
        learn.servicePendingLearn();
        CHECK (learn.getCcForParameter (ParamIDs::macroDrive) == 21);
        CHECK (! learn.isLearning());
    }
}
