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
}

LUTHIER_TEST (PracticeLooperConcurrency, recordedSampleMetadataIsAtomicAcrossThreads)
{
    const auto header = looperHeader();
    CHECK_MSG (header.existsAsFile(), "could not locate Source/Practice/Looper.h from the test source path");

    const auto source = header.loadFileAsString();
    CHECK_MSG (source.contains ("std::atomic<int> recordedSamples"),
               "LoopLayer::recordedSamples is written on the audio thread and read on the message thread; it must be atomic");
}
