#pragma once

/*  User IRs, cab match, EQ match and capture (tone-match.md).

    Three separate jobs share this file because they share one piece of
    machinery: a block of impulse response, resampled to the host's rate and
    handed to the audio thread without either thread ever waiting for the other.

      - The IR loader turns a file on disk into that block (section 1).
      - Cab match derives one by deconvolving a sweep (section 2).
      - EQ match derives one by fitting a spectrum (section 3).

    Rule 1 of section 0 is what shapes the handover: IRs are loaded on the
    message thread and given to the audio thread by pointer swap. The audio
    thread never opens a file, never resamples and never allocates; it reads
    whichever block the pointer currently names, and the message thread only ever
    frees a block the audio thread has finished with.

    Rule 5 is the other one: all of the analysis - the sweep deconvolution, the
    spectrum fitting - runs on a worker thread. The audio thread's only job in a
    capture is to copy samples into a buffer that already exists.
*/

#include "../DSP/Common/DspCommon.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace luthier
{

//==============================================================================
/** An impulse response in memory, at the host's sample rate. */
struct ImpulseResponse
{
    /** tone-move 0.3: anything longer than this is truncated on load. */
    static constexpr double kMaxSecondsDefault = 4.0;

    std::vector<float> samples;
    double sampleRate = 48000.0;

    juce::String name;
    juce::File sourceFile;

    bool isEmpty() const noexcept { return samples.empty(); }
    int getLength() const noexcept { return (int) samples.size(); }

    double getLengthMs() const noexcept
    {
        return sampleRate > 0.0 ? (double) samples.size() / sampleRate * 1000.0 : 0.0;
    }

    /** Peak-normalises to -0.1 dBFS, so swapping IRs does not change the level. */
    void normalise();
};

//==============================================================================
/** The metadata sidecar (tone-match 5). */
struct IrMetadata
{
    juce::String name;
    juce::String type;             ///< "cabinet", "body", "room", "special".
    double sampleRate = 48000.0;
    double lengthMs = 0.0;
    juce::String author;
    juce::StringArray tags;
    juce::String notes;

    juce::var toVar() const;
    static IrMetadata fromVar (const juce::var& state);

    /** Reads the `.json` beside an IR, or infers what it can from the filename
        when there is no sidecar (tone-match 5). */
    static IrMetadata forFile (const juce::File& irFile);

    bool saveFor (const juce::File& irFile) const;
};

//==============================================================================
/** One user IR slot (tone-match 1).

    A slot owns the settings and the loaded response. The audio thread reads the
    response through an atomic pointer; the message thread builds a new one, swaps
    it in, and only then lets the old one go. */
class IrSlot
{
public:
    IrSlot();
    ~IrSlot();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    //==========================================================================
    /** Loads a file. Message thread. Returns false and sets the error otherwise. */
    bool load (const juce::File& file);

    void unload();

    bool isLoaded() const noexcept { return active.load (std::memory_order_acquire) != nullptr; }

    juce::File getFile() const { return currentFile; }
    juce::String getName() const;
    double getLengthMs() const;

    juce::String getLastError() const { return lastError; }

    //==========================================================================
    void setEngaged (bool shouldBeEngaged) noexcept { engaged.store (shouldBeEngaged, std::memory_order_relaxed); }
    bool isEngaged() const noexcept { return engaged.load (std::memory_order_relaxed) && isLoaded(); }

    /** Which channel of a multi-channel IR to use; -1 sums to mono. */
    void setChannel (int channel);
    int getChannel() const noexcept { return channelChoice; }

    void setGainTrimDb (double db) noexcept;
    double getGainTrimDb() const noexcept { return gainTrimDb.load (std::memory_order_relaxed); }

    /** Samples trimmed from each end (tone-match 1). */
    void setStartTrim (int samples);
    void setEndTrim (int samples);
    int getStartTrim() const noexcept { return startTrim; }
    int getEndTrim() const noexcept { return endTrim; }

    void setPredelayMs (double ms);
    double getPredelayMs() const noexcept { return predelayMs; }

    void setReversed (bool shouldReverse);
    bool isReversed() const noexcept { return reversed; }

    /** 0 to 1: how much of the user IR against the built-in model. */
    void setMix (double mix) noexcept;
    double getMix() const noexcept { return mixAmount.load (std::memory_order_relaxed); }

    void setMaxSeconds (double seconds);
    double getMaxSeconds() const noexcept { return maxSeconds; }

    //==========================================================================
    /** Convolves in place, blending with the dry signal by the mix amount.
        Audio thread; never allocates. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** The latency the convolution reports. Matches the built-in cabinet's, per
        tone-match 0.4. */
    int getLatencySamples() const noexcept;

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    /** Rebuilds the processed response from the raw file data and the current
        settings, and swaps it in. Message thread. */
    void rebuild();

    /** Resamples to the host rate with a windowed sinc (tone-match 0.2). */
    static std::vector<float> resample (const std::vector<float>& source,
                                        double fromRate, double toRate);

    juce::AudioFormatManager formats;

    /** The file as it was read: original rate, all channels summed per the
        channel choice, before any trimming. */
    std::vector<float> rawSamples;
    double rawSampleRate = 48000.0;

    juce::File currentFile;
    juce::String lastError;

    /*  The live response and the one being retired.

        `active` is what the audio thread reads. `retired` holds the previous one
        until a block has gone by, so it is never freed under the audio thread's
        feet. */
    std::atomic<ImpulseResponse*> active { nullptr };
    std::unique_ptr<ImpulseResponse> live, retired;

    std::atomic<bool> engaged { false };
    std::atomic<double> gainTrimDb { 0.0 };
    std::atomic<double> mixAmount { 1.0 };

    int channelChoice = -1;
    int startTrim = 0, endTrim = 0;
    double predelayMs = 0.0;
    bool reversed = false;
    double maxSeconds = ImpulseResponse::kMaxSecondsDefault;

    double sr = 48000.0;
    int blockSize = 512;

    std::unique_ptr<juce::dsp::Convolution> convolution;
    juce::AudioBuffer<float> wetBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrSlot)
};

//==============================================================================
/** The capture path (tone-match 4).

    Records from whatever the caller feeds it into a buffer sized up front. The
    audio thread copies; everything else happens elsewhere. */
class Capture
{
public:
    /*  JUCE_DECLARE_NON_COPYABLE below declares a deleted copy constructor, and a
        user-declared constructor of any kind suppresses the implicit default one.
        The class is meant to be default-constructed - the processor holds one as a
        member - so ask for it back explicitly. */
    Capture() = default;

    /** tone-match 4: 100 ms to 60 s. */
    static constexpr double kMinSeconds = 0.1;
    static constexpr double kMaxSeconds = 60.0;

    void prepare (double sampleRate, double maxSeconds = kMaxSeconds);
    void reset() noexcept;

    void start (double seconds) noexcept;
    void stop() noexcept;

    bool isRecording() const noexcept { return recording.load (std::memory_order_relaxed); }
    bool isComplete() const noexcept { return complete.load (std::memory_order_relaxed); }

    int getRecordedSamples() const noexcept { return recorded.load (std::memory_order_relaxed); }
    double getProgress() const noexcept;

    /** Audio thread. Copies into the buffer and stops when it is full. */
    void processBlock (const float* const* channels, int numChannels, int numSamples) noexcept;

    const juce::AudioBuffer<float>& getBuffer() const noexcept { return buffer; }

    /** tone-match 4: trims silence off each end. */
    void autoTrim (double thresholdDb = -60.0);

    /** Writes the capture as 32-bit float WAV. */
    bool save (const juce::File& file) const;

    static juce::File getCaptureDirectory();

private:
    juce::AudioBuffer<float> buffer;

    double sr = 48000.0;
    int capacity = 0;
    int wanted = 0;

    std::atomic<bool> recording { false };
    std::atomic<bool> complete { false };
    std::atomic<int> recorded { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Capture)
};

//==============================================================================
/** Cab match (tone-match 2).

    Plays a known test signal, records what comes back, and works out the impulse
    response that turns one into the other. Everything here is offline. */
class CabMatch
{
public:
    /** tone-match 2's three test signals. */
    enum class TestSignal { sineSweep = 0, mls, transientBurst, numSignals };

    static const char* getTestSignalName (TestSignal signal) noexcept;

    /** Generates the test signal. `seconds` is ignored for MLS, which is sized
        by its own order. */
    static std::vector<float> generateTestSignal (TestSignal signal, double sampleRate,
                                                  double seconds = 6.0);

    /*  Recovers the impulse response that maps `testSignal` to `response`.

        For a sweep this is deconvolution by the sweep's inverse filter, which is
        the time-reversed sweep with a 6 dB/octave amplitude correction: an
        exponential sweep spends longer at low frequencies than at high ones, so
        reversing it alone would leave the recovered response tilted.

        For MLS and the burst it is a straight frequency-domain division with a
        regularisation floor, which is what stops a near-zero bin in the test
        signal from exploding into noise in the result.
    */
    static ImpulseResponse deconvolve (const std::vector<float>& testSignal,
                                       const std::vector<float>& response,
                                       double sampleRate,
                                       TestSignal signal,
                                       double maxSeconds = ImpulseResponse::kMaxSecondsDefault);

    /** tone-match 2: how well the matched IR reproduces the reference, as an RMS
        null in dB. More negative is better. */
    static double measureNull (const std::vector<float>& reference,
                               const std::vector<float>& matched);

    static juce::File getMatchDirectory();

    /** Writes an IR as a WAV with its sidecar. */
    static bool saveIr (const ImpulseResponse& ir, const juce::File& file,
                        const IrMetadata& metadata);
};

//==============================================================================
/** EQ match (tone-match 3).

    Measures the long-term magnitude spectrum of a reference and of Luthier's own
    output, and fits a minimum-phase FIR that shapes one toward the other.

    It is deliberately not a cab match: matching a magnitude spectrum reproduces
    tone but not time, so nothing about a cabinet's ringing or its early
    reflections survives. tone-match 3 asks for that to be said plainly rather
    than implied, which `getDescription` does. */
class EqMatch
{
public:
    /** tone-match 3: the filter lengths offered. */
    enum class FilterLength { short256 = 0, medium1024, long4096, numLengths };

    static int getTapCount (FilterLength length) noexcept;

    struct Options
    {
        FilterLength length = FilterLength::medium1024;

        /** The band to correct over. */
        double lowHz = 20.0;
        double highHz = 20000.0;

        /** 0 to 1: how much of the measured difference to apply. */
        double aggressiveness = 1.0;

        /** tone-match 3: correct only the shape, not the broadband level. */
        bool preserveDynamics = false;
    };

    /** The long-term magnitude spectrum of a signal, in `numBins` log-spaced
        bands. Averaged over overlapping windows, so a single transient cannot
        dominate the result. */
    static std::vector<double> measureSpectrum (const std::vector<float>& signal,
                                                double sampleRate, int numBins = 96);

    /** Fits the correction filter. The returned IR is minimum-phase, so applying
        it adds no more delay than its own length. */
    static ImpulseResponse fit (const std::vector<float>& reference,
                                const std::vector<float>& current,
                                double sampleRate,
                                const Options& options = Options());

    /** The magnitude response of a filter at a frequency, in dB. Used by the
        tests and by the panel's curve display. */
    static double magnitudeAt (const ImpulseResponse& ir, double frequencyHz, double sampleRate);

    static const char* getDescription() noexcept;
};

//==============================================================================
/** The IR library's folder convention (tone-match 5). */
class IrLibraryPaths
{
public:
    static juce::File getRoot();

    static juce::File getBodies();
    static juce::File getBodiesAcoustic();
    static juce::File getBodiesElectric();
    static juce::File getCabinets();
    static juce::File getCabinetsUser();
    static juce::File getCabinetsMatch();
    static juce::File getRooms();
    static juce::File getSpecial();

    /** Creates the whole tree, so a user dropping a file in has somewhere to
        drop it. */
    static void ensureExists();

    /** Every IR under the root, recursively. */
    static juce::Array<juce::File> findAll();

    /** tone-match 7: a preset stores a path relative to the root when the IR
        lives under it, and an absolute path otherwise. */
    static juce::String toPresetPath (const juce::File& file);
    static juce::File fromPresetPath (const juce::String& path);
};

} // namespace luthier
