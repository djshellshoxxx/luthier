// OWNER: WP2
// Release catalogue: which registered models a user may ADD to the board.
//
// The 0.1 beta ships a single reference pedal, the four-stage phaser
// (mxr-phase90-script), the first model developed. Every other model stays
// registered so saved sessions, presets and host automation that already name
// one keep loading and sounding the same; it simply is not offered by the
// browser, the Add menu or a drop. Message thread only, like PedalRegistry.
#pragma once

#include <cstring>
#include "PedalDesc.h"

namespace luthier::reference
{
struct ReleaseCatalogue
{
    static constexpr const char* offeredModels[] { "mxr-phase90-script" };

    static bool offers (const char* modelId) noexcept
    {
        if (modelId == nullptr)
            return false;
        for (const auto* offered : offeredModels)
            if (std::strcmp (offered, modelId) == 0)
                return true;
        return false;
    }

    static bool offers (const PedalDesc& desc) noexcept { return offers (desc.modelId); }
};
} // namespace luthier::reference
