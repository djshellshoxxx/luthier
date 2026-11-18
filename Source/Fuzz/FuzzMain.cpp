/*  libFuzzer entry for Luthier's untrusted-input parsers.

    Build:  cmake -B build-fuzz -DLUTHIER_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
            cmake --build build-fuzz --target LuthierFuzz
    Run:    LUTHIER_FUZZ_TARGET=host_state build-fuzz/LuthierFuzz_artefacts/Release/LuthierFuzz corpus/host_state -max_total_time=600

    Targets (LUTHIER_FUZZ_TARGET):
      host_state     LuthierAudioProcessor::setStateInformation, then a save and a block of audio
      board_state    reference::BoardState::restore into a BoardModel, then save
      board_extract  reference::BoardState::extractFromHostJson
      preset_json    PresetManager::fromVar of parsed JSON
      tune_json      TuneFile::fromVar of parsed JSON
      midi           importMidiData (a .mid from memory)
      gp_legacy      GuitarProLegacyReader::read (Guitar Pro 3/4/5 bytes)
      ascii_tab      NotationImporter::readAsciiTab
      musicxml       NotationImporter::readMusicXml

    Every input is hostile: the only acceptable outcomes are a clean refusal or
    a finite, bounded result. Sanitizers (address, undefined) report the rest.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardState.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../Tune/TuneFile.h"
#include "../Tune/TuneImport.h"
#include "../Notation/GuitarProLegacyReader.h"
#include "../Notation/NotationExport.h"

using namespace luthier;

namespace
{
enum class Target { hostState, boardState, boardExtract, presetJson, tuneJson, midi, gpLegacy, asciiTab, musicXml };

Target target = Target::hostState;
// Deliberately never destroyed: JUCE creates some of its own statics lazily
// (ShutdownDetector, the timer thread) after these exist, so any exit-time
// teardown order touches freed JUCE state. They stay reachable, so
// LeakSanitizer does not report them; the process exit reclaims them.
juce::ScopedJuceInitialiser_GUI* juceInit = nullptr;
LuthierAudioProcessor* processor = nullptr;

juce::String asText (const uint8_t* data, size_t size)
{
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (data), (int) size);
}

void requireFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (c, i)) || std::abs (buffer.getSample (c, i)) > 64.0f)
                std::abort();   // a restored state must never make the engine blow up
}

void runHostState (const uint8_t* data, size_t size)
{
    processor->setStateInformation (data, (int) size);
    juce::MemoryBlock saved;
    processor->getStateInformation (saved);

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor->getTotalNumOutputChannels()), 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
    buffer.clear();
    processor->processBlock (buffer, midi);
    requireFinite (buffer);
}

void runBoardState (const uint8_t* data, size_t size)
{
    reference::BoardModel model;
    juce::String error;
    if (reference::BoardState::restore (model, asText (data, size), &error))
    {
        juce::String again;
        reference::BoardState::save (model, again, &error);
    }
}

void runBoardExtract (const uint8_t* data, size_t size)
{
    juce::String board, error;
    reference::BoardState::extractFromHostJson (asText (data, size), board, &error);
}

void runPresetJson (const uint8_t* data, size_t size)
{
    juce::var parsed;
    if (juce::JSON::parse (asText (data, size), parsed).wasOk())
        processor->getPresetManager().fromVar (parsed);
}

void runTuneJson (const uint8_t* data, size_t size)
{
    juce::var parsed;
    if (juce::JSON::parse (asText (data, size), parsed).wasOk())
    {
        Tune tune;
        TuneFile::fromVar (parsed, tune);
    }
}

void runMidi (const uint8_t* data, size_t size)
{
    Tune tune;
    juce::String error;
    importMidiData (data, size, "fuzz.mid", tune, TuneImportOptions {}, error);
}
void runGpLegacy (const uint8_t* data, size_t size)
{
    GuitarProLegacyReader reader;
    PerformanceScore score;
    reader.read (data, size, score);
}

void runAsciiTab (const uint8_t* data, size_t size)
{
    NotationImporter importer;
    PerformanceScore score;
    importer.readAsciiTab (asText (data, size), score);
}

void runMusicXml (const uint8_t* data, size_t size)
{
    NotationImporter importer;
    PerformanceScore score;
    importer.readMusicXml (asText (data, size), score);
}
} // namespace

extern "C" int LLVMFuzzerInitialize (int*, char***)
{
    juceInit = new juce::ScopedJuceInitialiser_GUI();

    const char* name = std::getenv ("LUTHIER_FUZZ_TARGET");
    const juce::String t (name != nullptr ? name : "host_state");
    target = t == "board_state"   ? Target::boardState
           : t == "board_extract" ? Target::boardExtract
           : t == "preset_json"   ? Target::presetJson
           : t == "tune_json"     ? Target::tuneJson
           : t == "midi"          ? Target::midi
           : t == "gp_legacy"     ? Target::gpLegacy
           : t == "ascii_tab"     ? Target::asciiTab
           : t == "musicxml"      ? Target::musicXml
                                  : Target::hostState;

    reference::PedalRegistry::registerAll();

    if (target == Target::hostState || target == Target::presetJson)
    {
        processor = new LuthierAudioProcessor();
        processor->prepareToPlay (48000.0, 256);
    }

    // LUTHIER_FUZZ_WRITE_SEEDS=<dir>: write real session states (and their
    // preset sections) for the first factory presets into <dir>/host_state and
    // <dir>/preset_json, so the search starts from valid, deep inputs.
    if (const char* seeds = std::getenv ("LUTHIER_FUZZ_WRITE_SEEDS"); seeds != nullptr && processor != nullptr)
    {
        const juce::File root (juce::String::fromUTF8 (seeds));
        const auto hostDir = root.getChildFile ("host_state"), presetDir = root.getChildFile ("preset_json");
        hostDir.createDirectory();
        presetDir.createDirectory();
        auto& presets = processor->getPresetManager();
        for (int i = 0; i < juce::jmin (25, presets.getNumPresets()); ++i)
        {
            presets.loadPreset (i);
            juce::MemoryBlock state;
            processor->getStateInformation (state);
            hostDir.getChildFile ("seed" + juce::String (i) + ".json").replaceWithData (state.getData(), state.getSize());
            juce::var parsed;
            if (juce::JSON::parse (state.toString(), parsed).wasOk())
                presetDir.getChildFile ("seed" + juce::String (i) + ".json")
                         .replaceWithText (juce::JSON::toString (parsed.getProperty ("preset", {}), true));
        }
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    if (size > 512 * 1024)
        return -1;   // keep inputs to plausible session sizes; larger ones only slow the search

    switch (target)
    {
        case Target::hostState:    runHostState (data, size); break;
        case Target::boardState:   runBoardState (data, size); break;
        case Target::boardExtract: runBoardExtract (data, size); break;
        case Target::presetJson:   runPresetJson (data, size); break;
        case Target::tuneJson:     runTuneJson (data, size); break;
        case Target::midi:         runMidi (data, size); break;
        case Target::gpLegacy:     runGpLegacy (data, size); break;
        case Target::asciiTab:     runAsciiTab (data, size); break;
        case Target::musicXml:     runMusicXml (data, size); break;
    }
    return 0;
}
