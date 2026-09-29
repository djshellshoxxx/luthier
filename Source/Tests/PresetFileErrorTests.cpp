/*  SPEC-SWEEP: error-recovery.md 1 for a preset file - each failure is refused
    with its own sentence and leaves the current sound alone. ER-8, ER-9,
    ER-10, ER-12, ER-13, FF-2, PF-11. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::File scratchFile (const juce::String& name)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("luthier-preset-error-tests");
        folder.createDirectory();
        return folder.getNonexistentChildFile (name, PresetManager::kFileExtension, false);
    }

    juce::String preset (const juce::String& schemaField, double gain)
    {
        return juce::String ("{ \"magic\": \"") + PresetManager::kMagic + "\", " + schemaField
                 + " \"name\": \"Schema\", \"parameters\": { \"" + ParamIDs::masterGain + "\": "
                 + juce::String (gain) + " } }";
    }

    float masterGain (LuthierAudioProcessor& processor)
    {
        return processor.getState().getParameter (ParamIDs::masterGain)->getValue();
    }
}

//==============================================================================
LUTHIER_TEST (Presets, aNewerSchemaIsRefusedWithTheUpdateMessage)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();
    const float before = masterGain (processor);

    auto file = scratchFile ("Newer");
    file.replaceWithText (preset ("\"schema\": 99,", 0.123));

    CHECK_MSG (! presets.loadPreset (file), "a preset from a newer schema loaded");
    CHECK_MSG (presets.getLastLoadError().contains ("newer Luthier version. Update to open."),
               "the refusal did not say why: " + presets.getLastLoadError());
    CHECK_MSG (std::abs (masterGain (processor) - before) < 1.0e-6f, "a refused preset changed the sound");

    file.deleteFile();
}

LUTHIER_TEST (Presets, aSchemaBelowOneIsNoLongerSupported)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto file = scratchFile ("Ancient");
    file.replaceWithText (preset ("\"schemaVersion\": 0,", 0.2));

    CHECK (! presets.loadPreset (file));
    CHECK_MSG (presets.getLastLoadError().contains ("no longer supports"), presets.getLastLoadError());

    file.deleteFile();
}

//==============================================================================
/*  FF-2: `schema` is the field's name; a file with neither it nor the old
    `schemaVersion` is schema 1, and a save writes both. */
LUTHIER_TEST (Presets, aFileWithoutASchemaIsReadAsSchemaOne)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto file = scratchFile ("NoSchema");
    file.replaceWithText (preset ({}, 0.25));

    CHECK_MSG (presets.loadPreset (file), "a preset without a schema was refused: " + presets.getLastLoadError());
    CHECK_NEAR (masterGain (processor), 0.25f, 1.0e-4f);

    const auto saved = presets.toVar ("Saved");
    CHECK ((int) saved.getProperty ("schema", 0) == PresetManager::kSchemaVersion);
    CHECK ((int) saved.getProperty ("schemaVersion", 0) == PresetManager::kSchemaVersion);

    file.deleteFile();
}

//==============================================================================
LUTHIER_TEST (Presets, aLatin1FileIsRefusedAsNotUtf8)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto file = scratchFile ("Latin1");

    // "Café" in Latin-1: 0xE9 on its own is not UTF-8.
    const auto text = preset ("\"schemaVersion\": 1,", 0.3).replace ("Schema", "Caf_");
    juce::MemoryBlock bytes (text.toRawUTF8(), text.getNumBytesAsUTF8());
    const int at = text.indexOf ("Caf_") + 3;
    static_cast<char*> (bytes.getData())[at] = (char) 0xE9;
    file.replaceWithData (bytes.getData(), bytes.getSize());

    CHECK (! presets.loadPreset (file));
    CHECK_MSG (presets.getLastLoadError().contains ("is not a valid Luthier file"), presets.getLastLoadError());

    file.deleteFile();
}

LUTHIER_TEST (Presets, notJsonSuggestsReSaving)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto file = scratchFile ("NotJson");
    file.replaceWithText ("this is not json {");

    CHECK (! presets.loadPreset (file));
    CHECK_MSG (presets.getLastLoadError().contains ("Re-save it from a working install of Luthier."),
               presets.getLastLoadError());

    file.deleteFile();
}

#if JUCE_LINUX || JUCE_MAC
LUTHIER_TEST (Presets, anUnreadableFileSaysPermissionDenied)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto file = scratchFile ("Locked");
    file.replaceWithText (preset ("\"schemaVersion\": 1,", 0.3));
    file.setReadOnly (true);
    juce::ChildProcess chmod;
    chmod.start ("chmod 000 \"" + file.getFullPathName() + "\"");
    chmod.waitForProcessToFinish (5000);

    // Root reads anything, so there is nothing to test as root.
    if (file.hasReadAccess() && file.loadFileAsString().isNotEmpty())
    {
        file.deleteFile();
        return;
    }

    CHECK (! presets.loadPreset (file));
    CHECK_MSG (presets.getLastLoadError().contains ("permission denied"), presets.getLastLoadError());

    file.deleteFile();
}
#endif

//==============================================================================
/*  ER-19/20/21, FF-32, PF-5 (file-formats 13, error-recovery 2): a save that
    cannot be written, or cannot be renamed into place, leaves the old file
    intact, leaves no temp file, and says why. */
LUTHIER_TEST (Presets, aFailedSaveLeavesTheOldFileAndSaysWhy)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("luthier-atomic-save-test");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto target = folder.getChildFile ("Kept.luthierpreset");
    target.replaceWithText ("{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"old\"}");
    const auto before = target.loadFileAsString();

    for (int stage : { 1, 2 })
    {
        PresetManager::failNextWriteForTesting = stage;
        CHECK_MSG (! presets.exportPreset (target), "an injected write failure reported success");

        CHECK_MSG (target.loadFileAsString() == before,
                   "a failed save (stage " + juce::String (stage) + ") changed the file it was replacing");
        CHECK_MSG (presets.getLastSaveError().isNotEmpty(), "a failed save was silent");

        int leftovers = 0;

        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false))
            if (f != target)
                ++leftovers;

        CHECK_MSG (leftovers == 0, "a failed save left a partial file behind");
    }

    CHECK (presets.exportPreset (target));
    CHECK_MSG (presets.getLastSaveError().isEmpty(), "a good save left the last error standing");
    CHECK (target.loadFileAsString() != before);

    folder.deleteRecursively();
}

//==============================================================================
/*  PF-7 / FF-44 (file-formats 13): the sweep keeps 30 days, dated by folder
    name, including the backups a save files beside a category's presets. */
LUTHIER_TEST (Presets, backupPruningKeepsThirtyDays)
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("luthier-backup-prune-test");
    root.deleteRecursively();

    const auto now = juce::Time::getCurrentTime();
    const auto day = [&] (int daysAgo) { return (now - juce::RelativeTime::days (daysAgo)).formatted ("%Y-%m-%d"); };

    const auto old1 = root.getChildFile ("Backup").getChildFile (day (40));
    const auto new1 = root.getChildFile ("Backup").getChildFile (day (2));
    const auto old2 = root.getChildFile ("Rock").getChildFile ("Backup").getChildFile (day (60));
    const auto new2 = root.getChildFile ("Rock").getChildFile ("Backup").getChildFile (day (29));

    for (auto f : { old1, new1, old2, new2 })
        f.getChildFile ("a.luthierpreset").create();

    PresetManager::pruneOldBackupsUnder (root, now);

    CHECK_MSG (! old1.exists(), "a 40-day-old backup survived the sweep");
    CHECK_MSG (! old2.exists(), "a category folder's 60-day-old backup survived the sweep");
    CHECK_MSG (new1.exists() && new2.exists(), "the sweep deleted a backup younger than 30 days");

    root.deleteRecursively();
}

//==============================================================================
/*  ER-38 (error-recovery 6): an arm with no MIDI for 30 s cancels itself. */
LUTHIER_TEST (MidiLearn, armingTimesOutAfterThirtySeconds)
{
    LuthierAudioProcessor processor;
    auto& learn = processor.getMidiLearn();

    learn.setArmed (true);
    const auto start = juce::Time::getMillisecondCounter();

    CHECK_MSG (! learn.expireIfIdle (start + 1000), "the arm expired after one second");
    CHECK (learn.isArmed());

    CHECK_MSG (learn.expireIfIdle (start + MidiLearnManager::kArmTimeoutMs + 100),
               "the arm did not expire after thirty seconds");
    CHECK (! learn.isArmed());
    CHECK (! learn.isLearning());

    // A learn claimed from the arm times out the same way.
    learn.setArmed (true);
    CHECK (learn.claimArmedLearn (ParamIDs::ampGain));
    CHECK (learn.expireIfIdle (juce::Time::getMillisecondCounter() + MidiLearnManager::kArmTimeoutMs + 100));
    CHECK (! learn.isLearning());
}

//==============================================================================
/*  ER-54 (error-recovery 9): switching to an empty B says so. */
LUTHIER_TEST (StateModel, anEmptyBSlotSaysSo)
{
    LuthierAudioProcessor processor;
    processor.takeStateNotices();

    processor.setSlotBActive (true);
    CHECK (processor.takeStateNotices().contains ("B slot is empty; save current state to B first."));

    processor.setSlotBActive (false);
    processor.setSlotBActive (true);   // B holds something now
    CHECK (processor.takeStateNotices().isEmpty());
}
