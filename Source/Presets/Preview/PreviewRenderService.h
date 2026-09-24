#pragma once

/*  preset-browser-previews.md 3.3: finds or makes the preview for a preset.

    Owned by LuthierAudioProcessor, so a render queued by a save finishes with
    the editor closed. One low-priority thread, one job at a time, in priority
    order: interactive (at most one; a newer request replaces one not yet
    started), prefetch (the neighbouring rows), onSave, background. Background
    work pauses while a transport runs or CPU relief is on.

    For each preset it tries, in order: the shipped factory clip (5.3), the
    disk cache (5.2), the in-memory store (an unwritable cache, 15), another
    instance's render in progress (the lock), and finally a render of its own.
    Results come back on the message thread through callAsync, guarded by a
    WeakReference. Shutdown cancels and joins within 500 ms.
*/

#include "PreviewRenderer.h"
#include "PreviewCache.h"
#include "../Search/PresetIndex.h"
#include <deque>
#include <map>
#include <mutex>

namespace luthier
{

class PreviewRenderService : private juce::Thread
{
public:
    enum class Priority { interactive = 0, prefetch, onSave, background };

    struct Ready
    {
        juce::File file;
        juce::String soundHash;
        bool ok = false;
        bool approximate = false;
        bool stale = false;          ///< a shipped clip from another plugin version (5.3)
        bool fromShipped = false, fromCache = false, fromMemory = false, rendered = false;
        bool timedOut = false;
        juce::String error;
        std::shared_ptr<const juce::AudioBuffer<float>> audio;   ///< 48 kHz stereo
        double sampleRate = PreviewRenderer::kSampleRate;
        std::array<float, PreviewResult::kNumPeaks> peaks {};
        ToneFeatures tone;
        double renderMs = 0.0;
    };

    /** `rangeSource` is the live processor: its parameter ranges read the files. */
    PreviewRenderService (const juce::AudioProcessor& rangeSource,
                          juce::File cacheFolder = PreviewCache::getDefaultFolder(),
                          PreviewRenderer::Factory factory = {});
    ~PreviewRenderService() override;

    //==========================================================================
    // Message thread.

    void request (const juce::File& presetFile, Priority, const juce::String& uid = {});

    /** Parses files for the index on the worker; results arrive through onParsed. */
    void requestParse (const juce::Array<juce::File>& files);

    /** Drops every queued job and cancels the running one. */
    void cancelAll();

    void setPaused (bool transportRunning, bool cpuRelief) noexcept;
    bool isPaused() const noexcept { return pausedTransport.load() || pausedCpu.load(); }

    int getNumQueued() const;
    bool isBusy() const noexcept { return busy.load(); }

    /** Called on the message thread for every finished job. */
    std::function<void (const Ready&)> onReady;
    std::function<void (const PresetIndex::Parsed&)> onParsed;

    /** 7.5 / 15: the cache folder could not be written; one banner. */
    bool isCacheUnwritable() const noexcept { return cacheUnwritable.load(); }

    /** Decoded clips kept for instant replay (message thread, 16 MB LRU). */
    std::shared_ptr<const juce::AudioBuffer<float>> getDecoded (const juce::String& soundHash) const;

    PreviewCache& getCache() noexcept { return cache; }

    /** Tests: a decoded clip as if a job had delivered it. */
    void injectDecodedForTesting (const juce::String& soundHash, std::shared_ptr<const juce::AudioBuffer<float>> audio)
    {
        decoded[soundHash] = std::move (audio);
    }

    /** The shipped factory previews (5.3), read once. */
    static juce::File getShippedFolder();
    void setShippedFolderForTesting (const juce::File& folder);

    /** How many renders this service has actually run (PB-17). */
    int getNumRenders() const noexcept { return renders.load(); }

    /** Tests: forwards to the renderer (PB-18's forced timeout). */
    void setTestDelayPerBlockMs (double ms) noexcept { renderer.testDelayPerBlockMs = ms; }
    void setTimeoutSeconds (double s) noexcept { timeoutSeconds.store (s); }

    /** How long the offline instance lives after the queue empties. */
    static constexpr int kIdleReleaseMs = 30 * 1000;

private:
    struct Job
    {
        juce::File file;
        juce::String uid;
        Priority priority = Priority::background;
    };

    void run() override;
    Ready process (const Job&);
    bool tryShipped (const juce::var& json, const juce::String& hash, const juce::String& uid,
                     PreviewPhrase::Id phrase, Ready&);
    bool decodeInto (const juce::MemoryBlock& ogg, Ready&) const;
    juce::var sidecarFor (const PreviewResult&) const;
    void deliver (Ready);
    void loadManifest();

    PreviewRenderer renderer;
    PreviewCache cache;
    PresetFeatureReader reader;

    mutable std::mutex queueLock;
    std::deque<Job> queue;                 ///< kept sorted by priority, stable
    std::deque<juce::Array<juce::File>> parseQueue;
    juce::WaitableEvent wake;

    std::atomic<bool> cancelCurrent { false };
    std::atomic<bool> pausedTransport { false }, pausedCpu { false };
    std::atomic<bool> busy { false };
    std::atomic<bool> cacheUnwritable { false };
    std::atomic<int> renders { 0 };
    std::atomic<double> timeoutSeconds { PreviewRenderer::kTimeoutSeconds };

    // Worker-owned: the manifest and the in-memory fallback store.
    juce::File shippedFolder;
    bool manifestLoaded = false;
    std::map<juce::String, juce::var> manifestByHash, manifestByUid;
    std::map<juce::String, juce::MemoryBlock> memoryStore;
    std::map<juce::String, juce::var> memorySidecars;

    // Message-thread decoded clips.
    std::map<juce::String, std::shared_ptr<const juce::AudioBuffer<float>>> decoded;
    std::deque<juce::String> decodedOrder;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PreviewRenderService)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewRenderService)
};

} // namespace luthier
