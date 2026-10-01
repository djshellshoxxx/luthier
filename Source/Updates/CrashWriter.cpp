#include "CrashWriter.h"

#include "Telemetry.h"

#include <atomic>

namespace luthier
{

namespace CrashWriter
{
    namespace
    {
        juce::CriticalSection infoLock;
        Info currentInfo;
        std::atomic<bool> installed { false };

        void handleCrash (void*)
        {
            writeDump (Telemetry::getDiagnosticsDirectory(), juce::SystemStats::getStackBacktrace());
        }
    }

    void setInfo (const Info& info)
    {
        const juce::ScopedLock sl (infoLock);
        currentInfo = info;
    }

    juce::String formatDump (const juce::String& backtrace)
    {
        Info info;
        {
            const juce::ScopedLock sl (infoLock);
            info = currentInfo;
        }

        // UT-19: stack, build, OS and host only. No audio, MIDI or preset data
        // is reachable from here, by construction.
        juce::String text;
        text << "Luthier crash report\n"
             << "time: " << juce::Time::getCurrentTime().toISO8601 (true) << "\n"
             << "build: " << info.build << "\n"
             << "os: " << juce::SystemStats::getOperatingSystemName() << "\n"
             << "cpu: " << juce::SystemStats::getCpuModel() << "\n"
             << "host: " << info.host << "\n"
             << "format: " << info.wrapper << "\n"
             << "\nstack:\n" << backtrace << "\n";
        return text;
    }

    juce::File writeDump (const juce::File& directory, const juce::String& backtrace)
    {
        if (! directory.isDirectory() && ! directory.createDirectory())
            return {};

        const auto name = "crash-" + juce::Time::getCurrentTime().formatted ("%Y%m%d%H%M%S") + ".dmp";
        const auto file = directory.getChildFile (name).getNonexistentSibling (false);

        return file.replaceWithText (formatDump (backtrace)) ? file : juce::File();
    }

    void install()
    {
        if (installed.exchange (true))
            return;

        juce::SystemStats::setApplicationCrashHandler (handleCrash);
    }

    bool isInstalled() noexcept
    {
        return installed.load();
    }
}

} // namespace luthier
