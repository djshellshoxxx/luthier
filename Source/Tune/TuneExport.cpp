#include "TuneExport.h"
#include "TuneFile.h"
#include "../Export/MidiPerformance.h"
#include "../Routing/TapBuffers.h"
#include "../Support/ThreadProbe.h"

namespace luthier
{

//==============================================================================
juce::File TuneExportRequest::fileFor (const juce::String& suffix, const juce::String& extension) const
{
    auto name = juce::File::createLegalFileName (baseName.trim().isNotEmpty() ? baseName.trim()
                                                                               : juce::String ("Untitled Tune"));

    if (suffix.isNotEmpty())
        name << " - " << juce::File::createLegalFileName (suffix);

    return folder.getChildFile (name + extension);
}

juce::String TuneExportReport::describe() const
{
    juce::StringArray lines;

    for (const auto& f : files)
        lines.add (f.getFileName());

    for (const auto& e : errors)
        lines.add ("! " + e);

    return lines.joinIntoString ("\n");
}

//==============================================================================
namespace TuneExport
{

bool writeMidi (const Tune& tune, const TuneMidiOptions& midiOptions, const TuneExportRequest& request,
                const juce::File& destination, juce::String& error)
{
    ThreadProbe::noteFileAccess();

    if (tune.getPlayOrder().empty())
    {
        error = "The tune has no sections to export.";
        return false;
    }

    auto tuneOptions = midiOptions;
    tuneOptions.includeRealism = request.realism;

    const auto performance = buildTunePerformance (tune, request.sampleRate, tuneOptions);

    MidiExportOptions options;
    options.profile = request.profile;
    options.split = request.split;
    options.ppq = request.ppq;
    options.includeRealism = request.realism;

    if (! destination.getParentDirectory().createDirectory().wasOk())
    {
        error = destination.getParentDirectory().getFullPathName() + ": could not create the folder";
        return false;
    }

    juce::String why;

    if (! MidiProfiles::exportToFile (performance, options, destination, &why))
    {
        error = destination.getFileName() + ": " + (why.isNotEmpty() ? why : juce::String ("could not be written"));
        return false;
    }

    return true;
}

bool writeNotation (const Tune& tune, const TuneExportRequest& request,
                    const juce::File& destination, juce::String& error)
{
    ThreadProbe::noteFileAccess();

    PerformanceScore score;
    TuneScoreOptions scoreOptions;
    scoreOptions.includeChordSymbols = request.chordSymbols;
    buildTuneScore (tune, score, scoreOptions);

    NotationExporter exporter;
    NotationExportOptions options;
    options.chordSymbols = request.chordSymbols;

    if (! exporter.write (score, request.notationFormat, destination, options))
    {
        error = destination.getFileName() + ": " + exporter.getLastError();
        return false;
    }

    return true;
}

bool writeProject (const Tune& tune, const juce::File& destination, juce::String& error)
{
    return TuneFile::save (tune, destination, error, false);
}

TuneExportReport writeFiles (const Tune& tune, const TuneMidiOptions& midiOptions, const TuneExportRequest& request)
{
    TuneExportReport report;

    auto attempt = [&report] (bool ok, const juce::File& file, const juce::String& error)
    {
        if (ok)
            report.files.add (file);
        else
            report.errors.add (error);
    };

    if (request.midi)
    {
        const auto file = request.fileFor ({}, ".mid");
        juce::String error;
        attempt (writeMidi (tune, midiOptions, request, file, error), file, error);
    }

    if (request.notation)
    {
        const auto file = request.fileFor ({}, getNotationFormatExtension (request.notationFormat));
        juce::String error;
        attempt (writeNotation (tune, request, file, error), file, error);
    }

    if (request.project)
    {
        const auto file = request.fileFor ({}, TuneFile::kFileExtension);
        juce::String error;
        attempt (writeProject (tune, file, error), file, error);
    }

    return report;
}

//==============================================================================
double getTuneLengthSeconds (const Tune& tune)
{
    return tune.getTotalBeats() * 60.0 / juce::jmax (1.0, tune.meta.tempoBpm);
}

juce::MidiMessageSequence makeRenderSequence (const Tune& tune)
{
    juce::MidiMessageSequence sequence;
    const double end = juce::jmax (0.25, getTuneLengthSeconds (tune));

    // Only the length matters: the instance plays the tune itself. All notes
    // off on a channel the tune never uses, so the message is harmless.
    sequence.addEvent (juce::MidiMessage::allNotesOff (16), end);
    return sequence;
}

AudioExporter::Options makeAudioOptions (const Tune& tune, const TuneExportRequest& request,
                                         const juce::File& destination)
{
    AudioExporter::Options options;
    options.outputFile = destination;
    options.format = request.audioFormat;
    options.bitDepth = request.bitDepth;
    options.sampleRate = request.sampleRate;
    options.tailSeconds = juce::jlimit (0.0, 5.0, request.tailSeconds);
    options.normalise = false;
    options.tempoBpm = tune.meta.tempoBpm;
    options.numChannels = 2;
    return options;
}

juce::StringArray getStemNames (TuneStemChoice choice)
{
    juce::StringArray names { "Main" };

    if (choice == TuneStemChoice::everyBus)
        for (int i = 0; i < kNumAuxStrips; ++i)
            names.add (getAuxBusName (i));

    return names;
}

int getStemBusIndex (int stemIndex) noexcept
{
    if (stemIndex <= 0)
        return 0;

    // Aux 1-7 are buses 1-7; Aux 8 was declared last, after the per-string buses.
    const int aux = stemIndex - 1;
    return aux < kNumAuxBuses ? 1 + aux : 1 + kNumAuxBuses + kNumPerStringBuses;
}

std::unique_ptr<juce::AudioProcessor> wrapForBus (std::unique_ptr<juce::AudioProcessor> instance, int busIndex)
{
    if (instance == nullptr || busIndex <= 0)
        return instance;

    return std::make_unique<TuneBusRenderProcessor> (std::move (instance), busIndex);
}

} // namespace TuneExport

//==============================================================================
TuneBusRenderProcessor::TuneBusRenderProcessor (std::unique_ptr<juce::AudioProcessor> innerProcessor, int busIndex)
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      inner (std::move (innerProcessor)),
      bus (busIndex)
{
}

TuneBusRenderProcessor::~TuneBusRenderProcessor() = default;

const juce::String TuneBusRenderProcessor::getName() const
{
    return inner != nullptr ? inner->getName() + " (bus " + juce::String (bus) + ")" : juce::String ("Bus render");
}

double TuneBusRenderProcessor::getTailLengthSeconds() const
{
    return inner != nullptr ? inner->getTailLengthSeconds() : 0.0;
}

void TuneBusRenderProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    if (inner == nullptr)
        return;

    // Every bus on, so the routing panel's sends land where they do live.
    inner->enableAllBuses();
    inner->setRateAndBufferSizeDetails (sampleRate, maximumExpectedSamplesPerBlock);
    inner->prepareToPlay (sampleRate, maximumExpectedSamplesPerBlock);

    const int channels = juce::jmax (inner->getTotalNumInputChannels(), inner->getTotalNumOutputChannels(), 2);
    wide.setSize (channels, juce::jmax (1, maximumExpectedSamplesPerBlock), false, true, false);
}

void TuneBusRenderProcessor::releaseResources()
{
    if (inner != nullptr)
        inner->releaseResources();
}

void TuneBusRenderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (inner == nullptr || numSamples <= 0)
        return;

    if (wide.getNumSamples() < numSamples)
        wide.setSize (wide.getNumChannels(), numSamples, false, true, false);

    juce::AudioBuffer<float> block (wide.getArrayOfWritePointers(), wide.getNumChannels(), numSamples);
    block.clear();
    inner->processBlock (block, midi);

    if (! juce::isPositiveAndBelow (bus, inner->getBusCount (false)))
        return;

    auto out = inner->getBusBuffer (block, false, bus);

    for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), out.getNumChannels()); ++ch)
        buffer.copyFrom (ch, 0, out, ch, 0, numSamples);

    // A mono bus fills both sides.
    if (out.getNumChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, out, 0, 0, numSamples);
}

void TuneBusRenderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (inner != nullptr)
        inner->setStateInformation (data, sizeInBytes);
}

void TuneBusRenderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (inner != nullptr)
        inner->getStateInformation (destData);
}

} // namespace luthier
