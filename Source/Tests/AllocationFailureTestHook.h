#pragma once
#include <cstddef>
namespace luthier::tests
{
// Test executable only. Disabled by default; fails one matching allocation on
// the calling thread, then automatically disarms before error reporting.
void failNextAllocationOfSizeOnThisThread (std::size_t) noexcept;
void clearAllocationFailureOnThisThread() noexcept;
bool allocationFailureTriggeredOnThisThread() noexcept;
long allocationsOnThisThread() noexcept;
}
