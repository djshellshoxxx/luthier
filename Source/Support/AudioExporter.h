#pragma once

/*  Offline audio export (build spec, "Audio export") and the audition phrases.

    Rendering happens through a *second, offline instance* of the plugin, created
    from a factory and loaded with the live instance's state. That guarantees the
    exported file is exactly what the user is hearing, including latency-compensated
    convolution tails, without ever touching the live audio thread.

    The render runs on its own thread and reports progress; the UI never blocks.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>

namespace luthier
{

//==============================================================================
/** Built-in phrases for the AUDITION button. */
class AuditionPhrase
{
public:
    enum class Type
    {
        ChromaticScale,
        MajorScale,
        MinorPentatonic,
        OpenChords,
        PowerChords,
        StrummedProgression,
        FingerpickedArpeggio,
        SingleNote,
        BendAndVibrato,
        SlideRun,
        HarmonicsDemo,
        NumTypes
    };

    static const char* getName (Type t) noexcept;

    /** Builds the phrase as a timed MIDI sequence. Timestamps are in seconds. */
    static juce::MidiMessageSequence build (Type t, double tempoBpm = 100.0, int rootNote = 52);

    /** How long the phrase runs, in seconds, excluding the release tail. */
    static double getDurationSeconds (Type t, double tempoBpm = 100.0);
};

//==============================================================================
class AudioExporter : private juce::Thread
{
public:
    enum class Format { Wav, Aiff, Flac };

    struct Options
    {
        juce::File outputFile;
        Format format = Format::Wav;
        int    bitDepth = 24;
        double sampleRate = 48000.0;
        double tailSeconds = 4.0;      ///< Extra time after the last note, for the decay.
        bool   normalise = false;
        double normaliseTargetDb = -1.0;
        double tempoBpm = 120.0;
        int    numChannels = 2;
    };

    struct Result
    {
        bool success = false;
        juce::String message;
        juce::File file;
        double lengthSeconds = 0.0;
        double peakDb = -100.0;
        juce::String qualityDescription;
    };

    using ProcessorFactory = std::function<std::unique_ptr<juce::AudioProcessor>()>;
    using CompletionCallback = std::function<void (const Result&)>;

    AudioExporter();
    ~AudioExporter() override;

    /** Starts a render. Returns false if one is already running. */
    bool startExport (const Options& options,
                      const juce::MidiMessageSequence& sequence,
                      const juce::MemoryBlock& pluginState,
                      ProcessorFactory factory,
                      CompletionCallback onComplete);

    bool isExporting() const noexcept { return running.load(); }
    double getProgress() const noexcept { return progress.load(); }
    void cancelExport();

    /** File extension for a format, including the dot. */
    static juce::String getExtension (Format f) noexcept;
    static juce::String getFormatName (Format f) noexcept;

    /** Bit depths a format supports. */
    static juce::Array<int> getSupportedBitDepths (Format f);

    /** "48 kHz / 24-bit WAV, stereo" - shown in the success message. */
    static juce::String describeQuality (const Options& o);

private:
    void run() override;

    Options options;
    juce::MidiMessageSequence sequence;
    juce::MemoryBlock state;
    ProcessorFactory factory;
    CompletionCallback completion;

    std::atomic<bool> running { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioExporter)
};

} // namespace luthier
