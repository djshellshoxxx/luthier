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

LUTHIER_TEST (PracticeLooperConcurrency, audioCallbacksUseAtomicGateAdmission)
{
    const auto header = looperHeader();
    const auto implementation = looperSource();

    CHECK_MSG (header.existsAsFile(), "could not locate Source/Practice/Looper.h from the test source path");
    CHECK_MSG (implementation.existsAsFile(), "could not locate Source/Practice/Looper.cpp from the test source path");

    const auto headerText = header.loadFileAsString();
    const auto sourceText = implementation.loadFileAsString();

    CHECK_MSG (headerText.contains ("mutable detail::StorageAccessGate storageGate"),
               "all callback and storage paths must share one atomic gate");
    CHECK_MSG (sourceText.contains ("detail::StorageAccessGate::CallbackAccess callbackAccess (storageGate)"),
               "audio and MIDI callbacks must enter through the gate");
    CHECK_MSG (! headerText.contains ("storageAccessPaused") && ! headerText.contains ("callbacksInFlight"),
               "the split pause flag and callback counter must not return");
}

LUTHIER_TEST (PracticeLooperConcurrency, callbacksReadTransportOnlyAfterGateAdmission)
{
    const auto implementation = looperSource();
    const auto source = implementation.loadFileAsString();
    const juce::StringArray functionNames {
        "void Looper::processBlock", "void Looper::renderPlaybackMidi", "void Looper::captureMidi"
    };

    for (const auto& functionName : functionNames)
    {
        const int start = source.indexOf (functionName);
        const int gate = source.indexOf (start, "StorageAccessGate::CallbackAccess callbackAccess (storageGate)");
        const int state = source.indexOf (start, "getState()");
        CHECK_MSG (start >= 0 && gate > start && state > gate,
                   functionName + " must enter the gate before reading transport state");
    }
}

LUTHIER_TEST (PracticeLooperConcurrency, loadHoldsExclusiveAccessWhileReplacingLayers)
{
    const auto implementation = looperSource();
    const auto source = implementation.loadFileAsString();
    const int loadStart = source.indexOf ("bool Looper::load (const juce::File& file)");
    const int nextFunction = source.indexOf (loadStart, "SessionRecorder::SessionRecorder");
    CHECK_MSG (loadStart >= 0 && nextFunction > loadStart, "could not isolate Looper::load");

    if (loadStart < 0 || nextFunction <= loadStart)
        return;

    const auto loadBody = source.substring (loadStart, nextFunction);
    const int guard = loadBody.indexOf ("AudioStorageGuard storageAccess (*this);");
    const int settings = loadBody.indexOf ("layer.settingsFromVar");
    const int audioRead = loadBody.indexOf ("reader->read");
    const int midiAppend = loadBody.indexOf ("layer.getMidi().addEvent");

    CHECK_MSG (guard >= 0 && settings > guard && audioRead > guard && midiAppend > guard,
               "Looper::load must keep exclusive access through settings, audio, and MIDI replacement");
}

LUTHIER_TEST (PracticeLooperConcurrency, saveSnapshotsStorageBeforeDiskIo)
{
    const auto implementation = looperSource();
    CHECK_MSG (implementation.existsAsFile(), "could not locate Source/Practice/Looper.cpp from the test source path");

    const auto source = implementation.loadFileAsString();
    const int saveStart = source.indexOf ("bool Looper::save (const juce::File& file) const");
    const int nextFunction = source.indexOf (saveStart + 1, "int Looper::importLayer");

    CHECK_MSG (saveStart >= 0, "could not locate Looper::save");
    CHECK_MSG (nextFunction > saveStart, "could not isolate Looper::save body");

    if (saveStart < 0 || nextFunction <= saveStart)
        return;

    const auto saveBody = source.substring (saveStart, nextFunction);
    const int barrierStart = saveBody.indexOf ("AudioStorageGuard storageAccess (*this);");
    const int barrierEnd = saveBody.indexOf ("storageAccess.release();");
    const int firstDiskWrite = saveBody.indexOf ("writeWav (");

    CHECK_MSG (barrierStart >= 0 && barrierEnd > barrierStart,
               "Looper::save must snapshot shared layer audio/MIDI while the audio thread is quiesced");
    CHECK_MSG (firstDiskWrite < 0 || (barrierEnd >= 0 && barrierEnd < firstDiskWrite),
               "Looper::save must release the storage barrier before WAV/JSON disk I/O");
}
