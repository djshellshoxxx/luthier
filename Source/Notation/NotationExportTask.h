#pragma once

/*  Notation export off the message thread (notation-export.md 0.1).

    "Notation export is offline. It runs on a worker thread." The renderers in
    NotationExporter are pure functions of a PerformanceScore, and a score is
    a value, so the UI hands a copy of the score (or, for MIDI, the take as a
    MidiPerformance in the MIDI OUT profile) to this task and gets on with
    painting. The worker renders and writes; progress and the result come back
    on the message thread through the callbacks given to start().

    Cancel is honoured between the stages a format has - before the render,
    before the write - because a render is one call and a write is atomic
    through a temporary file; a cancel that lands after the file is written
    finds an export that succeeded. One export at a time per task; a second
    start() while one runs is refused.

    Nothing here touches the audio thread.
*/

#include "NotationExport.h"

#include "../Export/MidiProfiles.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

namespace luthier
{

class NotationExportTask
{
public:
    struct Request
    {
        NotationFormat format = NotationFormat::musicXml;
        juce::File destination;

        /** Every format but MIDI renders this. */
        PerformanceScore score;
        NotationExportOptions options;

        /** MIDI writes this, in the MIDI OUT profile (midi-export 0.1). */
        MidiPerformance performance;
        MidiExportOptions midiOptions;

        /** Holds each stage at least this long, checking for a cancel every
            few milliseconds. 0 in use; the tests use it to cancel a running
            export deterministically. */
        int minimumStageMs = 0;
    };

    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        juce::String error;
        juce::File destination;
    };

    /** Both are called on the message thread, and never after the task is destroyed. */
    using ProgressFn = std::function<void (double fraction)>;
    using DoneFn = std::function<void (const Result&)>;

    NotationExportTask();

    /** Cancels a running export and waits for the worker. */
    ~NotationExportTask();

    /** Starts the export. Returns false, and calls nothing, while one is running. */
    bool start (Request request, ProgressFn onProgress = {}, DoneFn onDone = {});

    /** Asks a running export to stop at its next stage. */
    void cancel() noexcept;

    bool isRunning() const noexcept;

    /** 0..1, from any thread. */
    double getProgress() const noexcept { return progress.load (std::memory_order_relaxed); }

    /** Blocks until the worker has finished (a shutdown, the tests), then
        returns the result; -1 waits for as long as it takes. */
    Result waitForCompletion (int timeoutMs = -1);

    /** The last export's result, once isRunning() is false. */
    Result getResult() const;

private:
    class Worker;
    friend class Worker;

    /** What the worker and the message-thread callbacks share; the callbacks
        hold a weak reference, so a task destroyed before they run is a no-op. */
    struct Callbacks
    {
        ProgressFn onProgress;
        DoneFn onDone;
    };

    void setProgress (double fraction);
    void finish (Result result);

    std::unique_ptr<Worker> worker;
    std::shared_ptr<Callbacks> callbacks;

    std::atomic<double> progress { 0.0 };
    std::atomic<bool> running { false };

    mutable juce::CriticalSection resultLock;
    Result result;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotationExportTask)
};

} // namespace luthier
