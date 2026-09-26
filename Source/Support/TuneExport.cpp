#include "TuneExport.h"
#include "../PluginProcessor.h"
#include "../Routing/TapBuffers.h"
#include "../Tune/TuneFile.h"
#include "../Tune/TuneMidi.h"

namespace luthier
{

//==============================================================================
// 9.1 Audio
//==============================================================================
double TuneExport::getTuneSeconds (const Tune& tune)
{
    return tune.getTotalBeats() * 60.0 / juce::jmax (1.0, tune.meta.tempoBpm);
}

bool TuneExport::renderAudio (const juce::MemoryBlock& pluginState, double sampleRate, int blockSize,
                              double tailSeconds, bool stems, Render& result,
                              const std::function<bool (double)>& progress)
{
    auto instance = LuthierAudioProcessor::createOfflineInstance();
    auto* processor = dynamic_cast<LuthierAudioProcessor*> (instance.get());

    if (processor == nullptr)
        return false;

    processor->setNonRealtime (true);

    if (stems)
        processor->enableAllBuses();

    processor->prepareToPlay (sampleRate, blockSize);
    processor->setStateInformation (pluginState.getData(), (int) pluginState.getSize());

    const auto& tune = processor->getTuneSession().getTune();
    const double seconds = getTuneSeconds (tune);

    if (seconds <= 0.0)
        return false;

    // The tune once through, from the top, on its own clock: no loop, no count-in, no click.
    auto& player = processor->getTunePlayer();
    player.stop();
    player.setLoop (false);
    player.setCountInBars (0);
    player.setMetronome (false);
    processor->serviceTune();
    player.play();

    const auto total = (juce::int64) std::ceil ((seconds + juce::jlimit (0.0, kMaxTailSeconds, tailSeconds)) * sampleRate);

    result.seconds = (double) total / sampleRate;
    result.main.setSize (2, (int) total);
    result.main.clear();
    result.aux.clear();

    if (stems)
        for (int i = 0; i < kNumAuxStems; ++i)
        {
            result.aux.emplace_back (2, (int) total);
            result.aux.back().clear();
        }

    juce::AudioBuffer<float> block (juce::jmax (2, processor->getTotalNumOutputChannels()), blockSize);

    // Aux 1-7 follow the main out; Aux 8 comes after the per-string buses.
    auto auxBusIndex = [] (int aux) { return aux < kNumAuxBuses ? 1 + aux : 1 + kNumAuxBuses + kNumPerStringBuses; };

    for (juce::int64 position = 0; position < total; position += blockSize)
    {
        const int count = (int) juce::jmin ((juce::int64) blockSize, total - position);

        // Every block: the offline render is not waiting on a UI timer.
        processor->serviceTune();

        block.setSize (block.getNumChannels(), count, false, false, true);
        block.clear();
        juce::MidiBuffer midi;
        processor->processBlock (block, midi);

        const auto main = processor->getBusBuffer (block, false, 0);

        for (int ch = 0; ch < 2; ++ch)
            result.main.copyFrom (ch, (int) position, main, juce::jmin (ch, main.getNumChannels() - 1), 0, count);

        if (stems)
        {
            for (int aux = 0; aux < kNumAuxStems; ++aux)
            {
                const int bus = auxBusIndex (aux);

                if (bus >= processor->getBusCount (false) || ! processor->getBus (false, bus)->isEnabled())
                    continue;

                const auto buffer = processor->getBusBuffer (block, false, bus);

                for (int ch = 0; ch < 2 && buffer.getNumChannels() > 0; ++ch)
                    result.aux[(size_t) aux].copyFrom (ch, (int) position, buffer,
                                                       juce::jmin (ch, buffer.getNumChannels() - 1), 0, count);
            }
        }

        if (progress != nullptr && ! progress ((double) (position + count) / (double) total))
            return false;
    }

    processor->releaseResources();
    return true;
}

juce::Array<juce::File> TuneExport::exportAudio (const juce::MemoryBlock& pluginState, const AudioOptions& options,
                                                 juce::String& error, const std::function<bool (double)>& progress)
{
    juce::Array<juce::File> written;
    Render render;

    if (! renderAudio (pluginState, options.sampleRate, 512, options.tailSeconds, options.stems, render, progress))
    {
        error = "The tune could not be rendered (it may be empty, or the render was cancelled).";
        return written;
    }

    const auto folder = options.folder != juce::File() ? options.folder : PresetManager::getRenderFolder();
    folder.createDirectory();

    const auto extension = AudioExporter::getExtension (options.format);
    auto write = [&] (const juce::AudioBuffer<float>& buffer, const juce::String& name) -> bool
    {
        const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + extension);
        file.deleteFile();

        std::unique_ptr<juce::AudioFormat> format;

        switch (options.format)
        {
            case AudioExporter::Format::Wav:  format = std::make_unique<juce::WavAudioFormat>(); break;
            case AudioExporter::Format::Aiff: format = std::make_unique<juce::AiffAudioFormat>(); break;
            case AudioExporter::Format::Flac: format = std::make_unique<juce::FlacAudioFormat>(); break;
        }

        auto bits = options.bitDepth;

        if (! format->getPossibleBitDepths().contains (bits))
            bits = format->getPossibleBitDepths().contains (24) ? 24 : format->getPossibleBitDepths().getLast();

        auto stream = std::make_unique<juce::FileOutputStream> (file);

        if (! stream->openedOk())
        {
            error = "Could not write " + file.getFullPathName();
            return false;
        }

        std::unique_ptr<juce::AudioFormatWriter> writer (format->createWriterFor (stream.get(), options.sampleRate,
                                                                                  2, bits, {}, 0));

        if (writer == nullptr)
        {
            error = "The " + AudioExporter::getFormatName (options.format) + " writer refused "
                      + juce::String (bits) + "-bit audio.";
            return false;
        }

        stream.release();   // the writer owns it now

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
        {
            error = "Could not write " + file.getFullPathName();
            return false;
        }

        written.add (file);
        return true;
    };

    if (! write (render.main, options.baseName))
        return written;

    for (size_t aux = 0; aux < render.aux.size(); ++aux)
        if (! write (render.aux[aux], options.baseName + " - Aux " + juce::String ((int) aux + 1) + " "
                                        + getAuxBusName ((int) aux)))
            return written;

    return written;
}

//==============================================================================
// 9.2 MIDI, through midi-export's profiles (C-53)
//==============================================================================
MidiPerformance TuneExport::buildPerformance (const Tune& tune, double sampleRate, bool includeRealism)
{
    TuneMidiFileOptions fileOptions;
    fileOptions.split = TuneMidiFileOptions::TrackSplit::perInstrument;
    fileOptions.midi.includeRealism = includeRealism;
    fileOptions.midi.includeMarkers = false;   // sections go as section events below

    const auto file = buildTuneMidiFile (tune, fileOptions);
    const double ticks = juce::jmax (1, (int) file.getTimeFormat());

    MidiPerformance performance (sampleRate);
    performance.setTempo (tune.meta.tempoBpm);
    performance.setTimeSignature (tune.meta.timeSigNumerator, tune.meta.timeSigDenominator);

    auto& meta = performance.getMeta();
    meta.title = tune.meta.title;
    meta.partNames = { "Guitar", "Bass" };

    // Track 0 is the conductor track; the instrument tracks follow, guitar
    // first. The bass is its own part so "per instrument" splits it out.
    const int bassChannel = fileOptions.midi.bassChannel;

    for (int t = 1; t < file.getNumTracks(); ++t)
    {
        for (const auto* event : *file.getTrack (t))
        {
            const auto& message = event->message;

            if (! MidiPerformance::isChannelVoiceMessage (message))
                continue;

            const auto sample = (juce::int64) std::llround (performance.beatToSample (message.getTimeStamp() / ticks));
            performance.addMessage (sample, message, message.getChannel() == bassChannel ? 1 : 0);
        }
    }

    // Section starts, which the per-section split needs.
    for (const auto& span : tune.getPlayOrder())
    {
        auto event = LuthierEvent::make (LuthierEventClass::section,
                                         (juce::int64) std::llround (performance.beatToSample (span.startBeat)));
        event.set ("edge", "start");
        event.set ("name", tune.arrangement.sections[(size_t) span.sectionIndex].name);
        performance.addEvent (event);
    }

    return performance;
}

bool TuneExport::exportMidi (const Tune& tune, const juce::File& destination, const MidiOptions& options, juce::String& error)
{
    const auto performance = buildPerformance (tune, 48000.0, options.includeRealism);

    if (performance.getMessages().empty())
    {
        error = "The tune has no notes to export.";
        return false;
    }

    return MidiProfiles::exportToFile (performance, options.profile, destination, &error);
}

//==============================================================================
// 9.3 Notation
//==============================================================================
bool TuneExport::exportNotation (const Tune& tune, NotationFormat format, const juce::File& destination,
                                 const NotationExportOptions& options, juce::String& error)
{
    PerformanceScore score;
    TuneScoreOptions scoreOptions;
    scoreOptions.includeChordSymbols = options.chordSymbols;
    buildTuneScore (tune, score, scoreOptions);

    NotationExporter exporter;

    if (exporter.write (score, format, destination, options))
        return true;

    error = exporter.getLastError();
    return false;
}

//==============================================================================
// 9.4 Project
//==============================================================================
bool TuneExport::exportProject (const Tune& tune, const juce::File& destination, bool bundle,
                                const juce::var& presetState, const juce::var& guitarState, juce::String& error)
{
    auto copy = tune;
    copy.extra.remove ("bundle");

    if (bundle)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("preset", presetState);
        object->setProperty ("guitar", guitarState);
        copy.extra.set ("bundle", juce::var (object));
    }

    return TuneFile::save (copy, destination, error, false);
}

juce::var TuneExport::getBundledPreset (const Tune& tune)
{
    return tune.extra.getWithDefault ("bundle", {}).getProperty ("preset", {});
}

juce::var TuneExport::getBundledGuitar (const Tune& tune)
{
    return tune.extra.getWithDefault ("bundle", {}).getProperty ("guitar", {});
}

} // namespace luthier
