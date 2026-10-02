#pragma once

/*  The worker half of output normalization (output-normalization.md 3.3,
    4.2-4.6).

    A low-priority thread that wakes every 10 ms, compares the change
    tracker's request serial with the last one it handled, and for a new
    request:
      1. captures the sound state (OutputNormalization::captureSoundState,
         worker-safe: parameter atomics plus the structural snapshot the
         message thread last published);
      2. hashes it (3.3) and looks the hash up in the in-memory LRU, the
         factory table and the disk cache, in that order;
      3. on a miss, renders NormalizationPhrase through a fresh offline
         LuthierAudioProcessor (normalization forced off in it) and measures
         the BS.1770 integrated loudness of its main output;
      4. hands {measured LUFS, flags, source} back to OutputNormalization,
         which turns it into a gain for the current target and publishes it
         to the audio thread as one atomic word.

    The measured loudness is a pure function of the hash (the render uses the
    quantised values the hash is made of), which is what makes every cache
    plain memoisation and every offline render deterministic (4.6).

    Spec deviation, recorded in docs/coverage/FEAT-NORMALIZE.md: the render
    instance is created fresh for every render rather than borrowed from the
    preset-preview service (which does not exist yet in this build) or reused.
    A reused instance carries noise-generator state from its last render, and
    that would make the measurement depend on history rather than the hash.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <vector>

namespace luthier
{

class OutputNormalization;

//==============================================================================
/** The sound configuration of 3.3: every parameter's normalised value (by
    parameter index) plus the structural state that is not a parameter. */
struct NormalizationSoundState
{
    std::vector<float> values;      ///< by parameter index, normalised
    juce::var structural;           ///< { preset, modulation, routing, character, toneMatch }
    bool valid = false;

    /*  The live rate's family: 1 up to 50 kHz, 2 up to 100 kHz, 4 above.
        4.3 assumes loudness is rate-invariant within 0.2 LU and renders every
        calibration at 48 kHz. This engine is not: it plays about 3 dB louder
        at 96 kHz than at 48 kHz (ON-30), so a 48 kHz gain would miss the
        target by that much. A family other than 1 renders at 48 kHz x family
        and is part of the hash; 44.1 and 48 kHz (family 1) share hashes, the
        factory table and every stored session calibration, exactly as 4.3
        has it. */
    int rateFamily = 1;

    static int rateFamilyFor (double sampleRate) noexcept
    {
        return sampleRate > 100000.0 ? 4 : (sampleRate > 50000.0 ? 2 : 1);
    }
};

//==============================================================================
class NormalizationCalibrator : private juce::Thread
{
public:
    /** 3.3: bumped when the phrase, the meter or the engine's level moves, or
        when the canonical key's contents change. Revision 2 (ON-27): the
        toolchain-derived character aging maps (fretWear, deadSpots) are no
        longer hashed - they are a per-CPU function of the retained seed, so
        they made the factory table machine-dependent. */
    static constexpr int kCalibrationRevision = 2;

    /** 4.3: a render is abandoned after this much wall time. */
    static constexpr double kRenderTimeoutSeconds = 10.0;

    /** 4.4: the in-memory LRU, shared by every instance in the process. */
    static constexpr int kLruCapacity = 512;

    enum class Source { none, factory, memory, disk, render, estimate, session };
    static const char* sourceName (Source s) noexcept;

    struct Measurement
    {
        bool ok = false;
        double measuredLufs = -120.0;
        bool unmeasurable = false;
        bool estimate = false;
        bool failed = false;
        Source source = Source::none;
        juce::String hash;
    };

    explicit NormalizationCalibrator (OutputNormalization& owner);
    ~NormalizationCalibrator() override;

    /** The last request serial whose result has been handed back (published
        or discarded). The offline wait of 4.6 polls this. */
    std::uint32_t getHandledSerial() const noexcept { return handledSerial.load (std::memory_order_acquire); }

    /** True while a render is running (the readout's "Measuring"). */
    bool isRendering() const noexcept { return rendering.load (std::memory_order_acquire); }

    /** Queues a calibration of a state that is not the live one (a preset
        morph endpoint, a tune's presets: 3.2, 4.4 prefetch). `done` runs on
        the worker. */
    void calibrateAsync (NormalizationSoundState state, std::function<void (const Measurement&)> done);

    //==========================================================================
    // Pure functions and the process-wide caches. Any thread.

    /** 3.3's hash, 64 hex digits. */
    static juce::String hashSoundState (const NormalizationSoundState& state, const juce::AudioProcessor& shape);

    /** The canonical JSON the hash is taken over (Diagnostics, tests). */
    static juce::String canonicalSoundState (const NormalizationSoundState& state, const juce::AudioProcessor& shape);

    /** The state block an offline instance restores to play the reference
        render: Performance parameters at their defaults, Mix forced neutral,
        Config quantised. */
    static juce::MemoryBlock makeRenderState (const NormalizationSoundState& state, const juce::AudioProcessor& shape);

    /** Renders the phrase for a state block through a fresh offline instance
        and measures it. Blocking; the worker's, RenderCli's and the tests'. */
    static Measurement renderAndMeasure (const juce::MemoryBlock& stateBlock,
                                         std::function<bool()> shouldCancel = {},
                                         double sampleRate = 48000.0);

    /** 4.4: cache lookups in order (no render). Source is set on a hit. */
    static bool lookupCached (const juce::String& hash, Measurement& out);

    static void storeInMemory (const juce::String& hash, const Measurement& m);
    static void storeOnDisk (const juce::String& hash, const Measurement& m);

    /** 4.5: the nearest factory-table entry, flagged estimate. */
    static bool estimateFor (int guitarType, int ampModel, double drive, Measurement& out);

    //==========================================================================
    // Locations.

    static juce::File getDiskCacheFolder();
    static void setDiskCacheFolderForTesting (const juce::File& folder);
    static void clearDiskCache();

    /** Resources/NormalizationFactory.json. */
    static juce::File getFactoryTableFile();
    static void setFactoryTableFileForTesting (const juce::File& file);
    static void reloadFactoryTable();
    static int getNumFactoryEntries();

    /** Adds an entry to the factory table in memory (RenderCli builds the file
        through this, and tests seed it). */
    static void addFactoryEntry (const juce::String& hash, double measuredLufs, int guitarType, int ampModel,
                                 double drive, const juce::String& preset);

    /** Drops every in-memory entry whose preset label names one of these presets
        (the `"Name / N"` and `"Name (own guitar)"` rows), so a partial regen can
        replace just those presets without leaving the old-hash rows behind. */
    static void removeFactoryEntriesForPresets (const juce::StringArray& presetNames);

    static bool writeFactoryTable (const juce::File& file);

    static void clearMemoryCache();

    //==========================================================================
    // Test hooks (ON-16, ON-27, ON-29).

    static std::atomic<int>& renderCount() noexcept;
    static std::atomic<bool>& failRendersForTesting() noexcept;
    static std::atomic<int>& maxInjectedDelayMsForTesting() noexcept;

    /** 4.3's 10 s, or a test's override (ON-29) when positive. */
    static std::atomic<double>& renderTimeoutOverrideForTesting() noexcept;
    static double getRenderTimeoutSeconds() noexcept;

    /** 3.3: the edition string that goes into every hash. */
    static juce::String getEditionName();

private:
    void run() override;
    void handleLiveRequest (std::uint32_t serial);
    Measurement measureState (const NormalizationSoundState& state, const std::function<bool()>& superseded);

    OutputNormalization& owner;

    std::atomic<std::uint32_t> handledSerial { 0 };
    std::atomic<bool> rendering { false };
    std::atomic<bool> cancelRender { false };   // set on shutdown

    std::mutex jobLock;
    std::vector<std::pair<NormalizationSoundState, std::function<void (const Measurement&)>>> jobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NormalizationCalibrator)
};

} // namespace luthier
