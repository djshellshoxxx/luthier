/*  Host-integration regressions found by the CLAP validator (host-integration.md 3,
    state-model.md): what a host restores must survive what a host does next.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::MemoryBlock saveState (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    juce::String asText (const juce::MemoryBlock& block)
    {
        return juce::String::fromUTF8 (static_cast<const char*> (block.getData()), (int) block.getSize());
    }

    /** Equal states; when not, both are left in the temp folder to diff. */
    bool sameState (const juce::String& a, const juce::String& b, const char* name)
    {
        if (a == b)
            return true;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        dir.getChildFile (juce::String (name) + "-a.json").replaceWithText (a);
        dir.getChildFile (juce::String (name) + "-b.json").replaceWithText (b);
        return false;
    }
}

LUTHIER_TEST (HostState, aSessionSurvivesThePrepareThatFollowsIt)
{
    // Hosts restore a session and then prepare (and prepare again on a sample
    // rate change). prepare() used to reset the LFOs' custom shapes, the
    // envelopes' curves, the sequencers' steps and the followers' sources.
    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (48000.0, 256);

    auto& lfo = source->getModMatrix().getLfo (0);
    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        lfo.setBreakpoint (i, i % 2 == 0 ? 0.9 : -0.3);

    auto step = source->getModMatrix().getSequencer (0).getStep (3);
    step.value = 0.123;
    source->getModMatrix().getSequencer (0).setStep (3, step);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());
    restored->prepareToPlay (44100.0, 512);

    for (int i = 0; i < ModLfo::kNumBreakpoints; ++i)
        CHECK_NEAR (restored->getModMatrix().getLfo (0).getBreakpoint (i), i % 2 == 0 ? 0.9 : -0.3, 1.0e-9);

    CHECK_NEAR (restored->getModMatrix().getSequencer (0).getStep (3).value, 0.123, 1.0e-9);
    CHECK (sameState (asText (saveState (*restored)), asText (saved), "luthier-restored"));
}

LUTHIER_TEST (HostState, anUnpreparedInstanceSavesTheSameStateAsAPreparedOne)
{
    // A host may save before it ever prepares (clap-validator
    // state-reproducibility-flush). The defaults must not depend on prepare().
    auto prepared = std::make_unique<LuthierAudioProcessor>();
    prepared->prepareToPlay (48000.0, 256);

    auto unprepared = std::make_unique<LuthierAudioProcessor>();

    CHECK (sameState (asText (saveState (*prepared)), asText (saveState (*unprepared)), "luthier-unprepared"));
}

LUTHIER_TEST (HostState, theMorphSliderAndTheCharacterAmountAreSaved)
{
    auto source = std::make_unique<LuthierAudioProcessor>();

    auto set = [] (LuthierAudioProcessor& p, const char* id, float normalised)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (normalised);
    };

    set (*source, ParamIDs::presetMorphPosition, 0.3f);
    set (*source, ParamIDs::macroCharacter, 0.8f);

    const auto saved = saveState (*source);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (saved.getData(), (int) saved.getSize());

    CHECK_NEAR (restored->getState().getParameter (ParamIDs::presetMorphPosition)->getValue(), 0.3f, 1.0e-4);
    CHECK_NEAR (restored->getState().getParameter (ParamIDs::macroCharacter)->getValue(), 0.8f, 1.0e-4);
}

LUTHIER_TEST (HostState, aHostWritingAGuitarTypeWithItsPartsKeepsTheParts)
{
    // A session or automation lands the guitar type and the body together, in
    // any order: the body the host wrote is kept (guitar-workshop 0.6's
    // shortcut writes only what the host did not).
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (48000.0, 256);

    auto* body = p->getState().getParameter (ParamIDs::bodyDepth);
    auto* type = p->getState().getParameter (ParamIDs::guitarType);
    CHECK (body != nullptr && type != nullptr);

    if (body == nullptr || type == nullptr)
        return;

    body->setValueNotifyingHost (0.013f);
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) + 3.0f));
    p->getParameterBridge().applyAllNow();

    CHECK_NEAR (body->getValue(), 0.013f, 1.0e-4);

    // A player's pick in the header (a gesture) loads the new guitar's parts.
    type->beginChangeGesture();
    type->setValueNotifyingHost (type->convertTo0to1 (type->convertFrom0to1 (type->getValue()) - 2.0f));
    type->endChangeGesture();
    p->getParameterBridge().applyAllNow();

    CHECK_MSG (std::abs (body->getValue() - 0.013f) > 1.0e-3, "the picked guitar did not bring its own body");
}

LUTHIER_TEST (HostState, parameterTextRoundTripsStably)
{
    // host-integration 3 (clap-validator param-conversions): value -> text ->
    // value -> text must not drift.
    LuthierAudioProcessor p;
    juce::Random rng (1234);

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);

        if (ranged == nullptr)
            continue;

        for (int k = 0; k < 8; ++k)
        {
            const float v = rng.nextFloat();
            const auto text = ranged->getText (v, 64);
            const float back = ranged->getValueForText (text);
            CHECK_MSG (ranged->getText (back, 64) == text,
                       ranged->getName (64) + ": '" + text + "' -> '" + ranged->getText (back, 64) + "'");
        }
    }
}



LUTHIER_TEST (HostState, processingDoesNotMoveParameters)
{
    // clap-validator param-set-wrong-namespace: a host that sends nothing
    // must see every parameter where it left it, before and after processing.
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->setRateAndBufferSizeDetails (48000.0, 512);

    std::vector<float> before;
    for (auto* param : p->getParameters())
        before.push_back (param->getValue());

    p->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int b = 0; b < 20; ++b)
    {
        buffer.clear();
        p->processBlock (buffer, midi);
    }

    juce::StringArray moved;
    for (int i = 0; i < p->getParameters().size(); ++i)
        if (std::abs (p->getParameters()[i]->getValue() - before[(size_t) i]) > 1.0e-6f)
            moved.add (p->getParameters()[i]->getName (64) + " " + juce::String (before[(size_t) i]) + " -> "
                       + juce::String (p->getParameters()[i]->getValue()));

    CHECK_MSG (moved.isEmpty(), "parameters moved by themselves: " + moved.joinIntoString (", "));
}

LUTHIER_TEST (HostState, theSameParametersGiveTheSameStateHoweverTheyArrived)
{
    // clap-validator state-reproducibility-flush: one instance gets random
    // values while processing, another gets them without ever processing.
    // Their saved states must match.
    juce::Random rng (777);
    std::vector<float> values;

    auto first = std::make_unique<LuthierAudioProcessor>();
    for (int i = 0; i < first->getParameters().size(); ++i)
        values.push_back (rng.nextFloat());

    first->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (first->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < first->getParameters().size(); ++i)
        first->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    for (int b = 0; b < 5; ++b)
    {
        buffer.clear();
        first->processBlock (buffer, midi);
    }

    auto second = std::make_unique<LuthierAudioProcessor>();

    for (int i = 0; i < second->getParameters().size(); ++i)
        second->getParameters()[i]->setValueNotifyingHost (values[(size_t) i]);

    CHECK (sameState (asText (saveState (*first)), asText (saveState (*second)), "luthier-arrival"));
}
