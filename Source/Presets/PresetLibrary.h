#pragma once

/*  preset-browser-previews.md: the processor-side hub of the preset browser.

    Owns the PresetIndex (5.6) and the PreviewRenderService (3.3), keeps the
    index in step with PresetManager's change broadcast, applies render results
    to the index, and turns "play this row" into a clip handed to the
    processor's PreviewPlayer - after the 4.3 gate says it may play.

    Created lazily by LuthierAudioProcessor on the message thread, so an
    offline render instance never starts a worker of its own. Message thread.
*/

#include "Search/PresetIndex.h"
#include "Search/PresetSearch.h"
#include "Preview/PreviewRenderService.h"
#include "PresetLibraryPrefs.h"

namespace luthier
{

class LuthierAudioProcessor;

class PresetLibrary : private juce::ChangeListener,
                      public juce::ChangeBroadcaster
{
public:
    explicit PresetLibrary (LuthierAudioProcessor&, juce::File cacheFolder = PreviewCache::getDefaultFolder());
    ~PresetLibrary() override;

    PresetIndex& getIndex() noexcept                  { return index; }
    PreviewRenderService& getService() noexcept        { return *service; }

    //==========================================================================
    /** The user preferences that shape playback (section 8), pushed by the UI. */
    struct Settings
    {
        bool enabled = true;
        bool hoverTrigger = true;       ///< false: "Click only"
        bool onSelect = true;
        double volumeDb = -6.0;
        bool whileTransport = false;
    };

    void setSettings (const Settings&);
    const Settings& getSettings() const noexcept { return settings; }

    /** Syncs with the manager and parses anything new on the worker. */
    void refresh();

    /** Parses on this thread instead (tests, luthier-render). */
    void refreshSynchronously();

    /** 2: queues every row without a valid preview, `first` ones first. */
    void queueUnrendered (const juce::Array<int>& first);

    /** Requests one entry's preview. */
    void request (int entry, PreviewRenderService::Priority);

    //==========================================================================
    /** 4.3: why a trigger may not play now, or empty when it may. */
    juce::String whyBlocked (bool explicitTrigger) const;

    /** Starts an entry's preview. Returns false (and says why in `hint`) when
        blocked; when the clip is not ready yet it is requested at interactive
        priority and plays on arrival if still wanted. */
    bool play (int entry, bool explicitTrigger, juce::String& hint);
    void stop();

    /** The entry whose preview is playing or wanted, or -1. */
    int getPlayingEntry() const;
    bool isPlaying() const;
    bool isWaitingFor (int entry) const noexcept { return wantedEntryKey.isNotEmpty() && entryKey (entry) == wantedEntryKey; }

    /** Records a load for Recent and the load counts (5.5). */
    void noteLoaded (int entry);

    /** Fired for each finished preview, after the index has taken it. */
    std::function<void (const PreviewRenderService::Ready&)> onPreviewReady;

    /** Fired when a preview starts (for the "Previewing <name>" announcement). */
    std::function<void (const juce::String& name)> onPreviewStarted;

    /** The last message-thread tick: collects dead clips, pauses background renders. */
    void tick();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleReady (const PreviewRenderService::Ready&);
    bool startClip (int entry, const juce::AudioBuffer<float>& audio, double rate);
    juce::String entryKey (int entry) const;

    LuthierAudioProcessor& processor;
    PresetIndex index;
    std::unique_ptr<PreviewRenderService> service;
    Settings settings;

    juce::String wantedEntryKey;     ///< waiting for its clip to arrive
    bool wantedExplicit = false;
    juce::String playingKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetLibrary)
};

} // namespace luthier
