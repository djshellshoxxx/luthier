/*  file-formats.md 2: a load that migrates a preset keeps the original in
    Presets/Backup/<date>/<name>-v<schema>.luthierpreset. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::File scratchFolder()
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("LuthierMigrationTest-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        folder.createDirectory();
        return folder;
    }

    void writePreset (const juce::File& file, const juce::var& data)
    {
        file.replaceWithText (juce::JSON::toString (data, false));
    }
}

//==============================================================================
LUTHIER_TEST (PresetMigration, aLoadThatMigratesBacksUpTheOriginalBesideIt)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();
    const auto folder = scratchFolder();
    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");

    // A file this build wrote: nothing to migrate, so nothing is backed up.
    const auto current = folder.getChildFile ("Current.luthierpreset");
    writePreset (current, presets.toVar ("Current", "Test"));

    CHECK_MSG (presets.loadPreset (current), presets.getLastLoadError());
    CHECK (! presets.didLastLoadMigrate());
    CHECK_MSG (! folder.getChildFile ("Backup").exists(), "an up-to-date preset was backed up on load");

    // A file from before file-formats.md named the marker: `format` instead
    // of `magic`, read and rewritten as `magic` on the next save. Migrated.
    auto legacy = presets.toVar ("Old Sound", "Test");

    if (auto* obj = legacy.getDynamicObject())
    {
        obj->removeProperty ("magic");
        obj->setProperty ("format", PresetManager::kLegacyMagic);
    }

    const auto legacyFile = folder.getChildFile ("Old Sound.luthierpreset");
    writePreset (legacyFile, legacy);
    const auto original = legacyFile.loadFileAsString();

    CHECK_MSG (presets.loadPreset (legacyFile), presets.getLastLoadError());
    CHECK (presets.didLastLoadMigrate());

    const auto backup = folder.getChildFile ("Backup").getChildFile (today)
                              .getChildFile ("Old Sound-v" + juce::String (PresetManager::kSchemaVersion) + ".luthierpreset");
    CHECK_MSG (backup.existsAsFile(), "no backup at " + backup.getFullPathName());
    CHECK_MSG (backup.loadFileAsString() == original, "the backup is not the original bytes");

    // The loaded file itself is untouched: a load does not write.
    CHECK (legacyFile.existsAsFile() && legacyFile.loadFileAsString() == original);

    // Loading the same original again that day keeps the one copy.
    CHECK (presets.loadPreset (legacyFile));
    CHECK_MSG (folder.getChildFile ("Backup").getChildFile (today).getNumberOfChildFiles (juce::File::findFiles) == 1,
               "a second load of the same original made a second backup");

    // The backup folder is not a preset folder: a refresh does not list it.
    presets.addSearchFolder (folder);
    presets.refresh();
    bool listedBackup = false;

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i))
            listedBackup = listedBackup || info->file == backup;

    CHECK_MSG (! listedBackup, "the migration backup showed up in the preset list");
    presets.removeSearchFolder (folder);

    folder.deleteRecursively();
}

LUTHIER_TEST (PresetMigration, aRetiredParameterCarriedOverCountsAsAMigration)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();
    const auto folder = scratchFolder();
    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");

    // ambiguity-resolutions 1: feedback_on with no feedback_amount becomes an amount.
    auto old = presets.toVar ("Switched On", "Test");

    if (auto* params = old.getProperty ("parameters", {}).getDynamicObject())
    {
        params->removeProperty (ParamIDs::feedbackAmount);
        params->setProperty (ParamIDs::feedbackOn, 1.0);
    }

    const auto file = folder.getChildFile ("Switched On.luthierpreset");
    writePreset (file, old);

    CHECK_MSG (presets.loadPreset (file), presets.getLastLoadError());
    CHECK (presets.didLastLoadMigrate());
    CHECK (folder.getChildFile ("Backup").getChildFile (today)
                 .getChildFile ("Switched On-v" + juce::String (PresetManager::kSchemaVersion) + ".luthierpreset").existsAsFile());

    // And the direct helper says what it made.
    const auto again = PresetManager::backupMigratedOriginal (file, 7);
    CHECK (again.existsAsFile() && again.getFileName() == "Switched On-v7.luthierpreset");
    CHECK (PresetManager::backupMigratedOriginal (folder.getChildFile ("missing.luthierpreset"), 1) == juce::File());

    folder.deleteRecursively();
}
