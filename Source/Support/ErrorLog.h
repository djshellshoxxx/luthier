#pragma once

/*  The error log (error-recovery.md 5 and 13).

    Ground rule 5 is "log everything", and it is deliberately not conditional on
    telemetry: a user who has opted out of sending anything still deserves a local
    record of what went wrong, and support cannot ask for a log that was never
    written.

    One JSON object per line, so the file is greppable, tailable, and readable by
    a person without a tool. Rotates monthly by filename; the 30-day sweep prunes
    old months alongside the preset backups.

    Message thread only. This opens files and formats strings, so nothing on the
    audio thread may call it - that path logs into Diagnostics' lock-free ring
    instead, and whatever drains the ring can report here.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

//==============================================================================
class ErrorLog
{
public:
    /** error-recovery 13's four severities. */
    enum class Severity { debug = 0, info, warn, error };

    static const char* getSeverityName (Severity severity) noexcept;

    /*  Writes one line.

        `module` is the subsystem ("PresetSystem", "ToneMatch"), `code` a stable
        SCREAMING_SNAKE identifier a support reply can be written against, and
        `context` any JSON object worth keeping - paths, sizes, the value that was
        out of range. */
    static void write (Severity severity,
                       const juce::String& module,
                       const juce::String& code,
                       const juce::String& message,
                       const juce::var& context = {});

    /** Options -> Diagnostics. `debug` and `info` are dropped unless this is on;
        `warn` and `error` are always written. */
    static void setVerbose (bool shouldBeVerbose) noexcept;
    static bool isVerbose() noexcept;

    /** `errors-<yyyymm>.log` in the diagnostics folder. */
    static juce::File getLogFile (juce::Time when = juce::Time::getCurrentTime());

    /** error-recovery 13: old months go with the 30-day backup sweep. */
    static void pruneOldLogs (int retentionDays = 30);

    /*  Points the log at another folder, for tests.

        Passing an empty file restores the real diagnostics folder. Without this a
        test would either write into the user's own log or not exercise the file
        path at all. */
    static void setFolderForTesting (const juce::File& folder);

    /** The folder currently being written to. */
    static juce::File getFolder();
};

} // namespace luthier
