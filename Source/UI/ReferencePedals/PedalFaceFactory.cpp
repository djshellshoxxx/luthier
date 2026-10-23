// OWNER: WP19
// STUB (WP1): the interface is frozen in PedalFaceFactory.h. WP19 adds the
// explicit list of generic faces and calls PedalFaceFactory::add for each
// branded face from WP20 / WP21; make() already falls back to a placeholder.

#include "PedalFaceFactory.h"
#include "GenericFace.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include <map>
#include <string>

namespace luthier::reference
{

namespace
{
    std::map<std::string, FaceMaker>& makers()
    {
        static std::map<std::string, FaceMaker> table;
        return table;
    }
}

void PedalFaceFactory::add (const char* modelId, FaceMaker maker)
{
    if (modelId != nullptr && maker != nullptr)
        makers()[modelId] = maker;
}

std::unique_ptr<PedalFace> PedalFaceFactory::make (const char* modelId)
{
    if (modelId != nullptr)
    {
        const auto it = makers().find (modelId);
        if (it != makers().end())
            if (auto face = it->second())
                return face;
    }
    return std::make_unique<GenericFace> (modelId ? PedalRegistry::find (modelId) : nullptr);
}

bool PedalFaceFactory::hasBrandedFace (const char* modelId)
{
    return modelId != nullptr && makers().count (modelId) != 0;
}

} // namespace luthier::reference
