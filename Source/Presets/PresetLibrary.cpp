#include "PresetLibrary.h"
#include "../PluginProcessor.h"

namespace luthier
{

PresetLibrary::PresetLibrary (LuthierAudioProcessor& p, juce::File cacheFolder)
    : processor (p)
{
    service = std::make_unique<PreviewRenderService> (p, std::move (cacheFolder));

    // The shipped calibration until the factory bank has been analysed (6.3).
    const auto shipped = DescriptorCalibration::fromVar (juce::JSON::parse (DescriptorCalibration::getShippedFile()));

    if (shipped.isValid())
        index.setCalibration (shipped);

    service->onParsed = [this] (const PresetIndex::Parsed& parsed) { index.applyParsed (parsed); };
    service->onReady = [this] (const PreviewRenderService::Ready& ready) { handleReady (ready); };

    processor.getPresetManager().addChangeListener (this);

    // 2: a save queues a high-priority render after its atomic write, so the
    // new preset has a preview and tags like a factory one.
    processor.getPresetManager().onPresetSaved = [this] (const juce::File& file)
    {
        refresh();
        const int i = index.indexOfFile (file);
        service->request (file, PreviewRenderService::Priority::onSave, i >= 0 ? index[i].key : juce::String());
    };

    index.syncWithManager (processor.getPresetManager());

    juce::Array<juce::File> files;

    for (const auto& e : index.getEntries())
        files.add (e.info.file);

    service->requestParse (files);
}

PresetLibrary::~PresetLibrary()
{
    processor.getPresetManager().removeChangeListener (this);
    processor.getPresetManager().onPresetSaved = nullptr;
    processor.getPreviewPlayer().stop();
    service.reset();
}

void PresetLibrary::setSettings (const Settings& s)
{
    settings = s;
    processor.getPreviewPlayer().setVolumeDb (s.volumeDb);

    // 8 / PB-33: previews off blocks every trigger and stops what is playing.
    if (! s.enabled)
        stop();
}

void PresetLibrary::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void PresetLibrary::refresh()
{
    index.syncWithManager (processor.getPresetManager());

    juce::Array<juce::File> files;

    for (const auto& e : index.getEntries())
        if (! e.parsed || e.info.modified != e.info.file.getLastModificationTime())
            files.add (e.info.file);

    if (! files.isEmpty())
        service->requestParse (files);

    sendChangeMessage();
}

void PresetLibrary::refreshSynchronously()
{
    index.syncWithManager (processor.getPresetManager());
    PresetFeatureReader reader (processor);
    index.parseAllSynchronously (reader);
    sendChangeMessage();
}

juce::String PresetLibrary::entryKey (int entry) const
{
    return juce::isPositiveAndBelow (entry, index.size()) ? index[entry].key : juce::String();
}

void PresetLibrary::request (int entry, PreviewRenderService::Priority priority)
{
    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& e = index[entry];

    // 15: a failed render is retried once (the next open), then only after the
    // sound changes, which resets the count.
    if (e.corrupt || e.failures >= 2)
        return;

    if (e.preview == PresetIndex::PreviewState::ready && service->getDecoded (e.soundHash) != nullptr)
        return;

    service->request (e.info.file, priority, e.key);

    if (e.preview != PresetIndex::PreviewState::ready && e.soundHash.isNotEmpty())
        index.setPreviewState (e.soundHash, PresetIndex::PreviewState::queued);
}

void PresetLibrary::queueUnrendered (const juce::Array<int>& first)
{
    for (int i : first)
        request (i, PreviewRenderService::Priority::prefetch);

    for (int i = 0; i < index.size(); ++i)
        if (! first.contains (i) && index[i].preview != PresetIndex::PreviewState::ready)
            request (i, PreviewRenderService::Priority::background);
}

//==============================================================================
juce::String PresetLibrary::whyBlocked (bool explicitTrigger) const
{
    if (! settings.enabled)
        return "Previews are off - Options -> Appearance";

    const auto gate = processor.getPreviewPlayer().getGate();

    if (gate.nonRealtime)
        return "Previews do not play while the host renders offline.";

    if (gate.killActive)
        return "Previews are silent while the kill switch is on.";

    if (gate.msSinceProcess > 200.0)
        return "The host is not processing audio. Arm or monitor the track to hear previews.";

    if ((gate.hostPlaying || processor.isTuneTransportRunning()) && ! settings.whileTransport)
        return "Previews pause while the host plays";

    // 4.3: hover and auto-on-select wait while the player plays; an explicit
    // request plays on top.
    if (! explicitTrigger && (gate.liveNotesHeld || gate.msSinceLiveNote < 500.0))
        return "Previews wait while you play.";

    return {};
}

bool PresetLibrary::startClip (int entry, const juce::AudioBuffer<float>& audio, double rate)
{
    const auto& e = index[entry];
    auto& player = processor.getPreviewPlayer();

    auto clip = PreviewClipPool::makeClip (audio, rate, player.getSampleRate(), e.key, e.info.name);
    const auto* ptr = processor.getPreviewClipPool().add (std::move (clip));

    player.setVolumeDb (settings.volumeDb);
    player.start (ptr);
    playingKey = e.key;

    // 5.2: each play touches the sidecar, for least-recently-played eviction.
    service->getCache().touch (e.soundHash);

    if (onPreviewStarted)
        onPreviewStarted (e.info.name);

    return true;
}

bool PresetLibrary::play (int entry, bool explicitTrigger, juce::String& hint)
{
    hint = whyBlocked (explicitTrigger);

    if (hint.isNotEmpty() || ! juce::isPositiveAndBelow (entry, index.size()))
        return false;

    const auto& e = index[entry];

    if (e.corrupt)
    {
        hint = "Preview could not be rendered: " + e.previewError + ". The preset can still be loaded.";
        return false;
    }

    if (auto audio = service->getDecoded (e.soundHash))
    {
        wantedEntryKey.clear();
        return startClip (entry, *audio, PreviewRenderer::kSampleRate);
    }

    // Not decoded yet: the front of the queue, and it plays on arrival.
    processor.getPreviewPlayer().stop();
    wantedEntryKey = e.key;
    wantedExplicit = explicitTrigger;
    service->request (e.info.file, PreviewRenderService::Priority::interactive, e.key);

    if (e.preview != PresetIndex::PreviewState::ready)
        index.setPreviewState (e.soundHash, PresetIndex::PreviewState::rendering);

    return true;
}

void PresetLibrary::stop()
{
    wantedEntryKey.clear();
    playingKey.clear();
    processor.getPreviewPlayer().stop();
}

int PresetLibrary::getPlayingEntry() const
{
    if (wantedEntryKey.isNotEmpty())
        return index.indexOfKey (wantedEntryKey);

    if (processor.getPreviewPlayer().isActive() && playingKey.isNotEmpty())
        return index.indexOfKey (playingKey);

    return -1;
}

bool PresetLibrary::isPlaying() const
{
    return processor.getPreviewPlayer().isActive();
}

void PresetLibrary::noteLoaded (int entry)
{
    if (juce::isPositiveAndBelow (entry, index.size()))
        PresetLibraryPrefs::get().noteLoaded (index[entry].key);
}

void PresetLibrary::handleReady (const PreviewRenderService::Ready& ready)
{
    if (ready.ok)
        index.applyAnalysis (ready.soundHash, ready.tone, ready.peaks, ready.approximate, ready.stale);
    else
        index.setPreviewState (ready.soundHash,
                               ready.error == "cancelled" ? PresetIndex::PreviewState::none : PresetIndex::PreviewState::failed,
                               ready.error);

    if (wantedEntryKey.isNotEmpty())
    {
        const int wanted = index.indexOfKey (wantedEntryKey);

        if (wanted >= 0 && index[wanted].soundHash == ready.soundHash)
        {
            wantedEntryKey.clear();

            // Still allowed? The gate may have closed while it rendered.
            if (ready.ok && ready.audio != nullptr && whyBlocked (wantedExplicit).isEmpty())
                startClip (wanted, *ready.audio, ready.sampleRate);
        }
    }

    if (onPreviewReady)
        onPreviewReady (ready);

    sendChangeMessage();
}

void PresetLibrary::tick()
{
    auto& player = processor.getPreviewPlayer();
    processor.getPreviewClipPool().collectGarbage (player);

    const auto gate = player.getGate();
    service->setPaused (gate.hostPlaying || processor.isTuneTransportRunning(), false);

    if (! player.isActive() && wantedEntryKey.isEmpty())
        playingKey.clear();
}

} // namespace luthier
