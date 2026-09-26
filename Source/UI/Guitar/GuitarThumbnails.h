#pragma once

/*  Preset-browser thumbnails (guitar-illustration.md 2.3 and 15).

    The same renderer at reduced detail (Options::thumbnail), 256 x 128 px,
    rendered on a worker thread and cached by the guitar's canonical-JSON key
    (2.1) - so a hundred presets on one guitar share one image. The cache holds
    200 images (17: ~2.4 MB) and evicts the least recently used.

    A preset file is read on the worker too: its `guitar` block (a reference, or
    the whole edited guitar) or, for a preset older than the Workshop, its guitar
    type. The worker keeps its own PartLibrary so nothing it does touches the
    message thread's.

    get() and getForPreset() never block: they return the cached image, or a
    null one and queue the render; onThumbnailReady fires on the message thread
    when one lands.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Model/Workshop/PartLibrary.h"

#include <list>
#include <map>
#include <memory>
#include <set>

namespace luthier
{

class GuitarThumbnails : private juce::Thread,
                         private juce::AsyncUpdater
{
public:
    static constexpr int kWidth = 256, kHeight = 128;
    static constexpr int kCapacity = 200;

    GuitarThumbnails();
    ~GuitarThumbnails() override;

    /** The thumbnail for a guitar, or a null image with the render queued. */
    juce::Image get (const WorkshopGuitar& guitar);

    /** The thumbnail for a preset file's guitar, or a null image with the work queued. */
    juce::Image getForPreset (const juce::File& presetFile);

    /** Message thread: called when a queued thumbnail is ready. */
    std::function<void()> onThumbnailReady;

    //==========================================================================
    /** The render itself (section 15's reduced detail). Any thread. */
    static juce::Image render (const WorkshopGuitar& guitar);

    /** The cache key: the guitar's canonical serialisation, hashed (2.1). */
    static juce::int64 keyFor (const WorkshopGuitar& guitar);

    /** A preset file's guitar: its override, its reference, or its guitar type. */
    static bool guitarForPreset (const juce::File& presetFile, const PartLibrary& library, WorkshopGuitar& out);

    int getCacheSize() const;
    bool isCached (juce::int64 key) const;

    /** Blocks until the queue is empty (tests). */
    bool waitUntilIdle (int timeoutMs);

private:
    void run() override;
    void handleAsyncUpdate() override;

    juce::Image lookup (juce::int64 key);   // touches the entry (most recent)
    void store (juce::int64 key, const juce::Image& image);

    struct Job
    {
        juce::File presetFile;             ///< either this ...
        std::unique_ptr<WorkshopGuitar> guitar;   ///< ... or this
        juce::int64 key = 0;
    };

    juce::CriticalSection lock;
    std::list<juce::int64> recency;        ///< most recent first
    std::map<juce::int64, std::pair<juce::Image, std::list<juce::int64>::iterator>> cache;
    std::map<juce::String, juce::int64> presetKeys;   ///< file + modification time -> guitar key
    std::list<Job> queue;
    std::set<juce::String> queuedPresets;
    std::set<juce::int64> queuedGuitars;
    bool busy = false;
    juce::WaitableEvent wake;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarThumbnails)
};

} // namespace luthier
