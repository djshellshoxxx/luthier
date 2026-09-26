#pragma once

/*  installer.md 5.1: the update banner's "Download" button fetches the
    platform installer to the user's Downloads folder. It never launches it.

    The fetch runs on its own thread and reports back on the message thread.
    The network is behind Fetcher, one method, so a test can substitute a fake
    exactly as Telemetry's Transport does (a Transport returns text; an
    installer is binary, hence a stream here).
*/

#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

namespace luthier
{

class UpdateDownloader : private juce::Thread
{
public:
    /** Anything that can stream a URL's bytes. Blocking; worker thread. */
    struct Fetcher
    {
        virtual ~Fetcher() = default;

        /** Writes the body to `out`; false on failure (with `error` set).
            Should give up promptly when `shouldStop()` turns true. */
        virtual bool fetch (const juce::String& url, juce::OutputStream& out,
                            const std::function<bool()>& shouldStop, juce::String& error) = 0;
    };

    /** The shipping fetcher: juce::URL. */
    static std::unique_ptr<Fetcher> createUrlFetcher();

    struct Outcome
    {
        bool succeeded = false;
        juce::File file;
        juce::String error;
    };

    explicit UpdateDownloader (std::unique_ptr<Fetcher> fetcher = createUrlFetcher());
    ~UpdateDownloader() override;

    /** The user's Downloads folder (~/Downloads). */
    static juce::File getDownloadsFolder();

    /** Where a URL's file lands: <downloads>/<last path segment>, query and
        fragment dropped, made a legal file name, and never an existing file
        (a second download of the same installer gets "name (2).ext"). */
    static juce::File destinationFor (const juce::String& url,
                                      const juce::File& downloads = getDownloadsFolder());

    /** Starts a download; `onDone` runs on the message thread. False if one is
        already running. The file is written to a .part name and renamed only
        when complete, so a cancelled or failed fetch leaves no half installer. */
    bool start (const juce::String& url, std::function<void (Outcome)> onDone,
                const juce::File& downloads = getDownloadsFolder());

    /** Synchronous form, for tests and the worker itself. */
    Outcome downloadNow (const juce::String& url, const juce::File& downloads);

    bool isDownloading() const noexcept { return isThreadRunning(); }

private:
    void run() override;

    std::unique_ptr<Fetcher> fetcher;
    juce::String pendingUrl;
    juce::File pendingFolder;
    std::function<void (Outcome)> pendingDone;
};

} // namespace luthier
