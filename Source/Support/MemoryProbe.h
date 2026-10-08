#pragma once

/*  performance-budget.md 3 / 10: the process's resident memory, for the memory
    budget tests and the diagnostics page. Message thread (it reads a file on
    Linux). Returns 0 where the platform gives no answer.
*/

#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1   // windows.h min/max macros break std::min in every includer (PR #2)
 #endif
 #include <windows.h>
 #include <psapi.h>
#elif JUCE_MAC
 #include <mach/mach.h>
#endif

namespace luthier::MemoryProbe
{
   #if JUCE_LINUX || JUCE_BSD
    /** A "Name:   1234 kB" line of /proc/self/status, in bytes. */
    inline size_t readStatusKb (const char* field)
    {
        const auto status = juce::File ("/proc/self/status").loadFileAsString();

        for (const auto& line : juce::StringArray::fromLines (status))
            if (line.startsWith (field))
                return (size_t) line.fromFirstOccurrenceOf (":", false, false).trim().getLargeIntValue() * 1024u;

        return 0;
    }
   #endif

    /** Current resident set size, in bytes. */
    inline size_t residentBytes()
    {
       #if JUCE_LINUX || JUCE_BSD
        return readStatusKb ("VmRSS:");
       #elif JUCE_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc {};
        return GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)) ? (size_t) pmc.WorkingSetSize : 0;
       #elif JUCE_MAC
        mach_task_basic_info info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        return task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS
                 ? (size_t) info.resident_size : 0;
       #else
        return 0;
       #endif
    }

    /** Peak resident set size so far, in bytes. */
    inline size_t peakResidentBytes()
    {
       #if JUCE_LINUX || JUCE_BSD
        return readStatusKb ("VmHWM:");
       #elif JUCE_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc {};
        return GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)) ? (size_t) pmc.PeakWorkingSetSize : 0;
       #elif JUCE_MAC
        mach_task_basic_info info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        return task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS
                 ? (size_t) info.resident_size_max : 0;
       #else
        return 0;
       #endif
    }

    inline double toMB (size_t bytes) noexcept { return (double) bytes / (1024.0 * 1024.0); }
}
