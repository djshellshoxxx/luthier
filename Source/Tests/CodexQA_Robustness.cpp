/* Deterministic host-facing robustness matrix. Reproduce with the seed below. */
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kSeed = 0xC0DE513;
    constexpr float kRunawayPeak = 4.0f; // generous safety ceiling, not a loudness target
    constexpr double rates[] = { 44100.0, 48000.0, 96000.0 };
    constexpr int sizes[] = { 1, 17, 257, 1024 };

    juce::String context (const juce::String& label, int iteration, double rate, int size)
    {
        return label + " case=" + juce::String (iteration) + " rate=" + juce::String (rate, 0)
             + " block=" + juce::String (size) + " seed=" + juce::String (kSeed);
    }

    struct Result
    {
        bool finite = true;
        float peak = 0.0f;
    };

    Result render (LuthierAudioProcessor& processor, int size, juce::Random& rng)
    {
        juce::AudioBuffer<float> audio (juce::jmax (2, processor.getTotalNumOutputChannels()), size);
        Result result;

        for (int block = 0; block < 3; ++block)
        {
            juce::MidiBuffer midi;

            if (block < 2)
            {
                const int note = 36 + rng.nextInt (49);
                midi.addEvent (juce::MidiMessage::noteOn (1 + rng.nextInt (4), note,
                                                           (juce::uint8) (20 + rng.nextInt (108))),
                               rng.nextInt (size));
            }
            else
            {
                // Release every channel used above. The old fixture sent channel 1
                // All Notes Off before scheduling another random-channel note-on in
                // this same block, so it normally ended with a held voice and never
                // isolated the release path.
                for (int channel = 1; channel <= 4; ++channel)
                    midi.addEvent (juce::MidiMessage::allNotesOff (channel), 0);
            }

            audio.clear();
            processor.processBlock (audio, midi);

            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                for (int sample = 0; sample < size; ++sample)
                {
                    const float value = audio.getSample (ch, sample);
                    result.finite = result.finite && std::isfinite (value);

                    if (std::isfinite (value))
                        result.peak = juce::jmax (result.peak, std::abs (value));
                }
        }

        return result;
    }

    void checkRender (TestContext& ctx, const Result& result, const juce::String& label)
    {
        CHECK_MSG (result.finite, label + " non-finite sample");
        CHECK_MSG (result.peak <= kRunawayPeak,
                   label + " peak=" + juce::String (result.peak, 6));
    }

    std::vector<float> values (const LuthierAudioProcessor& processor)
    {
        std::vector<float> snapshot;
        for (auto* parameter : processor.getParameters())
            snapshot.push_back (parameter->getValue());
        return snapshot;
    }
}

LUTHIER_TEST (CodexRobustness, everyFactoryPresetAtVaryingHostConfigurations)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    FactoryPresets::setProcessorForRanges (processor.get());
    juce::Random rng (kSeed);
    const int count = FactoryPresets::getNumPresets();
    CHECK_MSG (count > 0, "empty factory bank");

    for (int index = 0; index < count; ++index)
    {
        // getPreset returns a thread-local view; keep the name before another call.
        const auto& definition = FactoryPresets::getPreset (index);
        const juce::String name (definition.name);
        const auto rate = rates[index % 3];
        const auto size = sizes[index % 4];
        const auto label = context ("preset=" + name, index, rate, size);
        processor->prepareToPlay (rate, size);
        CHECK_MSG (processor->getPresetManager().fromVar (FactoryPresets::toVar (definition, *processor)),
                   label + " load failed");
        processor->getParameterBridge().applyAllNow();
        checkRender (ctx, render (*processor, size, rng), label);

        // Save a host session, disturb the parameters, restore, then compare
        // the normalized values. Jam transient controls are intentionally excluded
        // by the preset/state model, so compare the persistent parameters only.
        juce::MemoryBlock state;
        processor->getStateInformation (state);
        CHECK_MSG (state.getSize() > 0, label + " saved empty state");
        const auto before = values (*processor);
        for (auto* parameter : processor->getParameters())
            parameter->setValueNotifyingHost (rng.nextFloat());
        processor->setStateInformation (state.getData(), (int) state.getSize());
        processor->getParameterBridge().applyAllNow();

        int parameterIndex = 0;
        for (auto* parameter : processor->getParameters())
        {
            const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
            const juce::String id = withId != nullptr ? withId->paramID : juce::String();
            if (! ParamIDs::isJamTransient (id) && id != ParamIDs::presetMorphPosition)
                CHECK_MSG (std::abs (parameter->getValue() - before[(size_t) parameterIndex]) <= 2.0e-4f,
                           label + " state drift " + id + " " + juce::String (before[(size_t) parameterIndex], 7)
                                 + " -> " + juce::String (parameter->getValue(), 7));
            ++parameterIndex;
        }
    }
    FactoryPresets::setProcessorForRanges (nullptr);
}

LUTHIER_TEST (CodexRobustness, seededParameterMidiAndBlockMatrix)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    juce::Random rng (kSeed);

    // Boundary values plus seeded interior values. The rate and block combinations
    // rotate independently, including a one-sample block and a non-power-of-two block.
    for (int iteration = 0; iteration < 12; ++iteration)
    {
        const auto rate = rates[iteration % 3];
        const auto size = sizes[(iteration / 3) % 4];
        const auto label = context ("random", iteration, rate, size);
        processor->prepareToPlay (rate, size);

        for (auto* parameter : processor->getParameters())
        {
            const float value = iteration % 4 == 0 ? 0.0f
                              : iteration % 4 == 1 ? 1.0f : rng.nextFloat();
            parameter->setValueNotifyingHost (value);
        }

        processor->getParameterBridge().applyAllNow();
        checkRender (ctx, render (*processor, size, rng), label);
    }
}
