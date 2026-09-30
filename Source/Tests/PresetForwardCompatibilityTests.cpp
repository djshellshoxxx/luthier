/* file-formats.md 0.3 / PRESET_FORMAT.md Safety: a newer preset's
   top-level data survives, without leaking into the next preset. */
#include "TestFramework.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (PresetForwardCompatibility, futureSchemaKeepsOpaqueTopLevelData)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& presets = processor->getPresetManager();
    auto future = presets.toVar ("Future", "Test");
    future.getDynamicObject()->setProperty ("schemaVersion", PresetManager::kSchemaVersion + 1);
    const auto payload = juce::JSON::parse (R"({"nested":[null,true,12.5,{"text":"kept"}]})");
    future.getDynamicObject()->setProperty ("futurePayload", payload);

    CHECK (presets.fromVar (future));
    const auto saved = presets.toVar ("Future", "Test");
    CHECK (juce::JSON::toString (saved.getProperty ("futurePayload", {}))
           == juce::JSON::toString (payload));
    CHECK (saved.getProperty ("magic", {}).toString() == PresetManager::kMagic);
    CHECK ((int) saved.getProperty ("schemaVersion", 0) == PresetManager::kSchemaVersion);
}

LUTHIER_TEST (PresetForwardCompatibility, loadingAnotherPresetClearsPreviousUnknownFields)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& presets = processor->getPresetManager();
    // Deep copy the clean fixture before attaching the future payload.
    const auto clean = juce::JSON::parse (juce::JSON::toString (presets.toVar ("Clean", "Test")));
    auto future = presets.toVar ("Future", "Test");
    future.getDynamicObject()->setProperty ("futurePayload", "only in Future");
    CHECK (presets.fromVar (future));
    CHECK (presets.toVar().hasProperty ("futurePayload"));
    CHECK (presets.fromVar (clean));
    CHECK (! presets.toVar().hasProperty ("futurePayload"));
}

LUTHIER_TEST (PresetForwardCompatibility, refusingAnImpostorKeepsCurrentUnknownFields)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& presets = processor->getPresetManager();
    auto future = presets.toVar ("Future", "Test");
    future.getDynamicObject()->setProperty ("futurePayload", "still here");
    CHECK (presets.fromVar (future));
    const auto impostor = juce::JSON::parse (R"({"magic":"other.product","schemaVersion":1,"futurePayload":"wrong"})");
    CHECK (! presets.fromVar (impostor));
    CHECK (presets.toVar().getProperty ("futurePayload", {}).toString() == "still here");
}
