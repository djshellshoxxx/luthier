#include "AudioExporter.h"

namespace luthier
{

//==============================================================================
//  Audition phrases
//==============================================================================
const char* AuditionPhrase::getName (Type t) noexcept
{
    switch (t)
    {
        case Type::ChromaticScale:       return "Chromatic Scale";
        case Type::MajorScale:           return "Major Scale";
        case Type::MinorPentatonic:      return "Minor Pentatonic";
        case Type::OpenChords:           return "Open Chords";
        case Type::PowerChords:          return "Power Chords";
        case Type::StrummedProgression:  return "Strummed Progression";
        case Type::FingerpickedArpeggio: return "Fingerpicked Arpeggio";
        case Type::SingleNote:           return "Single Note";
        case Type::BendAndVibrato:       return "Bend and Vibrato";
        case Type::SlideRun:             return "Slide Run";
        case Type::HarmonicsDemo:        return "Harmonics";
        case Type::NumTypes:
        default:                         return "Major Scale";
    }
}

namespace
{
    void addNote (juce::MidiMessageSequence& seq, int note, double startSeconds,
                  double lengthSeconds, int velocity = 90, int channel = 1)
    {
        note = juce::jlimit (0, 127, note);
        velocity = juce::jlimit (1, 127, velocity);

        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity), startSeconds);
        seq.addEvent (juce::MidiMessage::noteOff (channel, note), startSeconds + lengthSeconds);
    }

    void addChord (juce::MidiMessageSequence& seq, const int* notes, int count,
                   double startSeconds, double lengthSeconds, int velocity = 88)
    {
        for (int i = 0; i < count; ++i)
            addNote (seq, notes[i], startSeconds, lengthSeconds, velocity);
    }
}

juce::MidiMessageSequence AuditionPhrase::build (Type t, double tempoBpm, int rootNote)
{
    juce::MidiMessageSequence seq;

    const double beat = 60.0 / juce::jmax (30.0, tempoBpm);
    const double eighth = beat * 0.5;

    switch (t)
    {
        case Type::ChromaticScale:
        {
            for (int i = 0; i <= 12; ++i)
                addNote (seq, rootNote + i, (double) i * eighth, eighth * 0.9, 82 + (i % 5) * 4);
            break;
        }

        case Type::MajorScale:
        {
            const int degrees[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };

            for (int i = 0; i < 8; ++i)
                addNote (seq, rootNote + degrees[i], (double) i * eighth, eighth * 0.9, 84 + (i % 4) * 5);

            for (int i = 0; i < 7; ++i)
                addNote (seq, rootNote + degrees[6 - i], (8.0 + i) * eighth, eighth * 0.9, 80 + (i % 4) * 5);
            break;
        }

        case Type::MinorPentatonic:
        {
            const int degrees[6] = { 0, 3, 5, 7, 10, 12 };

            for (int i = 0; i < 6; ++i)
                addNote (seq, rootNote + degrees[i], (double) i * eighth, eighth * 0.95, 88);

            for (int i = 0; i < 5; ++i)
                addNote (seq, rootNote + degrees[4 - i], (6.0 + i) * eighth, eighth * 0.95, 84);
            break;
        }

        case Type::OpenChords:
        {
            // E minor, C major, G major, D major: the first chords anyone learns.
            const int em[]  = { 40, 47, 52, 55, 59, 64 };
            const int cmaj[] = { 48, 52, 55, 60, 64 };
            const int gmaj[] = { 43, 47, 50, 55, 59, 67 };
            const int dmaj[] = { 50, 57, 62, 66 };

            addChord (seq, em,   6, 0.0,        beat * 2.0 * 0.95);
            addChord (seq, cmaj, 5, beat * 2.0, beat * 2.0 * 0.95);
            addChord (seq, gmaj, 6, beat * 4.0, beat * 2.0 * 0.95);
            addChord (seq, dmaj, 4, beat * 6.0, beat * 2.0 * 0.95);
            break;
        }

        case Type::PowerChords:
        {
            for (int i = 0; i < 8; ++i)
            {
                const int root = rootNote + ((i % 4 == 3) ? 5 : 0);
                const int notes[2] = { root, root + 7 };
                addChord (seq, notes, 2, (double) i * eighth, eighth * 0.55, 104);
            }
            break;
        }

        case Type::StrummedProgression:
        {
            const int amin[] = { 45, 52, 57, 60, 64 };
            const int fmaj[] = { 41, 48, 53, 57, 60, 65 };
            const int cmaj[] = { 48, 52, 55, 60, 64 };
            const int gmaj[] = { 43, 47, 50, 55, 59, 67 };

            const int* chords[4] = { amin, fmaj, cmaj, gmaj };
            const int counts[4] = { 5, 6, 5, 6 };

            for (int bar = 0; bar < 4; ++bar)
            {
                const double barStart = bar * beat * 4.0;

                // Down, down, up, down-up: a standard strum pattern.
                addChord (seq, chords[bar], counts[bar], barStart,             beat * 0.95, 96);
                addChord (seq, chords[bar], counts[bar], barStart + beat * 1.5, beat * 0.45, 72);
                addChord (seq, chords[bar], counts[bar], barStart + beat * 2.0, beat * 0.95, 92);
                addChord (seq, chords[bar], counts[bar], barStart + beat * 3.0, beat * 0.45, 68);
                addChord (seq, chords[bar], counts[bar], barStart + beat * 3.5, beat * 0.45, 76);
            }
            break;
        }

        case Type::FingerpickedArpeggio:
        {
            const int pattern[8] = { 40, 52, 47, 55, 40, 59, 47, 64 };

            for (int rep = 0; rep < 2; ++rep)
                for (int i = 0; i < 8; ++i)
                    addNote (seq, pattern[i], (rep * 8 + i) * eighth, beat * 2.0,
                             70 + (i == 0 ? 18 : 0));
            break;
        }

        case Type::SingleNote:
        {
            addNote (seq, rootNote + 12, 0.0, beat * 4.0, 96);
            break;
        }

        case Type::BendAndVibrato:
        {
            // Hold a note, bend it a whole step, then add vibrato.
            addNote (seq, rootNote + 15, 0.0, beat * 6.0, 100);

            const int steps = 24;

            for (int i = 0; i <= steps; ++i)
            {
                const double t = beat * 0.6 + (double) i / steps * beat * 1.2;
                const double bend = (double) i / steps;
                seq.addEvent (juce::MidiMessage::pitchWheel (1, 8192 + (int) (bend * 8191.0)), t);
            }

            for (int i = 0; i < 48; ++i)
            {
                const double t = beat * 2.2 + (double) i * beat * 0.05;
                const double v = std::sin ((double) i * 0.6) * 0.5 + 0.5;
                seq.addEvent (juce::MidiMessage::controllerEvent (1, 1, (int) (v * 90.0)), t);
            }
            break;
        }

        case Type::SlideRun:
        {
            // Overlapping notes on the same string trigger the legato slide path.
            const int notes[6] = { 0, 5, 7, 12, 7, 5 };

            for (int i = 0; i < 6; ++i)
                addNote (seq, rootNote + notes[i], (double) i * eighth * 0.9,
                         eighth * 1.4, 70);
            break;
        }

        case Type::HarmonicsDemo:
        {
            // CC 73 arms natural harmonics; the notes land on the 12th, 7th and
            // 5th fret nodes.
            seq.addEvent (juce::MidiMessage::controllerEvent (1, 73, 127), 0.0);

            addNote (seq, rootNote + 12, beat * 0.2, beat * 1.6, 80);
            addNote (seq, rootNote + 7,  beat * 2.0, beat * 1.6, 80);
            addNote (seq, rootNote + 5,  beat * 3.8, beat * 1.6, 80);

            seq.addEvent (juce::MidiMessage::controllerEvent (1, 73, 0), beat * 5.6);
            break;
        }

        case Type::NumTypes:
        default:
            break;
    }

    seq.updateMatchedPairs();
    seq.sort();

    return seq;
}

double AuditionPhrase::getDurationSeconds (Type t, double tempoBpm)
{
    const auto seq = build (t, tempoBpm);
    return seq.getEndTime();
}

//==============================================================================
//  Audio exporter
//==============================================================================
AudioExporter::AudioExporter()
    : juce::Thread ("Luthier Export")
{
}

AudioExporter::~AudioExporter()
{
    stopThread (4000);
}

juce::String AudioExporter::getExtension (Format f) noexcept
{
    switch (f)
    {
        case Format::Wav:  return ".wav";
        case Format::Aiff: return ".aiff";
        case Format::Flac: return ".flac";
        default:           return ".wav";
    }
}

juce::String AudioExporter::getFormatName (Format f) noexcept
{
    switch (f)
    {
        case Format::Wav:  return "WAV";
        case Format::Aiff: return "AIFF";
        case Format::Flac: return "FLAC";
        default:           return "WAV";
    }
}

juce::Array<int> AudioExporter::getSupportedBitDepths (Format f)
{
    switch (f)
    {
        case Format::Flac: return { 16, 24 };            // FLAC has no float mode
        case Format::Aiff: return { 16, 24, 32 };
        case Format::Wav:
        default:           return { 16, 24, 32 };
    }
}

juce::String AudioExporter::describeQuality (const Options& o)
{
    return juce::String (o.sampleRate / 1000.0, 1) + " kHz / "
           + juce::String (o.bitDepth) + "-bit " + getFormatName (o.format)
           + ", " + (o.numChannels > 1 ? "stereo" : "mono");
}

//==============================================================================
bool AudioExporter::startExport (const Options& opts,
                                 const juce::MidiMessageSequence& seq,
                                 const juce::MemoryBlock& pluginState,
                                 ProcessorFactory processorFactory,
                                 CompletionCallback onComplete)
{
    if (running.load() || isThreadRunning())
        return false;

    options = opts;
    sequence = seq;
    state = pluginState;
    factory = std::move (processorFactory);
    completion = std::move (onComplete);

    progress.store (0.0);
    running.store (true);

    startThread (juce::Thread::Priority::normal);
    return true;
}

void AudioExporter::cancelExport()
{
    signalThreadShouldExit();
    stopThread (3000);
    running.store (false);
}

//==============================================================================
void AudioExporter::run()
{
    Result result;
    result.file = options.outputFile;

    auto finish = [this, &result]
    {
        running.store (false);
        progress.store (1.0);

        if (completion)
        {
            auto callback = completion;
            auto r = result;
            juce::MessageManager::callAsync ([callback, r] { callback (r); });
        }
    };

    if (! factory)
    {
        result.message = "No plugin factory was supplied to the exporter.";
        finish();
        return;
    }

    auto processor = factory();

    if (processor == nullptr)
    {
        result.message = "Could not create an offline instance for rendering.";
        finish();
        return;
    }

    if (state.getSize() > 0)
        processor->setStateInformation (state.getData(), (int) state.getSize());

    const int blockSize = 512;
    const double sr = juce::jlimit (8000.0, 192000.0, options.sampleRate);
    const int numChannels = juce::jlimit (1, 2, options.numChannels);

    processor->setPlayConfigDetails (0, numChannels, sr, blockSize);
    processor->setNonRealtime (true);   // cpu-quality-modes 2.6: an offline render, so High
    processor->prepareToPlay (sr, blockSize);

    const double musicSeconds = juce::jmax (0.25, sequence.getEndTime());
    const double totalSeconds = musicSeconds + juce::jlimit (0.0, 30.0, options.tailSeconds);
    const int64_t totalSamples = (int64_t) (totalSeconds * sr);

    // Render into memory first: normalisation needs the whole file, and a guitar
    // render is short enough that holding it is cheap.
    juce::AudioBuffer<float> rendered (numChannels, (int) juce::jmin<int64_t> (totalSamples, 1 << 28));
    rendered.clear();

    juce::AudioBuffer<float> block (numChannels, blockSize);

    int64_t position = 0;
    int eventIndex = 0;
    double peak = 0.0;

    while (position < totalSamples && ! threadShouldExit())
    {
        const int numSamples = (int) juce::jmin<int64_t> (blockSize, totalSamples - position);

        block.setSize (numChannels, numSamples, false, false, true);
        block.clear();

        juce::MidiBuffer midi;

        const double blockStartSeconds = (double) position / sr;
        const double blockEndSeconds = (double) (position + numSamples) / sr;

        while (eventIndex < sequence.getNumEvents())
        {
            const auto* event = sequence.getEventPointer (eventIndex);

            if (event == nullptr)
            {
                ++eventIndex;
                continue;
            }

            const double timestamp = event->message.getTimeStamp();

            if (timestamp >= blockEndSeconds)
                break;

            const int offset = juce::jlimit (0, numSamples - 1,
                                             (int) ((timestamp - blockStartSeconds) * sr));
            midi.addEvent (event->message, offset);
            ++eventIndex;
        }

        processor->processBlock (block, midi);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int destSamples = juce::jmin (numSamples, rendered.getNumSamples() - (int) position);

            if (destSamples > 0)
                rendered.copyFrom (ch, (int) position, block, ch, 0, destSamples);

            peak = juce::jmax (peak, (double) block.getMagnitude (ch, 0, numSamples));
        }

        position += numSamples;
        progress.store (juce::jlimit (0.0, 0.98, (double) position / (double) totalSamples));
    }

    processor->releaseResources();

    if (threadShouldExit())
    {
        result.message = "Export cancelled.";
        finish();
        return;
    }

    // ---- normalise ------------------------------------------------------------
    if (options.normalise && peak > 1.0e-6)
    {
        const double target = std::pow (10.0, juce::jlimit (-24.0, 0.0, options.normaliseTargetDb) / 20.0);
        const float gain = (float) (target / peak);

        rendered.applyGain (gain);
        peak *= gain;
    }

    // ---- write ----------------------------------------------------------------
    options.outputFile.getParentDirectory().createDirectory();

    std::unique_ptr<juce::AudioFormat> format;

    switch (options.format)
    {
        case Format::Aiff: format = std::make_unique<juce::AiffAudioFormat>(); break;
        case Format::Flac: format = std::make_unique<juce::FlacAudioFormat>(); break;
        case Format::Wav:
        default:           format = std::make_unique<juce::WavAudioFormat>(); break;
    }

    auto supported = getSupportedBitDepths (options.format);
    int bitDepth = options.bitDepth;

    if (! supported.contains (bitDepth))
        bitDepth = supported.getLast();

    if (auto stream = options.outputFile.createOutputStream())
    {
        stream->setPosition (0);
        stream->truncate();

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format->createWriterFor (stream.get(), sr, (unsigned int) numChannels,
                                     bitDepth, {}, 0));

        if (writer != nullptr)
        {
            stream.release();   // the writer owns it now

            const int written = juce::jmin ((int) position, rendered.getNumSamples());

            if (writer->writeFromAudioSampleBuffer (rendered, 0, written))
            {
                writer.reset();

                result.success = true;
                result.lengthSeconds = (double) written / sr;
                result.peakDb = (peak > 1.0e-6) ? 20.0 * std::log10 (peak) : -100.0;
                result.qualityDescription = describeQuality (options);
                result.message = "Exported " + options.outputFile.getFileName();
            }
            else
            {
                result.message = "The file could not be written. Check that there is "
                                 "enough free disk space.";
            }
        }
        else
        {
            result.message = "That format and bit depth combination is not supported.";
        }
    }
    else
    {
        result.message = "Could not create the file. Check that you have permission to "
                         "write to that folder.";
    }

    finish();
}

} // namespace luthier
