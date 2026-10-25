#include <cstdlib>
#include <new>
#include <cstddef>
namespace
{
    thread_local long threadAllocationCount = 0;
    thread_local std::size_t failedAllocationSize = 0;
    thread_local bool allocationFailureTriggered = false;
    thread_local std::size_t allocationsBeforeFailure = static_cast<std::size_t> (-1);
    bool failAllocation (std::size_t size) noexcept
    {
        if (allocationsBeforeFailure != static_cast<std::size_t> (-1))
        {
            if (failedAllocationSize != 0 && size != failedAllocationSize) return false;
            if (allocationsBeforeFailure-- != 0) return false;
            allocationsBeforeFailure = static_cast<std::size_t> (-1); failedAllocationSize = 0;
            allocationFailureTriggered = true; return true;
        }
        if (failedAllocationSize == 0 || size != failedAllocationSize) return false;
        failedAllocationSize = 0; allocationFailureTriggered = true; return true;
    }
}

/*  notation-export 7.1 (MODEL-GAPS, TODO 2k): the counter the other suites'
    no-allocation checks read; CMake defines LUTHIER_ALLOCATION_COUNTER for the
    test target so those checks compile in. */
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept { return threadAllocationCount; }
    void failNextAllocationOfSizeOnThisThread (std::size_t size) noexcept
    { failedAllocationSize = size; allocationsBeforeFailure = static_cast<std::size_t> (-1); allocationFailureTriggered = false; }
    void failAllocationAfterOnThisThread (std::size_t successfulAllocations) noexcept
    { failedAllocationSize = 0; allocationsBeforeFailure = successfulAllocations; allocationFailureTriggered = false; }
    void failMatchingAllocationAfterOnThisThread (std::size_t size, std::size_t successfulMatches) noexcept
    { failedAllocationSize = size; allocationsBeforeFailure = successfulMatches; allocationFailureTriggered = false; }
    void clearAllocationFailureOnThisThread() noexcept { failedAllocationSize = 0; allocationsBeforeFailure = static_cast<std::size_t> (-1); }
    bool allocationFailureTriggeredOnThisThread() noexcept { return allocationFailureTriggered; }
}

struct AllocationCounter
{
    static long count() noexcept { return threadAllocationCount; }
};

/** The same count for other test files (REALISM-A's budget tests). */
long luthierAllocationCount() noexcept { return threadAllocationCount; }
// REALISM-B: the same counter for the suites in other files.
long luthierAllocationsOnThisThread() noexcept { return threadAllocationCount; }
// TECHNIQUES: the same count for other test files (TechniqueLayerTests).
long luthierTestAllocationCount() noexcept { return threadAllocationCount; }

void* operator new (std::size_t size)
{
    ++threadAllocationCount;
    if (failAllocation (size)) throw std::bad_alloc();

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    ++threadAllocationCount;
    if (failAllocation (size)) throw std::bad_alloc();

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void operator delete (void* p) noexcept                   { std::free (p); }
void operator delete[] (void* p) noexcept                 { std::free (p); }
void operator delete (void* p, std::size_t) noexcept      { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept    { std::free (p); }

// Match the test executable's malloc/free family for standard-library nothrow
// temporary buffers too. Delegate so allocation counters/fault injection agree.
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{ try { return ::operator new (size); } catch (...) { return nullptr; } }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{ try { return ::operator new[] (size); } catch (...) { return nullptr; } }
void operator delete (void* p, const std::nothrow_t&) noexcept { ::operator delete (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { ::operator delete[] (p); }
