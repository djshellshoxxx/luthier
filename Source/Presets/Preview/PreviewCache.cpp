#include "PreviewCache.h"
#include <mutex>

namespace luthier
{

const char* const PreviewCache::kMagic = "luthier.preview";

namespace
{
    /*  Two instances in one host share this process: the lock file is checked
        then created, which is not atomic, so the same step is also taken
        under a process-wide mutex. Across processes the file alone decides. */
    std::mutex& lockMutex()
    {
        static std::mutex m;
        return m;
    }
}

namespace
{
    juce::File& defaultOverride()
    {
        static juce::File f;
        return f;
    }
}

void PreviewCache::setDefaultFolderOverride (const juce::File& folder)
{
    defaultOverride() = folder;
}

juce::File PreviewCache::getDefaultFolder()
{
    if (defaultOverride() != juce::File())
        return defaultOverride();

   #if defined (LUTHIER_FREE_EDITION) && LUTHIER_FREE_EDITION
    const juce::String product ("Luthier Free");   // editions.md 7.2
   #else
    const juce::String product ("Luthier");
   #endif

   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
             .getChildFile ("Library/Caches").getChildFile (product).getChildFile ("PresetPreviews");
   #elif JUCE_WINDOWS
    auto local = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
    juce::File base = local.isNotEmpty() ? juce::File (local)
                                         : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    return base.getChildFile (product).getChildFile ("Cache").getChildFile ("PresetPreviews");
   #else
    auto xdg = juce::SystemStats::getEnvironmentVariable ("XDG_CACHE_HOME", {});
    juce::File base = xdg.isNotEmpty() && juce::File::isAbsolutePath (xdg)
                        ? juce::File (xdg)
                        : juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache");
    return base.getChildFile (product.toLowerCase().replaceCharacter (' ', '-')).getChildFile ("preset-previews");
   #endif
}

PreviewCache::PreviewCache (juce::File f) : folder (std::move (f)) {}

bool PreviewCache::isWritable() const
{
    if (! folder.isDirectory() && ! folder.createDirectory())
        return false;

    auto probe = folder.getNonexistentChildFile (".probe", ".tmp", false);

    if (! probe.replaceWithText ("x"))
        return false;

    probe.deleteFile();
    return true;
}

juce::var PreviewCache::lookup (const juce::String& hash) const
{
    const auto sidecar = sidecarFileFor (hash);

    if (! sidecar.existsAsFile() || ! oggFileFor (hash).existsAsFile())
        return {};

    auto parsed = juce::JSON::parse (sidecar.loadFileAsString());

    if (parsed.getProperty ("magic", {}).toString() != kMagic
        || parsed.getProperty ("hash", {}).toString() != hash)
        return {};

    return parsed;
}

bool PreviewCache::readOgg (const juce::String& hash, juce::MemoryBlock& out) const
{
    return oggFileFor (hash).loadFileAsData (out) && out.getSize() > 0;
}

bool PreviewCache::store (const juce::String& hash, const juce::MemoryBlock& ogg, const juce::var& sidecar)
{
    if (! folder.isDirectory() && ! folder.createDirectory())
        return false;

    // file-formats.md 13: temp file, then rename. The Ogg lands before the
    // sidecar, so a sidecar always names a complete clip.
    auto writeAtomically = [] (const juce::File& target, const void* data, size_t size)
    {
        juce::TemporaryFile temp (target);

        if (! temp.getFile().replaceWithData (data, size))
            return false;

        return temp.overwriteTargetFileWithTemporary();
    };

    if (! writeAtomically (oggFileFor (hash), ogg.getData(), ogg.getSize()))
        return false;

    const auto text = juce::JSON::toString (sidecar, false);

    if (! writeAtomically (sidecarFileFor (hash), text.toRawUTF8(), text.getNumBytesAsUTF8()))
        return false;

    if (++writesSincePrune >= kPruneEveryWrites)
    {
        writesSincePrune = 0;
        prune();
    }

    return true;
}

void PreviewCache::remove (const juce::String& hash)
{
    oggFileFor (hash).deleteFile();
    sidecarFileFor (hash).deleteFile();
}

void PreviewCache::touch (const juce::String& hash)
{
    sidecarFileFor (hash).setLastModificationTime (juce::Time::getCurrentTime());
}

bool PreviewCache::isLockedByAnother (const juce::String& hash) const
{
    const auto lock = lockFileFor (hash);

    if (! lock.existsAsFile())
        return false;

    const auto age = juce::Time::getCurrentTime() - lock.getLastModificationTime();
    return age.inMilliseconds() < kStaleLockMs;
}

bool PreviewCache::tryLock (const juce::String& hash)
{
    if (! folder.isDirectory() && ! folder.createDirectory())
        return false;

    const std::lock_guard<std::mutex> guard (lockMutex());
    const auto lock = lockFileFor (hash);

    // A lock older than 30 s is stale: its owner died mid-render.
    if (lock.existsAsFile() && ! isLockedByAnother (hash))
        lock.deleteFile();

    if (lock.existsAsFile())
        return false;

    juce::FileOutputStream stream (lock);

    if (stream.failedToOpen())
        return false;

    stream.writeText (juce::String (juce::SystemStats::getComputerName()) + " "
                        + juce::String (juce::Time::currentTimeMillis()), false, false, nullptr);
    return true;
}

void PreviewCache::unlock (const juce::String& hash)
{
    const std::lock_guard<std::mutex> guard (lockMutex());
    lockFileFor (hash).deleteFile();
}

juce::int64 PreviewCache::getTotalBytes() const
{
    juce::int64 total = 0;

    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles))
        total += e.getFileSize();

    return total;
}

int PreviewCache::getNumEntries() const
{
    int n = 0;

    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "*.json", juce::File::findFiles))
    {
        juce::ignoreUnused (e);
        ++n;
    }

    return n;
}

void PreviewCache::prune (juce::int64 capBytes)
{
    struct Item { juce::String stem; juce::Time played; juce::int64 bytes; };
    std::vector<Item> items;
    juce::int64 total = 0;

    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles))
    {
        const auto f = e.getFile();
        total += e.getFileSize();

        if (f.hasFileExtension ("json"))
        {
            const auto ogg = f.withFileExtension ("ogg");
            items.push_back ({ f.getFileNameWithoutExtension(), f.getLastModificationTime(),
                               e.getFileSize() + (ogg.existsAsFile() ? ogg.getSize() : 0) });
        }
        else if (f.hasFileExtension ("ogg") && ! f.withFileExtension ("json").existsAsFile())
        {
            // An orphaned clip (its sidecar never landed) is dead weight.
            total -= e.getFileSize();
            f.deleteFile();
        }
    }

    if (total <= capBytes)
        return;

    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b)
    {
        return a.played != b.played ? a.played < b.played : a.stem < b.stem;
    });

    for (const auto& item : items)
    {
        if (total <= capBytes)
            break;

        folder.getChildFile (item.stem + ".ogg").deleteFile();
        folder.getChildFile (item.stem + ".json").deleteFile();
        total -= item.bytes;
    }
}

void PreviewCache::clear()
{
    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles))
    {
        const auto f = e.getFile();

        if (f.hasFileExtension ("ogg;json;tmp"))
            f.deleteFile();
    }
}

} // namespace luthier
