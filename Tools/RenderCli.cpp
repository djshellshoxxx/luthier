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
#include "../Source/Notation/NotationExport.h"   // cli-tools.md: --convert / --inspect / --validate
#include "../Source/Notation/TabFingering.h"      // cli-tools.md: MIDI->tab, --transpose, --retune

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

    // cli-tools.md: notation conversion and inspection.
    juce::File convertFile;            ///< --convert <in>
    juce::String convertBatchSpec;     ///< --convert-batch <dir-or-glob> (raw, for globbing)
    juce::String toFormat;             ///< --to <format>
    juce::File inspectFile;            ///< --inspect <file>
    juce::String validateSpec;         ///< --validate <file-or-glob> (raw)
    int transpose = 0;                 ///< --transpose <semitones>
    juce::String retuneName;           ///< --retune <tuning>
    bool noClobber = false;            ///< --no-clobber
    bool listFormats = false;          ///< --list-formats
    bool doConvert = false;
    bool doConvertBatch = false;
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
        "NOTATION CONVERSION (cli-tools.md)\n"
        "  --convert <in> --to <fmt> [--out <dir|file>]\n"
        "                           convert one notation file (format by extension)\n"
        "  --convert-batch <dir|glob> --to <fmt> --out <dir>\n"
        "                           convert every matching file into <dir>\n"
        "  --to <fmt>               target: midi | tab | musicxml | gp\n"
        "  --inspect <file>         print a file's format, bars, notes, tuning, etc.\n"
        "  --validate <file|glob>   parse-check; nonzero exit if any file is invalid\n"
        "  --transpose <semitones>  shift pitches (re-fingered) during convert\n"
        "  --retune <tuning>        re-fret onto a named tuning (drop-d, dadgad, ...)\n"
        "  --no-clobber             skip an output that already exists\n"
        "  --list-formats           list the import/export formats\n"
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
        else if (arg == "--convert")                   { options.convertFile = juce::File::getCurrentWorkingDirectory().getChildFile (next (i)); options.doConvert = true; }
        else if (arg == "--convert-batch")             { options.convertBatchSpec = next (i); options.doConvertBatch = true; }
        else if (arg == "--to")                        options.toFormat = next (i);
        else if (arg == "--inspect")                   options.inspectFile = juce::File::getCurrentWorkingDirectory().getChildFile (next (i));
        else if (arg == "--validate")                  options.validateSpec = next (i);
        else if (arg == "--transpose")                 options.transpose = next (i).getIntValue();
        else if (arg == "--retune")                    options.retuneName = next (i);
        else if (arg == "--no-clobber")                options.noClobber = true;
        else if (arg == "--list-formats")              options.listFormats = true;
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

//==============================================================================
//==============================================================================
/*  Notation conversion and inspection (spec/cli-tools.md).

    Every command here is `import by detected format -> PerformanceScore ->
    export`, over the existing NotationImporter / NotationExporter / TabFingering.
    The CLI owns only argument parsing, format detection, batching and reporting;
    the parsers and writers are reused unchanged. Nothing here constructs the
    audio engine, so these commands are dispatched before RenderHost is built. */

/** cli-tools 1: a --to name to a NotationFormat. */
bool parseTargetFormat (const juce::String& name, NotationFormat& out)
{
    const auto n = name.trim().toLowerCase();

    if (n == "midi" || n == "mid")                                     { out = NotationFormat::midi;      return true; }
    if (n == "tab" || n == "ascii" || n == "asciitab" || n == "txt")   { out = NotationFormat::asciiTab;  return true; }
    if (n == "musicxml" || n == "xml")                                 { out = NotationFormat::musicXml;  return true; }
    if (n == "gp" || n == "guitarpro" || n == "gpif")                  { out = NotationFormat::guitarPro; return true; }

    return false;
}

/** cli-tools 1: the extension written for a target format. */
juce::String outputExtensionFor (NotationFormat format)
{
    switch (format)
    {
        case NotationFormat::midi:      return ".mid";
        case NotationFormat::asciiTab:  return ".tab";
        case NotationFormat::musicXml:  return ".musicxml";
        case NotationFormat::guitarPro: return ".gp";
        case NotationFormat::numFormats:
        default:                        return ".txt";
    }
}

/** A human name for the detected input format, from its extension. */
juce::String detectedFormatName (const juce::File& file)
{
    const auto e = file.getFileExtension().toLowerCase();

    if (e == ".mid" || e == ".midi")       return "MIDI";
    if (e == ".tab" || e == ".txt")        return "ASCII Tab";
    if (e == ".musicxml" || e == ".xml")   return "MusicXML";
    if (e == ".gp" || e == ".gp5" || e == ".gpx" || e == ".ptb") return "Guitar Pro (binary)";

    return "unknown";
}

//==============================================================================
/** cli-tools 4: a named tuning, highest string first. */
struct NamedTuning
{
    const char* key;      ///< normalized match key (lowercase, hyphenated)
    const char* display;  ///< what gets written into the score's tuning name
    int numStrings;
    std::array<int, kMaxStrings> tuning;
};

const std::vector<NamedTuning>& namedTunings()
{
    static const std::vector<NamedTuning> table =
    {
        { "standard",       "Standard",        6, { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } } },
        { "drop-d",         "Drop D",          6, { { 64, 59, 55, 50, 45, 38, 0, 0, 0, 0, 0, 0 } } },
        { "drop-c",         "Drop C",          6, { { 62, 57, 53, 48, 43, 36, 0, 0, 0, 0, 0, 0 } } },
        { "drop-b",         "Drop B",          6, { { 61, 56, 52, 47, 42, 35, 0, 0, 0, 0, 0, 0 } } },
        { "dadgad",         "DADGAD",          6, { { 62, 57, 55, 50, 45, 38, 0, 0, 0, 0, 0, 0 } } },
        { "open-g",         "Open G",          6, { { 62, 59, 55, 50, 43, 38, 0, 0, 0, 0, 0, 0 } } },
        { "open-d",         "Open D",          6, { { 62, 57, 54, 50, 45, 38, 0, 0, 0, 0, 0, 0 } } },
        { "half-step-down", "Eb Standard",     6, { { 63, 58, 54, 49, 44, 39, 0, 0, 0, 0, 0, 0 } } },
        { "full-step-down", "D Standard",      6, { { 62, 57, 53, 48, 43, 38, 0, 0, 0, 0, 0, 0 } } },
        { "7-string",       "7-String",        7, { { 64, 59, 55, 50, 45, 40, 35, 0, 0, 0, 0, 0 } } },
        { "bass",           "Bass",            4, { { 43, 38, 33, 28, 0, 0, 0, 0, 0, 0, 0, 0 } } },
    };

    return table;
}

/** Resolves a --retune value: a named tuning, or an explicit note list
    ("D A D G A D"). Returns the tuning highest string first. */
bool resolveTuning (const juce::String& spec, std::array<int, kMaxStrings>& out,
                    int& numStrings, juce::String& canonicalName)
{
    const auto key = spec.trim().toLowerCase().replaceCharacter (' ', '-').replaceCharacter ('_', '-');

    for (const auto& nt : namedTunings())
    {
        if (key == juce::String (nt.key))
        {
            out = nt.tuning;
            numStrings = nt.numStrings;
            canonicalName = nt.display;
            return true;
        }
    }

    // A note list, parsed by the same reader the tab importer uses.
    std::vector<int> midiHighFirst;

    if (AsciiTabReader::parseTuningNames (spec, midiHighFirst, true) && ! midiHighFirst.empty())
    {
        out = {};
        numStrings = (int) juce::jmin ((size_t) kMaxStrings, midiHighFirst.size());

        for (int i = 0; i < numStrings; ++i)
            out[(size_t) i] = midiHighFirst[(size_t) i];

        canonicalName = spec.trim();
        return true;
    }

    return false;
}

//==============================================================================
template <typename Fn>
void forEachNote (PerformanceScore& score, Fn&& fn)
{
    for (int t = 0; t < score.getNumTracks(); ++t)
        for (auto& measure : score.getTrack (t).measures)
            for (auto& voice : measure.voices)
                for (auto& note : voice.notes)
                    fn (note);
}

/** cli-tools 4: shift every pitch, then re-finger so the tab is playable.
    Returns the number of notes clamped onto the instrument. */
int transposeScore (PerformanceScore& score, int semitones)
{
    if (semitones == 0)
        return 0;

    forEachNote (score, [semitones] (ScoreNote& n)
    {
        n.midiNote = juce::jlimit (0, 127, n.midiNote + semitones);
        n.pitchHz  = 440.0 * std::pow (2.0, (n.midiNote - 69) / 12.0);
    });

    int clamped = 0;

    for (int t = 0; t < score.getNumTracks(); ++t)
        clamped += TabFingering::assign (score, t).notesClamped;

    return clamped;
}

/** cli-tools 4: keep the pitches, change the instrument, re-fret. */
int retuneScore (PerformanceScore& score, const std::array<int, kMaxStrings>& tuning,
                 int numStrings, const juce::String& tuningName)
{
    for (int t = 0; t < score.getNumTracks(); ++t)
    {
        auto& track = score.getTrack (t);
        track.tuning = tuning;
        track.numStrings = numStrings;
    }

    score.getMeta().tuningName = tuningName;

    int clamped = 0;

    for (int t = 0; t < score.getNumTracks(); ++t)
        clamped += TabFingering::assign (score, t).notesClamped;

    return clamped;
}

//==============================================================================
struct Modifiers
{
    int transpose = 0;
    bool hasRetune = false;
    std::array<int, kMaxStrings> tuning {};
    int numStrings = 6;
    juce::String tuningName;
};

/** Builds the convert modifiers once, failing early on an unknown tuning. */
bool buildModifiers (const Options& options, Modifiers& m, juce::String& error)
{
    m.transpose = options.transpose;

    if (options.retuneName.isNotEmpty())
    {
        if (! resolveTuning (options.retuneName, m.tuning, m.numStrings, m.tuningName))
        {
            error = "Unknown tuning: \"" + options.retuneName
                  + "\". Try a name (drop-d, dadgad, half-step-down, ...) or a note list "
                    "like \"D A D G A D\".";
            return false;
        }

        m.hasRetune = true;
    }

    return true;
}

int applyModifiers (PerformanceScore& score, const Modifiers& m)
{
    int clamped = 0;

    if (m.transpose != 0)
        clamped += transposeScore (score, m.transpose);

    if (m.hasRetune)
        clamped += retuneScore (score, m.tuning, m.numStrings, m.tuningName);

    return clamped;
}

//==============================================================================
struct ConvertResult
{
    bool ok = false;
    juce::String reason;
    juce::File out;
    int clamped = 0;
};

/** cli-tools 2/3: one file, imported, modified and written. Never throws. */
ConvertResult convertOne (const juce::File& in, NotationFormat target, const juce::File& outFile,
                          const Modifiers& mods, bool noClobber)
{
    ConvertResult r;

    if (! in.existsAsFile())                      { r.reason = "no such file"; return r; }
    if (! NotationImporter::canRead (in))         { r.reason = "unsupported format (" + in.getFileExtension() + ")"; return r; }
    if (noClobber && outFile.existsAsFile())      { r.reason = "exists"; return r; }

    PerformanceScore score;
    NotationImporter importer;

    if (! importer.read (in, score))
    {
        r.reason = importer.getLastError().isNotEmpty() ? importer.getLastError() : juce::String ("parse failed");
        return r;
    }

    if (score.getTotalNoteCount() == 0) { r.reason = "no notes"; return r; }

    r.clamped = applyModifiers (score, mods);

    NotationExporter exporter;

    if (! exporter.write (score, target, outFile))
    {
        r.reason = exporter.getLastError().isNotEmpty() ? exporter.getLastError() : juce::String ("write failed");
        return r;
    }

    r.ok = true;
    r.out = outFile;
    return r;
}

//==============================================================================
/** cli-tools 3: a directory or a glob, filtered to readable files, sorted. */
juce::Array<juce::File> gatherInputs (const juce::String& spec)
{
    juce::Array<juce::File> files;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto resolved = cwd.getChildFile (spec);

    if (resolved.isDirectory())
    {
        resolved.findChildFiles (files, juce::File::findFiles, false, "*");
    }
    else if (spec.containsAnyOf ("*?"))
    {
        const auto wild = spec.fromLastOccurrenceOf ("/", false, false);
        const auto dirPart = spec.upToLastOccurrenceOf ("/", false, false);
        const auto dir = (dirPart.isEmpty() || dirPart == spec) ? cwd : cwd.getChildFile (dirPart);

        dir.findChildFiles (files, juce::File::findFiles, false, wild.isEmpty() ? "*" : wild);
    }
    else if (resolved.existsAsFile())
    {
        files.add (resolved);
    }

    files.sort();
    return files;
}

//==============================================================================
int runConvert (const Options& options)
{
    NotationFormat target;

    if (! parseTargetFormat (options.toFormat, target))
    {
        std::cerr << "Unknown target format: \"" << options.toFormat
                  << "\". Use --list-formats." << std::endl;
        return 1;
    }

    Modifiers mods;
    juce::String error;

    if (! buildModifiers (options, mods, error)) { std::cerr << error << std::endl; return 1; }

    const auto in = options.convertFile;
    const auto ext = outputExtensionFor (target);

    juce::File outFile;

    if (options.outputFile == juce::File())
    {
        // Beside the input, new extension; never clobber the source with itself.
        outFile = in.getParentDirectory().getChildFile (in.getFileNameWithoutExtension() + ext);

        if (outFile == in)
            outFile = in.getParentDirectory().getChildFile (in.getFileNameWithoutExtension() + " (converted)" + ext);
    }
    else if (options.outputFile.getFileExtension().isEmpty())
    {
        // No extension on --out means a directory (cli-tools 2).
        outFile = options.outputFile.getChildFile (in.getFileNameWithoutExtension() + ext);
    }
    else
    {
        outFile = options.outputFile;
    }

    const auto r = convertOne (in, target, outFile, mods, options.noClobber);

    if (r.ok)
    {
        std::cout << "OK " << in.getFileName() << " -> " << r.out.getFullPathName();
        if (r.clamped > 0) std::cout << " (" << r.clamped << " note(s) clamped)";
        std::cout << std::endl;
        return 0;
    }

    std::cerr << "skip " << in.getFileName() << " (" << r.reason << ")" << std::endl;
    return 1;
}

int runConvertBatch (const Options& options)
{
    NotationFormat target;

    if (! parseTargetFormat (options.toFormat, target))
    {
        std::cerr << "Unknown target format: \"" << options.toFormat
                  << "\". Use --list-formats." << std::endl;
        return 1;
    }

    if (options.outputFile == juce::File())
    {
        std::cerr << "--convert-batch needs --out <dir>." << std::endl;
        return 1;
    }

    Modifiers mods;
    juce::String error;

    if (! buildModifiers (options, mods, error)) { std::cerr << error << std::endl; return 1; }

    const auto inputs = gatherInputs (options.convertBatchSpec);

    if (inputs.isEmpty())
    {
        std::cerr << "No files matched: " << options.convertBatchSpec << std::endl;
        return 2;
    }

    options.outputFile.createDirectory();

    const auto ext = outputExtensionFor (target);
    int ok = 0, skipped = 0;

    for (const auto& in : inputs)
    {
        const auto outFile = options.outputFile.getChildFile (in.getFileNameWithoutExtension() + ext);
        const auto r = convertOne (in, target, outFile, mods, options.noClobber);

        if (r.ok)
        {
            ++ok;
            std::cout << "OK   " << in.getFileName() << " -> " << outFile.getFileName();
            if (r.clamped > 0) std::cout << " (" << r.clamped << " clamped)";
            std::cout << std::endl;
        }
        else
        {
            ++skipped;
            std::cout << "skip " << in.getFileName() << " (" << r.reason << ")" << std::endl;
        }
    }

    std::cout << "converted " << ok << " of " << inputs.size()
              << " (" << skipped << " skipped)" << std::endl;

    // Nonzero only if every input failed (cli-tools 3).
    return ok > 0 ? 0 : 1;
}

int runInspect (const Options& options)
{
    const auto& in = options.inspectFile;

    if (! in.existsAsFile())
    {
        std::cerr << "No such file: " << in.getFullPathName() << std::endl;
        return 1;
    }

    if (! NotationImporter::canRead (in))
    {
        std::cout << in.getFileName() << ": unsupported format (" << in.getFileExtension()
                  << "); readable: .mid .midi .tab .txt .musicxml .xml" << std::endl;
        return 1;
    }

    PerformanceScore score;
    NotationImporter importer;

    if (! importer.read (in, score))
    {
        std::cerr << in.getFileName() << ": " << importer.getLastError() << std::endl;
        return 1;
    }

    Modifiers mods;
    juce::String error;

    if (! buildModifiers (options, mods, error)) { std::cerr << error << std::endl; return 1; }

    const int clamped = applyModifiers (score, mods);

    const auto& diag = importer.getLastDiagnostics();
    const auto& meta = score.getMeta();
    const auto& track = score.getTrack (0);

    juce::String openStrings;
    for (int s = 0; s < track.numStrings && s < kMaxStrings; ++s)
        openStrings += (s > 0 ? " " : "") + PerformanceScore::getNoteName (track.tuning[(size_t) s]);

    int totalBars = 0;
    for (int t = 0; t < score.getNumTracks(); ++t)
        totalBars = juce::jmax (totalBars, (int) score.getTrack (t).measures.size());

    std::cout << "File       " << in.getFileName() << "\n"
              << "Format     " << detectedFormatName (in) << "\n"
              << "Title      " << meta.title << (meta.artist.isNotEmpty() ? " - " + meta.artist : juce::String()) << "\n"
              << "Tempo      " << juce::String (meta.tempoBpm, 2) << " bpm\n"
              << "Time sig   " << meta.timeSignatureNumerator << "/" << meta.timeSignatureDenominator << "\n"
              << "Tuning     " << meta.tuningName << "  [" << openStrings << "]\n"
              << "Capo       " << track.capoFret << "\n"
              << "Strings    " << track.numStrings << "\n"
              << "Tracks     " << score.getNumTracks() << "\n"
              << "Bars       " << totalBars << "\n"
              << "Notes      " << score.getTotalNoteCount() << std::endl;

    // Technique tally.
    std::array<int, (size_t) ScoreTechnique::Type::numTypes> tally {};
    forEachNote (score, [&tally] (ScoreNote& n)
    {
        for (const auto& tech : n.techniques)
            if ((int) tech.type >= 0 && (int) tech.type < (int) ScoreTechnique::Type::numTypes)
                ++tally[(size_t) tech.type];
    });

    juce::String techLine;
    for (int t = 0; t < (int) ScoreTechnique::Type::numTypes; ++t)
        if (tally[(size_t) t] > 0)
            techLine += (techLine.isEmpty() ? "" : ", ")
                      + juce::String (getTechniqueName ((ScoreTechnique::Type) t))
                      + " x" + juce::String (tally[(size_t) t]);

    std::cout << "Techniques " << (techLine.isEmpty() ? juce::String ("none") : techLine) << std::endl;

    for (const auto& w : diag.warnings)
        std::cout << "  ! " << w << std::endl;

    if (clamped > 0)
        std::cout << "  ! " << clamped << " note(s) clamped by --transpose/--retune" << std::endl;

    return 0;
}

int runValidate (const Options& options)
{
    const auto inputs = gatherInputs (options.validateSpec);

    if (inputs.isEmpty())
    {
        std::cerr << "No files matched: " << options.validateSpec << std::endl;
        return 2;
    }

    int invalid = 0;

    for (const auto& in : inputs)
    {
        if (! NotationImporter::canRead (in))
        {
            std::cout << "INVALID " << in.getFileName() << " (unsupported format "
                      << in.getFileExtension() << ")" << std::endl;
            ++invalid;
            continue;
        }

        PerformanceScore score;
        NotationImporter importer;

        if (! importer.read (in, score) || score.getTotalNoteCount() == 0)
        {
            const auto why = importer.getLastError().isNotEmpty() ? importer.getLastError()
                                                                  : juce::String ("no notes");
            std::cout << "INVALID " << in.getFileName() << " (" << why << ")" << std::endl;
            ++invalid;
            continue;
        }

        std::cout << "valid   " << in.getFileName() << " (" << score.getTotalNoteCount() << " notes";
        const auto& d = importer.getLastDiagnostics();
        if (! d.warnings.isEmpty()) std::cout << ", " << d.warnings.size() << " warning(s)";
        std::cout << ")" << std::endl;
    }

    std::cout << (inputs.size() - invalid) << " of " << inputs.size() << " valid" << std::endl;

    // Strict: nonzero if any file is invalid (cli-tools 4).
    return invalid > 0 ? 1 : 0;
}

int listFormats()
{
    std::cout <<
        "Notation formats (spec/cli-tools.md 1)\n"
        "\n"
        "  name        extensions        import  export\n"
        "  MIDI        .mid .midi         yes     yes\n"
        "  ASCII tab   .tab .txt          yes     yes\n"
        "  MusicXML    .musicxml .xml     yes     yes\n"
        "  Guitar Pro  .gp               no      yes\n"
        "\n"
        "  --to names: midi|mid, tab|ascii|asciitab|txt, musicxml|xml, gp|guitarpro\n"
        << std::endl;

    return 0;
}

} // namespace

//==============================================================================
//==============================================================================
/*  output-normalization.md 4.4: the factory calibration table. Every factory
    preset on its own guitar and on every guitar type is loaded into a fresh
    processor (as a user would load it), hashed exactly as the live instance
    hashes it, and measured by the same reference render. */
int calibrateFactory (const juce::File& out)
{
    const int presets = FactoryPresets::getNumPresets();
    const int types = (int) GuitarType::NumTypes;
    int done = 0, failed = 0;

    for (int pr = 0; pr < presets; ++pr)
    {
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

    // cli-tools.md: notation conversion / inspection. These do not need the audio
    // engine, so they are handled before the (heavy) RenderHost is constructed.
    if (options.listFormats)                     return listFormats();
    if (options.doConvert)                        return runConvert (options);
    if (options.doConvertBatch)                   return runConvertBatch (options);
    if (options.inspectFile != juce::File())      return runInspect (options);
    if (options.validateSpec.isNotEmpty())        return runValidate (options);

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
