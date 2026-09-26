/*  SPEC-SWEEP (state): tests for rows of docs/PRESET_FORMAT.md, error-recovery
    and factory-content that the code already met but nothing checked.
    PF-2, PF-3, PF-12, PF-19, ER-53, ER-78, FC-23. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Support/ErrorLog.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  PF-19: the MIDI map stores targets as integers, so the enum's values are
    part of the file format and must never move. */
LUTHIER_TEST (Presets, midiTargetValuesAreStable)
{
    CHECK ((int) MidiTarget::None == 0);
    CHECK ((int) MidiTarget::VibratoDepth == 1);
    CHECK ((int) MidiTarget::VibratoRate == 2);
    CHECK ((int) MidiTarget::WhammyBar == 3);
    CHECK ((int) MidiTarget::Expression == 4);
    CHECK ((int) MidiTarget::MasterLevel == 5);
    CHECK ((int) MidiTarget::PalmMute == 6);
    CHECK ((int) MidiTarget::MutedPick == 7);
    CHECK ((int) MidiTarget::PickPosition == 8);
    CHECK ((int) MidiTarget::SlideToggle == 9);
    CHECK ((int) MidiTarget::SlideGuitarToggle == 10);
    CHECK ((int) MidiTarget::PinchHarmonic == 11);
    CHECK ((int) MidiTarget::NaturalHarmonic == 12);
    CHECK ((int) MidiTarget::Tap == 13);
    CHECK ((int) MidiTarget::StrumSpeed == 14);
    CHECK ((int) MidiTarget::StrumDirection == 15);
    CHECK ((int) MidiTarget::Humanize == 16);
    CHECK ((int) MidiTarget::Drive == 17);
    CHECK ((int) MidiTarget::Tone == 18);
    CHECK ((int) MidiTarget::Space == 19);
    CHECK ((int) MidiTarget::Body == 20);
    CHECK ((int) MidiTarget::Attack == 21);
}

//==============================================================================
/*  PF-2: the folders are where the doc says. */
LUTHIER_TEST (Presets, folderLayoutMatchesTheDoc)
{
    const auto user = PresetManager::getUserPresetFolder().getFullPathName().replaceCharacter ('\\', '/');
    CHECK_MSG (user.endsWith ("Luthier/Presets/User"), "the user preset folder is " + user);

    const auto factory = PresetManager::getFactoryPresetFolder().getFullPathName().replaceCharacter ('\\', '/');
    CHECK_MSG (factory.endsWith ("Presets/Factory"), "the factory preset folder is " + factory);
}

//==============================================================================
/*  PF-3, PF-12: an added folder is scanned; its sub-folder is the category
    unless the file names one, and a file at its root is "User". */
LUTHIER_TEST (Presets, anAddedFolderIsScannedAndItsSubfolderIsTheCategory)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("luthier-added-preset-folder");
    folder.deleteRecursively();

    const juce::String magic (PresetManager::kMagic);

    folder.getChildFile ("Rock").getChildFile ("Sweep Folder Rock.luthierpreset").create();
    folder.getChildFile ("Rock").getChildFile ("Sweep Folder Rock.luthierpreset")
          .replaceWithText ("{ \"magic\": \"" + magic + "\", \"schema\": 1, \"name\": \"Sweep Folder Rock\" }");
    folder.getChildFile ("Sweep Folder Root.luthierpreset")
          .replaceWithText ("{ \"magic\": \"" + magic + "\", \"schema\": 1, \"name\": \"Sweep Folder Root\" }");
    folder.getChildFile ("Rock").getChildFile ("Sweep Folder Blues.luthierpreset")
          .replaceWithText ("{ \"magic\": \"" + magic + "\", \"schema\": 1, \"name\": \"Sweep Folder Blues\","
                            " \"category\": \"Blues\", \"tags\": [\"slow\", \"minor\"],"
                            " \"description\": \"twelve bars\" }");

    presets.addSearchFolder (folder);
    presets.refresh();

    auto find = [&] (const juce::String& name) -> const PresetInfo*
    {
        for (int i = 0; i < presets.getNumPresets(); ++i)
            if (presets.getPreset (i)->name == name)
                return presets.getPreset (i);

        return nullptr;
    };

    const auto* rock = find ("Sweep Folder Rock");
    const auto* root = find ("Sweep Folder Root");
    const auto* blues = find ("Sweep Folder Blues");

    CHECK_MSG (rock != nullptr && rock->category == "Rock", "a preset's sub-folder was not its category");
    CHECK_MSG (root != nullptr && root->category == "User", "a preset at an added folder's root was not filed under User");
    CHECK_MSG (blues != nullptr && blues->category == "Blues", "the file's category did not override the folder");

    if (blues != nullptr)
    {
        CHECK (blues->tags.contains ("minor"));
        CHECK (blues->description == "twelve bars");
    }

    presets.removeSearchFolder (folder);
    folder.deleteRecursively();
}

//==============================================================================
/*  ER-53 (error-recovery 9): with nothing to undo, undo is a no-op. */
LUTHIER_TEST (Undo, nothingToUndoIsANoOp)
{
    LuthierAudioProcessor processor;

    CHECK (! processor.canUndo());

    juce::MemoryBlock before, after;
    processor.getStateInformation (before);
    processor.undo();
    processor.getStateInformation (after);

    CHECK_MSG (before == after, "undo with an empty stack changed the state");
    CHECK (! processor.canUndo());
}

//==============================================================================
/*  ER-78 (error-recovery 12): the log rotates by month, in its file name. */
LUTHIER_TEST (ErrorLog, theFileNameFollowsTheMonth)
{
    const juce::Time april (2026, 3, 15, 12, 0);
    const juce::Time may (2026, 4, 1, 0, 30);

    CHECK_MSG (ErrorLog::getLogFile (april).getFileName() == "errors-202604.log",
               ErrorLog::getLogFile (april).getFileName());
    CHECK_MSG (ErrorLog::getLogFile (may).getFileName() == "errors-202605.log",
               ErrorLog::getLogFile (may).getFileName());
}

//==============================================================================
/*  FC-23 (factory-content 13): every factory preset loads through the real
    processor without a missing-guitar, missing-part or missing-IR notice. */
LUTHIER_TEST (Presets, everyFactoryPresetLoadsWithoutAMissingReference)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& presets = processor.getPresetManager();
    processor.takeGuitarNotices();

    int factory = 0;

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto* info = presets.getPreset (i);

        if (info == nullptr || ! info->isFactory)
            continue;

        ++factory;
        CHECK_MSG (presets.loadPreset (i), info->name + " failed to load: " + presets.getLastLoadError());
        processor.getParameterBridge().applyAllNow();

        const auto notices = processor.takeGuitarNotices();
        CHECK_MSG (notices.isEmpty(), info->name + " posted: " + notices.joinIntoString ("; "));
        CHECK_MSG (processor.getBodyIrSlot().getLastError().isEmpty()
                     && processor.getCabIrSlot (0).getLastError().isEmpty()
                     && processor.getCabIrSlot (1).getLastError().isEmpty(),
                   info->name + " asked for an IR that is not there");
    }

    CHECK_MSG (factory > 0, "no factory presets were found, so this proves nothing");
}

//==============================================================================
/*  SM-47 (state-model.md 8.1): a preset load clears Freeze and the E-Bow. Both
    are parameters, and a preset that does not switch them on - every factory
    preset, and any file that leaves the keys out (PF-14) - switches them off. */
LUTHIER_TEST (StateModel, aLoadClearsFreezeAndEBow)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 1)
        return;

    auto& state = processor.getState();
    state.getParameter (ParamIDs::freezeEnable)->setValueNotifyingHost (1.0f);
    state.getParameter (ParamIDs::ebowEnable)->setValueNotifyingHost (1.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.8f), 0);

    for (int i = 0; i < 8; ++i)
    {
        processor.processBlock (buffer, midi);
        midi.clear();
    }

    CHECK (presets.loadPreset (0));

    for (int i = 0; i < 8; ++i)
        processor.processBlock (buffer, midi);

    CHECK_MSG (state.getParameter (ParamIDs::freezeEnable)->getValue() < 0.5f, "Freeze survived a preset load");
    CHECK_MSG (state.getParameter (ParamIDs::ebowEnable)->getValue() < 0.5f, "the E-Bow survived a preset load");
    CHECK_MSG (! processor.getEngine().isEBowing(), "the engine is still driving the E-Bow after a load");
}

//==============================================================================
/*  FF-20 (file-formats 2): the `meta` block is written, read first, and a save
    keeps the file's `created`. */
LUTHIER_TEST (Presets, metaBlockRoundTripsAndKeepsCreated)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-meta-test");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto file = folder.getChildFile ("Meta.luthierpreset");
    file.replaceWithText (juce::String ("{ \"magic\": \"") + PresetManager::kMagic + "\", \"schema\": 1,"
                          " \"name\": \"flat name\", \"meta\": { \"name\": \"Meta Name\", \"category\": \"Blues\","
                          " \"created\": \"2026-01-14T09:32:00Z\", \"notes\": \"hand-written\" } }");

    // A file load names the preset after its file (as before); the state
    // itself reads meta.name first.
    CHECK (presets.loadPreset (file));
    CHECK (presets.fromVar (juce::JSON::parse (file)));
    CHECK_MSG (presets.getCurrentPresetName() == "Meta Name", "meta.name was not read first");

    const auto out = folder.getChildFile ("Exported.luthierpreset");
    CHECK (presets.exportPreset (out));

    const auto saved = juce::JSON::parse (out);
    const auto meta = saved.getProperty ("meta", {});

    CHECK_MSG (meta.getProperty ("created", {}).toString() == "2026-01-14T09:32:00Z", "a save lost the file's created time");
    CHECK_MSG (meta.getProperty ("modified", {}).toString().isNotEmpty(), "a save did not stamp modified");
    CHECK (meta.getProperty ("notes", {}).toString() == "hand-written");
    CHECK (meta.getProperty ("category", {}).toString() == "Blues");
    CHECK (saved.getProperty ("name", {}).toString() == "Meta Name");   // the flat key too

    folder.deleteRecursively();
}

//==============================================================================
/*  FF-5, FF-35: a setlist writes its magic, a file without one is refused
    with a reason, and the legacy `format` marker still loads. SM-31: entries
    whose preset is missing are flagged. */
LUTHIER_TEST (LiveSetlist, aFileWithoutTheMagicIsRefused)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-setlist-magic-test");
    folder.deleteRecursively();
    folder.createDirectory();

    Setlist list;
    list.setName ("Gig");
    SetlistEntry entry;
    entry.presetPath = folder.getChildFile ("There.luthierpreset").getFullPathName();
    folder.getChildFile ("There.luthierpreset").create();
    list.addEntry (entry);

    const auto good = folder.getChildFile ("Gig.luthierset");
    CHECK (list.saveTo (good));
    CHECK (juce::JSON::parse (good).getProperty ("magic", {}).toString() == Setlist::kMagic);

    Setlist loaded;
    CHECK (loaded.loadFrom (good));
    CHECK (loaded.getLoadError().isEmpty());

    const auto legacy = folder.getChildFile ("Legacy.luthierset");
    legacy.replaceWithText ("{ \"format\": \"luthierset\", \"name\": \"Old\", \"entries\": [] }");
    CHECK_MSG (loaded.loadFrom (legacy), "a setlist with the legacy marker was refused");

    const auto foreign = folder.getChildFile ("Foreign.luthierset");
    foreign.replaceWithText ("{ \"name\": \"Not ours\", \"entries\": [] }");
    CHECK_MSG (! loaded.loadFrom (foreign), "a JSON file without the setlist marker loaded");
    CHECK (loaded.getLoadError().contains ("not a Luthier setlist"));
    CHECK_MSG (loaded.getName() == "Old", "a refused setlist replaced the loaded one");

    const auto garbage = folder.getChildFile ("Garbage.luthierset");
    garbage.replaceWithText ("not json at all");
    CHECK (! loaded.loadFrom (garbage));
    CHECK (loaded.getLoadError().isNotEmpty());

    folder.deleteRecursively();
}

LUTHIER_TEST (LiveSetlist, missingEntriesAreFlagged)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-setlist-resolve-test");
    folder.deleteRecursively();
    folder.createDirectory();

    folder.getChildFile ("Here.luthierpreset").create();

    Setlist list;
    SetlistEntry here, gone;
    here.presetPath = folder.getChildFile ("Here.luthierpreset").getFullPathName();
    gone.presetPath = folder.getChildFile ("Gone.luthierpreset").getFullPathName();
    list.addEntry (here);
    list.addEntry (gone);

    const auto file = folder.getChildFile ("Set.luthierset");
    CHECK (list.saveTo (file));

    Setlist loaded;
    CHECK (loaded.loadFrom (file));
    CHECK (loaded.getEntry (0).resolved);
    CHECK_MSG (! loaded.getEntry (1).resolved, "a missing preset was not flagged");
    CHECK (loaded.getNumUnresolvedEntries() == 1);

    folder.deleteRecursively();
}

//==============================================================================
/*  FF-35 / SM-31: loading a setlist that is refused, or that has entries whose
    preset is gone, raises a warning for the window. */
LUTHIER_TEST (LiveSetlist, aCorruptOrIncompleteSetlistRaisesABanner)
{
    LuthierAudioProcessor processor;
    processor.takeStateWarnings();

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-setlist-banner-test");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto corrupt = folder.getChildFile ("Corrupt.luthierset");
    corrupt.replaceWithText ("{ not json");

    CHECK (! processor.loadSetlist (corrupt));
    CHECK_MSG (processor.takeStateWarnings().size() == 1, "a refused setlist was silent");

    Setlist list;
    SetlistEntry gone;
    gone.presetPath = folder.getChildFile ("Gone.luthierpreset").getFullPathName();
    list.addEntry (gone);

    const auto incomplete = folder.getChildFile ("Incomplete.luthierset");
    CHECK (list.saveTo (incomplete));

    processor.loadSetlist (incomplete);
    const auto warnings = processor.takeStateWarnings();
    CHECK_MSG (warnings.size() == 1 && warnings[0].contains ("marked missing"),
               "a setlist with a missing preset was silent");
    CHECK (! processor.getSetlist().getSetlist().getEntry (0).resolved);

    folder.deleteRecursively();
}

//==============================================================================
/*  PF-4: saving over a factory preset makes a user copy and leaves the factory
    file alone. */
LUTHIER_TEST (Presets, savingAFactoryPresetMakesAUserCopy)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    int factory = -1;

    for (int i = 0; i < presets.getNumPresets() && factory < 0; ++i)
        if (presets.getPreset (i)->isFactory)
            factory = i;

    if (factory < 0)
    {
        CHECK_MSG (false, "no factory preset to save over");
        return;
    }

    const auto factoryFile = presets.getPreset (factory)->file;
    const auto factoryText = factoryFile.loadFileAsString();
    const auto name = presets.getPreset (factory)->name;

    // Never touch a user preset that was already there.
    const auto expected = PresetManager::getUserPresetFolder().getChildFile ("User")
                            .getChildFile (juce::File::createLegalFileName (name) + PresetManager::kFileExtension);

    if (expected.exists())
        return;

    CHECK (presets.loadPreset (factory));
    CHECK (presets.saveCurrent());

    CHECK_MSG (factoryFile.loadFileAsString() == factoryText, "saving a factory preset rewrote the factory file");

    const auto* current = presets.getPreset (presets.getCurrentPresetIndex());
    CHECK_MSG (current != nullptr && ! current->isFactory
                 && current->file.isAChildOf (PresetManager::getUserPresetFolder()),
               "the save did not become a user preset");

    expected.deleteFile();
}
