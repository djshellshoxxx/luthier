/*  SPEC-SWEEP (state): tests for rows of docs/PRESET_FORMAT.md, error-recovery
    and factory-content that the code already met but nothing checked.
    PF-2, PF-3, PF-12, PF-19, ER-53, ER-78, FC-23. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Support/ErrorLog.h"
#include "../UI/UiPreferences.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/GenreKit.h"
#include "../Presets/FactoryPresets.h"

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

//==============================================================================
/*  ER-65/66 (error-recovery 10): a preferences file that does not parse, or
    names a newer schema, is set aside and the defaults used, once, with a
    notice for the window. */
LUTHIER_TEST (UiPreferences, aCorruptFileIsBackedUpAndReset)
{
    const auto file = UiPreferences::getConfigFile();
    const bool existed = file.existsAsFile();
    const auto original = existed ? file.loadFileAsString() : juce::String();

    auto asideCount = [&]
    {
        return file.getParentDirectory().findChildFiles (juce::File::findFiles, false,
                                                         file.getFileName() + ".corrupted-*").size();
    };

    const int before = asideCount();
    auto& prefs = UiPreferences::get();

    for (const auto* text : { "{ this is not json", "{ \"schema\": 99, \"someKey\": 5 }" })
    {
        file.getParentDirectory().createDirectory();
        file.replaceWithText (text);
        prefs.takeCorruptionNotice();

        CHECK (! prefs.load());
        CHECK_MSG (prefs.takeCorruptionNotice(), juce::String ("no notice for: ") + text);
        CHECK_MSG (! prefs.takeCorruptionNotice(), "the notice was given twice");
        CHECK_MSG (! file.existsAsFile(), "the corrupt file was left in place");
        CHECK (prefs.getInt ("someKey", -1) == -1);
    }

    CHECK_MSG (asideCount() == before + 2, "the corrupt files were not kept aside");

    for (const auto& f : file.getParentDirectory().findChildFiles (juce::File::findFiles, false,
                                                                  file.getFileName() + ".corrupted-*"))
        f.deleteFile();

    if (existed) file.replaceWithText (original);
    else         file.deleteFile();

    prefs.reset();
    prefs.load();
    prefs.takeCorruptionNotice();
}

//==============================================================================
/*  ER-79 (error-recovery 12): the startup sweep removes month logs older than
    the retention. */
LUTHIER_TEST (ErrorLog, oldLogsArePrunedAtStartup)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-errorlog-prune-test");
    folder.deleteRecursively();
    folder.createDirectory();

    ErrorLog::setFolderForTesting (folder);

    const auto thisMonth = ErrorLog::getLogFile (juce::Time::getCurrentTime());
    const auto old = folder.getChildFile ("errors-201901.log");
    thisMonth.replaceWithText ("{}\n");
    old.replaceWithText ("{}\n");

    // What the processor's startup does.
    { LuthierAudioProcessor processor; }

    CHECK_MSG (! old.existsAsFile(), "a 2019 log survived the startup sweep");
    CHECK_MSG (thisMonth.existsAsFile(), "the sweep deleted this month's log");

    ErrorLog::setFolderForTesting ({});
    folder.deleteRecursively();
}

//==============================================================================
/*  FF-5 / FF-12 (file-formats 0.5): a pattern file and a kit file carry their
    marker; an unmarked (older) file still loads, a wrong marker is refused. */
LUTHIER_TEST (RhythmPatterns, aFileWithTheWrongMagicIsRefused)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-pattern-magic-test");
    folder.deleteRecursively();
    folder.createDirectory();

    RhythmPattern pattern;
    pattern.setName ("Marked");
    const auto file = folder.getChildFile ("Marked.luthierpattern");
    CHECK (pattern.saveTo (file));
    CHECK (juce::JSON::parse (file).getProperty ("magic", {}).toString() == "luthier.pattern");

    RhythmPattern loaded;
    CHECK (loaded.loadFrom (file));
    CHECK (loaded.getName() == "Marked");

    // Older files: no marker at all.
    const auto legacy = folder.getChildFile ("Legacy.luthierpattern");
    auto data = pattern.toVar();
    data.getDynamicObject()->setProperty ("name", "Legacy");
    legacy.replaceWithText (juce::JSON::toString (data));
    CHECK (loaded.loadFrom (legacy));

    const auto wrong = folder.getChildFile ("Wrong.luthierpattern");
    data.getDynamicObject()->setProperty ("magic", "luthier.preset");
    wrong.replaceWithText (juce::JSON::toString (data));
    CHECK_MSG (! loaded.loadFrom (wrong), "a file marked as something else loaded as a pattern");
    CHECK (loaded.getName() == "Legacy");

    GenreKit kit = GenreKit::fromVar (juce::JSON::parse ("{ \"name\": \"Test Kit\", \"strum_patterns\": [\"x\"] }"));
    const auto kitFile = folder.getChildFile ("Kit.luthierkit");

    if (kit.saveTo (kitFile))
    {
        CHECK (juce::JSON::parse (kitFile).getProperty ("magic", {}).toString() == "luthier.genrekit");

        auto kitData = juce::JSON::parse (kitFile);
        kitData.getDynamicObject()->setProperty ("magic", "luthier.pattern");
        kitFile.replaceWithText (juce::JSON::toString (kitData));

        GenreKit other;
        CHECK_MSG (! other.loadFrom (kitFile), "a kit marked as a pattern loaded");
    }

    folder.deleteRecursively();
}

//==============================================================================
/*  SM-29 / SM-51 (state-model 8.3): a tune load stops the playing tune and
    starts the new one at bar 0; the setlist, Live Mode and uiState stay. */
LUTHIER_TEST (StateModel, aTuneLoadStopsPlaybackAndLeavesTheRestAlone)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    Tune first;
    first.addSection (TuneSection());
    processor.getTuneSession().newTune (first);

    Setlist list;
    SetlistEntry entry;
    entry.presetPath = "/nowhere/A.luthierpreset";
    list.addEntry (entry);
    list.addEntry (entry);
    processor.getSetlist().setSetlist (list);
    processor.getSetlist().goTo (1);
    processor.setLiveMode (true);
    processor.getUiState().advancedTab = 3;

    processor.getTunePlayer().play();

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < 20; ++i)
    {
        processor.serviceTune();
        processor.processBlock (buffer, midi);
    }

    Tune second;
    second.addSection (TuneSection());
    processor.getTuneSession().newTune (second);

    for (int i = 0; i < 4; ++i)
    {
        processor.serviceTune();
        processor.processBlock (buffer, midi);
    }

    CHECK_MSG (! processor.getTunePlayer().isPlaying(), "a tune load left the old tune playing");
    CHECK_MSG (processor.getTunePlayer().getPositionPpq() < 1.0e-6, "the new tune does not start at bar 0");
    CHECK_MSG (processor.getSetlist().getPosition() == 1, "a tune load stepped the setlist");
    CHECK (processor.isLiveMode());
    CHECK (processor.getUiState().advancedTab == 3);
}

//==============================================================================
/*  ER-27 (error-recovery 3): a layout the plugin does not advertise is refused. */
LUTHIER_TEST (PluginBuses, anUnadvertisedLayoutIsRefused)
{
    LuthierAudioProcessor processor;
    auto layout = processor.getBusesLayout();

    CHECK (processor.checkBusesLayoutSupported (layout));

    auto surround = layout;
    surround.outputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
    CHECK_MSG (! processor.checkBusesLayoutSupported (surround), "a 5.1 main output was accepted");

    auto mono = layout;
    mono.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK (processor.checkBusesLayoutSupported (mono));

    if (layout.inputBuses.size() > 0)
    {
        auto quad = layout;
        quad.inputBuses.getReference (0) = juce::AudioChannelSet::quadraphonic();
        CHECK_MSG (! processor.checkBusesLayoutSupported (quad), "a quad sidechain was accepted");
    }
}

//==============================================================================
/*  ER-35 (error-recovery 4): SysEx nobody asked for is ignored - no note, no
    parameter change. */
LUTHIER_TEST (Controllers, unknownSysExIsIgnored)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // The instrument's own noise floor (hum, hiss) is there with no note at all.
    float floor = 0.0f;

    for (int i = 0; i < 8; ++i)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
        floor = juce::jmax (floor, buffer.getMagnitude (0, 512));
    }

    juce::MemoryBlock before;
    processor.getStateInformation (before);

    juce::Random random (7);

    for (int i = 0; i < 16; ++i)
    {
        juce::uint8 payload[24];

        for (auto& b : payload)
            b = (juce::uint8) random.nextInt (128);

        payload[0] = 0x7d;   // non-commercial id, which is nobody's
        midi.clear();
        midi.addEvent (juce::MidiMessage::createSysExMessage (payload, (int) sizeof (payload)), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);
        CHECK_MSG (buffer.getMagnitude (0, 512) < floor * 2.0f + 1.0e-4f, "a SysEx message made a sound");
    }

    juce::MemoryBlock after;
    processor.getStateInformation (after);
    CHECK_MSG (before == after, "a SysEx message changed the state");
}

//==============================================================================
/*  SM-45 (state-model 8.1): Slide Mode, Live Mode and the practice drawer
    persist across a preset load; the snapshot strip's bank is the new preset's. */
LUTHIER_TEST (StateModel, slideLiveAndTheDrawerPersistAcrossALoad)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 1)
        return;

    auto* slide = processor.getState().getParameter (ParamIDs::slideGuitar);
    CHECK (slide != nullptr);

    if (slide == nullptr)
        return;

    slide->setValueNotifyingHost (1.0f);
    processor.setLiveMode (true);
    processor.getUiState().practiceDrawerOpen = true;
    processor.captureSnapshot (0, "Old bank");

    // A preset that leaves slide out (every factory preset writes every key, so
    // strip it from one).
    auto data = FactoryPresets::toVar (FactoryPresets::getPreset (0), processor);
    data.getProperty ("parameters", {}).getDynamicObject()->removeProperty (ParamIDs::slideGuitar);
    data.getProperty ("parameters", {}).getDynamicObject()->removeProperty (ParamIDs::slideMode);

    CHECK (presets.fromVar (data));

    CHECK_MSG (slide->getValue() > 0.5f, "a preset load turned Slide Mode off");
    CHECK (processor.isLiveMode());
    CHECK (processor.getUiState().practiceDrawerOpen);
    CHECK_MSG (processor.getSnapshots().getNumSnapshots() == 0, "the snapshot strip kept the old preset's bank");
}
