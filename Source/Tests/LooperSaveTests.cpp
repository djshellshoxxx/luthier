#include "TestFramework.h"
#include "../Practice/Looper.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (PracticeLooper, failedLayerWriteDoesNotPublishManifest)
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getNonexistentChildFile ("luthier-loop-save", "", false);
    CHECK (directory.createDirectory().wasOk());
    const auto target = directory.getChildFile ("take.luthierloop");
    const auto folder = directory.getChildFile ("take");
    const auto blockedLayer = folder.getChildFile ("layer1.wav");
    CHECK (blockedLayer.createDirectory().wasOk());
    CHECK (blockedLayer.getChildFile ("keep").replaceWithText ("block WAV creation"));

    Looper looper;
    looper.prepare (48000.0, 1.0);
    juce::AudioBuffer<float> audio (2, 64);
    audio.clear();
    audio.setSample (0, 0, 0.25f);
    CHECK (looper.importLayer (0, audio) == 64);
    CHECK (! looper.save (target));
    CHECK (! folder.getChildFile ("loop.json").existsAsFile());
    CHECK (looper.getLayer (0).hasContent());

    CHECK (blockedLayer.deleteRecursively());
    CHECK (looper.save (target));
    Looper restored;
    restored.prepare (48000.0, 1.0);
    CHECK (restored.load (target));
    CHECK (restored.getLayer (0).getRecordedSamples() == 64);
    CHECK (std::abs (restored.getLayer (0).readLeft()[0] - 0.25f) < 1.0e-5f);
    CHECK (directory.deleteRecursively());
}

LUTHIER_TEST (PracticeLooper, saveRejectsUnavailableDestination)
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getNonexistentChildFile ("luthier-loop-destination", "", false);
    CHECK (directory.createDirectory().wasOk());
    const auto blockedFolder = directory.getChildFile ("take");
    CHECK (blockedFolder.replaceWithText ("existing file"));
    Looper looper;
    looper.prepare (48000.0, 1.0);
    juce::AudioBuffer<float> audio (2, 64);
    audio.clear();
    CHECK (looper.importLayer (0, audio) == 64);
    CHECK (! looper.save (directory.getChildFile ("take.luthierloop")));
    CHECK (blockedFolder.loadFileAsString() == "existing file");
    CHECK (looper.getLayer (0).hasContent());
    CHECK (directory.deleteRecursively());
}

LUTHIER_TEST (PracticeLooper, recordedSampleCountKeepsLongestRecordedRegion)
{
    LoopLayer layer;
    layer.prepare (128);
    float audio[16] {};
    layer.record (audio, audio, 32, 16, 128);
    CHECK (layer.getRecordedSamples() == 48);
    layer.record (audio, audio, 0, 16, 128);
    CHECK (layer.getRecordedSamples() == 48);
    layer.reset();
    CHECK (layer.getRecordedSamples() == 0);
}
