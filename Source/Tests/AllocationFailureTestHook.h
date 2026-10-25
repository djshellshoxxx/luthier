#pragma once
#include <cstddef>
namespace luthier::tests
{
// Test executable only. Disabled by default; fails one matching allocation on
// the calling thread, then automatically disarms before error reporting.
void failNextAllocationOfSizeOnThisThread (std::size_t) noexcept;
// Fail after exactly this many successful standard new/new[] calls.
void failAllocationAfterOnThisThread (std::size_t successfulAllocations) noexcept;
// Same countdown restricted to matching payload size; no production hook.
void failMatchingAllocationAfterOnThisThread (std::size_t size, std::size_t successfulMatches) noexcept;
void clearAllocationFailureOnThisThread() noexcept;
bool allocationFailureTriggeredOnThisThread() noexcept;
long allocationsOnThisThread() noexcept;
}
