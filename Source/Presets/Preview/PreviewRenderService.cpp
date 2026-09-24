#include "PreviewRenderService.h"
#include "../../PluginProcessor.h"
#include "../../Support/IrLibrary.h"
#include "../../Support/ErrorLog.h"

namespace luthier
{

namespace
{
    constexpr size_t kDecodedBudgetBytes = 16 * 1024 * 1024;   // 11: decoded clips 16 MB or less

    juce::var peaksToVar (const std::array<float, PreviewResult::kNumPeaks>& peaks)
    {
        juce::Array<juce::var> a;

        for (float p : peaks)
            a.add ((double) p);

        return a;
    }

    std::array<float, PreviewResult::kNumPeaks> peaksFromVar (const juce::var& v)
    {
        std::array<float, PreviewResult::kNumPeaks> peaks {};

        if (auto* a = v.getArray())
            for (int i = 0; i < juce::jmin ((int) peaks.size(), a->size()); ++i)
                peaks[(size_t) i] = (float) (double) (*a)[i];

        return peaks;
    }
}

//==============================================================================
PreviewRenderService::PreviewRenderService (const juce::AudioProcessor& rangeSource, juce::File cacheFolder,
                                            PreviewRenderer::Factory factory)
    : juce::Thread ("Luthier preview renderer"),
      renderer (std::move (factory)),
      cache (std::move (cacheFolder)),
      reader (rangeSource),
      shippedFolder (getShippedFolder())
{
    // 5.2: pruned at service start.
    if (cache.isWritable())
        cache.prune();
    else
        cacheUnwritable = true;

    startThread (juce::Thread::Priority::low);
}

PreviewRenderService::~PreviewRenderService()
{
    // 3.3: cancel and join within 500 ms. The renderer polls the flag every
    // 256-sample block, so the join is one block away.
    cancelCurrent = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (2000);

    // The instance was made on the message thread; it is destroyed here too.
    renderer.releaseInstance();
}

juce::File PreviewRenderService::getShippedFolder()
{
    const auto resources = IrLibrary::getResourcesFolder();
    return resources == juce::File() ? juce::File()
                                     : resources.getChildFile ("Presets").getChildFile ("Previews");
}

void PreviewRenderService::setShippedFolderForTesting (const juce::File& folder)
{
    const std::lock_guard<std::mutex> guard (queueLock);
    shippedFolder = folder;
    manifestLoaded = false;
}

//==============================================================================
void PreviewRenderService::request (const juce::File& file, Priority priority, const juce::String& uid)
{
    {
        const std::lock_guard<std::mutex> guard (queueLock);

        // An interactive request replaces one not yet started (3.3).
        if (priority == Priority::interactive)
            queue.erase (std::remove_if (queue.begin(), queue.end(),
                                         [] (const Job& j) { return j.priority == Priority::interactive; }),
                         queue.end());

        // Already queued: it moves up if this request is more urgent.
        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            if (it->file != file)
                continue;

            if ((int) it->priority <= (int) priority)
                return;

            queue.erase (it);
            break;
        }

        Job job { file, uid, priority };
        auto at = std::find_if (queue.begin(), queue.end(),
                                [&] (const Job& j) { return (int) j.priority > (int) priority; });
        queue.insert (at, job);
    }

    wake.signal();
}

void PreviewRenderService::requestParse (const juce::Array<juce::File>& files)
{
    {
        const std::lock_guard<std::mutex> guard (queueLock);
        parseQueue.push_back (files);
    }

    wake.signal();
}

void PreviewRenderService::cancelAll()
{
    {
        const std::lock_guard<std::mutex> guard (queueLock);
        queue.clear();
    }

    cancelCurrent = true;
}

void PreviewRenderService::setPaused (bool transport, bool cpu) noexcept
{
    pausedTransport = transport;
    pausedCpu = cpu;

    if (! transport && ! cpu)
        wake.signal();
}

int PreviewRenderService::getNumQueued() const
{
    const std::lock_guard<std::mutex> guard (queueLock);
    return (int) queue.size();
}

std::shared_ptr<const juce::AudioBuffer<float>> PreviewRenderService::getDecoded (const juce::String& hash) const
{
    const auto it = decoded.find (hash);
    return it != decoded.end() ? it->second : nullptr;
}

//==============================================================================
void PreviewRenderService::run()
{
    double idleSince = juce::Time::getMillisecondCounterHiRes();

    while (! threadShouldExit())
    {
        // Index parsing goes first: names are already listed, and the filters
        // and the hashes wait on it.
        juce::Array<juce::File> parseBatch;
        bool haveParse = false;

        {
            const std::lock_guard<std::mutex> guard (queueLock);

            if (! parseQueue.empty())
            {
                parseBatch = parseQueue.front();
                parseQueue.pop_front();
                haveParse = true;
            }
        }

        if (haveParse)
        {
            for (const auto& f : parseBatch)
            {
                if (threadShouldExit())
                    return;

                auto parsed = PresetIndex::parseFile (f, reader);
                juce::WeakReference<PreviewRenderService> weak (this);

                juce::MessageManager::callAsync ([weak, parsed]
                {
                    if (auto* self = weak.get())
                        if (self->onParsed)
                            self->onParsed (parsed);
                });
            }

            continue;
        }

        Job job;
        bool haveJob = false;

        {
            const std::lock_guard<std::mutex> guard (queueLock);

            for (auto it = queue.begin(); it != queue.end(); ++it)
            {
                // 3.3: background work pauses while a transport runs or CPU relief is on.
                if (it->priority == Priority::background && isPaused())
                    continue;

                job = *it;
                queue.erase (it);
                haveJob = true;
                break;
            }
        }

        if (! haveJob)
        {
            // The offline instance goes 30 s after the queue empties (11).
            if (renderer.hasInstance() && juce::Time::getMillisecondCounterHiRes() - idleSince > kIdleReleaseMs)
            {
                renderer.releaseInstance();
            }

            wake.wait (250);
            continue;
        }

        cancelCurrent = false;
        busy = true;
        auto ready = process (job);
        busy = false;
        idleSince = juce::Time::getMillisecondCounterHiRes();

        if (! ready.error.contains ("cancelled") || ready.ok)
            deliver (std::move (ready));
    }
}

void PreviewRenderService::loadManifest()
{
    if (manifestLoaded)
        return;

    manifestLoaded = true;
    manifestByHash.clear();
    manifestByUid.clear();

    const auto manifestFile = shippedFolder.getChildFile ("previews.json");

    if (! manifestFile.existsAsFile())
        return;   // 15: factory presets render locally

    const auto parsed = juce::JSON::parse (manifestFile.loadFileAsString());

    if (auto* entries = parsed.getProperty ("entries", {}).getArray())
        for (const auto& e : *entries)
        {
            manifestByHash[e.getProperty ("soundHash", {}).toString()] = e;
            manifestByUid[e.getProperty ("uid", {}).toString()] = e;
        }
}

bool PreviewRenderService::decodeInto (const juce::MemoryBlock& ogg, Ready& r) const
{
    auto buffer = std::make_shared<juce::AudioBuffer<float>>();
    double rate = 0.0;

    if (! PreviewRenderer::decodeOgg (ogg.getData(), ogg.getSize(), *buffer, rate))
        return false;

    r.audio = buffer;
    r.sampleRate = rate;
    return true;
}

bool PreviewRenderService::tryShipped (const juce::var& json, const juce::String& hash, const juce::String& uid,
                                       PreviewPhrase::Id phrase, Ready& r)
{
    loadManifest();

    juce::var entry;
    bool stale = false;

    if (auto it = manifestByHash.find (hash); it != manifestByHash.end())
    {
        entry = it->second;
    }
    else if (auto byUid = manifestByUid.find (uid); uid.isNotEmpty() && byUid != manifestByUid.end())
    {
        // 5.3: the same sound from another plugin version (a dev build) still
        // plays, marked stale; an edited factory file does not.
        const auto shippedVersion = byUid->second.getProperty ("pluginVersion", {}).toString();

        if (shippedVersion != JucePlugin_VersionString
            && PreviewRenderer::computeSoundHash (json, phrase, shippedVersion)
                 == byUid->second.getProperty ("soundHash", {}).toString())
        {
            entry = byUid->second;
            stale = true;
        }
    }

    if (entry.isVoid())
        return false;

    juce::MemoryBlock ogg;

    if (! shippedFolder.getChildFile (entry.getProperty ("file", {}).toString()).loadFileAsData (ogg)
        || ! decodeInto (ogg, r))
        return false;

    r.ok = true;
    r.fromShipped = true;
    r.stale = stale;
    r.peaks = peaksFromVar (entry.getProperty ("peaks", {}));
    r.tone = ToneFeatures::fromVar (entry.getProperty ("features", {}));
    return true;
}

juce::var PreviewRenderService::sidecarFor (const PreviewResult& result) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("schema", PreviewCache::kSchema);
    o->setProperty ("magic", PreviewCache::kMagic);
    o->setProperty ("hash", result.soundHash);
    o->setProperty ("phrase", PreviewPhrase::getIdString (result.phrase));
    o->setProperty ("pluginVersion", JucePlugin_VersionString);
    o->setProperty ("renderMs", result.renderMs);
    o->setProperty ("gainDb", result.gainDb);
    o->setProperty ("approximate", result.approximate);
    o->setProperty ("peaks", peaksToVar (result.peaks));
    o->setProperty ("features", result.features.toVar());

    // The descriptors as the shipped calibration (or its stand-in) reads them;
    // the index recomputes its own with the current calibration.
    auto calibration = DescriptorCalibration::fromVar (juce::JSON::parse (DescriptorCalibration::getShippedFile()));

    if (! calibration.isValid())
        calibration = DescriptorCalibration::fallback();

    const auto descriptors = ToneDescriptors::attached (ToneDescriptors::evaluate (result.params, result.features, calibration, {}));
    o->setProperty ("descriptors", descriptors.joinIntoString (","));
    return juce::var (o);
}

PreviewRenderService::Ready PreviewRenderService::process (const Job& job)
{
    Ready r;
    r.file = job.file;

    const auto json = juce::JSON::parse (job.file.loadFileAsString());

    if (json.getDynamicObject() == nullptr || json.getProperty ("parameters", {}).getDynamicObject() == nullptr)
    {
        r.error = "the preset file is not readable";
        return r;
    }

    const auto params = reader.read (json);
    const auto phrase = PreviewRenderer::choosePhrase (json, params);
    const auto hash = PreviewRenderer::computeSoundHash (json, phrase);
    r.soundHash = hash;

    // 1. Shipped (factory and content packs).
    const bool haveShipped = tryShipped (json, hash, job.uid, phrase, r);

    if (haveShipped && ! r.stale)
        return r;

    // 2. The disk cache.
    auto fromCache = [&]
    {
        const auto sidecar = cache.lookup (hash);

        if (sidecar.isVoid())
            return false;

        juce::MemoryBlock ogg;

        if (! cache.readOgg (hash, ogg) || ! decodeInto (ogg, r))
        {
            cache.remove (hash);   // 15: a clip that fails to decode is rendered again
            return false;
        }

        cache.touch (hash);
        r.ok = true;
        r.stale = false;
        r.fromCache = true;
        r.fromShipped = false;
        r.approximate = (bool) sidecar.getProperty ("approximate", false);
        r.peaks = peaksFromVar (sidecar.getProperty ("peaks", {}));
        r.tone = ToneFeatures::fromVar (sidecar.getProperty ("features", {}));
        return true;
    };

    if (fromCache())
        return r;

    // 3. The in-memory store (the cache folder could not be written).
    if (auto it = memoryStore.find (hash); it != memoryStore.end() && decodeInto (it->second, r))
    {
        const auto& sidecar = memorySidecars[hash];
        r.ok = true;
        r.stale = false;
        r.fromMemory = true;
        r.fromShipped = false;
        r.approximate = (bool) sidecar.getProperty ("approximate", false);
        r.peaks = peaksFromVar (sidecar.getProperty ("peaks", {}));
        r.tone = ToneFeatures::fromVar (sidecar.getProperty ("features", {}));
        return r;
    }

    // A stale shipped clip plays now; the fresh render is background work.
    if (haveShipped && job.priority != Priority::background)
    {
        request (job.file, Priority::background, job.uid);
        return r;
    }

    // 4. Another instance may be rendering this very hash (5.2's lock).
    const bool writable = cache.isWritable();
    cacheUnwritable = ! writable;
    bool locked = false;

    if (writable)
    {
        const auto waitStart = juce::Time::getMillisecondCounterHiRes();

        while (! (locked = cache.tryLock (hash)))
        {
            if (fromCache())
                return r;

            if (threadShouldExit() || cancelCurrent.load()
                || juce::Time::getMillisecondCounterHiRes() - waitStart > PreviewCache::kStaleLockMs)
                break;

            wait (50);
        }

        if (! locked && fromCache())
            return r;

        // The lock may have come free because another instance just finished
        // this very hash (PB-17): look again before rendering it twice.
        if (locked && fromCache())
        {
            cache.unlock (hash);
            return r;
        }
    }

    // 5. Render, on a fresh render instance built here on the worker (see
    // LuthierAudioProcessor::ScopedOfflineRenderConstruction).
    r = {};
    r.file = job.file;
    r.soundHash = hash;

    auto result = renderer.render (job.file, &cancelCurrent, timeoutSeconds.load());
    ++renders;

    r.renderMs = result.renderMs;
    r.approximate = result.approximate;
    r.timedOut = result.timedOut;

    if (! result.ok)
    {
        if (locked)
            cache.unlock (hash);

        r.error = result.error;

        if (! result.cancelled)
            ErrorLog::write (ErrorLog::Severity::warn, "Presets", "PREVIEW_RENDER_FAILED",
                             job.file.getFileName() + ": " + result.error);

        return r;
    }

    const auto sidecar = sidecarFor (result);

    if (writable && cache.store (hash, result.ogg, sidecar))
    {
        cache.unlock (hash);
    }
    else
    {
        if (locked)
            cache.unlock (hash);

        // 15: previews live in memory for the session.
        cacheUnwritable = true;
        memoryStore[hash] = result.ogg;
        memorySidecars[hash] = sidecar;
    }

    auto buffer = std::make_shared<juce::AudioBuffer<float>>();

    // Played from the decoded Ogg, so a render sounds as it will from the cache.
    double rate = 0.0;

    if (! PreviewRenderer::decodeOgg (result.ogg.getData(), result.ogg.getSize(), *buffer, rate))
    {
        buffer->makeCopyOf (result.clip);
        rate = PreviewRenderer::kSampleRate;
    }

    r.ok = true;
    r.rendered = true;
    r.audio = buffer;
    r.sampleRate = rate;
    r.peaks = result.peaks;
    r.tone = result.features;
    return r;
}

void PreviewRenderService::deliver (Ready ready)
{
    juce::WeakReference<PreviewRenderService> weak (this);

    juce::MessageManager::callAsync ([weak, ready]
    {
        auto* self = weak.get();

        if (self == nullptr)
            return;

        if (ready.ok && ready.audio != nullptr)
        {
            // The decoded-clip budget, least recently delivered first out.
            self->decoded[ready.soundHash] = ready.audio;
            self->decodedOrder.erase (std::remove (self->decodedOrder.begin(), self->decodedOrder.end(), ready.soundHash),
                                      self->decodedOrder.end());
            self->decodedOrder.push_back (ready.soundHash);

            size_t bytes = 0;

            for (const auto& d : self->decoded)
                bytes += (size_t) d.second->getNumSamples() * (size_t) d.second->getNumChannels() * sizeof (float);

            while (bytes > kDecodedBudgetBytes && self->decodedOrder.size() > 1)
            {
                const auto oldest = self->decodedOrder.front();
                self->decodedOrder.pop_front();

                if (auto it = self->decoded.find (oldest); it != self->decoded.end())
                {
                    bytes -= (size_t) it->second->getNumSamples() * (size_t) it->second->getNumChannels() * sizeof (float);
                    self->decoded.erase (it);
                }
            }
        }

        if (self->onReady)
            self->onReady (ready);
    });
}

} // namespace luthier
