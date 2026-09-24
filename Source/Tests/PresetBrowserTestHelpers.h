#pragma once

/*  Shared by PresetPreviewTests, PresetSearchTests and PresetBrowserUiTests
    (preset-browser-previews.md 16).

    - pumpMessages: the runner has no dispatch loop (modal loops are off), and
      the preview service delivers through callAsync, so the tests drain the
      message queue by hand.
    - factoryCorpus: every factory preset rendered once per test run, shared by
      the rendering, descriptor, query and similarity tests.
*/

#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Presets/Preview/FactoryPreviews.h"
#include "../Presets/Search/PresetIndex.h"
#include "../Presets/FactoryPresets.h"
#include <juce_events/juce_events.h>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }
#endif

namespace luthier::tests::browser
{

/** Runs the message queue for `ms` milliseconds. */
inline void pumpMessages (int ms)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + ms;

    do
    {
       #if JUCE_MAC
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.002, true);
       #else
        if (! juce::detail::dispatchNextMessageOnSystemQueue (true))
            juce::Thread::sleep (1);
       #endif
    }
    while (juce::Time::getMillisecondCounterHiRes() < end);
}

/** Pumps until `done` is true or the timeout passes. Returns `done()`. */
inline bool waitFor (std::function<bool()> done, int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (! done())
    {
        if (juce::Time::getMillisecondCounterHiRes() > end)
            return false;

        pumpMessages (5);
    }

    return true;
}

/** Processes a few silent blocks so the preview gate sees a running host. */
inline void runAudio (LuthierAudioProcessor& p, int blocks = 2, int blockSize = 256)
{
    juce::AudioBuffer<float> buffer (juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels())), blockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < blocks; ++i)
    {
        buffer.clear();
        midi.clear();
        p.processBlock (buffer, midi);
    }
}

//==============================================================================
struct Corpus
{
    std::vector<FactoryPreviews::Rendered> bank;
    DescriptorCalibration calibration;
    double totalRenderMs = 0.0;

    const FactoryPreviews::Rendered* find (const juce::String& name) const
    {
        for (const auto& r : bank)
            if (r.name == name)
                return &r;

        return nullptr;
    }
};

/** The factory bank, rendered once per run. */
inline const Corpus& factoryCorpus()
{
    static const Corpus corpus = []
    {
        Corpus c;
        PreviewRenderer renderer;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        c.bank = FactoryPreviews::renderBank (renderer);
        c.totalRenderMs = juce::Time::getMillisecondCounterHiRes() - start;
        c.calibration = FactoryPreviews::calibrationFor (c.bank);
        return c;
    }();

    return corpus;
}

/** An index over the factory bank with every entry analysed from the corpus. */
inline void fillIndexFromCorpus (PresetIndex& index, const juce::AudioProcessor& ranges)
{
    const auto& corpus = factoryCorpus();
    PresetFeatureReader reader (ranges);

    index.clear();

    for (const auto& r : corpus.bank)
    {
        PresetIndex::Entry e;
        e.info.name = r.name;
        e.info.category = r.json.getProperty ("category", {}).toString();
        e.info.description = r.json.getProperty ("description", {}).toString();
        e.info.author = r.json.getProperty ("author", {}).toString();

        if (auto* tags = r.json.getProperty ("tags", {}).getArray())
            for (const auto& t : *tags)
                e.info.tags.add (t.toString());

        e.info.isFactory = true;
        e.info.uid = r.uid;
        e.key = r.uid;
        e.source = PresetIndex::Source::factory;
        e.parsed = true;
        e.params = reader.read (r.json);
        e.info.guitarName = e.params.guitarName;
        e.info.ampName = e.params.ampName;
        e.soundHash = r.result.soundHash;
        e.phrase = r.result.phrase;
        e.genres = ToneDescriptors::genresFor (e.info.name, e.info.category, e.info.tags);
        e.tone = r.result.features;
        e.peaks = r.result.peaks;
        e.preview = PresetIndex::PreviewState::ready;
        index.addEntryForTesting (e);
    }

    index.setCalibration (corpus.calibration);
}

/** A scratch folder, removed with the object. */
struct ScratchFolder
{
    ScratchFolder()
        : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getNonexistentChildFile ("luthier-browser-test", "", false))
    {
        folder.createDirectory();
    }

    ~ScratchFolder() { folder.deleteRecursively(); }

    juce::File folder;
};

} // namespace luthier::tests::browser
