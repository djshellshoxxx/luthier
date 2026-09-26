#include "UpdateDownloader.h"

namespace luthier
{

namespace
{
    struct UrlFetcher final : UpdateDownloader::Fetcher
    {
        bool fetch (const juce::String& url, juce::OutputStream& out,
                    const std::function<bool()>& shouldStop, juce::String& error) override
        {
            int status = 0;
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (15000)
                             .withStatusCode (&status)
                             .withNumRedirectsToFollow (5);

            auto in = juce::URL (url).createInputStream (options);

            if (in == nullptr)
            {
                error = "Could not connect.";
                return false;
            }

            if (status >= 400)
            {
                error = "The server answered " + juce::String (status) + ".";
                return false;
            }

            juce::HeapBlock<char> buffer (65536);

            while (! in->isExhausted())
            {
                if (shouldStop())
                {
                    error = "Cancelled.";
                    return false;
                }

                const auto n = in->read (buffer.get(), 65536);

                if (n < 0)
                {
                    error = "The download was interrupted.";
                    return false;
                }

                if (n == 0)
                    break;

                if (! out.write (buffer.get(), (size_t) n))
                {
                    error = "Could not write the file.";
                    return false;
                }
            }

            return true;
        }
    };
}

std::unique_ptr<UpdateDownloader::Fetcher> UpdateDownloader::createUrlFetcher()
{
    return std::make_unique<UrlFetcher>();
}

UpdateDownloader::UpdateDownloader (std::unique_ptr<Fetcher> f)
    : juce::Thread ("Luthier update download"), fetcher (std::move (f))
{
}

UpdateDownloader::~UpdateDownloader()
{
    stopThread (5000);
}

juce::File UpdateDownloader::getDownloadsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
}

juce::File UpdateDownloader::destinationFor (const juce::String& url, const juce::File& downloads)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("#", false, false);

    while (path.endsWithChar ('/'))
        path = path.dropLastCharacters (1);

    auto name = juce::URL::removeEscapeChars (path.fromLastOccurrenceOf ("/", false, false));
    name = juce::File::createLegalFileName (name).trim();

    // Nothing usable after the host (or a path like "/.."): a generic name.
    if (name.isEmpty() || name == "." || name == ".." || path.endsWith ("//" + name))
        name = "Luthier-update";

    auto destination = downloads.getChildFile (name);

    if (destination.exists())
        destination = downloads.getNonexistentChildFile (destination.getFileNameWithoutExtension(),
                                                         destination.getFileExtension(), true);

    return destination;
}

UpdateDownloader::Outcome UpdateDownloader::downloadNow (const juce::String& url, const juce::File& downloads)
{
    Outcome outcome;

    if (fetcher == nullptr || url.isEmpty())
    {
        outcome.error = "Nothing to download.";
        return outcome;
    }

    if (! downloads.isDirectory() && ! downloads.createDirectory().wasOk())
    {
        outcome.error = "Could not create " + downloads.getFullPathName() + ".";
        return outcome;
    }

    const auto destination = destinationFor (url, downloads);
    const auto part = destination.getSiblingFile (destination.getFileName() + ".part");
    part.deleteFile();

    bool ok = false;

    {
        juce::FileOutputStream out (part);

        if (! out.openedOk())
        {
            outcome.error = "Could not write to " + downloads.getFullPathName() + ".";
            return outcome;
        }

        ok = fetcher->fetch (url, out, [this] { return threadShouldExit(); }, outcome.error);
        out.flush();
        ok = ok && ! out.getStatus().failed();
    }

    if (! ok || ! part.moveFileTo (destination))
    {
        part.deleteFile();

        if (outcome.error.isEmpty())
            outcome.error = "The download failed.";

        return outcome;
    }

    outcome.succeeded = true;
    outcome.file = destination;
    return outcome;
}

bool UpdateDownloader::start (const juce::String& url, std::function<void (Outcome)> onDone,
                              const juce::File& downloads)
{
    if (isThreadRunning())
        return false;

    pendingUrl = url;
    pendingFolder = downloads;
    pendingDone = std::move (onDone);
    return startThread();
}

void UpdateDownloader::run()
{
    auto outcome = downloadNow (pendingUrl, pendingFolder);

    if (threadShouldExit())
        return;

    if (auto done = pendingDone)
        juce::MessageManager::callAsync ([done, outcome] { done (outcome); });
}

} // namespace luthier
