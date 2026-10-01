/*  luthier-render - offline batch rendering.

    Renders a MIDI file to audio through the full Luthier engine, with a preset,
    from the command line. Useful for batch work, for regression-testing a build
    against known-good renders, and for anyone who would rather not open a DAW.

        luthier-render --midi riff.mid --out riff.wav
        luthier-render --midi riff.mid --preset "Modern Metal Chug" --out riff.wav
        luthier-render --midi riff.mid --preset-file my.luthierpreset --out riff.wav
        luthier-render --list-presets
        luthier-render --audition "Major Scale" --guitar "Les Paul" --out demo.wav

    It runs the same code the plugin does, so what comes out is what the plugin
    would have played.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "../Source/LuthierEngine.h"
#include "../Source/Parameters.h"
#include "../Source/Presets/PresetManager.h"
#include "../Source/Presets/FactoryPresets.h"
#include "../Source/Support/AudioExporter.h"
#include "../Source/Support/IrLibrary.h"
#include "../Source/Rhythm/GenreKit.h"
#include "../Source/PluginProcessor.h"                      // output-normalization.md 4.4
#include "../Source/Support/NormalizationCalibrator.h"      // output-normalization.md 4.4
#include "../Source/Riffs/RiffLibrary.h"        // riff-library 2.3: --export-riffs
#include "../Source/Riffs/RiffDestinations.h"
#include "../Source/Presets/Preview/FactoryPreviews.h"   // preset-browser-previews.md 2 (FEAT-BROWSER)

using namespace luthier;

namespace
{
//==============================================================================
/** The same minimal host the tests use: a real APVTS and a real engine, without
    the plugin wrapper. */
class RenderHost : public juce::AudioProcessor
{
public:
    RenderHost()
        : AudioProcessor (BusesProperties()
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
          bridge (apvts, engine),
          presets (*this, apvts, engine, ranges)
    {
        // Same order as the plugin: the recipes need the parameter ranges before
        // the bank can be written, and the bank has to be on disk before a scan
        // will find it. Without this the CLI lists no presets at all.
        FactoryPresets::setProcessorForRanges (this);
        bridge.cachePointers();
        presets.ensureFactoryPresetsInstalled();
        presets.refresh();
    }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        engine.prepare (sampleRate, blockSize);
        bridge.applyAllNow();
    }

    void releaseResources() override { engine.releaseResources(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        bridge.applyToEngine();
        engine.processBlock (buffer, midi);
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "luthier-render"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    LuthierEngine engine;
    juce::AudioProcessorValueTreeState apvts;
    ParameterBridge bridge;
    RangeState ranges;   // advanced-ranges.md: presets carry their ranges block
    PresetManager presets;
};

//==============================================================================
struct Options
{
    juce::File midiFile;
    juce::File presetFile;
    juce::File outputFile;

    juce::String presetName;
    juce::String guitarName;
    juce::String auditionName;

    double sampleRate = 48000.0;
    int bitDepth = 24;
    double tailSeconds = 4.0;
    double tempo = 120.0;
    bool normalise = false;
    double normaliseDb = -1.0;
    bool verbose = false;

    bool listPresets = false;
    bool listGuitars = false;
    bool listPhrases = false;
    bool showHelp = false;

    juce::File writeRhythmResourcesTo;

    // output-normalization.md 4.4: build Resources/NormalizationFactory.json.
    juce::File calibrateFactoryTo;
    juce::File exportRiffsTo;          ///< riff-library 2.3
    bool riffsGeneric = false;
    juce::File renderPreviewsTo;   // preset-browser-previews.md 2
};

void printUsage()
{
    std::cout <<
        "luthier-render " JucePlugin_VersionString "  -  offline rendering for Luthier\n"
        "\n"
        "USAGE\n"
        "  luthier-render --midi <file> --out <file> [options]\n"
        "  luthier-render --audition <phrase> --out <file> [options]\n"
        "\n"
        "SOURCE (one required)\n"
        "  --midi <file>            a .mid file to render\n"
        "  --audition <name>        render a built-in phrase instead\n"
        "\n"
        "SOUND\n"
        "  --preset <name>          load a factory or user preset by name\n"
        "  --preset-file <file>     load a .luthierpreset from disk\n"
        "  --guitar <name>          override the instrument\n"
        "\n"
        "OUTPUT\n"
        "  --out <file>             where to write; the extension picks the format\n"
        "                           (.wav, .aiff, .flac)\n"
        "  --rate <hz>              sample rate, default 48000\n"
        "  --depth <bits>           16, 24 or 32, default 24\n"
        "  --tail <seconds>         extra time after the last note, default 4\n"
        "  --tempo <bpm>            for tempo-synced effects, default 120\n"
        "  --normalise [dBFS]       normalise the result, default target -1 dBFS\n"
        "\n"
        "PRESET PREVIEWS\n"
        "  --render-previews <dir>  render every factory preset's preview into\n"
        "                           <dir>/Previews (Ogg + previews.json) and write\n"
        "                           <dir>/descriptor-calibration.json\n"
        "\n"
        "INFORMATION\n"
        "  --calibrate-factory <f>  measure every factory preset x guitar type for output\n"
        "                           normalization and write the factory table (4.4)\n"
        "  --export-riffs <dir>     write every factory riff as a .mid file, by genre\n"
        "  --profile luthier|generic  the .mid profile for --export-riffs, default luthier\n"
        "  --list-presets           list every preset that can be loaded\n"
        "  --list-guitars           list every instrument\n"
        "  --list-phrases           list the built-in audition phrases\n"
        "  --verbose                report progress and settings\n"
        "  --help                   this message\n"
        << std::endl;
}

bool parseArguments (int argc, char* argv[], Options& options)
{
    auto next = [argc, argv] (int& i) -> juce::String
    {
        if (i + 1 < argc)
            return juce::String (argv[++i]);

        return {};
    };

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--help" || arg == "-h")            options.showHelp = true;
        else if (arg == "--midi")                      options.midiFile = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--out" || arg == "-o")        options.outputFile = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--preset")                    options.presetName = next (i);
        else if (arg == "--preset-file")               options.presetFile = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--guitar")                    options.guitarName = next (i);
        else if (arg == "--audition")                  options.auditionName = next (i);
        else if (arg == "--rate")                      options.sampleRate = next (i).getDoubleValue();
        else if (arg == "--depth")                     options.bitDepth = next (i).getIntValue();
        else if (arg == "--tail")                      options.tailSeconds = next (i).getDoubleValue();
        else if (arg == "--tempo")                     options.tempo = next (i).getDoubleValue();
        else if (arg == "--verbose" || arg == "-v")    options.verbose = true;
        else if (arg == "--list-presets")              options.listPresets = true;
        else if (arg == "--list-guitars")              options.listGuitars = true;
        else if (arg == "--list-phrases")              options.listPhrases = true;
        else if (arg == "--calibrate-factory")         options.calibrateFactoryTo = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--export-riffs")              options.exportRiffsTo = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--profile")                   options.riffsGeneric = next (i).equalsIgnoreCase ("generic");
        else if (arg == "--render-previews")           options.renderPreviewsTo = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--write-rhythm-resources")    options.writeRhythmResourcesTo = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--normalise" || arg == "--normalize")
        {
            options.normalise = true;

            // An optional numeric argument.
            if (i + 1 < argc && ! juce::String (argv[i + 1]).startsWith ("-"))
                options.normaliseDb = next (i).getDoubleValue();
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }

    return true;
}

//==============================================================================
int listPresets (RenderHost& host)
{
    auto& presets = host.presets;

    std::cout << presets.getNumPresets() << " presets\n" << std::endl;

    juce::String lastCategory;

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto* info = presets.getPreset (i);

        if (info == nullptr)
            continue;

        if (info->category != lastCategory)
        {
            std::cout << "\n" << info->category << std::endl;
            lastCategory = info->category;
        }

        std::cout << "  " << info->name.paddedRight (' ', 28)
                  << (info->isFactory ? "factory" : "user") << std::endl;
    }

    std::cout << std::endl;
    return 0;
}

//==============================================================================
/*  The factory patterns and genre kits are built in code, so that the plugin has
    them whether or not its resource folder survived installation. rhythm-engine
    sections 6 and 7 also want them on disk as editable files, and this writes
    that copy from the same tables, so the two can never drift apart.
*/
/*  riff-library 2.3: batch .mid packs, for marketing and review, through the
    same C++ path as the drag-out (RiffDestinations::writeDragFile). */
int exportRiffs (const juce::File& root, bool generic)
{
    RiffLibrary library;
    library.setFolders (RiffLibrary::getDefaultFactoryFolder(), {}, {});
    library.loadIndexNow();

    if (library.isFactoryMissing() || library.getNumEntries() == 0)
    {
        std::cerr << "Factory riffs not found beside the renderer." << std::endl;
        return 1;
    }

    int written = 0;

    for (int i = 0; i < library.getNumEntries(); ++i)
    {
        const auto* entry = library.getEntry (i);
        const auto riff = library.getRiff (entry->id);

        if (riff == nullptr)
        {
            std::cerr << "Unreadable: " << entry->file.getFullPathName() << std::endl;
            continue;
        }

        const auto compiled = RiffCompiler::compile (*riff, {}, GuitarSpecSummary::forRiff (*riff));
        const int genre = RiffVocabulary::indexOfGenre (riff->genre);
        const auto folder = root.getChildFile (genre >= 0 ? RiffVocabulary::genres()[(size_t) genre].folder : "User");

        juce::String error;
        const auto file = RiffDestinations::writeDragFile (*riff, *compiled, generic ? MidiProfile::generic : MidiProfile::luthier,
                                                          folder, 960, 0.0, &error);

        if (file == juce::File())
            std::cerr << "Could not write " << riff->meta.id << ": " << error << std::endl;
        else
            ++written;
    }

    std::cout << written << " riffs written to " << root.getFullPathName() << std::endl;
    return written == library.getNumEntries() ? 0 : 1;
}

int writeRhythmResources (const juce::File& root)
{
    const auto patternDirectory = root.getChildFile ("Rhythm");
    const auto genreDirectory   = root.getChildFile ("Genres");

    if (! patternDirectory.createDirectory() || ! genreDirectory.createDirectory())
    {
        std::cerr << "Could not create " << root.getFullPathName() << std::endl;
        return 1;
    }

    luthier::PatternLibrary patterns;
    int written = 0;

    for (int i = 0; i < patterns.getNumPatterns(); ++i)
    {
        const auto& pattern = patterns.getPattern (i);

        const auto file = patternDirectory
                            .getChildFile (juce::File::createLegalFileName (pattern.getName())
                                             + ".luthierpattern");

        if (! pattern.saveTo (file))
        {
            std::cerr << "Failed to write " << file.getFullPathName() << std::endl;
            return 1;
        }

        ++written;
    }

    luthier::GenreKitLibrary kits;
    int kitsWritten = 0;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        const auto file = genreDirectory
                            .getChildFile (juce::File::createLegalFileName (kit.name) + ".json");

        if (! kit.saveTo (file))
        {
            std::cerr << "Failed to write " << file.getFullPathName() << std::endl;
            return 1;
        }

        ++kitsWritten;
    }

    std::cout << "Wrote " << written << " patterns to "
              << patternDirectory.getFullPathName() << "\n"
              << "Wrote " << kitsWritten << " genre kits to "
              << genreDirectory.getFullPathName() << std::endl;

    return 0;
}

int listGuitars()
{
    std::cout << (int) GuitarType::NumTypes << " instruments\n" << std::endl;

    juce::String lastCategory;

    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        const auto& spec = GuitarLibrary::get ((GuitarType) g);
        const juce::String category (GuitarLibrary::getCategoryName (spec.category));

        if (category != lastCategory)
        {
            std::cout << "\n" << category << std::endl;
            lastCategory = category;
        }

        std::cout << "  " << juce::String (spec.name).paddedRight (' ', 22)
                  << spec.description << std::endl;
    }

    std::cout << std::endl;
    return 0;
}

int listPhrases()
{
    std::cout << "Audition phrases\n" << std::endl;

    for (int t = 0; t < (int) AuditionPhrase::Type::NumTypes; ++t)
        std::cout << "  " << AuditionPhrase::getName ((AuditionPhrase::Type) t) << std::endl;

    std::cout << std::endl;
    return 0;
}

//==============================================================================
bool applyPreset (RenderHost& host, const Options& options)
{
    if (options.presetFile != juce::File())
    {
        if (! options.presetFile.existsAsFile())
        {
            std::cerr << "Preset file not found: "
                      << options.presetFile.getFullPathName() << std::endl;
            return false;
        }

        if (! host.presets.loadPreset (options.presetFile))
        {
            std::cerr << "Could not read that preset file." << std::endl;
            return false;
        }

        host.bridge.applyAllNow();
        return true;
    }

    if (options.presetName.isNotEmpty())
    {
        for (int i = 0; i < host.presets.getNumPresets(); ++i)
        {
            const auto* info = host.presets.getPreset (i);

            if (info != nullptr && info->name.equalsIgnoreCase (options.presetName))
            {
                if (! host.presets.loadPreset (i))
                {
                    std::cerr << "Failed to load preset: " << options.presetName << std::endl;
                    return false;
                }

                host.bridge.applyAllNow();
                return true;
            }
        }

        std::cerr << "No preset called \"" << options.presetName
                  << "\". Use --list-presets to see what is available." << std::endl;
        return false;
    }

    return true;
}

bool applyGuitar (RenderHost& host, const Options& options)
{
    if (options.guitarName.isEmpty())
        return true;

    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        if (juce::String (GuitarLibrary::getName ((GuitarType) g))
              .equalsIgnoreCase (options.guitarName))
        {
            if (auto* param = host.apvts.getParameter (ParamIDs::guitarType))
                param->setValueNotifyingHost ((float) g / (float) ((int) GuitarType::NumTypes - 1));

            host.bridge.applyAllNow();
            return true;
        }
    }

    std::cerr << "No instrument called \"" << options.guitarName
              << "\". Use --list-guitars to see what is available." << std::endl;
    return false;
}

bool buildSequence (const Options& options, juce::MidiMessageSequence& sequence)
{
    if (options.auditionName.isNotEmpty())
    {
        for (int t = 0; t < (int) AuditionPhrase::Type::NumTypes; ++t)
        {
            const auto type = (AuditionPhrase::Type) t;

            if (juce::String (AuditionPhrase::getName (type))
                  .equalsIgnoreCase (options.auditionName))
            {
                sequence = AuditionPhrase::build (type, options.tempo);
                return true;
            }
        }

        std::cerr << "No phrase called \"" << options.auditionName
                  << "\". Use --list-phrases to see what is available." << std::endl;
        return false;
    }

    if (! options.midiFile.existsAsFile())
    {
        std::cerr << "MIDI file not found: " << options.midiFile.getFullPathName() << std::endl;
        return false;
    }

    juce::FileInputStream stream (options.midiFile);
    juce::MidiFile file;

    if (! stream.openedOk() || ! file.readFrom (stream))
    {
        std::cerr << "Could not read that MIDI file." << std::endl;
        return false;
    }

    file.convertTimestampTicksToSeconds();

    for (int t = 0; t < file.getNumTracks(); ++t)
        sequence.addSequence (*file.getTrack (t), 0.0);

    sequence.updateMatchedPairs();
    sequence.sort();

    if (sequence.getNumEvents() == 0)
    {
        std::cerr << "That MIDI file contains no events." << std::endl;
        return false;
    }

    return true;
}

//==============================================================================
int render (RenderHost& host, const Options& options, const juce::MidiMessageSequence& sequence)
{
    const int blockSize = 512;
    const double sr = juce::jlimit (8000.0, 192000.0, options.sampleRate);

    host.setPlayConfigDetails (0, 2, sr, blockSize);
    host.prepareToPlay (sr, blockSize);

    const double musicSeconds = juce::jmax (0.25, sequence.getEndTime());
    const double totalSeconds = musicSeconds + juce::jlimit (0.0, 30.0, options.tailSeconds);
    const int64_t totalSamples = (int64_t) (totalSeconds * sr);

    if (options.verbose)
    {
        std::cout << "Instrument   " << host.engine.getGuitarSpec().name << "\n"
                  << "Preset       " << host.presets.getCurrentPresetName() << "\n"
                  << "Resources    " << IrLibrary::describe() << "\n"
                  << "Events       " << sequence.getNumEvents() << "\n"
                  << "Music        " << juce::String (musicSeconds, 2) << " s\n"
                  << "Tail         " << juce::String (options.tailSeconds, 2) << " s\n"
                  << "Latency      " << host.engine.getLatencySamples() << " samples\n"
                  << "Rendering    " << juce::String (totalSeconds, 2) << " s at "
                  << juce::String (sr / 1000.0, 1) << " kHz..." << std::endl;
    }

    juce::AudioBuffer<float> rendered (2, (int) totalSamples);
    rendered.clear();

    juce::AudioBuffer<float> block (2, blockSize);

    int64_t position = 0;
    int eventIndex = 0;
    double peak = 0.0;
    int lastPercent = -1;

    while (position < totalSamples)
    {
        const int numSamples = (int) juce::jmin<int64_t> (blockSize, totalSamples - position);

        block.setSize (2, numSamples, false, false, true);
        block.clear();

        juce::MidiBuffer midi;

        const double blockStart = (double) position / sr;
        const double blockEnd = (double) (position + numSamples) / sr;

        while (eventIndex < sequence.getNumEvents())
        {
            const auto* event = sequence.getEventPointer (eventIndex);

            if (event == nullptr) { ++eventIndex; continue; }

            const double timestamp = event->message.getTimeStamp();

            if (timestamp >= blockEnd)
                break;

            midi.addEvent (event->message,
                           juce::jlimit (0, numSamples - 1,
                                         (int) ((timestamp - blockStart) * sr)));
            ++eventIndex;
        }

        host.processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            rendered.copyFrom (ch, (int) position, block, ch, 0, numSamples);
            peak = juce::jmax (peak, (double) block.getMagnitude (ch, 0, numSamples));
        }

        position += numSamples;

        if (options.verbose)
        {
            const int percent = (int) (100.0 * (double) position / (double) totalSamples);

            if (percent != lastPercent && percent % 10 == 0)
            {
                std::cout << "  " << percent << "%" << std::endl;
                lastPercent = percent;
            }
        }
    }

    host.releaseResources();

    // ---- normalise --------------------------------------------------------------
    if (options.normalise && peak > 1.0e-6)
    {
        const double target = std::pow (10.0, juce::jlimit (-24.0, 0.0, options.normaliseDb) / 20.0);
        const float gain = (float) (target / peak);
        rendered.applyGain (gain);
        peak *= gain;
    }

    // ---- write ------------------------------------------------------------------
    const auto extension = options.outputFile.getFileExtension().toLowerCase();

    std::unique_ptr<juce::AudioFormat> format;

    if (extension == ".aiff" || extension == ".aif")
        format = std::make_unique<juce::AiffAudioFormat>();
    else if (extension == ".flac")
        format = std::make_unique<juce::FlacAudioFormat>();
    else
        format = std::make_unique<juce::WavAudioFormat>();

    int bitDepth = options.bitDepth;

    if (extension == ".flac" && bitDepth > 24)
        bitDepth = 24;

    options.outputFile.getParentDirectory().createDirectory();
    options.outputFile.deleteFile();

    auto stream = options.outputFile.createOutputStream();

    if (stream == nullptr)
    {
        std::cerr << "Could not create " << options.outputFile.getFullPathName() << std::endl;
        return 1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format->createWriterFor (stream.get(), sr, 2, bitDepth, {}, 0));

    if (writer == nullptr)
    {
        std::cerr << "That format does not support " << bitDepth << "-bit." << std::endl;
        return 1;
    }

    stream.release();

    if (! writer->writeFromAudioSampleBuffer (rendered, 0, (int) position))
    {
        std::cerr << "Writing failed. Check the free disk space." << std::endl;
        return 1;
    }

    writer.reset();

    const double peakDb = (peak > 1.0e-6) ? 20.0 * std::log10 (peak) : -100.0;

    std::cout << "Wrote " << options.outputFile.getFullPathName() << "\n"
              << "  " << juce::String ((double) position / sr, 2) << " s, "
              << juce::String (sr / 1000.0, 1) << " kHz, " << bitDepth << "-bit, stereo\n"
              << "  peak " << juce::String (peakDb, 2) << " dBFS" << std::endl;

    return 0;
}

} // namespace

//==============================================================================
//==============================================================================
/*  output-normalization.md 4.4: the factory calibration table. Every factory
    preset on its own guitar and on every guitar type is loaded into a fresh
    processor (as a user would load it), hashed exactly as the live instance
    hashes it, and measured by the same reference render. */
int calibrateFactory (const juce::File& out, const juce::StringArray& onlyPresets = {})
{
    const int presets = FactoryPresets::getNumPresets();
    const int types = (int) GuitarType::NumTypes;
    int done = 0, failed = 0;

    for (int pr = 0; pr < presets; ++pr)
    {
        // When a subset is named (a partial regen, merged afterwards), skip the
        // presets not in it. The name is snapshotted because getPreset returns a
        // thread_local view that later calls overwrite.
        if (! onlyPresets.isEmpty() && ! onlyPresets.contains (FactoryPresets::getPreset (pr).name, true))
            continue;

        for (int g = -1; g < types; ++g)
        {
            auto p = std::make_unique<LuthierAudioProcessor>();
            p->prepareToPlay (48000.0, 256);

            auto& manager = p->getPresetManager();
            const auto& def = FactoryPresets::getPreset (pr);
            // FactoryPresets::getPreset returns a view backed by a thread_local
            // Definition that later getPreset calls (from loadPreset, the
            // structural capture, ...) overwrite, so snapshot the name now for
            // the entry label below - otherwise every entry is labelled with
            // whichever preset was fetched last.
            const juce::String presetName = def.name;
            const int index = manager.indexOfPreset (presetName);

            if (index < 0 || ! manager.loadPreset (index))
            {
                std::cerr << "cannot load " << presetName << std::endl;
                ++failed;
                continue;
            }

            p->getParameterBridge().applyAllNow();

            if (g >= 0)
                if (auto* prm = p->getState().getParameter (ParamIDs::guitarType))
                {
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) g));
                    p->getParameterBridge().applyAllNow();
                }

            auto& n = p->getOutputNormalization();
            n.refreshStructuralSnapshot();
            const auto state = n.captureSoundState();
            const auto hash = NormalizationCalibrator::hashSoundState (state, *p);
            const auto m = NormalizationCalibrator::renderAndMeasure (NormalizationCalibrator::makeRenderState (state, *p));

            if (! m.ok)
            {
                std::cerr << "render failed: " << def.name << " / " << g << std::endl;
                ++failed;
                continue;
            }

            auto plain = [&p] (const char* id) -> double
            {
                if (auto* prm = p->getState().getParameter (id))
                    return prm->convertFrom0to1 (prm->getValue());
                return 0.0;
            };

            auto normalised = [&p] (const char* id) -> double
            {
                if (auto* prm = p->getState().getParameter (id))
                    return prm->getValue();
                return 0.0;
            };

            NormalizationCalibrator::addFactoryEntry (hash, m.measuredLufs, (int) std::lround (plain ("guitar_type")),
                                                      (int) std::lround (plain ("amp_model")), normalised ("amp_gain"),
                                                      presetName + (g < 0 ? juce::String (" (own guitar)")
                                                                          : " / " + juce::String (g)));

            if (++done % 25 == 0)
                std::cout << "calibrated " << done << " / " << presets * (types + 1) << std::endl;
        }
    }

    if (! NormalizationCalibrator::writeFactoryTable (out))
    {
        std::cerr << "cannot write " << out.getFullPathName() << std::endl;
        return 1;
    }

    std::cout << "wrote " << NormalizationCalibrator::getNumFactoryEntries() << " entries to "
              << out.getFullPathName() << " (" << failed << " failed)" << std::endl;
    return failed == 0 ? 0 : 2;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --calibrate-presets <table> <Name1,Name2,...> : regenerate just the named
    // factory presets' entries (all guitar types) in-place in <table>, keeping
    // every other preset's rows byte-for-byte. Much faster than a full
    // --calibrate-factory when only a few presets changed; see
    // scripts/regen_normalization_factory.sh.
    for (int i = 1; i + 2 < argc; ++i)
        if (juce::String (argv[i]) == "--calibrate-presets")
        {
            const juce::String tableArg (juce::String::fromUTF8 (argv[i + 1]));
            const juce::File table = juce::File::isAbsolutePath (tableArg) ? juce::File (tableArg)
                                                                           : juce::File::getCurrentWorkingDirectory().getChildFile (tableArg);
            juce::StringArray names;
            names.addTokens (juce::String::fromUTF8 (argv[i + 2]), ",", "");
            names.trim();
            names.removeEmptyStrings();

            // Load the existing table, drop the named presets' (stale) rows, then
            // regenerate just those presets and write the merged table back.
            NormalizationCalibrator::setFactoryTableFileForTesting (table);
            NormalizationCalibrator::reloadFactoryTable();
            NormalizationCalibrator::removeFactoryEntriesForPresets (names);
            return calibrateFactory (table, names);
        }

    Options options;

    if (! parseArguments (argc, argv, options))
        return 1;

    if (options.showHelp || argc == 1)
    {
        printUsage();
        return 0;
    }

    // preset-browser-previews.md 2: the factory previews, through the plugin's own
    // PreviewRenderer (a headless LuthierAudioProcessor per preset), so the CLI
    // and the plugin render identically.
    if (options.renderPreviewsTo != juce::File())
    {
        PreviewRenderer renderer;
        const auto bank = FactoryPreviews::renderBank (renderer, [] (int done, int total, const juce::String& name)
        {
            std::cout << "  [" << (done + 1) << "/" << total << "] " << name << std::endl;
        });

        juce::String error;

        if (! FactoryPreviews::write (bank, options.renderPreviewsTo, error))
        {
            std::cerr << "Could not write the previews: " << error << std::endl;
            return 1;
        }

        std::cout << "Wrote " << bank.size() << " previews to "
                  << options.renderPreviewsTo.getChildFile ("Previews").getFullPathName() << std::endl;
        return 0;
    }

    RenderHost host;

    if (options.listPresets) return listPresets (host);
    if (options.listGuitars) return listGuitars();
    if (options.listPhrases) return listPhrases();

    if (options.calibrateFactoryTo != juce::File())
    {
        // Start from an empty table: this run is the whole of it.
        NormalizationCalibrator::setFactoryTableFileForTesting (juce::File::createTempFile (".json"));
        return calibrateFactory (options.calibrateFactoryTo);
    }

    if (options.writeRhythmResourcesTo != juce::File())
        return writeRhythmResources (options.writeRhythmResourcesTo);

    if (options.exportRiffsTo != juce::File())
        return exportRiffs (options.exportRiffsTo, options.riffsGeneric);

    if (options.outputFile == juce::File())
    {
        std::cerr << "No output file. Use --out <file>." << std::endl;
        return 1;
    }

    if (options.midiFile == juce::File() && options.auditionName.isEmpty())
    {
        std::cerr << "Nothing to render. Use --midi <file> or --audition <phrase>." << std::endl;
        return 1;
    }

    if (! applyPreset (host, options))
        return 1;

    if (! applyGuitar (host, options))
        return 1;

    juce::MidiMessageSequence sequence;

    if (! buildSequence (options, sequence))
        return 1;

    return render (host, options, sequence);
}
