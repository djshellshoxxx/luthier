#pragma once

/*  preset-browser-previews.md 5.2: the on-disk preview cache.

    A cache, not user data, so it lives in the OS cache folder rather than in
    Documents (which is often synced). Each entry is <hash32>.ogg plus a
    <hash32>.json sidecar; writes are temp-then-rename; a <hash32>.lock created
    exclusively stops two instances rendering the same hash; the folder is
    capped at 128 MB, evicting the least recently played (a play touches the
    sidecar's modification time).

    Thread-safe for use by the render worker and the message thread: every
    operation is a file-system call on distinct entries, and the only shared
    state is the write counter.
*/

#include <juce_core/juce_core.h>
#include <atomic>

namespace luthier
{

class PreviewCache
{
public:
    static constexpr juce::int64 kCapBytes = 128 * 1024 * 1024;
    static constexpr int kPruneEveryWrites = 50;
    static constexpr int kStaleLockMs = 30 * 1000;
    static const char* const kMagic;   ///< "luthier.preview"
    static constexpr int kSchema = 1;

    /** The default folder for this OS (5.2's table). */
    static juce::File getDefaultFolder();

    explicit PreviewCache (juce::File folder = getDefaultFolder());

    const juce::File& getFolder() const noexcept { return folder; }

    /** Probes the folder by writing to it. */
    bool isWritable() const;

    /** The first 32 hex digits name the files. */
    static juce::String shortHash (const juce::String& hash) { return hash.substring (0, 32); }

    juce::File oggFileFor (const juce::String& hash) const     { return folder.getChildFile (shortHash (hash) + ".ogg"); }
    juce::File sidecarFileFor (const juce::String& hash) const { return folder.getChildFile (shortHash (hash) + ".json"); }
    juce::File lockFileFor (const juce::String& hash) const    { return folder.getChildFile (shortHash (hash) + ".lock"); }

    /** The sidecar for a hash, or void when there is no valid entry (a sidecar
        whose magic or full hash does not match counts as none). */
    juce::var lookup (const juce::String& hash) const;

    /** Reads the entry's Ogg bytes. */
    bool readOgg (const juce::String& hash, juce::MemoryBlock& out) const;

    /** Writes an entry atomically (temp file then rename). */
    bool store (const juce::String& hash, const juce::MemoryBlock& ogg, const juce::var& sidecar);

    /** Removes one entry (a clip that failed to decode, 15). */
    void remove (const juce::String& hash);

    /** Marks an entry as just played, for the eviction order. */
    void touch (const juce::String& hash);

    /** Exclusive lock for a render; false when another instance holds a fresh one. */
    bool tryLock (const juce::String& hash);
    void unlock (const juce::String& hash);
    bool isLockedByAnother (const juce::String& hash) const;

    /** Evicts least-recently-played entries until the folder is under the cap. */
    void prune (juce::int64 capBytes = kCapBytes);

    juce::int64 getTotalBytes() const;
    int getNumEntries() const;

    /** Options -> FILE LOCATIONS "Clear". */
    void clear();

private:
    juce::File folder;
    std::atomic<int> writesSincePrune { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewCache)
};

} // namespace luthier
