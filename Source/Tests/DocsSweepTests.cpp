/*  SPEC-SWEEP (docs worker): tests for claims the user documentation and
    spec/README.md make that had nothing checking them.

    Each test names the audit row it closes (docs/audit/sweep_parts/<spec>.md).
    They are small and self-contained on purpose: a documentation claim should
    fail loudly when the code drifts away from it.
*/

#include "TestFramework.h"

#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Effects/EffectsChain.h"
#include "../DSP/Effects/Pedal.h"
#include "../Export/LuthierMidiEvents.h"
#include "../Export/MidiPerformance.h"
#include "../Export/MidiProfiles.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Parameters.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../Support/AudioExporter.h"
#include "../Support/Diagnostics.h"
#include "../Support/ErrorLog.h"
#include "../Support/IrLibrary.h"
#include "../Support/MidiLearn.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/OptionsPages.h"
#include "../UI/RangesUi.h"
#include "../UI/UiPreferences.h"
#include "../UI/Widgets.h"
#include "../UI/Overlays.h"
#include "../UI/WorkshopPanel.h"

#include <cstring>
#include <map>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSweepSr = 48000.0;
    constexpr int kSweepBlock = 512;

    juce::File sweepTempFolder (const juce::String& name)
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
        folder.deleteRecursively();
        folder.createDirectory();
        return folder;
    }

    bool pathEndsWith (const juce::File& file, const juce::String& tail)
    {
        return file.getFullPathName().replaceCharacter ('\\', '/').endsWith (tail);
    }

    template <typename T>
    T* findFirstChildOfType (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (child))
                return t;

            if (auto* found = findFirstChildOfType<T> (*child))
                return found;
        }

        return nullptr;
    }

    int countWavs (const juce::File& folder)
    {
        int n = 0;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.wav", juce::File::findFiles))
        {
            juce::ignoreUnused (entry);
            ++n;
        }

        return n;
    }
}

//==============================================================================
/*  PRESET_FORMAT PF-19: presets store MidiTarget as integers, so the values the
    doc's table lists must never move. Appending is fine; reordering is not. */
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

/*  README RM-6: "22 pedals (21 effects plus the Doubler)". */
LUTHIER_TEST (Effects, pedalCountMatchesTheReadme)
{
    CHECK ((int) PedalType::NumTypes - 1 == 22);
    CHECK ((int) PedalType::Doubler == 22);
}

/*  JUCE_CLAUDE_GUIDELINES JG-4: the oversampling factor reaches every drive
    pedal through a virtual call (no dynamic_cast), including one fitted after
    the change. Each drive pedal reports its oversampler's latency. */
LUTHIER_TEST (Effects, oversamplingChangeReachesEveryDrivePedal)
{
    EffectsChain chain;
    chain.prepare (kSweepSr, kSweepBlock);

    chain.setSlotType (0, PedalType::Overdrive);
    chain.setSlotType (1, PedalType::Fuzz);
    chain.setSlotType (2, PedalType::Chorus);

    std::vector<double> left ((size_t) kSweepBlock, 0.0), right ((size_t) kSweepBlock, 0.0);
    chain.processStereo (left.data(), right.data(), kSweepBlock);    // installs the swaps

    chain.setOversamplingFactor (1);
    const int atOne = chain.getLatencySamples();

    chain.setOversamplingFactor (4);
    const int atFour = chain.getLatencySamples();

    CHECK_MSG (atFour > atOne, juce::String (atOne) + " -> " + juce::String (atFour));

    if (auto* overdrive = chain.getPedal (0))
        CHECK (overdrive->getLatencySamples() * 2 == atFour - atOne);

    // A drive pedal fitted now starts at the chain's current factor.
    chain.setSlotType (3, PedalType::Distortion);
    chain.processStereo (left.data(), right.data(), kSweepBlock);

    if (auto* overdrive = chain.getPedal (0))
        if (auto* distortion = chain.getPedal (3))
            CHECK (distortion->getLatencySamples() == overdrive->getLatencySamples());
}

/*  README RM-8: "720 impulse responses - 216 for bodies, 504 for cabinets". */
LUTHIER_TEST (IrLibrary, theShippedLibraryHas720Irs)
{
    CHECK_MSG (IrLibrary::isAvailable(), "the Resources folder was not found beside the tests");

    const int bodies = countWavs (IrLibrary::getBodyIrFolder());
    const int cabs = countWavs (IrLibrary::getCabIrFolder());

    CHECK_MSG (bodies == 216, "body IRs: " + juce::String (bodies));
    CHECK_MSG (cabs == 504, "cabinet IRs: " + juce::String (cabs));
}

//==============================================================================
/*  README RM-22, USER_MANUAL UM-48, PRESET_FORMAT PF-2: where things live. */
LUTHIER_TEST (Presets, foldersAreWhereTheDocsSayTheyAre)
{
    const auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("Luthier");

    const auto user = PresetManager::getUserPresetFolder();
    const auto renders = PresetManager::getRenderFolder();
    const auto diagnostics = Diagnostics::getDiagnosticsFolder();
    const auto factory = PresetManager::getFactoryPresetFolder();

    CHECK_MSG (user == documents.getChildFile ("Presets").getChildFile ("User"), user.getFullPathName());
    CHECK_MSG (renders == documents.getChildFile ("Renders"), renders.getFullPathName());
    CHECK_MSG (diagnostics == documents.getChildFile ("Diagnostics"), diagnostics.getFullPathName());
    CHECK (user.isDirectory() && renders.isDirectory() && diagnostics.isDirectory());

    // The factory bank sits beside the plugin when that can be written, and falls
    // back to Documents when it cannot.
    const auto resources = IrLibrary::getResourcesFolder();
    const bool besideThePlugin = resources != juce::File()
                                   && factory == resources.getChildFile ("Presets").getChildFile ("Factory");
    const bool inDocuments = factory == documents.getChildFile ("Presets").getChildFile ("Factory");

    CHECK_MSG (besideThePlugin || inDocuments, factory.getFullPathName());
    CHECK (pathEndsWith (factory, "Presets/Factory"));
}

/*  TROUBLESHOOTING TS-6, PRESET_FORMAT PF-3: only the exact extension is
    scanned, a sub-folder is the category and the root is "User", and a damaged
    file never crashes the scan and refuses to load with a reason. */
LUTHIER_TEST (Presets, onlyTheExactExtensionIsScannedAndSubfoldersAreCategories)
{
    const auto folder = sweepTempFolder ("LuthierSweepPresetScan");
    const auto ext = juce::String (PresetManager::kFileExtension);

    const juce::String good = "{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"SweepScanRoot\",\"parameters\":{}}";
    const juce::String rock = "{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"SweepScanRock\",\"parameters\":{}}";

    folder.getChildFile ("root" + ext).replaceWithText (good);
    folder.getChildFile ("Rock").getChildFile ("rock" + ext).create();
    folder.getChildFile ("Rock").getChildFile ("rock" + ext).replaceWithText (rock);
    folder.getChildFile ("hidden" + ext + ".txt").replaceWithText (
        "{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"SweepScanHidden\",\"parameters\":{}}");
    folder.getChildFile ("SweepScanCut" + ext).replaceWithText ("{\"magic\":\"luthier.preset\",\"name\":\"Sweep");

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    auto& presets = processor.getPresetManager();
    presets.addSearchFolder (folder);
    presets.refresh();

    const int rootIndex = presets.indexOfPreset ("SweepScanRoot");
    const int rockIndex = presets.indexOfPreset ("SweepScanRock");

    CHECK (rootIndex >= 0);
    CHECK (rockIndex >= 0);
    CHECK (presets.indexOfPreset ("SweepScanHidden") < 0);

    if (rootIndex >= 0)
        CHECK_MSG (presets.getPreset (rootIndex)->category == "User", presets.getPreset (rootIndex)->category);

    if (rockIndex >= 0)
        CHECK_MSG (presets.getPreset (rockIndex)->category == "Rock", presets.getPreset (rockIndex)->category);

    // The truncated file is listed under its file name and refuses to load, saying why.
    const int cutIndex = presets.indexOfPreset ("SweepScanCut");
    CHECK (cutIndex >= 0);

    if (cutIndex >= 0)
    {
        CHECK (! presets.loadPreset (cutIndex));
        CHECK (presets.getLastLoadError().isNotEmpty());
    }

    presets.removeSearchFolder (folder);
    folder.deleteRecursively();
}

/*  USER_MANUAL UM-50, PRESET_FORMAT PF-4: saving over a factory preset makes a
    user copy and leaves the factory file alone. */
LUTHIER_TEST (Presets, savingAFactoryPresetMakesAUserCopy)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    auto& presets = processor.getPresetManager();
    presets.refresh();

    int factoryIndex = -1;

    for (int i = 0; i < presets.getNumPresets() && factoryIndex < 0; ++i)
        if (presets.getPreset (i)->isFactory)
            factoryIndex = i;

    CHECK_MSG (factoryIndex >= 0, "no factory preset was found");

    if (factoryIndex < 0)
        return;

    const auto factoryFile = presets.getPreset (factoryIndex)->file;
    const auto name = presets.getPreset (factoryIndex)->name;

    juce::MemoryBlock before;
    CHECK (factoryFile.loadFileAsData (before));

    CHECK (presets.loadPreset (factoryIndex));

    const auto userCopy = PresetManager::getUserPresetFolder().getChildFile ("User")
                            .getChildFile (juce::File::createLegalFileName (name) + PresetManager::kFileExtension);
    const bool existedBefore = userCopy.existsAsFile();

    CHECK (presets.saveCurrent());

    juce::MemoryBlock after;
    CHECK (factoryFile.loadFileAsData (after));
    CHECK_MSG (before == after, "saving changed the factory file " + factoryFile.getFullPathName());
    CHECK_MSG (userCopy.existsAsFile(), "no user copy at " + userCopy.getFullPathName());

    const auto* current = presets.getPreset (presets.getCurrentPresetIndex());
    CHECK (current != nullptr && ! current->isFactory);

    if (! existedBefore)
        userCopy.deleteFile();

    presets.refresh();
}

/*  PROGRESS PR-31, PRESET_FORMAT PF-11: a preset from a newer schema loads and
    the log says so. */
LUTHIER_TEST (Presets, aNewerSchemaPresetLoadsAndIsLogged)
{
    const auto folder = sweepTempFolder ("LuthierSweepNewerSchema");
    ErrorLog::setFolderForTesting (folder);
    ErrorLog::setVerbose (true);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    auto file = folder.getChildFile (juce::String ("Future") + PresetManager::kFileExtension);
    file.replaceWithText ("{\"magic\":\"luthier.preset\",\"schemaVersion\":99,\"name\":\"Future\","
                          "\"someFutureBlock\":{\"x\":1},\"parameters\":{\"macro_drive\":0.9}}");

    CHECK (processor.getPresetManager().loadPreset (file));

    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    CHECK (drive != nullptr && std::abs (drive->getValue() - 0.9f) < 1.0e-3f);

    const auto log = ErrorLog::getLogFile().loadFileAsString();
    CHECK_MSG (log.contains ("NEWER_SCHEMA"), log);

    ErrorLog::setVerbose (false);
    ErrorLog::setFolderForTesting ({});
    folder.deleteRecursively();
}

//==============================================================================
/*  PROGRESS PR-30: the error-log sweep dates a month from its file name, not
    from the file's timestamp. */
LUTHIER_TEST (ErrorLog, retentionSweepDatesFromTheFilename)
{
    const auto folder = sweepTempFolder ("LuthierSweepErrorLogRetention");
    ErrorLog::setFolderForTesting (folder);

    const auto now = juce::Time::getCurrentTime();
    const auto old = folder.getChildFile ("errors-201901.log");
    const auto current = ErrorLog::getLogFile (now);

    old.replaceWithText ("{}\n");          // both freshly written: a fresh mtime
    current.replaceWithText ("{}\n");

    ErrorLog::pruneOldLogs (30);

    CHECK_MSG (! old.exists(), "a 2019 log survived the 30-day sweep");
    CHECK_MSG (current.existsAsFile(), "this month's log was pruned");

    ErrorLog::setFolderForTesting ({});
    folder.deleteRecursively();
}

//==============================================================================
/*  USER_MANUAL UM-41: sustain (64) and sostenuto (66) are skipped while learning. */
LUTHIER_TEST (MidiLearn, sustainAndSostenutoAreSkippedWhileLearning)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    auto& learn = processor.getMidiLearn();
    learn.clearAllMappings();
    learn.startLearning (ParamIDs::macroDrive);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 66, 127), 1);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 64), 2);

    learn.processMidi (midi);
    learn.dispatchPendingLearn();

    CHECK_MSG (learn.getCcForParameter (ParamIDs::macroDrive) == 20,
               "mapped to CC " + juce::String (learn.getCcForParameter (ParamIDs::macroDrive)));
    CHECK (learn.getParameterForCc (64).isEmpty());
    CHECK (learn.getParameterForCc (66).isEmpty());

    learn.clearAllMappings();
}

//==============================================================================
/*  TROUBLESHOOTING TS-24, USER_MANUAL UM-59, include INC-25/INC-29: the crash
    log is off on every load, even when the saved session had it on; when on it
    starts with the report, which carries LICENCE and MIDI sections. */
LUTHIER_TEST (Diagnostics, crashLogIsOffOnEveryLoadAndStartsWithTheReport)
{
    juce::MemoryBlock state;

    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSweepSr, kSweepBlock);
        CHECK (! processor.getDiagnostics().isCrashLogEnabled());

        auto& diagnostics = processor.getDiagnostics();
        diagnostics.setCrashLogEnabled (true);
        CHECK (diagnostics.isCrashLogEnabled());

        const auto report = diagnostics.buildTroubleshootingReport ("{}", "ok");
        diagnostics.flushCrashLog (report);

        const auto file = diagnostics.getCrashLogFile();
        CHECK_MSG (file.existsAsFile(), "no crash log at " + file.getFullPathName());
        CHECK (file.getParentDirectory() == Diagnostics::getDiagnosticsFolder());

        const auto text = file.loadFileAsString();
        CHECK (text.contains ("LUTHIER CRASH LOG"));
        CHECK (text.contains ("LUTHIER TROUBLESHOOTING REPORT"));
        CHECK (text.contains ("---- HOST"));

        // include.md INC-29: the report has LICENCE and MIDI sections too.
        CHECK (report.contains ("---- LICENCE"));
        CHECK (report.contains ("---- MIDI"));
        CHECK (report.contains ("MIDI Learn:"));

        processor.getStateInformation (state);

        diagnostics.setCrashLogEnabled (false);
        file.deleteFile();
    }

    LuthierAudioProcessor reloaded;
    reloaded.prepareToPlay (kSweepSr, kSweepBlock);
    reloaded.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_MSG (! reloaded.getDiagnostics().isCrashLogEnabled(), "the crash log came back on with the session");
}

//==============================================================================
/*  USER_MANUAL UM-51, INCLUDE INC-15, PROGRESS PR-7, spec SP-109: Export audio
    renders the phrase plus its tail on its own thread into WAV, AIFF and FLAC at
    the rate and depth asked for, and normalises to the target. */
LUTHIER_TEST (AudioExporter, rendersThePhraseToWavAiffAndFlac)
{
    const auto folder = sweepTempFolder ("LuthierSweepAudioExport");

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    const auto state = processor.captureStateBlock();
    const auto phrase = AuditionPhrase::build (AuditionPhrase::Type::SingleNote, 120.0);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    struct Case { AudioExporter::Format format; int bits; double rate; bool normalise; };

    for (const auto& c : { Case { AudioExporter::Format::Wav,  24, 48000.0, true },
                           Case { AudioExporter::Format::Aiff, 16, 44100.0, false },
                           Case { AudioExporter::Format::Flac, 24, 48000.0, false } })
    {
        AudioExporter exporter;

        AudioExporter::Options options;
        options.format = c.format;
        options.bitDepth = c.bits;
        options.sampleRate = c.rate;
        options.tailSeconds = 0.5;
        options.normalise = c.normalise;
        options.normaliseTargetDb = -1.0;
        options.outputFile = folder.getChildFile ("take" + AudioExporter::getExtension (c.format));

        const auto label = AudioExporter::getFormatName (c.format);

        // The completion is posted to the message thread after this test is
        // gone, so it must not capture anything of the test's.
        CHECK (exporter.startExport (options, phrase, state,
                                     [] { return LuthierAudioProcessor::createOfflineInstance(); },
                                     [] (const AudioExporter::Result&) {}));

        CHECK (! exporter.startExport (options, phrase, state,
                                       [] { return LuthierAudioProcessor::createOfflineInstance(); },
                                       [] (const AudioExporter::Result&) {}));

        for (int i = 0; i < 30000 && exporter.isExporting(); ++i)
            juce::Thread::sleep (2);

        CHECK_MSG (! exporter.isExporting(), label + " never finished");
        CHECK_MSG (options.outputFile.existsAsFile(), label + ": no file");

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (options.outputFile));
        CHECK_MSG (reader != nullptr, label + ": the file does not read back");

        if (reader == nullptr)
            continue;

        const auto expected = (juce::int64) ((juce::jmax (0.25, phrase.getEndTime()) + options.tailSeconds) * c.rate);

        CHECK_MSG (std::abs (reader->sampleRate - c.rate) < 0.5, label + ": " + juce::String (reader->sampleRate));
        CHECK_MSG ((int) reader->bitsPerSample == c.bits, label + ": " + juce::String ((int) reader->bitsPerSample) + " bits");
        CHECK (reader->numChannels == 2);
        CHECK_MSG (std::abs (reader->lengthInSamples - expected) <= 1,
                   label + ": " + juce::String (reader->lengthInSamples) + " samples, expected " + juce::String (expected));

        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);

        const float peak = audio.getMagnitude (0, audio.getNumSamples());
        CHECK_MSG (peak > 1.0e-3f, label + " is silent");

        if (c.normalise)
            CHECK_MSG (std::abs (juce::Decibels::gainToDecibels (peak) - (-1.0f)) < 0.1f,
                       label + " normalised to " + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dB");
    }

    CHECK (AudioExporter::describeQuality ({}).contains ("48"));

    // include.md INC-16: what the user is told when it ends.
    AudioExporter::Result done;
    done.success = true;
    done.file = folder.getChildFile ("take.wav");
    done.lengthSeconds = 3.25;
    done.peakDb = -1.0;
    done.qualityDescription = AudioExporter::describeQuality ({});

    const auto told = AudioExporter::describeResult (done);
    CHECK (told.contains ("take.wav"));
    CHECK (told.contains (folder.getFullPathName()));
    CHECK (told.contains ("3.25 seconds"));
    CHECK (told.contains (done.qualityDescription));

    AudioExporter::Result failed;
    failed.message = "Could not create the file.";
    CHECK (AudioExporter::describeResult (failed) == failed.message);

    // A path that cannot be written reports failure and leaves nothing behind.
    {
        AudioExporter exporter;
        AudioExporter::Options options;
        options.tailSeconds = 0.0;
        options.outputFile = juce::File ("/proc/luthier-cannot-write-here/take.wav");

        if (juce::File ("/proc").isDirectory())
        {
            CHECK (exporter.startExport (options, phrase, state,
                                         [] { return LuthierAudioProcessor::createOfflineInstance(); },
                                         [] (const AudioExporter::Result&) {}));

            for (int i = 0; i < 30000 && exporter.isExporting(); ++i)
                juce::Thread::sleep (2);

            CHECK (! options.outputFile.exists());
        }
    }

    folder.deleteRecursively();
}

//==============================================================================
/*  MIDI_EXPORT_LUTHIER_PROFILE MX-2 (section 1): SMPTE timing is refused. */
LUTHIER_TEST (MidiExport, smpteTimingIsRefused)
{
    const juce::uint8 bytes[] = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0xE7, 0x28,
                                  'M', 'T', 'r', 'k', 0, 0, 0, 4, 0x00, 0xFF, 0x2F, 0x00 };

    MidiPerformance back;
    const auto result = MidiProfiles::importFromMemory (bytes, sizeof (bytes), back, kSweepSr);

    CHECK (! result.ok);
    CHECK_MSG (result.error.contains ("SMPTE"), result.error);
    CHECK (result.errorByteOffset == 12);
}

/*  MX-16 (section 7): a newer wire version is refused in a known class's SysEx
    and in the header. (Kept-as-bytes for an unknown class is
    MidiExport::everyEventClassRoundTripsWithEveryField's FUTURE_BLOB.) */
LUTHIER_TEST (MidiExport, aNewerWireVersionIsRefusedInAKnownClassAndTheHeader)
{
    MidiPerformance source (kSweepSr);
    source.addMessage (0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100));
    source.addMessage (24000, juce::MidiMessage::noteOff (1, 52));

    auto strum = LuthierEvent::make (LuthierEventClass::strum, 100);
    strum.set ("dir", "down");
    source.addEvent (strum);

    const auto bytes = MidiProfiles::exportToMemory (source, MidiExportOptions {});

    MidiProfiles::SmfFile smf;
    juce::String error;
    juce::int64 at = -1;
    CHECK (MidiProfiles::parseSmf (bytes.getData(), bytes.getSize(), smf, error, at));

    const auto* data = static_cast<const juce::uint8*> (bytes.getData());
    size_t sysExWire = 0, headerWire = 0;

    for (const auto& track : smf.tracks)
    {
        for (const auto& event : track.events)
        {
            // F0 7D 'L' 'T' <wire>: the payload starts at 7D.
            if (sysExWire == 0 && event.isSysEx() && event.dataSize > 4
                  && LuthierEvents::isLuthierSysEx (data + event.dataOffset, (int) event.dataSize))
                sysExWire = event.dataOffset + 3;

            // FF 7F <len> 7D 'L' 'U' 'T' 'H' 'I' 'E' 'R' <wire>
            if (headerWire == 0 && event.isMeta() && event.metaType == 0x7F && event.dataSize > 8
                  && std::memcmp (data + event.dataOffset + 1, "LUTHIER", 7) == 0)
                headerWire = event.dataOffset + 8;
        }
    }

    CHECK (sysExWire > 0);
    CHECK (headerWire > 0);

    // Untouched, the file loads.
    {
        MidiPerformance back;
        const auto result = MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), back, kSweepSr);
        CHECK_MSG (result.ok, result.error);
    }

    for (const auto offset : { sysExWire, headerWire })
    {
        if (offset == 0)
            continue;

        juce::MemoryBlock patched (bytes);
        auto* p = static_cast<juce::uint8*> (patched.getData());
        CHECK (p[offset] == LuthierEvents::kWireVersion);
        p[offset] = (juce::uint8) (LuthierEvents::kWireVersion + 1);

        MidiPerformance back;
        const auto result = MidiProfiles::importFromMemory (patched.getData(), patched.getSize(), back, kSweepSr);

        CHECK_MSG (! result.ok, "a newer wire version at byte " + juce::String ((juce::int64) offset) + " was accepted");
        CHECK_MSG (result.error.contains ("format 2") || result.error.contains ("later version"), result.error);
    }
}

/*  MX-18 (section 8): a Generic import reads channel messages only; a track
    named "Bass..." is part 1 and extension text is ignored. */
LUTHIER_TEST (MidiImport, aGenericBassTrackIsPartOne)
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (960);

    juce::MidiMessageSequence guitar, bass;

    auto guitarName = juce::MidiMessage::textMetaEvent (3, "Guitar");
    guitarName.setTimeStamp (0);
    guitar.addEvent (guitarName);
    guitar.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100).withTimeStamp (0));
    guitar.addEvent (juce::MidiMessage::noteOff (1, 64).withTimeStamp (960));

    auto bassName = juce::MidiMessage::textMetaEvent (3, "Bass guitar");
    bassName.setTimeStamp (0);
    bass.addEvent (bassName);
    auto realism = juce::MidiMessage::textMetaEvent (1, "LUTHIER: STRUM dir=down cv=200");
    realism.setTimeStamp (0);
    bass.addEvent (realism);
    bass.addEvent (juce::MidiMessage::noteOn (2, 40, (juce::uint8) 100).withTimeStamp (0));
    bass.addEvent (juce::MidiMessage::noteOff (2, 40).withTimeStamp (960));

    file.addTrack (guitar);
    file.addTrack (bass);

    juce::MemoryOutputStream out;
    CHECK (file.writeTo (out, 1));

    MidiPerformance back;
    const auto result = MidiProfiles::importFromMemory (out.getData(), out.getDataSize(), back, kSweepSr);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::generic);
    CHECK (result.ignoredRealismTexts == 1);
    CHECK (back.getEvents().empty());

    int guitarNotes = 0, bassNotes = 0;

    for (const auto& m : back.getMessages())
    {
        if (! m.message.isNoteOn())
            continue;

        if (m.message.getNoteNumber() == 64)
        {
            ++guitarNotes;
            CHECK (m.part == 0);
        }
        else if (m.message.getNoteNumber() == 40)
        {
            ++bassNotes;
            CHECK_MSG (m.part == 1, "the Bass track's note came in as part " + juce::String (m.part));
        }
    }

    CHECK (guitarNotes == 1);
    CHECK (bassNotes == 1);
}

//==============================================================================
/*  JUCE_CLAUDE_GUIDELINES JG-18: a host fills its buffers with whatever it
    likes; every enabled output bus comes back written - the strings the guitar
    does not have and a muted aux as silence, never as the host's garbage. */
LUTHIER_TEST (PluginBuses, anEnabledBusNobodyWritesIsSilent)
{
    LuthierAudioProcessor processor;

    auto layout = processor.getBusesLayout();

    for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
    {
        const auto* declared = processor.getBus (false, bus);
        const bool isString = declared != nullptr && declared->getName().startsWith ("String ");
        layout.outputBuses.getReference (bus) = isString ? juce::AudioChannelSet::mono()
                                                         : juce::AudioChannelSet::stereo();
    }

    CHECK_MSG (processor.setBusesLayout (layout), "a host could not enable every bus");

    processor.getRouting().setAuxMuted (0, true);
    processor.prepareToPlay (kSweepSr, 256);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), 256);

    for (int block = 0; block < 4; ++block)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.75f, buffer.getNumSamples());

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    for (int bus = 0; bus < processor.getBusCount (false); ++bus)
    {
        auto out = processor.getBusBuffer (buffer, false, bus);
        const auto name = processor.getBus (false, bus)->getName();

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            CHECK_MSG (out.getMagnitude (ch, 0, out.getNumSamples()) < 0.1f,
                       name + " channel " + juce::String (ch) + " still holds the host's data");

        // Strings 7-12 on a six-string: exactly silent.
        if (name.startsWith ("String ") && name.getTrailingIntValue() > processor.getEngine().getNumStrings())
            CHECK_MSG (out.getMagnitude (0, 0, out.getNumSamples()) == 0.0f, name + " is not silent");
    }
}

//==============================================================================
namespace
{
    void setChoice (LuthierAudioProcessor& processor, const juce::String& id, int index)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    }

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    /** One second of an open-E chord through the whole plugin, left channel. */
    std::vector<float> renderChord (LuthierAudioProcessor& processor)
    {
        setPlain (processor, ParamIDs::macroHumanize, 0.0f);
        processor.prepareToPlay (kSweepSr, 256);
        processor.getParameterBridge().applyAllNow();

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels()), 256);

        for (int block = 0; block < (int) (kSweepSr / 256); ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                for (int note : { 40, 47, 52, 56, 59, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
        }

        return out;
    }

    /** RMS of a - b over RMS of a. */
    double relativeDifference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double diff = 0.0, ref = 0.0;

        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
        {
            diff += ((double) a[i] - (double) b[i]) * ((double) a[i] - (double) b[i]);
            ref += (double) a[i] * (double) a[i];
        }

        return std::sqrt (diff / juce::jmax (1.0e-20, ref));
    }
}

/*  issues ISS-1 / ISS-3: a pedal switched on through its parameters - the way
    the rack and a preset do it - changes what comes out, in the pre and the post
    chain, and a full pre chain differs from an empty one. */
LUTHIER_TEST (Effects, aPedalSwitchedOnThroughItsParametersChangesTheOutput)
{
    LuthierAudioProcessor dryProcessor;

    for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        for (const bool post : { false, true })
            setChoice (dryProcessor, ParamIDs::slotType (post, slot), (int) PedalType::None);

    const auto dry = renderChord (dryProcessor);

    double dryPeak = 0.0;
    for (auto v : dry) dryPeak = juce::jmax (dryPeak, (double) std::abs (v));
    CHECK_MSG (dryPeak > 1.0e-3, "the dry chord is silent");

    struct Rig { const char* name; std::vector<std::pair<bool, PedalType>> pedals; };

    const Rig rigs[] = {
        { "pre Overdrive", { { false, PedalType::Overdrive } } },
        { "post Delay",    { { true, PedalType::Delay } } },
        { "full pre chain", { { false, PedalType::Compressor }, { false, PedalType::Wah },
                              { false, PedalType::EnvelopeFilter }, { false, PedalType::Octaver },
                              { false, PedalType::PitchShifter }, { false, PedalType::Overdrive },
                              { false, PedalType::Fuzz }, { false, PedalType::Boost } } },
    };

    for (const auto& rig : rigs)
    {
        LuthierAudioProcessor wetProcessor;

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            for (const bool post : { false, true })
                setChoice (wetProcessor, ParamIDs::slotType (post, slot), (int) PedalType::None);

        int preSlot = 0, postSlot = 0;

        for (const auto& [post, type] : rig.pedals)
        {
            const int slot = post ? postSlot++ : preSlot++;
            setChoice (wetProcessor, ParamIDs::slotType (post, slot), (int) type);
            setChoice (wetProcessor, ParamIDs::slotBypass (post, slot), 0);
            setPlain (wetProcessor, ParamIDs::slotMix (post, slot), 1.0f);
        }

        const auto wet = renderChord (wetProcessor);
        const double difference = relativeDifference (dry, wet);

        CHECK_MSG (difference > 0.1, juce::String (rig.name) + " changed the output by only "
                                       + juce::String (difference * 100.0, 2) + "%");
    }
}

//==============================================================================
/*  USER_MANUAL UM-8: two comparison slots; A>B copies the current one across,
    from either slot. */
LUTHIER_TEST (Presets, abSlotsCompareAndCopyTheCurrentOneAcross)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    CHECK (drive != nullptr);

    if (drive == nullptr)
        return;

    auto near = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };

    drive->setValueNotifyingHost (0.2f);          // A
    processor.copyAtoB();
    processor.setSlotBActive (true);
    CHECK_MSG (near (drive->getValue(), 0.2f), "B did not receive A: " + juce::String (drive->getValue()));

    drive->setValueNotifyingHost (0.8f);          // B diverges
    processor.setSlotBActive (false);
    CHECK_MSG (near (drive->getValue(), 0.2f), "A was not recalled: " + juce::String (drive->getValue()));

    processor.setSlotBActive (true);
    CHECK_MSG (near (drive->getValue(), 0.8f), "B was not kept: " + juce::String (drive->getValue()));

    // From B, the copy goes to A.
    processor.copyAtoB();
    processor.setSlotBActive (false);
    CHECK_MSG (near (drive->getValue(), 0.8f), "copying from B did not reach A: " + juce::String (drive->getValue()));
}

/*  USER_MANUAL UM-15: the MIDI activity dot is fed by a flag the audio thread
    sets and the header's timer consumes once. */
LUTHIER_TEST (Engine, midiActivityIsFlaggedOnceAndConsumed)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, 256);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), 256);
    juce::MidiBuffer none;
    processor.processBlock (buffer, none);
    processor.getEngine().consumeMidiActivity();                 // whatever start-up left

    processor.processBlock (buffer, none);
    CHECK (! processor.getEngine().consumeMidiActivity());

    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
    processor.processBlock (buffer, note);

    CHECK (processor.getEngine().consumeMidiActivity());
    CHECK (! processor.getEngine().consumeMidiActivity());
}

//==============================================================================
/*  README RM-23, USER_MANUAL UM-49, TROUBLESHOOTING TS-5: Options > FILE
    LOCATIONS has a button for each folder, Add a preset folder and Rescan; a
    rescan finds a preset dropped into an added folder, filed under its
    sub-folder. (The open-folder buttons are only checked for a handler: they
    launch the system file browser.) */
LUTHIER_TEST (Options, fileLocationsHasItsButtonsAndRescanFindsANewPreset)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    FileLocationsPage page (processor);
    page.setSize (800, 600);

    std::map<juce::String, juce::TextButton*> buttons;

    for (auto* child : page.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child))
            buttons[button->getButtonText()] = button;

    for (const auto* text : { "Open user preset folder", "Open render folder", "Open factory preset folder",
                              "Open diagnostics folder", "Add a preset folder...", "Rescan presets", "Remove folder" })
    {
        auto it = buttons.find (text);
        CHECK_MSG (it != buttons.end(), juce::String ("no \"") + text + "\" button");

        if (it != buttons.end())
        {
            CHECK_MSG (it->second->isVisible(), juce::String (text) + " is hidden");
            CHECK_MSG (it->second->onClick != nullptr, juce::String (text) + " does nothing");
        }
    }

    const auto folder = sweepTempFolder ("LuthierSweepRescan");
    auto& presets = processor.getPresetManager();
    presets.addSearchFolder (folder);
    CHECK (presets.indexOfPreset ("SweepRescanFound") < 0);

    folder.getChildFile ("Blues").createDirectory();
    folder.getChildFile ("Blues").getChildFile (juce::String ("found") + PresetManager::kFileExtension)
        .replaceWithText ("{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"SweepRescanFound\",\"parameters\":{}}");

    if (auto it = buttons.find ("Rescan presets"); it != buttons.end() && it->second->onClick != nullptr)
        it->second->onClick();

    const int index = presets.indexOfPreset ("SweepRescanFound");
    CHECK_MSG (index >= 0, "Rescan did not find the new preset");

    if (index >= 0)
        CHECK (presets.getPreset (index)->category == "Blues");

    // spec.md SP-108: the added folder can be removed again; the user folder cannot.
    auto* list = findFirstChildOfType<juce::ListBox> (page);
    auto removeIt = buttons.find ("Remove folder");
    CHECK (list != nullptr && removeIt != buttons.end());

    if (list != nullptr && removeIt != buttons.end())
    {
        list->selectRow (presets.getSearchFolders().indexOf (folder));
        removeIt->second->onClick();
        CHECK (! presets.getSearchFolders().contains (folder));
        CHECK (presets.indexOfPreset ("SweepRescanFound") < 0);

        list->selectRow (presets.getSearchFolders().indexOf (PresetManager::getUserPresetFolder()));
        removeIt->second->onClick();
        CHECK (presets.getSearchFolders().contains (PresetManager::getUserPresetFolder()));
    }

    presets.removeSearchFolder (folder);
    folder.deleteRecursively();
}

//==============================================================================
/*  GAPS GAP-40: a rhythm state from a build that kept its own capo still sets
    the capo (TuningEngine's one capo), and is never written back. */
LUTHIER_TEST (Rhythm, anOldRhythmStateCapoIsAppliedButNotWrittenBack)
{
    LuthierEngine engine;
    engine.prepare (kSweepSr, kSweepBlock);

    auto& rhythm = engine.getRhythmEngine();
    auto state = rhythm.toVar();

    CHECK (! state.hasProperty ("capoFret"));

    if (auto* obj = state.getDynamicObject())
        obj->setProperty ("capoFret", 3);

    rhythm.fromVar (state);

    CHECK_MSG (rhythm.getCapoFret() == 3, "capo " + juce::String (rhythm.getCapoFret()));
    CHECK (! rhythm.toVar().hasProperty ("capoFret"));
}

//==============================================================================
namespace
{
    template <typename T>
    T* findFirstChild (juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            return t;

        for (auto* child : root.getChildren())
            if (auto* found = findFirstChild<T> (*child))
                return found;

        return nullptr;
    }

    juce::TextButton* findTextButton (juce::Component& root, const juce::String& text)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->getButtonText() == text)
                return button;

            if (auto* found = findTextButton (*child, text))
                return found;
        }

        return nullptr;
    }
}

/*  USER_MANUAL UM-5 and UM-14: the header's preset arrows step the bank (one
    undo entry each), the name opens the browser, and Workshop is the WORKSHOP
    tab in Advanced mode and an overlay in Easy mode. */
LUTHIER_TEST (Editor, headerAndFooterDoWhatTheManualSays)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());
    CHECK (window != nullptr);

    if (window == nullptr)
        return;

    window->setVisible (true);
    window->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto* header = findFirstChild<HeaderBar> (*window);
    auto* host = findFirstChild<OverlayHost> (*window);
    CHECK (header != nullptr && host != nullptr);

    if (header == nullptr || host == nullptr)
        return;

    // ---- USER_MANUAL UM-60 / TROUBLESHOOTING TS-16: the footer --------------------
    const auto footer = window->getFooterText();
    CHECK_MSG (footer.contains ("CPU") && footer.contains ("%"), footer);
    CHECK_MSG (footer.contains ("latency " + juce::String (processor.getLatencySamples())), footer);

    // ---- preset arrows -------------------------------------------------------
    auto& presets = processor.getPresetManager();
    CHECK (presets.getNumPresets() > 2);
    CHECK (presets.loadPreset (0));

    auto* next = findTextButton (*header, ">");
    auto* prev = findTextButton (*header, "<");
    CHECK (next != nullptr && prev != nullptr);

    if (next != nullptr && prev != nullptr)
    {
        const bool couldUndo = processor.canUndo();
        next->onClick();
        CHECK_MSG (presets.getCurrentPresetIndex() == 1, "> went to " + juce::String (presets.getCurrentPresetIndex()));
        next->onClick();
        prev->onClick();
        CHECK_MSG (presets.getCurrentPresetIndex() == 1, "< went to " + juce::String (presets.getCurrentPresetIndex()));
        CHECK (processor.canUndo() || couldUndo);
    }

    // ---- the name opens the browser -------------------------------------------
    CHECK (header->onOpenPresetBrowser != nullptr);

    if (header->onOpenPresetBrowser != nullptr)
    {
        header->onOpenPresetBrowser();
        CHECK (dynamic_cast<PresetBrowserPanel*> (host->getCurrentOverlay()) != nullptr);
        host->dismiss();
    }

    // ---- Workshop: overlay in Easy, tab in Advanced ------------------------------
    CHECK (header->onOpenWorkshop != nullptr);

    if (header->onOpenWorkshop == nullptr)
        return;

    auto setAdvanced = [&] (bool advanced)
    {
        if (processor.getUiState().advancedMode != advanced)
            if (const auto* binding = AccessibilitySettings::get().findShortcut ("toggleAdvanced"))
                window->keyPressed (binding->key);

        CHECK (processor.getUiState().advancedMode == advanced);
    };

    setAdvanced (false);
    header->onOpenWorkshop();
    CHECK_MSG (dynamic_cast<WorkshopOverlay*> (host->getCurrentOverlay()) != nullptr,
               "Workshop in Easy mode did not open the Workshop overlay");
    host->dismiss();

    setAdvanced (true);
    header->onOpenWorkshop();

    auto* advanced = findFirstChild<AdvancedPanel> (*window);
    CHECK (advanced != nullptr);

    if (advanced != nullptr)
        CHECK_MSG (advanced->getWorkspaceTabName (advanced->getWorkspaceTab()) == "WORKSHOP",
                   "Workshop in Advanced mode showed " + advanced->getWorkspaceTabName (advanced->getWorkspaceTab()));

    CHECK (! host->isShowingOverlay());
}

//==============================================================================
/*  DECISIONS DEC-49 (second half): reset() clears the cabinet's tail - a
    silent block after a reset is exactly silent, whatever rang before it. */
LUTHIER_TEST (Cabinet, resetClearsTheConvolutionTail)
{
    CabinetEngine cab;
    cab.prepare (kSweepSr, kSweepBlock);

    CabinetConfig config;
    config.cabinet = CabinetType::Cab4x12;
    config.speaker = SpeakerType::Vintage30;
    config.mic = MicType::SM57;
    cab.setConfigA (config);

    juce::AudioBuffer<float> buffer (2, kSweepBlock);

    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    cab.processBlock (buffer);

    buffer.clear();
    cab.processBlock (buffer);
    CHECK_MSG (buffer.getMagnitude (0, kSweepBlock) > 0.0f, "no tail to clear, so this proves nothing");

    cab.reset();

    for (int block = 0; block < 4; ++block)
    {
        buffer.clear();
        cab.processBlock (buffer);
        CHECK_MSG (buffer.getMagnitude (0, kSweepBlock) == 0.0f,
                   "block " + juce::String (block) + " after reset carries "
                     + juce::String (buffer.getMagnitude (0, kSweepBlock)));
    }
}

//==============================================================================
namespace
{
    /** One and a half seconds of a hard-hit chord straight through the engine. */
    juce::AudioBuffer<float> renderEngineChord (LuthierEngine& engine)
    {
        engine.reset();

        const int total = (int) (kSweepSr * 1.5);
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> block (2, kSweepBlock);

        for (int position = 0; position < total; position += kSweepBlock)
        {
            const int count = juce::jmin (kSweepBlock, total - position);
            block.setSize (2, count, false, false, true);
            block.clear();

            juce::MidiBuffer midi;

            if (position == 0)
                for (int note : { 40, 47, 52, 55, 59, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);

            engine.processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, position, block, ch, 0, count);
        }

        return out;
    }
}

/*  DECISIONS DEC-52: the tone strip's wet/dry and width sit ahead of the master
    limiter - so no blend of a hot rig passes the ceiling - and width 0 is mono. */
LUTHIER_TEST (Engine, wetDryIsBeforeTheLimiterAndWidthZeroIsMono)
{
    LuthierEngine engine;
    engine.prepare (kSweepSr, kSweepBlock);
    engine.getMasterBus().setGainDb (18.0);
    engine.getPostEffects().setSlotType (0, PedalType::Chorus);

    for (const double mix : { 0.0, 0.5, 1.0 })
    {
        engine.setOutputMix (mix);
        engine.setStereoWidth (1.0);

        const auto out = renderEngineChord (engine);
        const float peak = juce::jmax (out.getMagnitude (0, 0, out.getNumSamples()),
                                       out.getMagnitude (1, 0, out.getNumSamples()));

        CHECK_MSG (peak > 0.1f, "mix " + juce::String (mix) + " is too quiet to prove anything");
        CHECK_MSG (peak <= 1.0f, "mix " + juce::String (mix) + " passed the ceiling: "
                                   + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dBFS");
    }

    engine.setOutputMix (1.0);
    engine.setStereoWidth (0.0);
    renderEngineChord (engine);                 // let the width smoother arrive
    const auto mono = renderEngineChord (engine);

    float worst = 0.0f;

    for (int i = 0; i < mono.getNumSamples(); ++i)
        worst = juce::jmax (worst, std::abs (mono.getSample (0, i) - mono.getSample (1, i)));

    CHECK_MSG (worst < 1.0e-5f, "width 0 left L and R apart by " + juce::String (worst));
}

//==============================================================================
namespace
{
    /*  UiPreferences writes through to the user's real config file (the first
        unlock marks the explainer as shown); put it back as it was. */
    struct SweepPreservedPreferences
    {
        SweepPreservedPreferences()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String())
        {
        }

        ~SweepPreservedPreferences()
        {
            if (existed)
                file.replaceWithText (contents);
            else
                file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };
}

/*  DECISIONS DEC-4: the Options RANGES master toggle clears per-control unlocks,
    both ways - a lock takes them away, and an unlock-all makes them redundant. */
LUTHIER_TEST (RangesUi, theMasterToggleClearsPerControlUnlocks)
{
    const SweepPreservedPreferences preserved;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSweepSr, kSweepBlock);

    juce::Component owner;
    const juce::String gain (ParamIDs::ampGain);

    RangesPage page (processor);
    page.setSize (700, 500);

    for (const bool unlockAll : { false, true })
    {
        applyParameterMenuResult (kUnlockRangeMenuId, owner, processor, gain);
        CHECK (processor.getRanges().isUnlockedIndividually (gain));

        page.setAllFamilies (unlockAll);

        CHECK_MSG (! processor.getRanges().isUnlockedIndividually (gain),
                   juce::String (unlockAll ? "unlock all" : "lock all") + " kept the per-control unlock");

        page.setAllFamilies (false);
    }
}
