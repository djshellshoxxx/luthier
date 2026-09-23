#pragma once

/*  Counters that let the tests prove two guitar-workshop.md 10 claims:

    - "No filesystem access on the audio thread during any swap." Every place
      the Workshop and the IR libraries touch the disk calls noteFileAccess();
      a thread that has called markAsAudioThread() and then reaches one of
      them is counted. A host's audio thread is never marked, so in the
      plugin this is one thread-local read per file access and nothing else.

    - "Derived acoustics are cached": mapSpec counts its calls, so a test can
      show one swap maps once, however many blocks are rendered after it.
*/

#include <atomic>

namespace luthier::ThreadProbe
{
    inline thread_local bool isMarkedAudioThread = false;
    inline std::atomic<int> audioThreadFileAccesses { 0 };
    inline std::atomic<int> mapSpecCalls { 0 };

    inline void markAsAudioThread (bool isAudio = true) noexcept { isMarkedAudioThread = isAudio; }

    inline void noteFileAccess() noexcept
    {
        if (isMarkedAudioThread)
            audioThreadFileAccesses.fetch_add (1, std::memory_order_relaxed);
    }

    inline void noteMapSpec() noexcept { mapSpecCalls.fetch_add (1, std::memory_order_relaxed); }
}
