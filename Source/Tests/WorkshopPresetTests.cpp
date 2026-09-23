/*  The guitar in presets and state: guitar-workshop.md 6-8, file-formats.md 2. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    float plainValue (LuthierAudioProcessor& p, const char* id)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    void setPlain (LuthierAudioProcessor& p, const char* id, float value)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    /** A pickup part other than the one fitted in `slot`. */
    PartPtr anotherPickup (LuthierAudioProcessor& p, GuitarSlot slot)
    {
        const auto fitted = p.getCurrentGuitar().get (slot);

        for (const auto& part : p.getPartLibrary().getParts (PartType::pickup))
            if (fitted == nullptr || part->name != fitted->name)
                return part;

        return nullptr;
    }

    juce::MemoryBlock stateOf (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }
}

//==============================================================================
LUTHIER_TEST (WorkshopPresets, aFreshInstanceNamesItsFactoryGuitar)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.hasPartsGuitar());
    CHECK_MSG (processor.getGuitarReference().startsWith ("Factory/"),
               "reference is \"" + processor.getGuitarReference() + "\"");
    CHECK (! processor.isGuitarEdited());

    const auto block = processor.getGuitarBlock();
    CHECK (block.getProperty ("override", juce::var ("absent")).isVoid());
}

LUTHIER_TEST (WorkshopPresets, anEditedGuitarTravelsWholeInTheState)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    auto guitar = source.getCurrentGuitar();
    const auto other = anotherPickup (source, GuitarSlot::pickupBridge);
    CHECK (other != nullptr);

    guitar.parts[(size_t) GuitarSlot::pickupBridge] = other;
    source.applyEditedGuitar (guitar);

    CHECK (source.isGuitarEdited());

    const auto state = stateOf (source);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.isGuitarEdited());
    CHECK_MSG (restored.getCurrentGuitar() == source.getCurrentGuitar(), "the edited guitar came back different");
    CHECK (restored.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name == other->name);
}

LUTHIER_TEST (WorkshopPresets, aStateLoadKeepsItsOwnRefinements)
{
    // The parameters that overlap parts are refinements saved with the preset;
    // loading the preset, or re-applying everything, must not reset them to the
    // guitar's own values.
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    setPlain (source, ParamIDs::setupActionTreble, 2.4f);
    source.getParameterBridge().applyAllNow();
    CHECK_MSG (std::abs (plainValue (source, ParamIDs::setupActionTreble) - 2.4f) < 0.01f,
               "a full apply reset the action to " + juce::String (plainValue (source, ParamIDs::setupActionTreble)));

    const auto state = stateOf (source);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_MSG (std::abs (plainValue (restored, ParamIDs::setupActionTreble) - 2.4f) < 0.01f,
               "the restored action is " + juce::String (plainValue (restored, ParamIDs::setupActionTreble)));
}

LUTHIER_TEST (WorkshopPresets, choosingAGuitarTypeFitsItsParts)
{
    // The user picking a type is not a state load: the guitar's own values are
    // written, so the controls show the guitar that is playing.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setPlain (processor, ParamIDs::setupActionTreble, 2.9f);

    auto* type = processor.getState().getParameter (ParamIDs::guitarType);
    type->setValueNotifyingHost (type->convertTo0to1 ((float) GuitarType::Classical));
    processor.getParameterBridge().applyAllNow();

    CHECK (processor.getGuitarReference().contains ("Classical"));
    CHECK_MSG (std::abs (plainValue (processor, ParamIDs::setupActionTreble)
                         - (float) processor.getCurrentGuitar().setup.actionTrebleMm) < 0.01f,
               "action treble reads " + juce::String (plainValue (processor, ParamIDs::setupActionTreble)));
}

LUTHIER_TEST (WorkshopPresets, aMissingGuitarFileFallsBackToItsType)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    auto preset = source.getPresetManager().toVar ("Missing guitar");
    auto* guitarBlock = new juce::DynamicObject();
    guitarBlock->setProperty ("reference", "User/No Such Guitar 9431.luthierguitar");
    guitarBlock->setProperty ("override", juce::var());
    preset.getDynamicObject()->setProperty ("guitar", juce::var (guitarBlock));

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.takeGuitarNotices();

    CHECK (processor.getPresetManager().fromVar (preset));
    processor.getParameterBridge().applyAllNow();

    CHECK (processor.hasPartsGuitar());

    const auto notices = processor.takeGuitarNotices();
    CHECK_MSG (notices.joinIntoString (";").contains ("No Such Guitar 9431"),
               "notices: " + notices.joinIntoString ("; "));
}

LUTHIER_TEST (WorkshopPresets, saveAsGuitarWritesAFileAndPointsThePresetAtIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto guitar = processor.getCurrentGuitar();
    guitar.parts[(size_t) GuitarSlot::pickupBridge] = anotherPickup (processor, GuitarSlot::pickupBridge);
    processor.applyEditedGuitar (guitar);

    const juce::String name = "Luthier Test Guitar " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    const auto file = processor.saveGuitarAs (name);

    CHECK (file.existsAsFile());
    CHECK (! processor.isGuitarEdited());
    CHECK (processor.getGuitarReference() == "User/" + file.getFileName());

    // A new instance loading this state finds the guitar by its reference.
    const auto state = stateOf (processor);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name
           == processor.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name);
    CHECK (! restored.isGuitarEdited());

    file.deleteFile();
}

LUTHIER_TEST (WorkshopPresets, saveAsPartMakesAUserPartAndFitsIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const juce::String name = "Luthier Test Pickup " + juce::String (juce::Random::getSystemRandom().nextInt (1000000));
    const auto saved = processor.savePartAs (GuitarSlot::pickupBridge, name);

    CHECK (saved != nullptr);

    if (saved != nullptr)
    {
        CHECK (! saved->isFactory);
        CHECK (saved->file.existsAsFile());
        CHECK (processor.getCurrentGuitar().get (GuitarSlot::pickupBridge)->name == name);
        CHECK (processor.getPartLibrary().find (PartType::pickup, name) != nullptr);

        saved->file.deleteFile();
        processor.getPartLibrary().refresh();
    }
}
