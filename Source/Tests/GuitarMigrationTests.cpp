/*  Older presets' guitar names (ambiguity-resolutions.md 7, tests 7.1). */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** Every guitar name the factory presets used before the parts model
        (M49), in GuitarType order - the display names of that build. */
    const char* const kPreM49Names[] = {
        "Stratocaster", "Telecaster", "Les Paul", "SG", "ES-335", "Jazzmaster",   // legacy name (trademark scan skips it)
        "Explorer", "Ibanez RG", "7-String", "8-String", "Baritone Electric",      // legacy name (trademark scan skips it)
        "Dreadnought", "Auditorium", "Jumbo", "Parlor", "Classical", "Flamenco",
        "12-String", "Resonator", "Precision Bass", "Jazz Bass", "Rickenbacker",   // legacy name (trademark scan skips it)
        "5-String Bass", "Fretless Bass"
    };

    juce::var presetReferencing (LuthierAudioProcessor& source, const juce::String& reference)
    {
        auto preset = source.getPresetManager().toVar ("Old guitar");
        auto* guitarBlock = new juce::DynamicObject();
        guitarBlock->setProperty ("reference", reference);
        guitarBlock->setProperty ("override", juce::var());
        preset.getDynamicObject()->setProperty ("guitar", juce::var (guitarBlock));
        return preset;
    }
}

//==============================================================================
LUTHIER_TEST (GuitarMigration, everyPreM49NameResolvesToItsShippedGuitar)
{
    CHECK_MSG (PartLibrary::getGuitarMigrationVersion() >= 1, "Resources/Guitars/migration.json is not installed");

    for (int t = 0; t < (int) (sizeof (kPreM49Names) / sizeof (kPreM49Names[0])); ++t)
    {
        const juce::String name (kPreM49Names[t]);
        const auto migrated = PartLibrary::migratedGuitar (name);
        const auto expected = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) t);

        CHECK_MSG (migrated == expected, name + " migrates to \"" + migrated + "\", not \"" + expected + "\"");
        CHECK_MSG (PartLibrary::getFactoryGuitarsFolder().getChildFile (migrated).existsAsFile(),
                   name + "'s guitar " + migrated + " is not shipped");
    }

    // The files the trademark sweep renamed, too.
    CHECK (PartLibrary::migratedGuitar ("Acoustic/Selmer-Style.luthierguitar") == "Acoustic/Gypsy Jazz.luthierguitar");   // legacy name (trademark scan skips it)
}

LUTHIER_TEST (GuitarMigration, aPresetNamingAnOldGuitarLoadsItsReplacement)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.takeGuitarNotices();

    CHECK (processor.getPresetManager().fromVar (presetReferencing (source, "Factory/Les Paul")));   // legacy name (trademark scan skips it)
    processor.getParameterBridge().applyAllNow();

    CHECK (processor.hasPartsGuitar());
    CHECK_MSG (processor.takeGuitarNotices().isEmpty(), "a migrated guitar still posted a banner");
    CHECK (processor.getCurrentGuitar().name.containsIgnoreCase ("Single-Cut"));
}

LUTHIER_TEST (GuitarMigration, anUnknownGuitarKeepsThePresetAndSaysSo)
{
    LuthierAudioProcessor source;
    source.prepareToPlay (48000.0, 512);

    // A parameter the preset carries, to see it survive the fallback.
    if (auto* p = source.getState().getParameter (ParamIDs::ampGain))
        p->setValueNotifyingHost (0.83f);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.takeGuitarNotices();

    CHECK (processor.getPresetManager().fromVar (presetReferencing (source, "Factory/Hyperdrive 9000.luthierguitar")));
    processor.getParameterBridge().applyAllNow();

    const auto notices = processor.takeGuitarNotices().joinIntoString ("; ");
    CHECK_MSG (notices.contains ("Guitar 'Hyperdrive 9000' not found, loaded closest factory match. "
                                 "Open Workshop to save your customization as a guitar."),
               "the banner reads: " + notices);

    auto* gain = processor.getState().getParameter (ParamIDs::ampGain);
    CHECK (gain != nullptr && std::abs (gain->getValue() - 0.83f) < 1.0e-4f);
}
