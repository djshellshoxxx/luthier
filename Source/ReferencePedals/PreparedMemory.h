// OWNER: WP2
#pragma once
#include <cstdint>
#include <limits>
namespace luthier::reference
{
inline constexpr uint64_t kPreparedPedalMemoryLimit = 64000000; // BOARD_CORE:64MB, deliberately not64MiB
inline constexpr uint64_t kUnknownPreparedMemoryBytes = std::numeric_limits<uint64_t>::max();
// Off-audio preflight, before factory construction. Owned includes sizeof(model)
// plus all owned prepared allocation capacities. Temporary is additional peak
// prepare/load scratch. Separately budgeted paged-looper media is excluded;
// shared immutable tables and board routing scratch are separately inventoried.
struct PreparedMemoryRequirement
{
    uint64_t ownedBytes = kUnknownPreparedMemoryBytes;
    uint64_t temporaryBytes = 0;
};
using PreparedMemoryEstimator = PreparedMemoryRequirement (*) (double sampleRate, int maxBlock,
    int oversamplePolicy, const float* normalisedParams) noexcept;
// Only for an audited model whose entire prepared state is embedded in T.
template<class T> PreparedMemoryRequirement fixedPreparedMemory (double, int, int, const float*) noexcept
{ return { sizeof (T), 0 }; }
inline bool addPreparedBytes (uint64_t a, uint64_t b, uint64_t& result) noexcept
{
    if (a == kUnknownPreparedMemoryBytes || b == kUnknownPreparedMemoryBytes
        || b >= kUnknownPreparedMemoryBytes - a) return false;
    result = a + b; return true;
}
}
