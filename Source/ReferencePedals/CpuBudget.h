// OWNER: WP2
#pragma once
#include "BoardTypes.h"
#include "PedalRegistry.h"
#include <limits>
namespace luthier::reference
{
// Common admission/reporting scale:1000 units per percentage point. Descriptor
// budgets retain their declared scale; round fractional units upward so the
// conversion never undercharges. Metadata is not measured T-CPU evidence.
inline constexpr uint64_t kCpuBudgetUnitsPerPercent = 1000;
inline uint64_t normalisedCpuUnits (const PedalDesc& descriptor) noexcept
{
    if (descriptor.cpuUnitsPerPercent == 0) return std::numeric_limits<uint64_t>::max();
    return (static_cast<uint64_t> (descriptor.cpuUnits) * kCpuBudgetUnitsPerPercent
        + descriptor.cpuUnitsPerPercent - 1) / descriptor.cpuUnitsPerPercent;
}
inline uint64_t estimatedCpuUnits (const std::vector<PedalInstance>& instances)
{
    uint64_t units = 0;
    for (const auto& instance : instances)
        if (! instance.missing)
            if (const auto* descriptor = PedalRegistry::find (instance.modelId.c_str()))
            {
                const auto cost = normalisedCpuUnits (*descriptor);
                if (cost > std::numeric_limits<uint64_t>::max() - units) return std::numeric_limits<uint64_t>::max();
                units += cost;
            }
    return units;
}
}
