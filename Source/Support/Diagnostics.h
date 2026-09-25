#pragma once

/*  Diagnostics: the debug window, the crash log and the troubleshooting file
    (include.md, "extra features").

    Three separate things, deliberately:

      Live data stream   - a lock-free ring of everything happening inside the
                           plugin. Feeds the debug window and the scrolling
                           readout in the UI.
      Crash log          - opt-in, off on every load. Once armed, it writes a
                           timestamped file that begins with a copy of the
                           troubleshooting report, so a crash report arrives with
                           its context attached.
      Troubleshooting    - a one-shot snapshot: settings, audio and MIDI config,
                           host, version, and a light self-test.

    Nothing here allocates on the audio thread: the stream is a fixed ring of
    fixed-size records, and formatting happens on the message thread.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
enum class LogCategory
{
    Midi, Parameter, Audio, Engine, Preset, Validator, Performance, Error,
    NumCategories
};

const char* getLogCategoryName (LogCategory c) noexcept;

//==============================================================================
class Diagnostics
{
public:
    static constexpr int kRingSize = 512;
    static constexpr int kMaxTextLength = 96;

    struct Record
    {
        LogCategory category = LogCategory::Engine;
        char text[kMaxTextLength] = {};
        int64_t timestampSamples = 0;
        double value = 0.0;
        bool hasValue = false;
    };

    Diagnostics();

    void prepare (double sampleRate);
    void reset() noexcept;

    //==========================================================================
    void setEnabled (bool e) noexcept { enabled.store (e); }
    bool isEnabled() const noexcept { return enabled.load(); }

    /** Pushes a line into the ring. Real-time safe: copies at most
        kMaxTextLength bytes and never allocates. */
    void log (LogCategory category, const char* text, int64_t samplePos = 0) noexcept;
    void logValue (LogCategory category, const char* text, double value, int64_t samplePos = 0) noexcept;

    /** Reads out the newest records, oldest first. Returns how many were written. */
    int getRecords (Record* dest, int maxRecords) const noexcept;

    /** Total records pushed since the last reset, including ones that have
        scrolled out of the ring. */
    int getTotalRecords() const noexcept { return (int) juce::jmin (totalWritten.load(), (int64_t) std::numeric_limits<int>::max()); }

    /** Formats one record for display. */
    static juce::String formatRecord (const Record& r, double sampleRate);

    //==========================================================================
    // Crash logging.

    void setCrashLogEnabled (bool e);
    bool isCrashLogEnabled() const noexcept { return crashLogEnabled.load(); }

    /** The file the current crash log is being written to, if any. */
    juce::File getCrashLogFile() const;

    /** Appends the current ring to the crash log. Called periodically from a
        timer on the message thread, and at shutdown. */
    void flushCrashLog (const juce::String& troubleshootingReport);

    //==========================================================================
    // Troubleshooting report.

    struct HostInfo
    {
        juce::String hostName;
        juce::String pluginFormat;
        double sampleRate = 0.0;
        int blockSize = 0;
        int numInputChannels = 0;
        int numOutputChannels = 0;
        double reportedLatencySamples = 0.0;
    };

    void setHostInfo (const HostInfo& info);
    HostInfo getHostInfo() const;

    /** Builds the full troubleshooting text. `settingsJson` is the current preset
        state, included verbatim so support can reproduce the exact setup. */
    juce::String buildTroubleshootingReport (const juce::String& settingsJson,
                                             const juce::String& validatorSummary,
                                             const juce::String& extraNotes = {}) const;

    /** Writes the report. Returns the file written, or an invalid File on failure. */
    juce::File writeTroubleshootingReport (const juce::String& settingsJson,
                                           const juce::String& validatorSummary,
                                           const juce::File& destinationFolder = {}) const;

    /** Where diagnostic files are written. */
    static juce::File getDiagnosticsFolder();

    //==========================================================================
    /** Light self-test run as part of the troubleshooting report: checks the
        folders exist and are writable, that the factory bank is present, and that
        the plugin's own resources resolve. */
    struct SelfTestResult
    {
        juce::StringArray passed;
        juce::StringArray failed;
        juce::StringArray warnings;

        bool allPassed() const noexcept { return failed.isEmpty(); }
    };

    static SelfTestResult runSelfTest();

private:
    std::atomic<bool> enabled { false };
    std::atomic<bool> crashLogEnabled { false };

    double sr = 44100.0;

    std::array<Record, kRingSize> ring {};
    // 64-bit: an int wrapped negative after 2^31 records and `% kRingSize`
    // then indexed before the ring.
    std::atomic<int64_t> writeIndex { 0 };
    std::atomic<int64_t> totalWritten { 0 };

    mutable juce::CriticalSection infoLock;
    HostInfo hostInfo;

    juce::File crashLogFile;
    int64_t crashLogFlushedUpTo = 0;
    bool crashLogHeaderWritten = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Diagnostics)
};

} // namespace luthier
