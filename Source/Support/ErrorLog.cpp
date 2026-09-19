#include "ErrorLog.h"
#include "Diagnostics.h"

namespace luthier
{

namespace
{
    juce::CriticalSection& logLock()
    {
        static juce::CriticalSection lock;
        return lock;
    }

    juce::File& overrideFolder()
    {
        static juce::File folder;
        return folder;
    }

    std::atomic<bool>& verboseFlag()
    {
        static std::atomic<bool> verbose { false };
        return verbose;
    }

    /** ISO 8601 UTC with milliseconds, which is what the spec's example shows. */
    juce::String timestamp (juce::Time when)
    {
        return when.toISO8601 (true);
    }
}

//==============================================================================
const char* ErrorLog::getSeverityName (Severity severity) noexcept
{
    switch (severity)
    {
        case Severity::debug: return "debug";
        case Severity::info:  return "info";
        case Severity::warn:  return "warn";
        case Severity::error: return "error";
    }

    return "info";
}

void ErrorLog::setVerbose (bool shouldBeVerbose) noexcept
{
    verboseFlag().store (shouldBeVerbose);
}

bool ErrorLog::isVerbose() noexcept
{
    return verboseFlag().load();
}

void ErrorLog::setFolderForTesting (const juce::File& folder)
{
    const juce::ScopedLock sl (logLock());
    overrideFolder() = folder;
}

juce::File ErrorLog::getFolder()
{
    {
        const juce::ScopedLock sl (logLock());

        if (overrideFolder() != juce::File())
            return overrideFolder();
    }

    return Diagnostics::getDiagnosticsFolder();
}

juce::File ErrorLog::getLogFile (juce::Time when)
{
    // Monthly rotation is the filename, so nothing has to move a file on a date
    // boundary and a month's worth of context stays in one place.
    return getFolder().getChildFile ("errors-" + when.formatted ("%Y%m") + ".log");
}

//==============================================================================
void ErrorLog::write (Severity severity,
                      const juce::String& module,
                      const juce::String& code,
                      const juce::String& message,
                      const juce::var& context)
{
    // error-recovery 13: the two quiet severities are opt-in, the two that matter
    // are unconditional.
    if ((severity == Severity::debug || severity == Severity::info) && ! isVerbose())
        return;

    const auto now = juce::Time::getCurrentTime();

    auto* entry = new juce::DynamicObject();

    entry->setProperty ("ts", timestamp (now));
    entry->setProperty ("severity", getSeverityName (severity));
    entry->setProperty ("module", module);
    entry->setProperty ("code", code);
    entry->setProperty ("message", message);

    if (context.isObject())
        entry->setProperty ("context", context);

    /*  One line, so the file stays greppable and a truncated write costs one
        record rather than the whole file. JSON::toString with allOnOneLine is what
        guarantees that: a pretty-printed object would span lines and break every
        line-oriented tool pointed at this. */
    const auto line = juce::JSON::toString (juce::var (entry), true) + "\n";

    const juce::ScopedLock sl (logLock());

    auto file = getLogFile (now);

    file.getParentDirectory().createDirectory();
    file.appendText (line, false, false, "\n");
}

//==============================================================================
void ErrorLog::pruneOldLogs (int retentionDays)
{
    const auto folder = getFolder();

    if (! folder.isDirectory())
        return;

    const auto cutoff = juce::Time::getCurrentTime()
                          - juce::RelativeTime::days ((double) juce::jmax (1, retentionDays));

    const juce::ScopedLock sl (logLock());

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "errors-*.log",
                                                            juce::File::findFiles))
    {
        const auto file = entry.getFile();

        /*  Dated from the name rather than the filesystem, for the same reason the
            preset backup sweep does: copying a diagnostics folder to send it to
            support rewrites every timestamp, and that should not decide what gets
            deleted. */
        const auto stamp = file.getFileNameWithoutExtension().fromLastOccurrenceOf ("-", false, false);

        if (stamp.length() != 6)
            continue;

        const int year = stamp.substring (0, 4).getIntValue();
        const int month = stamp.substring (4, 6).getIntValue();

        if (year < 2000 || month < 1 || month > 12)
            continue;

        // The month is stale once its last day is past the cutoff.
        const juce::Time endOfMonth (month == 12 ? year + 1 : year,
                                     month == 12 ? 0 : month,
                                     1, 0, 0);

        if (endOfMonth < cutoff)
            file.deleteFile();
    }
}

} // namespace luthier
