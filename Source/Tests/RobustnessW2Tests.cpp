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
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/OwnedFileChooser.h"
#include "../Tune/TuneModel.h"

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
