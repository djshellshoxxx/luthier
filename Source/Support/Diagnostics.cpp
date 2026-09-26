#include "Diagnostics.h"
#include "../Presets/PresetManager.h"
#include "IrLibrary.h"

namespace luthier
{

const char* getLogCategoryName (LogCategory c) noexcept
{
    switch (c)
    {
        case LogCategory::Midi:        return "MIDI";
        case LogCategory::Parameter:   return "PARAM";
        case LogCategory::Audio:       return "AUDIO";
        case LogCategory::Engine:      return "ENGINE";
        case LogCategory::Preset:      return "PRESET";
        case LogCategory::Validator:   return "CHECK";
        case LogCategory::Performance: return "PERF";
        case LogCategory::Error:       return "ERROR";
        case LogCategory::NumCategories:
        default:                       return "LOG";
    }
}

juce::String getFullVersionString()
{
#if defined (LUTHIER_BUILD_STRING)
    return juce::String (JucePlugin_VersionString) + "+" + LUTHIER_BUILD_STRING;
#else
    return juce::String (JucePlugin_VersionString) + "+dev";
#endif
}

//==============================================================================
Diagnostics::Diagnostics() = default;

void Diagnostics::prepare (double sampleRate)
{
    sr = sampleRate;
}

void Diagnostics::reset() noexcept
{
    writeIndex.store (0);
    totalWritten.store (0);

    for (auto& r : ring)
        r = Record {};
}

//==============================================================================
void Diagnostics::log (LogCategory category, const char* text, int64_t samplePos) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || text == nullptr)
        return;

    const int index = (int) (writeIndex.fetch_add (1, std::memory_order_relaxed) % kRingSize);
    auto& r = ring[(size_t) index];

    r.category = category;
    r.timestampSamples = samplePos;
    r.hasValue = false;
    r.value = 0.0;

    // Bounded copy, no allocation.
    int i = 0;

    for (; i < kMaxTextLength - 1 && text[i] != '\0'; ++i)
        r.text[i] = text[i];

    r.text[i] = '\0';

    totalWritten.fetch_add (1, std::memory_order_relaxed);
}

void Diagnostics::logValue (LogCategory category, const char* text, double value, int64_t samplePos) noexcept
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    log (category, text, samplePos);

    const int index = (int) ((writeIndex.load (std::memory_order_relaxed) - 1 + kRingSize) % kRingSize);
    ring[(size_t) index].value = value;
    ring[(size_t) index].hasValue = true;
}

int Diagnostics::getRecords (Record* dest, int maxRecords) const noexcept
{
    if (dest == nullptr || maxRecords <= 0)
        return 0;

    const int64_t total = totalWritten.load (std::memory_order_relaxed);
    const int available = (int) juce::jmin (total, (int64_t) kRingSize);
    const int count = juce::jmin (available, maxRecords);
    const int64_t write = writeIndex.load (std::memory_order_relaxed);

    for (int i = 0; i < count; ++i)
    {
        const int index = (int) (((write - count + i) % kRingSize + kRingSize) % kRingSize);
        dest[i] = ring[(size_t) index];
    }

    return count;
}

juce::String Diagnostics::formatRecord (const Record& r, double sampleRate)
{
    const double seconds = (sampleRate > 0.0) ? (double) r.timestampSamples / sampleRate : 0.0;

    juce::String line;
    line << juce::String (seconds, 3).paddedLeft (' ', 9) << "  "
         << juce::String (getLogCategoryName (r.category)).paddedRight (' ', 6) << "  "
         << r.text;

    if (r.hasValue)
        line << " = " << juce::String (r.value, 4);

    return line;
}

//==============================================================================
juce::File Diagnostics::getDiagnosticsFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("Luthier")
                    .getChildFile ("Diagnostics");

    if (! folder.exists())
        folder.createDirectory();

    return folder;
}

void Diagnostics::setCrashLogEnabled (bool e)
{
    if (e == crashLogEnabled.load())
        return;

    crashLogEnabled.store (e);

    if (e)
    {
        // A fresh file per session, named for when it was armed, so several
        // attempts at reproducing a crash do not overwrite each other.
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        crashLogFile = getDiagnosticsFolder().getChildFile ("Luthier Crash Log " + stamp + ".txt");
        crashLogHeaderWritten = false;
        crashLogFlushedUpTo = totalWritten.load();

        // Turning on the crash log implies turning on the data stream that feeds it.
        enabled.store (true);
    }
    else
    {
        crashLogFile = juce::File();
    }
}

juce::File Diagnostics::getCrashLogFile() const
{
    return crashLogFile;
}

void Diagnostics::flushCrashLog (const juce::String& troubleshootingReport)
{
    if (! crashLogEnabled.load() || crashLogFile == juce::File())
        return;

    juce::String text;

    if (! crashLogHeaderWritten)
    {
        // The troubleshooting report goes at the top, so whoever opens the crash
        // log sees the configuration before the events.
        text << "================================================================\n"
             << " LUTHIER CRASH LOG\n"
             << " Armed: " << juce::Time::getCurrentTime().toString (true, true) << "\n"
             << "================================================================\n\n"
             << troubleshootingReport
             << "\n\n================================================================\n"
             << " EVENT STREAM\n"
             << "================================================================\n";

        crashLogHeaderWritten = true;
    }

    const int64_t total = totalWritten.load();

    if (total > crashLogFlushedUpTo)
    {
        const int toWrite = (int) juce::jmin (total - crashLogFlushedUpTo, (int64_t) kRingSize);

        std::vector<Record> records ((size_t) toWrite);
        const int count = getRecords (records.data(), toWrite);

        for (int i = 0; i < count; ++i)
            text << formatRecord (records[(size_t) i], sr) << "\n";

        crashLogFlushedUpTo = total;
    }

    if (text.isNotEmpty())
        crashLogFile.appendText (text, false, false, "\n");
}

//==============================================================================
void Diagnostics::setHostInfo (const HostInfo& info)
{
    const juce::ScopedLock sl (infoLock);
    hostInfo = info;
}

Diagnostics::HostInfo Diagnostics::getHostInfo() const
{
    const juce::ScopedLock sl (infoLock);
    return hostInfo;
}

//==============================================================================
Diagnostics::SelfTestResult Diagnostics::runSelfTest()
{
    SelfTestResult result;

    auto check = [&result] (bool ok, const juce::String& description, bool warningOnly = false)
    {
        if (ok)
            result.passed.add (description);
        else if (warningOnly)
            result.warnings.add (description);
        else
            result.failed.add (description);
    };

    // ---- folders ---------------------------------------------------------------
    const auto userFolder = PresetManager::getUserPresetFolder();
    check (userFolder.isDirectory(), "User preset folder exists: " + userFolder.getFullPathName());

    if (userFolder.isDirectory())
    {
        auto probe = userFolder.getChildFile (".luthier_write_test");
        const bool writable = probe.replaceWithText ("ok");
        probe.deleteFile();
        check (writable, "User preset folder is writable");
    }

    const auto factoryFolder = PresetManager::getFactoryPresetFolder();
    check (factoryFolder.isDirectory(), "Factory preset folder exists: " + factoryFolder.getFullPathName());

    int factoryCount = 0;

    if (factoryFolder.isDirectory())
    {
        for (const auto& e : juce::RangedDirectoryIterator (factoryFolder, true, "*.luthierpreset",
                                                            juce::File::findFiles))
        {
            juce::ignoreUnused (e);
            ++factoryCount;
        }
    }

    check (factoryCount > 0, "Factory presets found (" + juce::String (factoryCount) + ")");

    const auto renderFolder = PresetManager::getRenderFolder();
    check (renderFolder.isDirectory(), "Render folder exists: " + renderFolder.getFullPathName());

    const auto diagFolder = getDiagnosticsFolder();
    check (diagFolder.isDirectory(), "Diagnostics folder exists: " + diagFolder.getFullPathName());

    // ---- resources ---------------------------------------------------------------
    check (IrLibrary::isAvailable(), "Resources folder found: " + IrLibrary::describe(), true);

    if (IrLibrary::isAvailable())
    {
        check (IrLibrary::countBodyIrs() > 0,
               "Body impulse responses (" + juce::String (IrLibrary::countBodyIrs()) + ")", true);
        check (IrLibrary::countCabIrs() > 0,
               "Cabinet impulse responses (" + juce::String (IrLibrary::countCabIrs()) + ")", true);
    }

    // ---- disk space ---------------------------------------------------------------
    const auto freeBytes = userFolder.getBytesFreeOnVolume();
    check (freeBytes > 100 * 1024 * 1024,
           "At least 100 MB free on the preset volume ("
           + juce::File::descriptionOfSizeInBytes (freeBytes) + " free)", true);

    return result;
}

//==============================================================================
juce::String Diagnostics::buildTroubleshootingReport (const juce::String& settingsJson,
                                                      const juce::String& validatorSummary,
                                                      const juce::String& extraNotes) const
{
    const auto info = getHostInfo();
    const auto selfTest = runSelfTest();

    juce::String r;

    r << "================================================================\n"
      << " LUTHIER TROUBLESHOOTING REPORT\n"
      << "================================================================\n"
      << "Generated:        " << juce::Time::getCurrentTime().toString (true, true) << "\n"
      << "Plugin version:   " << getFullVersionString() << "\n"
      << "Plugin format:    " << (info.pluginFormat.isNotEmpty() ? info.pluginFormat : juce::String ("unknown")) << "\n"
      << "\n"
      << "---- HOST ------------------------------------------------------\n"
      << "Host:             " << (info.hostName.isNotEmpty() ? info.hostName : juce::String ("unknown")) << "\n"
      << "Sample rate:      " << juce::String (info.sampleRate, 1) << " Hz\n"
      << "Block size:       " << info.blockSize << " samples\n"
      << "Channels:         " << info.numInputChannels << " in / " << info.numOutputChannels << " out\n"
      << "Reported latency: " << juce::String (info.reportedLatencySamples, 0) << " samples";

    if (info.sampleRate > 0.0)
        r << " (" << juce::String (info.reportedLatencySamples / info.sampleRate * 1000.0, 2) << " ms)";

    r << "\n"
      << "\n"
      << "---- SYSTEM ----------------------------------------------------\n"
      << "OS:               " << juce::SystemStats::getOperatingSystemName() << "\n"
      << "CPU:              " << juce::SystemStats::getCpuModel() << "\n"
      << "CPU vendor:       " << juce::SystemStats::getCpuVendor() << "\n"
      << "Cores:            " << juce::SystemStats::getNumCpus()
      << " (" << juce::SystemStats::getNumPhysicalCpus() << " physical)\n"
      << "CPU speed:        " << juce::SystemStats::getCpuSpeedInMegahertz() << " MHz\n"
      << "Memory:           " << juce::SystemStats::getMemorySizeInMegabytes() << " MB\n"
      << "SIMD:             "
      << (juce::SystemStats::hasSSE2() ? "SSE2 " : "")
      << (juce::SystemStats::hasAVX() ? "AVX " : "")
      << (juce::SystemStats::hasAVX2() ? "AVX2 " : "")
      << (juce::SystemStats::hasNeon() ? "NEON " : "")
      << "\n"
      << "User language:    " << juce::SystemStats::getUserLanguage() << "\n"
      << "\n"
      << "---- FOLDERS ---------------------------------------------------\n"
      << "Factory presets:  " << PresetManager::getFactoryPresetFolder().getFullPathName() << "\n"
      << "User presets:     " << PresetManager::getUserPresetFolder().getFullPathName() << "\n"
      << "Renders:          " << PresetManager::getRenderFolder().getFullPathName() << "\n"
      << "Diagnostics:      " << getDiagnosticsFolder().getFullPathName() << "\n"
      << "\n"
      << "---- SELF TEST -------------------------------------------------\n";

    for (const auto& line : selfTest.passed)
        r << "  [ok]      " << line << "\n";

    for (const auto& line : selfTest.warnings)
        r << "  [warning] " << line << "\n";

    for (const auto& line : selfTest.failed)
        r << "  [FAILED]  " << line << "\n";

    r << "\nResult: " << (selfTest.allPassed() ? "all checks passed" : "one or more checks FAILED")
      << "\n\n"
      << "---- ENGINE VALIDATOR ------------------------------------------\n"
      << (validatorSummary.isNotEmpty() ? validatorSummary : juce::String ("No data.")) << "\n";

    if (extraNotes.isNotEmpty())
        r << "\n---- NOTES -----------------------------------------------------\n" << extraNotes << "\n";

    r << "\n"
      << "---- CURRENT SETTINGS ------------------------------------------\n"
      << settingsJson << "\n"
      << "\n================================================================\n"
      << " End of report. If the plugin is crashing, please also enable\n"
      << " \"Create log file on crash\" in Help > Debug, reproduce the crash,\n"
      << " and send BOTH files to support with a description of what you\n"
      << " were doing.\n"
      << "================================================================\n";

    return r;
}

juce::File Diagnostics::writeTroubleshootingReport (const juce::String& settingsJson,
                                                    const juce::String& validatorSummary,
                                                    const juce::File& destinationFolder) const
{
    const auto folder = destinationFolder.isDirectory() ? destinationFolder : getDiagnosticsFolder();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    const auto file = folder.getChildFile ("Luthier Troubleshooting " + stamp + ".txt");

    if (file.replaceWithText (buildTroubleshootingReport (settingsJson, validatorSummary)))
        return file;

    return {};
}

} // namespace luthier
