/*  SPEC-SWEEP: the preset carries the processor's blocks.

    state-model.md 1 and file-formats.md 2 put the modulation matrix, the
    snapshot bank, the MIDI Learn mappings, the rhythm engine, the routing, the
    character and the tone-match slots inside the `.luthierpreset`. They used to
    survive only in host session state, so a preset file lost them, and a
    setlist entry recalled its snapshot from the previous preset's bank.

    SM-1, SM-16, SM-59, FF-24..27, FF-29, RIO-30..32, LP-9, RE-40, PF-14, SM-33,
    SM-46.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::File scratchFolder()
    {
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("luthier-preset-block-tests");
        folder.createDirectory();
        return folder;
    }

    juce::File writePreset (PresetManager& presets, const juce::String& name)
    {
        auto file = scratchFolder().getNonexistentChildFile (name, PresetManager::kFileExtension, false);
        file.replaceWithText (juce::JSON::toString (presets.toVar (name), false));
        return file;
    }

    void setGain (LuthierAudioProcessor& processor, float normalised)
    {
        if (auto* gain = processor.getState().getParameter (ParamIDs::ampGain))
            gain->setValueNotifyingHost (normalised);
    }

    float gain (LuthierAudioProcessor& processor)
    {
        auto* p = processor.getState().getParameter (ParamIDs::ampGain);
        return p != nullptr ? p->getValue() : -1.0f;
    }

    void render (LuthierAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            processor.getSnapshots().advancePending();
        }
    }

    /** Every block this suite puts in a preset, set away from its default. */
    void dressEveryBlock (LuthierAudioProcessor& processor)
    {
        ModRoute route;
        route.sourceId = "macro1";
        route.destinationId = ParamIDs::ampGain;
        route.depth = 0.5f;
        processor.getModMatrix().addRoute (route);

        processor.getMidiLearn().addMapping (ParamIDs::ampGain, 74, 0);
        processor.getEngine().getRhythmEngine().setEnabled (true);

        auto& routing = processor.getRouting();
        routing.setAuxMuted ((int) AuxBus::di, true);
        routing.setSidechainToAmp (true);

        auto cfg = routing.getMidiOutConfig();
        cfg.enabled = true;
        cfg.channel = 7;
        cfg.macroCc[0] = 21;
        routing.setMidiOutConfig (cfg);

        processor.getEngine().getCharacterEngine().setSeed (123456789ull);

        setGain (processor, 0.2f);
        processor.captureSnapshot (0, "Verse");
        setGain (processor, 0.8f);
        processor.captureSnapshot (1, "Chorus");
    }
}

//==============================================================================
/*  FF-24..27, FF-29, RIO-30..32, LP-9, RE-40, SM-1, SM-16, SM-59: save, load
    something else, load the file back, and every block is the saved one. The
    something else is a factory preset, which carries none of the blocks - so
    the middle step also checks that an absent block means its default. */
LUTHIER_TEST (Presets, processorBlocksTravelInThePresetFile)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 1)
    {
        CHECK_MSG (false, "no factory presets to load, so this proves nothing");
        return;
    }

    dressEveryBlock (processor);
    const auto layoutBefore = processor.getRouting().getActiveLayout();
    const auto file = writePreset (presets, "Every Block");

    // ---- the file says so ------------------------------------------------------
    {
        const auto parsed = juce::JSON::parse (file);

        for (auto key : { "modulation", "snapshots", "midi_mappings", "rhythm_engine",
                          "routing", "character", "tone_match" })
            CHECK_MSG (parsed.hasProperty (key), juce::String ("the preset file has no ") + key + " block");
    }

    // ---- a preset without the blocks puts them back to their defaults ----------
    CHECK (presets.loadPreset (0));

    CHECK_MSG (processor.getModMatrix().getNumRoutes() == 0, "the last preset's mod route survived a load");
    // Except MIDI Learn: a preset without mappings keeps the controller setup
    // (live-performance 11, docs/PRESET_FORMAT.md). Cleared here so the load
    // below has to bring it back.
    CHECK_MSG (processor.getMidiLearn().getNumMappings() == 1,
               "a preset without MIDI mappings wiped the controller setup");
    processor.getMidiLearn().fromVar (juce::var (juce::Array<juce::var>()));
    CHECK_MSG (! processor.getEngine().getRhythmEngine().isEnabled(), "the rhythm engine stayed on across a load");
    CHECK_MSG (! processor.getRouting().isAuxMuted ((int) AuxBus::di), "the last preset's aux mute survived a load");
    CHECK_MSG (! processor.getRouting().isSidechainToAmp(), "sidechain-to-amp survived a load");
    CHECK_MSG (processor.getSnapshots().getNumSnapshots() == 0, "the last preset's snapshot bank survived a load");
    CHECK_MSG (processor.getEngine().getCharacterEngine().getSeed() != 123456789ull,
               "the last preset's character seed survived a load");

    // ---- and loading the file brings every one of them back --------------------
    CHECK (presets.loadPreset (file));

    const auto routes = processor.getModMatrix().getRoutes();
    CHECK_MSG (routes.size() == 1 && routes[0].destinationId == ParamIDs::ampGain,
               "modulation did not travel in the preset file");
    CHECK_MSG (processor.getMidiLearn().getNumMappings() == 1, "MIDI Learn mappings did not travel");
    CHECK_MSG (processor.getEngine().getRhythmEngine().isEnabled(), "the rhythm engine state did not travel");

    const auto& routing = processor.getRouting();
    CHECK_MSG (routing.isAuxMuted ((int) AuxBus::di), "the aux mute did not travel (RIO-30)");
    CHECK_MSG (routing.isSidechainToAmp(), "sidechain-to-amp did not travel (RIO-32)");

    const auto cfg = routing.getMidiOutConfig();
    CHECK_MSG (cfg.enabled && cfg.channel == 7 && cfg.macroCc[0] == 21, "the MIDI-out config did not travel (RIO-31)");
    CHECK_MSG (routing.getActiveLayout() == layoutBefore, "a preset load changed the bus layout");

    CHECK_MSG (processor.getEngine().getCharacterEngine().getSeed() == 123456789ull,
               "the character seed did not travel");

    CHECK_MSG (processor.getSnapshots().getNumSnapshots() == 2, "the snapshot bank did not travel (LP-9)");

    if (processor.getSnapshots().getNumSnapshots() == 2)
        CHECK (processor.getSnapshots().getSnapshot (1).label == "Chorus");

    file.deleteFile();
}

//==============================================================================
/*  SM-33 (live-performance 4): a setlist entry's snapshot index is into that
    entry's preset's bank, not whichever bank was loaded before. */
LUTHIER_TEST (LiveSetlist, anEntrySnapshotRecallsFromItsPresetsBank)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    // The entry's preset: snapshot 1 is loud.
    setGain (processor, 0.1f);
    processor.captureSnapshot (0, "Quiet");
    setGain (processor, 0.9f);
    processor.captureSnapshot (1, "Loud");
    setGain (processor, 0.5f);

    const auto file = writePreset (presets, "Setlist Entry");

    // What is loaded before the entry: a bank whose snapshot 1 is quiet.
    processor.getSnapshots().clear();
    setGain (processor, 0.3f);
    processor.captureSnapshot (0, "A");
    setGain (processor, 0.05f);
    processor.captureSnapshot (1, "B");
    setGain (processor, 0.3f);

    Setlist list;
    SetlistEntry entry;
    entry.presetPath = file.getFullPathName();
    entry.snapshotIndex = 1;
    list.addEntry (entry);

    processor.getSetlist().setSetlist (list);
    CHECK (processor.getSetlist().goTo (0));
    CHECK (processor.applyCurrentSetlistEntry());

    render (processor, 24);

    CHECK_MSG (processor.getSnapshots().getNumSnapshots() == 2
                 && processor.getSnapshots().getSnapshot (1).label == "Loud",
               "the entry did not bring its preset's snapshot bank");
    CHECK_MSG (gain (processor) > 0.8f,
               "the entry's snapshot was recalled from the previously loaded bank");

    file.deleteFile();
}

//==============================================================================
/*  PF-14 (docs/PRESET_FORMAT.md): a key the file leaves out is that
    parameter's default, not the previous preset's value. */
LUTHIER_TEST (Presets, aMissingKeyFallsBackToItsDefault)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();
    auto* drive = processor.getState().getParameter (ParamIDs::macroDrive);
    auto* level = processor.getState().getParameter (ParamIDs::masterGain);

    CHECK (drive != nullptr && level != nullptr);

    if (drive == nullptr || level == nullptr)
        return;

    drive->setValueNotifyingHost (drive->getDefaultValue() > 0.5f ? 0.05f : 0.95f);

    // A hand-written preset that names one parameter.
    auto file = scratchFolder().getNonexistentChildFile ("Minimal", PresetManager::kFileExtension, false);
    file.replaceWithText (juce::String ("{ \"magic\": \"") + PresetManager::kMagic
                            + "\", \"schemaVersion\": 1, \"name\": \"Minimal\","
                              " \"parameters\": { \"" + ParamIDs::masterGain + "\": 0.25 } }");

    CHECK (presets.loadPreset (file));

    CHECK_NEAR (level->getValue(), 0.25f, 1.0e-4f);
    CHECK_MSG (std::abs (drive->getValue() - drive->getDefaultValue()) < 1.0e-4f,
               "a parameter the preset does not name kept the last preset's value");

    file.deleteFile();
}

//==============================================================================
/*  SM-46 (state-model.md 8.1): a preset load clears A/B compare and says so. */
LUTHIER_TEST (StateModel, aPresetLoadClearsABCompareWithABanner)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 1)
    {
        CHECK_MSG (false, "no factory presets to load, so this proves nothing");
        return;
    }

    processor.setSlotBActive (true);
    processor.takeStateNotices();

    CHECK (presets.loadPreset (0));

    CHECK_MSG (! processor.isSlotBActive(), "a preset load left A/B on slot B");
    CHECK_MSG (processor.takeStateNotices().contains ("A/B cleared by preset load."),
               "the A/B clear was silent");

    // Toggling A/B afterwards has nothing stale to go back to: B starts as a copy
    // of what is now loaded.
    setGain (processor, 0.33f);
    processor.setSlotBActive (true);
    processor.setSlotBActive (false);
    CHECK_NEAR (gain (processor), 0.33f, 1.0e-3f);

    // An undo or an A/B toggle goes through the session state, not a preset
    // file load, and must not clear anything.
    processor.setSlotBActive (true);
    processor.takeStateNotices();
    processor.setSlotBActive (false);
    CHECK_MSG (processor.takeStateNotices().isEmpty(), "an A/B toggle cleared A/B");
}

//==============================================================================
/*  SM-8 (state-model.md 1, 10): session state is not saved. MIDI Learn arm and
    the A/B buffers do not come back from the host session. */
LUTHIER_TEST (StateModel, sessionStateIsNotSaved)
{
    juce::MemoryBlock state;

    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        processor.getMidiLearn().setArmed (true);
        processor.copyAtoB();
        processor.getStateInformation (state);
    }

    LuthierAudioProcessor restored;
    restored.prepareToPlay (kSr, kBlock);
    restored.setStateInformation (state.getData(), (int) state.getSize());

    CHECK_MSG (! restored.getMidiLearn().isArmed(), "MIDI Learn arm came back from the session");
}

//==============================================================================
/*  SM-39 (state-model.md 8.1): a preset load during a snapshot recall's
    crossfade supersedes the recall; the loaded values win. */
LUTHIER_TEST (StateModel, aPresetLoadSupersedesAPendingRecall)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    setGain (processor, 0.9f);
    processor.captureSnapshot (0, "Loud");
    setGain (processor, 0.15f);
    const auto file = writePreset (presets, "Quiet Preset");   // gain 0.15, bank {Loud}

    processor.getSnapshots().clear();
    setGain (processor, 0.9f);
    processor.captureSnapshot (0, "Loud");
    setGain (processor, 0.5f);

    CHECK (processor.recallSnapshot (0));
    render (processor, 1);                       // the crossfade has started
    CHECK (presets.loadPreset (file));           // and the load lands inside it
    render (processor, 24);

    CHECK_MSG (std::abs (gain (processor) - 0.15f) < 1.0e-3f,
               "a recall that started before a preset load finished after it");

    file.deleteFile();
}

//==============================================================================
/*  SM-22 (state-model.md 3 step 2): a recall brings back the mod matrix the
    snapshot captured. */
LUTHIER_TEST (LiveSnapshots, recallRestoresTheModMatrix)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = ParamIDs::ampGain;
    route.depth = 0.5f;
    CHECK (processor.getModMatrix().addRoute (route));

    CHECK (processor.captureSnapshot (0, "Routed"));

    processor.getModMatrix().clearRoutes();
    route.destinationId = ParamIDs::masterGain;
    CHECK (processor.getModMatrix().addRoute (route));
    CHECK (processor.captureSnapshot (1, "Other"));

    CHECK (processor.recallSnapshot (0));
    render (processor, 24);

    const auto routes = processor.getModMatrix().getRoutes();
    CHECK_MSG (routes.size() == 1 && routes[0].destinationId == ParamIDs::ampGain,
               "recalling a snapshot did not bring back its mod route");
}

//==============================================================================
/*  SM-43 (state-model.md 8.1, 8.7): MIDI Learn's targets are parameters, and
    every preset has every parameter, so an arm or a learn in progress always
    survives a load. */
LUTHIER_TEST (StateModel, aLearnInProgressSurvivesAPresetLoad)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& presets = processor.getPresetManager();

    if (presets.getNumPresets() < 1)
        return;

    processor.getMidiLearn().startLearning (ParamIDs::ampGain);
    CHECK (presets.loadPreset (0));

    CHECK_MSG (processor.getMidiLearn().isLearning()
                 && processor.getMidiLearn().getLearningParameterId() == ParamIDs::ampGain,
               "a preset load dropped a MIDI Learn in progress");
}
