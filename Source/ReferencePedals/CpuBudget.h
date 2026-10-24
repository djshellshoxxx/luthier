// OWNER: WP2
#pragma once
#include "BoardTypes.h"
#include "PedalRegistry.h"
namespace luthier::reference
{
// ESTIMATED descriptor metadata: one unit denotes one percent of a reference
// core. This admission policy is not a substitute for measured T-CPU evidence.
inline uint64_t estimatedCpuUnits (const std::vector<PedalInstance>& instances)
{
    uint64_t units = 0;
    for (const auto& instance : instances)
        if (! instance.missing)
            if (const auto* descriptor = PedalRegistry::find (instance.modelId.c_str())) units += descriptor->cpuUnits;
    return units;
}
}
