/*  W2 robustness: what would crash, corrupt a project or lose state in a DAW.

    - File choosers die with the component that opened them (MIDI import
      chooser lifetime, CODEX_COMPLETENESS_LEDGER).
    - The Strings column's mute goes through the audio thread's queue (CB-17).
    - The host state blob's format version (HI-20), forward (HI-24) and
      backward (HI-25) compatibility, and an unreadable blob (error-recovery 0.2).
    - A tune keeps at least one section (ER-48).
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Support/ErrorLog.h"
#include "../Support/ConfigRecovery.h"
#include "../Live/LiveInput.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/Notifications.h"
#include "../UI/OwnedFileChooser.h"
#include "../Tune/TuneModel.h"
#include "../Tune/TuneMelody.h"
#include "../Model/Workshop/PartLibrary.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    void render (LuthierAudioProcessor& p, int blocks)
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), kBlock);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            p.processBlock (buffer, midi);
        }
    }

    template <typename T>
    T* findChild (juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            return t;

        for (auto* c : root.getChildren())
            if (auto* found = findChild<T> (*c))
                return found;

        return nullptr;
    }

    juce::var stateOf (LuthierAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return juce::JSON::parse (LuthierAudioProcessor::stateBlobToText (block.getData(), (int) block.getSize()));
    }

    void restore (LuthierAudioProcessor& p, const juce::var& state)
    {
        const auto json = juce::JSON::toString (state, true);
        p.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());
    }

    /** Points the error log (and so the state backups) at a fresh folder. */
    struct TempDiagnostics
    {
        TempDiagnostics()
        {
            folder.deleteRecursively();
            folder.createDirectory();
            ErrorLog::setFolderForTesting (folder);
        }

        ~TempDiagnostics()
        {
            ErrorLog::setFolderForTesting ({});
            folder.deleteRecursively();
        }

        int numBackups() const
        {
            return LuthierAudioProcessor::getStateBackupFolder()
                       .getNumberOfChildFiles (juce::File::findFiles, "session-state-*.json");
        }

        juce::File folder { juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("luthier-w2-diagnostics") };
    };

    struct NonNativeChoosers
    {
        NonNativeChoosers()  { OwnedFileChooser::useNativeDialogs() = false; }
        ~NonNativeChoosers() { OwnedFileChooser::useNativeDialogs() = true; }
    };
}

//==============================================================================
/*  CODEX_COMPLETENESS_LEDGER, MIDI import chooser lifetime: closing the window
    while File -> Import MIDI is open cancels the dialog; its callback never
    runs on the destroyed header. */
LUTHIER_TEST (ChooserLifetime, closingTheWindowWithTheMidiImportChooserOpenCancelsIt)
{
    NonNativeChoosers nonNative;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* header = findChild<HeaderBar> (*editor);
    CHECK (header != nullptr);

    if (header == nullptr)
        return;

    auto called = std::make_shared<bool> (false);
    header->onImportMidi = [called] (const juce::File&) { *called = true; };

    auto* modal = juce::ModalComponentManager::getInstance();
    const int modalBefore = modal->getNumModalComponents();

    header->handleFileMenuResult (14);   // File -> Import MIDI...
    CHECK (header->hasOpenFileChooser());
    CHECK_MSG (modal->getNumModalComponents() > modalBefore, "the Import MIDI dialog did not open");

    editor.reset();   // the host closes the window

    CHECK_MSG (modal->getNumModalComponents() == modalBefore, "the dialog outlived the window that opened it");
    CHECK (! *called);
}

/*  The helper on its own: a chooser whose owner has gone never calls back,
    and replacing one cancels the first. */
LUTHIER_TEST (ChooserLifetime, anOwnedChooserIsCancelledWithItsOwner)
{
    NonNativeChoosers nonNative;
    auto* modal = juce::ModalComponentManager::getInstance();
    const int modalBefore = modal->getNumModalComponents();

    auto called = std::make_shared<int> (0);

    {
        juce::Component owner;
        OwnedFileChooser chooser;

        chooser.launch (owner, "one", {}, "*.mid", juce::FileBrowserComponent::openMode
                                                     | juce::FileBrowserComponent::canSelectFiles,
                        [called] (const juce::File&) { ++*called; });
        chooser.launch (owner, "two", {}, "*.mid", juce::FileBrowserComponent::openMode
                                                     | juce::FileBrowserComponent::canSelectFiles,
                        [called] (const juce::File&) { ++*called; });

        CHECK_MSG (modal->getNumModalComponents() == modalBefore + 1, "a replaced chooser stayed open");
    }

    CHECK (modal->getNumModalComponents() == modalBefore);
    CHECK (*called == 0);
}

//==============================================================================
/*  CB-17: the Strings column's mute square posts to the processor, which the
    audio thread applies; the fretboard's menu and the column show one state. */
LUTHIER_TEST (StateModel, theStringsColumnMuteGoesThroughTheAudioThreadQueue)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    render (processor, 2);

    StringRow row (processor, 2);
    row.setBounds (0, 0, 220, StringRow::preferredHeight);
    (void) row.createComponentSnapshot (row.getLocalBounds());   // lays out the mute square

    const int pending = processor.getNumPendingEngineCommands();
    CHECK (! processor.isStringMuted (2));

    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> at ((float) row.getWidth() - 9.0f, (float) row.getHeight() * 0.5f);
    const juce::MouseEvent e (source, at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &row, &row, now, at, now, 1, false);

    row.mouseDown (e);

    CHECK (processor.isStringMuted (2));
    CHECK_MSG (processor.getNumPendingEngineCommands() == pending + 1,
               "the mute did not go through the engine command queue");

    render (processor, 1);
    CHECK (processor.getNumPendingEngineCommands() == 0);

    // Muted elsewhere (the fretboard menu): the row follows, and unmutes it.
    processor.setStringMuted (2, false);
    render (processor, 1);
    processor.setStringMuted (2, true);
    (void) row.createComponentSnapshot (row.getLocalBounds());
    row.mouseDown (e);
    CHECK (! processor.isStringMuted (2));
}

//==============================================================================
/*  HI-20: every blob says which format it is and which build wrote it. */
LUTHIER_TEST (HostState, theBlobCarriesItsFormatVersion)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const auto state = stateOf (processor);
    CHECK ((int) state.getProperty ("stateFormat", 0) == LuthierAudioProcessor::kStateFormatVersion);
    CHECK (state.getProperty ("savedBy", {}).toString() == JucePlugin_VersionString);
}

/*  HI-24, host-integration 4.1: a newer blob loads what this build knows, says
    so, and writes back the sections (and the newer preset) it could not read. */
LUTHIER_TEST (HostState, aNewerBlobKeepsWhatItCannotReadOnWriteBack)
{
    TempDiagnostics diagnostics;

    LuthierAudioProcessor source;
    source.prepareToPlay (kSr, kBlock);

    auto state = stateOf (source);
    auto* root = state.getDynamicObject();
    CHECK (root != nullptr);

    if (root == nullptr)
        return;

    auto* future = new juce::DynamicObject();
    future->setProperty ("depth", 7);
    root->setProperty ("stateFormat", LuthierAudioProcessor::kStateFormatVersion + 5);
    root->setProperty ("savedBy", "9.0.0");
    root->setProperty ("hologramRig", juce::var (future));

    if (auto* preset = root->getProperty ("preset").getDynamicObject())
        preset->setProperty ("schema", 99);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.takeStateWarnings();
    restore (processor, state);

    const auto warnings = processor.takeStateWarnings();
    CHECK_MSG (warnings.joinIntoString ("|").contains ("newer Luthier"), "no warning: " + warnings.joinIntoString ("|"));
    CHECK_MSG (warnings.joinIntoString ("|").contains ("sound could not be loaded"),
               "the refused preset was silent: " + warnings.joinIntoString ("|"));

    const auto written = stateOf (processor);
    CHECK ((int) written.getProperty ("hologramRig", {}).getProperty ("depth", 0) == 7);
    CHECK ((int) written.getProperty ("preset", {}).getProperty ("schema", 0) == 99);
    CHECK ((int) written.getProperty ("stateFormat", 0) == LuthierAudioProcessor::kStateFormatVersion);
    CHECK (diagnostics.numBackups() == 0);

    // Choosing another sound replaces the newer preset; the unknown section stays.
    CHECK (processor.getPresetManager().loadPreset (0));
    const auto afterLoad = stateOf (processor);
    CHECK ((int) afterLoad.getProperty ("preset", {}).getProperty ("schema", 0) != 99);
    CHECK ((int) afterLoad.getProperty ("hologramRig", {}).getProperty ("depth", 0) == 7);

    // A current-format blob keeps nothing extra: an unknown key there is not carried.
    auto current = stateOf (source);
    current.getDynamicObject()->setProperty ("strayKey", 1);
    restore (processor, current);
    CHECK (! stateOf (processor).hasProperty ("strayKey"));
}

/*  HI-25, host-integration 4.2: an older blob is copied to the diagnostics
    folder before it is migrated; a current one is not. */
LUTHIER_TEST (HostState, anOlderBlobIsBackedUpBeforeItIsMigrated)
{
    TempDiagnostics diagnostics;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto state = stateOf (processor);
    restore (processor, state);
    CHECK (diagnostics.numBackups() == 0);

    state.getDynamicObject()->removeProperty ("stateFormat");   // as every build before HI-20 wrote it
    restore (processor, state);
    CHECK (diagnostics.numBackups() == 1);

    const auto backups = LuthierAudioProcessor::getStateBackupFolder()
                             .findChildFiles (juce::File::findFiles, false, "session-state-*.json");

    if (backups.size() == 1)
        CHECK (juce::JSON::parse (backups[0].loadFileAsString()).hasProperty ("preset"));
}

/*  error-recovery 0.2 / 0.3: a blob that is not a Luthier state changes
    nothing, is kept, and is reported - never a crash, never silence. */
LUTHIER_TEST (HostState, anUnreadableBlobChangesNothingAndIsKept)
{
    TempDiagnostics diagnostics;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.takeStateWarnings();

    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    drive->setValueNotifyingHost (0.77f);

    const char garbage[] = "\xff\xfe not a session { \x01";
    processor.setStateInformation (garbage, (int) sizeof (garbage));

    CHECK_NEAR (drive->getValue(), 0.77, 1.0e-3);
    CHECK (processor.takeStateWarnings().joinIntoString ("|").contains ("could not be read"));
    CHECK (diagnostics.numBackups() == 1);

    // Truncated and byte-flipped real states: refused or loaded, never a crash.
    juce::MemoryBlock good;
    processor.getStateInformation (good);
    juce::Random rng (0x57325232);

    for (int trial = 0; trial < 40; ++trial)
    {
        juce::MemoryBlock bad (good);
        auto* bytes = static_cast<char*> (bad.getData());

        if (trial % 2 == 0)
            bad.setSize ((size_t) rng.nextInt ((int) bad.getSize()));
        else
            for (int f = 0; f < 8; ++f)
                bytes[rng.nextInt ((int) bad.getSize())] = (char) rng.nextInt (256);

        processor.setStateInformation (bad.getData(), (int) bad.getSize());
        render (processor, 1);
    }

    CHECK (true);
}

/*  The blob is decoded by byte count: text past the first multi-byte
    character survives, and the bytes after the host's buffer are not read. */
LUTHIER_TEST (HostState, nonAsciiTextRoundTripsThroughTheBlob)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto state = stateOf (processor);
    const juce::String name (juce::CharPointer_UTF8 ("Caf\xc3\xa9 \xe2\x80\x93 \xc3\x9c" "berdrive \xe2\x99\xab"));
    juce::Array<juce::var> locks;
    locks.add (name);
    state.getDynamicObject()->setProperty ("lockedParameters", locks);

    // The host's buffer, followed by bytes that are not part of it.
    const auto json = juce::JSON::toString (state, true);
    juce::MemoryBlock buffer (json.toRawUTF8(), json.getNumBytesAsUTF8());
    buffer.append ("]]]]GARBAGE", 11);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);
    restored.setStateInformation (buffer.getData(), (int) json.getNumBytesAsUTF8());

    const auto written = stateOf (restored);
    const auto* writtenLocks = written.getProperty ("lockedParameters", {}).getArray();
    CHECK (writtenLocks != nullptr && writtenLocks->contains (name));
}

//==============================================================================
/*  ER-48, error-recovery 6: removing the last section is refused. */
LUTHIER_TEST (TuneBuilder, theLastSectionCannotBeRemoved)
{
    Tune tune;

    while (tune.getNumSections() > 1)
        CHECK (tune.removeSection (tune.getNumSections() - 1));

    if (tune.getNumSections() == 0)
    {
        TuneSection s;
        s.name = "Only";
        s.lengthBars = 4;
        tune.addSection (s);
    }

    CHECK (tune.getNumSections() == 1);
    CHECK (! tune.removeSection (0));
    CHECK (tune.getNumSections() == 1);
}

//==============================================================================
/*  ER-22, error-recovery 2: two instances save the same preset - the later
    save wins and says it overwrote another change. */
LUTHIER_TEST (Presets, aSaveOverAnotherInstancesChangeWinsAndSaysSo)
{
    TempDiagnostics diagnostics;

    LuthierAudioProcessor first, second;
    first.prepareToPlay (kSr, kBlock);
    second.prepareToPlay (kSr, kBlock);

    auto& a = first.getPresetManager();
    auto& b = second.getPresetManager();
    const juce::String name ("W2 Concurrent Save Test");

    CHECK (a.saveAs (name, "User"));
    CHECK (a.takeSaveNotice().isEmpty());

    const auto file = a.getCurrentPresetFile();
    CHECK (file.existsAsFile());
    CHECK (b.loadPreset (file));

    // Nobody else touched it: a plain save.
    CHECK (b.saveCurrent());
    CHECK (b.takeSaveNotice().isEmpty());

    // The first window saves in between (a later modification time) ...
    CHECK (a.saveCurrent());
    file.setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (30));

    // ... and the second one's save wins, and says so.
    CHECK (b.saveCurrent());
    CHECK (b.takeSaveNotice().contains ("overwrote another change"));
    CHECK (b.takeSaveNotice().isEmpty());   // taken once

    for (int i = 0; i < b.getNumPresets(); ++i)
        if (const auto* info = b.getPreset (i); info != nullptr && info->file == file)
            b.deletePreset (i);

    file.deleteFile();
}

//==============================================================================
/*  SM-62, state-model 11: two instances in one host share nothing but the
    user-global files - parameters, MIDI Learn (mappings and arming) and a
    learned CC's effect stay in the instance they belong to. */
LUTHIER_TEST (StateModel, twoInstancesAreIndependent)
{
    LuthierAudioProcessor a, b;
    a.prepareToPlay (kSr, kBlock);
    b.prepareToPlay (kSr, kBlock);

    const int bMappings = b.getMidiLearn().getNumMappings();

    a.getMidiLearn().addMapping (ParamIDs::macroDrive, 20);
    a.getMidiLearn().startLearning (ParamIDs::macroTone);

    CHECK (b.getMidiLearn().getNumMappings() == bMappings);
    CHECK (b.getMidiLearn().getCcForParameter (ParamIDs::macroDrive) != 20);
    CHECK (! b.getMidiLearn().isLearning());
    a.getMidiLearn().cancelLearning();

    auto* driveA = a.getState().getParameter (ParamIDs::macroDrive);
    auto* driveB = b.getState().getParameter (ParamIDs::macroDrive);
    driveB->setValueNotifyingHost (0.2f);
    const float bBefore = driveB->getValue();

    // CC 20 at full into A only.
    juce::AudioBuffer<float> buffer (a.getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 0);

    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        a.processBlock (buffer, midi);
        midi.clear();
        buffer.clear();
        juce::MidiBuffer none;
        b.processBlock (buffer, none);
    }

    CHECK_MSG (driveA->getValue() > 0.9f, "the learned CC did not reach its own instance");
    CHECK_NEAR (driveB->getValue(), bBefore, 1.0e-4);

    // Restoring A's session into B copies it once; they stay separate after.
    juce::MemoryBlock state;
    a.getStateInformation (state);
    b.setStateInformation (state.getData(), (int) state.getSize());
    driveA->setValueNotifyingHost (0.1f);
    CHECK (std::abs (driveB->getValue() - 0.1f) > 0.05f);
}

//==============================================================================
/*  ER-65/66, error-recovery 10: a settings file that cannot be read is kept
    aside as .corrupted-<timestamp>, defaults are used, and the window is told;
    a readable one loads and is left alone. */
LUTHIER_TEST (ErrorRecovery, aCorruptSettingsFileIsSetAsideAndReported)
{
    TempDiagnostics diagnostics;
    ConfigRecovery::takeRecoveredFiles();

    auto folder = diagnostics.folder.getChildFile ("config");
    folder.createDirectory();
    auto file = folder.getChildFile ("live-actions.json");

    file.replaceWithText ("{ \"actions\": [ oops");

    LiveActionMap map;
    map.setConfigFile (file);
    CHECK (! map.load());
    CHECK (! file.exists());
    CHECK (folder.getNumberOfChildFiles (juce::File::findFiles, "live-actions.json.corrupted-*") == 1);
    CHECK (ConfigRecovery::takeRecoveredFiles().contains ("live-actions.json"));
    CHECK (ConfigRecovery::takeRecoveredFiles().isEmpty());   // taken once

    // A newer schema than the reader allows goes the same way.
    auto versioned = folder.getChildFile ("versioned.json");
    versioned.replaceWithText ("{ \"schema\": 99 }");
    CHECK (ConfigRecovery::loadObject (versioned, "Test", 1).isVoid());
    CHECK (! versioned.exists());

    // A good file loads and stays.
    auto good = folder.getChildFile ("good.json");
    good.replaceWithText ("{ \"schema\": 1, \"x\": 2 }");
    CHECK ((int) ConfigRecovery::loadObject (good, "Test", 1).getProperty ("x", 0) == 2);
    CHECK (good.existsAsFile());
    CHECK (ConfigRecovery::takeRecoveredFiles().contains ("versioned.json"));
}

//==============================================================================
namespace
{
    struct ChangeCounter : juce::AudioProcessorListener
    {
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        int changes = 0;
    };
}

/*  HI-16, host-integration 3.1: a preset load and a snapshot recall change
    parameters through the host notification path, so automation lanes and
    the host's generic editor follow. */
LUTHIER_TEST (HostState, presetLoadsAndSnapshotRecallsNotifyTheHost)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();
    CHECK (presets.getNumPresets() >= 2);

    ChangeCounter counter;
    processor.addListener (&counter);

    CHECK (presets.loadPreset (1));
    CHECK_MSG (counter.changes > 0, "a preset load changed parameters without telling the host");

    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    drive->setValueNotifyingHost (0.1f);
    CHECK (processor.captureSnapshot (0));
    drive->setValueNotifyingHost (0.9f);

    counter.changes = 0;
    CHECK (processor.recallSnapshot (0));
    processor.getSnapshots().advance (1.0);   // past the 30 ms crossfade
    CHECK_MSG (counter.changes > 0, "a snapshot recall changed parameters without telling the host");
    CHECK_NEAR (drive->getValue(), 0.1, 0.02);

    processor.removeListener (&counter);
}

/*  HI-22 / HI-38, host-integration 4 and 9.2: a typical session is under
    200 KB (Logic's 500 KB comfortably), for every factory preset. */
LUTHIER_TEST (HostState, everyFactoryPresetsSessionIsUnder200KB)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();
    size_t largest = 0;
    juce::String largestName;

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto* info = presets.getPreset (i);

        if (info == nullptr || ! info->isFactory || ! presets.loadPreset (i))
            continue;

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        if (state.getSize() > largest)
        {
            largest = state.getSize();
            largestName = info->name;
        }
    }

    CHECK (largest > 0);
    CHECK_MSG (largest < 200 * 1024, largestName + " saves " + juce::String ((int) (largest / 1024)) + " KB");
}

/*  IR-26, input-routing 6: the sidechain is an input, never an output. With
    nothing consuming it, loud sidechain audio leaves the main output silent. */
LUTHIER_TEST (InputRouting, anUnconsumedSidechainNeverReachesTheMainOutput)
{
    LuthierAudioProcessor processor;

    auto layout = processor.getBusesLayout();
    CHECK (layout.inputBuses.size() >= 1);

    if (layout.inputBuses.isEmpty())
        return;

    layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (processor.setBusesLayout (layout));
    processor.prepareToPlay (kSr, kBlock);

    const int channels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, kBlock);
    juce::MidiBuffer midi;
    juce::Random rng (0x5c);
    float peak = 0.0f;

    for (int block = 0; block < 20; ++block)
    {
        auto sidechain = processor.getBusBuffer (buffer, true, 0);

        for (int ch = 0; ch < sidechain.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                sidechain.setSample (ch, i, rng.nextFloat() * 1.6f - 0.8f);

        processor.processBlock (buffer, midi);

        if (block >= 4)
        {
            auto main = processor.getBusBuffer (buffer, false, 0);
            peak = juce::jmax (peak, main.getMagnitude (0, kBlock));
        }
    }

    CHECK_MSG (peak < 1.0e-3f, "the sidechain leaked to the main output at " + juce::String (peak));
}

/*  ER-15 / ER-17, error-recovery 1: a damaged older preset either loads
    (migrating in memory) or is refused before anything is applied; either
    way the file on disk is byte-for-byte what it was. */
LUTHIER_TEST (Presets, aDamagedOldPresetNeverChangesTheFileOnDisk)
{
    TempDiagnostics diagnostics;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto folder = diagnostics.folder.getChildFile ("presets");
    folder.createDirectory();

    const char* bodies[] =
    {
        // an old (pre-`schema`) preset whose values are the wrong types
        R"({"format":"LuthierPreset","schemaVersion":1,"name":"Old","parameters":{"macro_drive":"loud","pickup_position_0":[1,2]},"strings":{"numStrings":"six","detuneCents":"x"}})",
        // the same with a truncated tail
        R"({"format":"LuthierPreset","schemaVersion":1,"name":"Old","parameters":{"macro_drive":0.5,)",
        // a structurally odd block
        R"({"format":"LuthierPreset","schemaVersion":1,"parameters":[],"strings":7,"ranges":"wide","routing":null})"
    };

    for (const auto* body : bodies)
    {
        auto file = folder.getNonexistentChildFile ("Damaged", PresetManager::kFileExtension, false);
        file.replaceWithText (body);
        juce::MemoryBlock before;
        file.loadFileAsData (before);

        processor.getPresetManager().loadPreset (file);
        render (processor, 1);

        juce::MemoryBlock after;
        file.loadFileAsData (after);
        CHECK_MSG (before == after, "loading " + file.getFileName() + " rewrote it");
    }
}

//==============================================================================
/*  ER-81, error-recovery 14: errors before warnings before info; arrival
    order within a level. */
LUTHIER_TEST (Editor, bannersShowTheMostSevereFirst)
{
    NotificationCentre centre;
    centre.setSize (600, NotificationCentre::preferredHeight);

    auto make = [] (const char* id, Notification::Level level)
    {
        Notification n;
        n.id = id;
        n.message = id;
        n.level = level;
        return n;
    };

    centre.post (make ("first-info", Notification::Level::info));        // shown at once
    centre.post (make ("second-info", Notification::Level::info));
    centre.post (make ("a-warning", Notification::Level::warning));
    centre.post (make ("an-error", Notification::Level::error));
    centre.post (make ("another-error", Notification::Level::error));

    juce::StringArray order { centre.getCurrentId() };

    while (centre.getNumQueued() > 0)
    {
        centre.dismissCurrent();
        order.add (centre.getCurrentId());
    }

    CHECK_MSG (order.joinIntoString (",") == "first-info,an-error,another-error,a-warning,second-info",
               order.joinIntoString (","));
}

//==============================================================================
/*  ER-44, error-recovery 5: an invalid guitar is refused with a reason and
    nothing is written; the factory guitar itself is valid. */
LUTHIER_TEST (Workshop, anInvalidGuitarSaveIsRefusedWithItsReason)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    CHECK (processor.hasPartsGuitar());

    auto guitar = processor.getCurrentGuitar();
    CHECK (LuthierAudioProcessor::describeGuitarSaveProblem (guitar).isEmpty());

    guitar.parts[(size_t) GuitarSlot::neck] = nullptr;
    const auto problem = LuthierAudioProcessor::describeGuitarSaveProblem (guitar);
    CHECK_MSG (problem.contains ("neck"), "reason: " + problem);

    const auto before = PartLibrary::getUserGuitarsFolder().getNumberOfChildFiles (juce::File::findFiles);
    CHECK (processor.saveGuitarAs ("   ") == juce::File());
    CHECK (processor.getLastGuitarSaveError().contains ("name"));
    CHECK (PartLibrary::getUserGuitarsFolder().getNumberOfChildFiles (juce::File::findFiles) == before);
}

//==============================================================================
/*  SM-50, state-model 8.2: a snapshot recall during A/B compare replaces the
    selected slot - switching away stores the recalled state, not the old one. */
LUTHIER_TEST (StateModel, aRecallDuringCompareReplacesTheSelectedSlot)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    drive->setValueNotifyingHost (0.5f);
    CHECK (processor.captureSnapshot (0));

    drive->setValueNotifyingHost (0.2f);   // A
    processor.setSlotBActive (true);
    drive->setValueNotifyingHost (0.8f);   // B
    processor.setSlotBActive (false);
    CHECK_NEAR (drive->getValue(), 0.2, 0.01);

    CHECK (processor.recallSnapshot (0));   // into A
    processor.getSnapshots().advance (1.0);
    CHECK_NEAR (drive->getValue(), 0.5, 0.02);

    processor.setSlotBActive (true);
    CHECK_NEAR (drive->getValue(), 0.8, 0.01);
    processor.setSlotBActive (false);
    CHECK_MSG (std::abs (drive->getValue() - 0.5f) < 0.02f, "A went back to its pre-recall state");
}

/*  SM-7, state-model 1: the per-instance UI state - mode, tab, Live, Slide
    Mode (a parameter), the Practice drawer, the bench slots - survives the host. */
LUTHIER_TEST (StateModel, theInstancesUiStateSurvivesTheHost)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& ui = processor.getUiState();
    ui.advancedMode = true;
    ui.advancedTab = 3;
    ui.practiceDrawerOpen = true;
    ui.liveMode = true;
    ui.benchSlots[0] = juce::var ("bench-a");

    if (auto* slide = processor.getState().getParameter (ParamIDs::slideGuitar))
        slide->setValueNotifyingHost (1.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    const auto& r = restored.getUiState();
    CHECK (r.advancedMode);
    CHECK (r.advancedTab == 3);
    CHECK (r.practiceDrawerOpen);
    CHECK (r.liveMode);
    CHECK (r.benchSlots[0].toString() == "bench-a");

    auto* slide = restored.getState().getParameter (ParamIDs::slideGuitar);
    CHECK (slide != nullptr && slide->getValue() > 0.5f);
}

//==============================================================================
/*  ER-46, error-recovery 6: "melody generation produces no notes - keep the
    previous melody". The generator places at least one note in every bar it
    plays, so this pins that invariant (empty progression, every length, many
    seeds); generateMelody / regenerateMelody and the TUNE panel also refuse an
    empty result and keep what was there, should that ever change. */
LUTHIER_TEST (TuneBuilder, generationAlwaysProducesNotesSoAMelodyIsNeverWipedOut)
{
    for (int bars = 1; bars <= 8; ++bars)
    {
        Tune tune;

        while (tune.getNumSections() > 1)
            tune.removeSection (tune.getNumSections() - 1);

        if (tune.getNumSections() == 0)
        {
            TuneSection s;
            s.name = "Only";
            tune.addSection (s);
        }

        auto* section = tune.getSection (0);
        CHECK (section != nullptr);

        if (section == nullptr)
            return;

        section->chords.clear();
        section->lengthBars = bars;

        for (int seed = 1; seed <= 20; ++seed)
            CHECK_MSG (! generateAutoMelody (tune, 0, seed).empty(),
                       juce::String (bars) + " bars, seed " + juce::String (seed) + " generated nothing");
    }
}
