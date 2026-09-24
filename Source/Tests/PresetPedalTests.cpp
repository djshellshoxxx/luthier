/*  A preset's pedals come with their settings.

    Found by the preset-morph tests: on a fresh processor, loading a preset
    wrote its pedal types and parameters, and then the structural pass built
    each new pedal and wrote its defaults over the parameters the preset had
    just set - so every factory preset's pedals played at their defaults the
    first time it was loaded. The bridge now keeps a pedal's parameters when
    they were written after its type, and writes the defaults only for a type
    written on its own (a player picking a pedal). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    void loadFactory (LuthierAudioProcessor& processor, const juce::String& name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
                processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), processor));

        processor.getParameterBridge().applyAllNow();
    }

    double plain (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* p = processor.getState().getParameter (id);
        return p != nullptr ? (double) p->getValue() : -1.0;
    }
}

//==============================================================================
LUTHIER_TEST (PresetPedals, aFreshLoadKeepsThePresetsPedalSettings)
{
    // Violin Bass Grind: an overdrive in pre slot 0 at 0.22 / 0.50 / 0.55 / 0.50 / 2.0.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    loadFactory (processor, "Violin Bass Grind");

    auto* pedal = processor.getEngine().getPreEffects().getPedal (0);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Overdrive);

    if (pedal == nullptr)
        return;

    const double expected[] = { 0.22, 0.50, 0.55, 0.50, 2.0 };

    for (int i = 0; i < 5; ++i)
        CHECK_MSG (std::abs (pedal->getParameterValue (i) - expected[i]) < 1.0e-3,
                   "overdrive parameter " + juce::String (i) + " is " + juce::String (pedal->getParameterValue (i), 3)
                     + ", not the preset's " + juce::String (expected[i], 3));
}

LUTHIER_TEST (PresetPedals, pickingAPedalStillStartsItAtItsDefaults)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    // Something odd left in the slot's parameters from before.
    for (int i = 0; i < Pedal::kMaxParams; ++i)
        if (auto* p = processor.getState().getParameter (ParamIDs::slotParam (true, 3, i)))
            p->setValueNotifyingHost (0.93f);

    // The player picks a Delay: the type alone is written.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 3)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Delay));

    processor.getParameterBridge().applyAllNow();

    auto* pedal = processor.getEngine().getPostEffects().getPedal (3);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Delay);

    if (pedal == nullptr)
        return;

    for (int i = 0; i < pedal->getNumParameters(); ++i)
    {
        const auto& d = pedal->getParameterDescriptor (i);
        CHECK_MSG (std::abs (pedal->getParameterValue (i) - d.defaultValue) < 1.0e-3,
                   juce::String (d.name) + " is " + juce::String (pedal->getParameterValue (i), 3)
                     + ", not its default " + juce::String (d.defaultValue, 3));
        CHECK (std::abs (plain (processor, ParamIDs::slotParam (true, 3, i)) - d.toNormalised (d.defaultValue)) < 1.0e-4);
    }
}

LUTHIER_TEST (PresetPedals, aSnapshotThatChangesAPedalKeepsItsSettings)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    // Snapshot 0: a chorus in post slot 5 with its rate turned right up.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 5)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Chorus));

    processor.getParameterBridge().applyAllNow();

    if (auto* rate = processor.getState().getParameter (ParamIDs::slotParam (true, 5, 0)))
        rate->setValueNotifyingHost (0.97f);

    processor.getSnapshots().capture (0, "Chorus");

    // Then a different pedal there, and back to the snapshot.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 5)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Phaser));

    processor.getParameterBridge().applyAllNow();
    processor.recallSnapshot (0);

    // Past the recall's crossfade, then the structural pass.
    processor.getSnapshots().advance (5.0);
    processor.getParameterBridge().applyAllNow();

    auto* pedal = processor.getEngine().getPostEffects().getPedal (5);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Chorus);
    CHECK_MSG (std::abs (plain (processor, ParamIDs::slotParam (true, 5, 0)) - 0.97) < 1.0e-3,
               "the recalled chorus's rate is " + juce::String (plain (processor, ParamIDs::slotParam (true, 5, 0)), 3));
}
