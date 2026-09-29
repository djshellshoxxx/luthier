#pragma once

/*  SPEC-SWEEP: UT-16 / UT-19 - updates-telemetry.md 4, the crash dump.

    When the user has switched crash reports on, a process crash handler writes
    Diagnostics/crash-<yyyymmddhhmmss>.dmp: the stack backtrace, the build, the
    OS and the host. Nothing else - never audio buffers, MIDI or preset state
    (updates-telemetry 4) - and it is written only from values gathered before
    the crash, so the handler itself does no parsing and no allocation beyond
    formatting one string. The next session offers the dump through
    Telemetry::describePendingCrashReport / uploadPendingCrashReport.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

namespace CrashWriter
{
    /** The non-identifying facts a dump carries besides the stack. */
    struct Info
    {
        juce::String build;      ///< version and build type
        juce::String host;       ///< host application name
        juce::String wrapper;    ///< plugin format
    };

    /** Sets what the next dump says about the build and host. Message thread. */
    void setInfo (const Info& info);

    /** The dump's text for `backtrace`: stack, build, OS, host. */
    juce::String formatDump (const juce::String& backtrace);

    /** Writes a dump into `directory` now and returns the file (or {} on a
        failed write). What the crash handler calls; tests call it directly. */
    juce::File writeDump (const juce::File& directory, const juce::String& backtrace);

    /** Installs the process crash handler (idempotent). Called only when the
        user has crash reports on. */
    void install();
    bool isInstalled() noexcept;
}

} // namespace luthier
