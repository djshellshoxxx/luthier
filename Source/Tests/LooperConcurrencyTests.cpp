#include "TestFramework.h"

#include <juce_core/juce_core.h>

using namespace luthier::tests;

namespace
{
    juce::File looperHeader()
    {
        auto testsDir = juce::File (__FILE__).getParentDirectory();
        return testsDir.getSiblingFile ("Practice").getChildFile ("Looper.h");
    }

    juce::File looperSource()
    {
        auto testsDir = juce::File (__FILE__).getParentDirectory();
        return testsDir.getSiblingFile ("Practice").getChildFile ("Looper.cpp");
    }
}

LUTHIER_TEST (PracticeLooperConcurrency, recordedSampleMetadataIsAtomicAcrossThreads)
{
    const auto header = looperHeader();
    CHECK_MSG (header.existsAsFile(), "could not locate Source/Practice/Looper.h from the test source path");

    const auto source = header.loadFileAsString();
    CHECK_MSG (source.contains ("std::atomic<int> recordedSamples"),
               "LoopLayer::recordedSamples is written on the audio thread and read on the message thread; it must be atomic");
}

LUTHIER_TEST (PracticeLooperConcurrency, destructiveStorageOperationsQuiesceAudioAccess)
{
    const auto header = looperHeader();
    const auto implementation = looperSource();

    CHECK_MSG (header.existsAsFile(), "could not locate Source/Practice/Looper.h from the test source path");
    CHECK_MSG (implementation.existsAsFile(), "could not locate Source/Practice/Looper.cpp from the test source path");

    const auto headerText = header.loadFileAsString();
    const auto sourceText = implementation.loadFileAsString();

    CHECK_MSG (headerText.contains ("std::atomic<bool> storageAccessPaused"),
               "message-thread storage operations need a pause flag so new callbacks cannot touch layer buffers");
    CHECK_MSG (headerText.contains ("std::atomic<int> callbacksInFlight"),
               "message-thread storage operations need an in-flight callback counter before touching layer buffers");
    CHECK_MSG (sourceText.contains ("callbacksInFlight.fetch_add"),
               "processBlock must publish entry before it can touch looper storage");
    CHECK_MSG (sourceText.contains ("storageAccessPaused.load"),
               "processBlock must bail out while message-thread storage access owns the layer buffers");
}
