#include "NotationExportTask.h"

// callAsync lives in juce_events; NotationExport.h only brings in juce_core.
#include <juce_events/juce_events.h>

namespace luthier
{

//==============================================================================
class NotationExportTask::Worker : public juce::Thread
{
public:
    Worker (NotationExportTask& owner, Request r)
        : juce::Thread ("Luthier notation export"), task (owner), request (std::move (r))
    {
    }

    void run() override
    {
        Result out;
        out.destination = request.destination;

        // Stage 1: about to render.
        task.setProgress (0.05);

        if (holdStage())
            return finishCancelled (out);

        if (request.format == NotationFormat::midi)
        {
            // MIDI: the take as a performance, written through a temporary file.
            task.setProgress (0.5);

            if (holdStage())
                return finishCancelled (out);

            juce::String error;
            out.ok = MidiProfiles::exportToFile (request.performance, request.midiOptions, request.destination, &error);
            out.error = error;
        }
        else
        {
            NotationExporter exporter;

            // Stage 2: rendered text, still nothing on disk.
            juce::String rendered;

            switch (request.format)
            {
                case NotationFormat::musicXml:  rendered = exporter.renderMusicXml (request.score, request.options); break;
                case NotationFormat::asciiTab:  rendered = exporter.renderAsciiTab (request.score, request.options); break;
                case NotationFormat::guitarPro:
                case NotationFormat::midi:
                case NotationFormat::numFormats:
                default: break;
            }

            task.setProgress (0.6);

            if (holdStage())
                return finishCancelled (out);

            // Stage 3: the write.
            if (request.score.getTotalNoteCount() == 0)
            {
                out.error = "There is nothing captured to export.";
            }
            else if (request.format == NotationFormat::guitarPro)
            {
                out.ok = exporter.write (request.score, request.format, request.destination, request.options);
                out.error = exporter.getLastError();
            }
            else
            {
                request.destination.getParentDirectory().createDirectory();
                juce::TemporaryFile temp (request.destination);

                out.ok = temp.getFile().replaceWithText (rendered) && temp.overwriteTargetFileWithTemporary();

                if (! out.ok)
                    out.error = "Could not write " + request.destination.getFullPathName();
            }
        }

        task.setProgress (1.0);
        task.finish (out);
    }

private:
    /** The stage's minimum time, in cancel-sized steps. True if cancelled. */
    bool holdStage()
    {
        for (int waited = 0; waited < request.minimumStageMs && ! threadShouldExit(); waited += 5)
            wait (5);

        return threadShouldExit();
    }

    void finishCancelled (Result out)
    {
        out.ok = false;
        out.cancelled = true;
        out.error = "Export cancelled.";
        task.finish (out);
    }

    NotationExportTask& task;
    Request request;
};

//==============================================================================
NotationExportTask::NotationExportTask() = default;

NotationExportTask::~NotationExportTask()
{
    cancel();

    if (worker != nullptr)
        worker->stopThread (-1);

    // Callbacks still queued on the message thread find nobody home.
    callbacks.reset();
}

bool NotationExportTask::start (Request request, ProgressFn onProgress, DoneFn onDone)
{
    if (running.load (std::memory_order_acquire))
        return false;

    if (worker != nullptr)
        worker->stopThread (-1);

    callbacks = std::make_shared<Callbacks> (Callbacks { std::move (onProgress), std::move (onDone) });

    {
        const juce::ScopedLock lock (resultLock);
        result = Result {};
        result.destination = request.destination;
    }

    progress.store (0.0, std::memory_order_relaxed);
    running.store (true, std::memory_order_release);

    worker = std::make_unique<Worker> (*this, std::move (request));
    worker->startThread();
    return true;
}

void NotationExportTask::cancel() noexcept
{
    if (worker != nullptr)
        worker->signalThreadShouldExit();
}

bool NotationExportTask::isRunning() const noexcept
{
    return running.load (std::memory_order_acquire);
}

NotationExportTask::Result NotationExportTask::waitForCompletion (int timeoutMs)
{
    if (worker != nullptr)
        worker->waitForThreadToExit (timeoutMs);

    return getResult();
}

NotationExportTask::Result NotationExportTask::getResult() const
{
    const juce::ScopedLock lock (resultLock);
    return result;
}

void NotationExportTask::setProgress (double fraction)
{
    progress.store (juce::jlimit (0.0, 1.0, fraction), std::memory_order_relaxed);

    std::weak_ptr<Callbacks> weak = callbacks;

    juce::MessageManager::callAsync ([weak, fraction]
    {
        if (auto shared = weak.lock())
            if (shared->onProgress != nullptr)
                shared->onProgress (fraction);
    });
}

void NotationExportTask::finish (Result finished)
{
    {
        const juce::ScopedLock lock (resultLock);
        result = finished;
    }

    running.store (false, std::memory_order_release);

    std::weak_ptr<Callbacks> weak = callbacks;

    juce::MessageManager::callAsync ([weak, finished]
    {
        if (auto shared = weak.lock())
            if (shared->onDone != nullptr)
                shared->onDone (finished);
    });
}

} // namespace luthier
