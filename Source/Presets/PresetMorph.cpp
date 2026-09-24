#include "PresetMorph.h"
#include "../PluginProcessor.h"

namespace luthier
{

PresetMorph::PresetMorph (LuthierAudioProcessor& p)
    : processor (p)
{
}

void PresetMorph::setEnabled (bool shouldMorph)
{
    if (enabled == shouldMorph)
        return;

    enabled = shouldMorph;
    appliedPosition = -1.0;
    loadedSide = -1;

    if (! enabled)
        return;

    // The morph starts from the sound the player has: A is it, and so is B
    // until another preset is loaded into B.
    const auto name = processor.getPresetManager().getCurrentPresetName();
    const auto current = processor.getPresetManager().toVar (name.isNotEmpty() ? name : juce::String ("Current"));

    if (! slots[slotA].isObject())
        setSlot (slotA, current, name.isNotEmpty() ? name : juce::String ("Current"));

    if (! slots[slotB].isObject())
        setSlot (slotB, current, name.isNotEmpty() ? name : juce::String ("Current"));
}

void PresetMorph::setSlot (Slot slot, const juce::var& presetState, const juce::String& name)
{
    slots[(size_t) slot] = presetState;
    names[(size_t) slot] = name;

    // The side whose structure is loaded may just have changed underneath.
    loadedSide = -1;

    if (enabled && hasBothSlots())
    {
        const double at = appliedPosition >= 0.0 ? appliedPosition : 0.0;
        appliedPosition = -1.0;
        apply (at);
    }
}

void PresetMorph::cancel()
{
    enabled = false;
    appliedPosition = -1.0;
    loadedSide = -1;
}

void PresetMorph::apply (double position)
{
    if (! enabled || ! hasBothSlots())
        return;

    const double b = juce::jlimit (0.0, 1.0, position);

    if (b == appliedPosition)
        return;

    // 5.1: structural state is the side the position is on. Crossing 0.5
    // loads that side's preset whole; the parameters are laid over it below.
    const int side = b >= 0.5 ? slotB : slotA;

    if (side != loadedSide)
    {
        auto& presets = processor.getPresetManager();
        presets.fromVar (slots[(size_t) side]);
        processor.getParameterBridge().applyAllNow();
        loadedSide = side;
    }

    auto* fromParams = slots[slotA].getProperty ("parameters", {}).getDynamicObject();
    auto* toParams = slots[slotB].getProperty ("parameters", {}).getDynamicObject();

    if (fromParams != nullptr && toParams != nullptr)
    {
        for (auto* p : processor.getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

            if (withId == nullptr || withId->paramID == ParamIDs::presetMorphPosition)
                continue;

            const juce::Identifier id (withId->paramID);
            const bool inA = fromParams->hasProperty (id), inB = toParams->hasProperty (id);

            if (! inA && ! inB)
                continue;

            // A parameter one preset does not name keeps the other's value.
            const double a = inA ? (double) fromParams->getProperty (id) : (double) toParams->getProperty (id);
            const double z = inB ? (double) toParams->getProperty (id) : a;

            double value = SnapshotBank::isDiscrete (*p) ? (side == slotB ? z : a)
                                                         : a + (z - a) * b;
            value = juce::jlimit (0.0, 1.0, value);

            if (std::abs (value - (double) withId->getValue()) > 1.0e-9)
                withId->setValueNotifyingHost ((float) value);
        }
    }

    appliedPosition = b;
}

} // namespace luthier
