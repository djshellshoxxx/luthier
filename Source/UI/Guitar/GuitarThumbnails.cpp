#include "GuitarThumbnails.h"
#include "GuitarRenderer.h"
#include "../../PluginProcessor.h"

#include <set>

namespace luthier
{

namespace
{
    juce::String presetKeyOf (const juce::File& f)
    {
        return f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds());
    }

    /** "Factory/<Family>/<Name>.luthierguitar" or "User/<Name>...": user first, then factory. */
    juce::File resolveReference (const juce::String& reference)
    {
        auto relative = reference;

        for (const auto* prefix : { "Factory/", "User/" })
            if (relative.startsWithIgnoreCase (prefix))
                relative = relative.substring ((int) std::strlen (prefix));

        relative = PartLibrary::renamedFactoryGuitar (relative);

        for (const auto& root : { PartLibrary::getUserGuitarsFolder(), PartLibrary::getFactoryGuitarsFolder() })
            if (const auto file = root.getChildFile (relative); file.existsAsFile())
                return file;

        const auto flat = PartLibrary::getUserGuitarsFolder().getChildFile (relative.fromLastOccurrenceOf ("/", false, false));
        return flat.existsAsFile() ? flat : juce::File();
    }

    /** The factory guitar file a guitar type stands for (guitar-workshop.md 0.6). */
    juce::File factoryFileForType (int type)
    {
        auto path = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) juce::jlimit (0, (int) GuitarType::NumTypes - 1, type));

        // The Custom type has no file of its own: it shows the electric template.
        if (path.isEmpty())
            path = PartLibrary::getFamilyTemplate ("electric");

        return PartLibrary::getFactoryGuitarsFolder().getChildFile (path);
    }
}

//==============================================================================
GuitarThumbnails::GuitarThumbnails() : juce::Thread ("Luthier guitar thumbnails")
{
    startThread (juce::Thread::Priority::low);
}

GuitarThumbnails::~GuitarThumbnails()
{
    cancelPendingUpdate();
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
}

juce::Image GuitarThumbnails::render (const WorkshopGuitar& guitar)
{
    GuitarRenderer::Options options;
    options.thumbnail = true;
    return GuitarRenderer::render (guitar, kWidth, kHeight, juce::Colours::transparentBlack, options);
}

juce::int64 GuitarThumbnails::keyFor (const WorkshopGuitar& guitar)
{
    GuitarRenderer::Options options;
    options.thumbnail = true;
    return GuitarRenderer::keyFor (guitar, options);
}

bool GuitarThumbnails::guitarForPreset (const juce::File& presetFile, const PartLibrary& library, WorkshopGuitar& out)
{
    juce::var json;

    if (juce::JSON::parse (presetFile.loadFileAsString(), json).failed() || ! json.isObject())
        return false;

    PartLibrary::LoadReport report;
    const auto block = json.getProperty ("guitar", {});

    // An edited guitar travels whole (file-formats.md 2).
    if (const auto override = block.getProperty ("override", {}); override.isObject())
        return library.buildGuitar (override, out, report);

    if (const auto file = resolveReference (block.getProperty ("reference", {}).toString()); file.existsAsFile())
        return library.loadGuitar (file, out, report);

    // A preset older than the Workshop names only its guitar type (normalised).
    const auto normalised = (double) json.getProperty ("parameters", {}).getProperty ("guitar_type", 0.0);
    const int type = juce::roundToInt (normalised * (double) ((int) GuitarType::NumTypes - 1));

    if (const auto file = factoryFileForType (type); file.existsAsFile())
        return library.loadGuitar (file, out, report);

    return false;
}

//==============================================================================
juce::Image GuitarThumbnails::lookup (juce::int64 key)
{
    const juce::ScopedLock sl (lock);
    const auto it = cache.find (key);

    if (it == cache.end())
        return {};

    recency.splice (recency.begin(), recency, it->second.second);
    return it->second.first;
}

void GuitarThumbnails::store (juce::int64 key, const juce::Image& image)
{
    const juce::ScopedLock sl (lock);

    if (const auto it = cache.find (key); it != cache.end())
    {
        it->second.first = image;
        recency.splice (recency.begin(), recency, it->second.second);
        return;
    }

    recency.push_front (key);
    cache[key] = { image, recency.begin() };

    // 17: 200 caches; the least recently used goes.
    while ((int) cache.size() > kCapacity)
    {
        cache.erase (recency.back());
        recency.pop_back();
    }
}

int GuitarThumbnails::getCacheSize() const
{
    const juce::ScopedLock sl (lock);
    return (int) cache.size();
}

bool GuitarThumbnails::isCached (juce::int64 key) const
{
    const juce::ScopedLock sl (lock);
    return cache.find (key) != cache.end();
}

juce::Image GuitarThumbnails::get (const WorkshopGuitar& guitar)
{
    const auto key = keyFor (guitar);

    if (auto image = lookup (key); image.isValid())
        return image;

    {
        const juce::ScopedLock sl (lock);

        if (queuedGuitars.insert (key).second)
        {
            Job job;
            job.guitar = std::make_unique<WorkshopGuitar> (guitar);
            job.key = key;
            queue.push_back (std::move (job));
        }
    }

    wake.signal();
    return {};
}

juce::Image GuitarThumbnails::getForPreset (const juce::File& presetFile)
{
    const auto presetKey = presetKeyOf (presetFile);

    {
        const juce::ScopedLock sl (lock);

        if (const auto it = presetKeys.find (presetKey); it != presetKeys.end())
        {
            const auto key = it->second;
            const juce::ScopedUnlock unlocked (lock);

            if (auto image = lookup (key); image.isValid())
                return image;
        }

        if (queuedPresets.insert (presetKey).second)
        {
            Job job;
            job.presetFile = presetFile;
            queue.push_back (std::move (job));
        }
    }

    wake.signal();
    return {};
}

bool GuitarThumbnails::waitUntilIdle (int timeoutMs)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (juce::Time::getMillisecondCounter() < until)
    {
        {
            const juce::ScopedLock sl (lock);
            if (queue.empty() && ! busy)
                return true;
        }

        juce::Thread::sleep (2);
    }

    return false;
}

void GuitarThumbnails::run()
{
    std::unique_ptr<PartLibrary> library;

    while (! threadShouldExit())
    {
        Job job;
        bool have = false;

        {
            const juce::ScopedLock sl (lock);

            if (! queue.empty())
            {
                job = std::move (queue.front());
                queue.pop_front();
                have = busy = true;
            }
        }

        if (! have)
        {
            wake.wait (250);
            continue;
        }

        if (library == nullptr)
            library = std::make_unique<PartLibrary>();

        WorkshopGuitar guitar;
        bool ok = true;
        juce::String presetKey;

        if (job.guitar != nullptr)
            guitar = *job.guitar;
        else
        {
            presetKey = presetKeyOf (job.presetFile);
            ok = guitarForPreset (job.presetFile, *library, guitar);
        }

        if (ok)
        {
            const auto key = job.guitar != nullptr ? job.key : keyFor (guitar);

            if (! isCached (key))
                store (key, render (guitar));
            else
                lookup (key);

            const juce::ScopedLock sl (lock);

            if (presetKey.isNotEmpty())
                presetKeys[presetKey] = key;
        }

        {
            const juce::ScopedLock sl (lock);
            queuedGuitars.erase (job.key);
            if (presetKey.isNotEmpty())
                queuedPresets.erase (presetKey);
            busy = false;
        }

        if (ok)
            triggerAsyncUpdate();
    }
}

void GuitarThumbnails::handleAsyncUpdate()
{
    if (onThumbnailReady)
        onThumbnailReady();
}

} // namespace luthier
