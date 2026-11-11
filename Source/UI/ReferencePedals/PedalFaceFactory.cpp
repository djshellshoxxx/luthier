// OWNER: WP19
// STUB (WP1): the interface is frozen in PedalFaceFactory.h. WP19 adds the
// explicit list of generic faces and calls PedalFaceFactory::add for each
// branded face from WP20 / WP21; make() already falls back to a placeholder.

#include "PedalFaceFactory.h"
#include "GenericFace.h"
#include "ModelFaceMakers.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include <map>
#include <string>

namespace luthier::reference
{

namespace
{
    std::map<std::string, FaceMaker>& makers()
    {
        static std::map<std::string, FaceMaker> table {
            {"boss-ds1", &makeBossDs1Face},
            {"ibanez-ts808", &makeIbanezTs808Face},
            {"boss-sd1", &makeBossSd1Face},
            {"mxr-distortion-plus", &makeMxrDistortionPlusFace},
            {"proco-rat", &makeProcoRatFace},
            {"mxr-dynacomp", &makeMxrDynacompFace},
            {"ehx-big-muff-nyc", &makeEhxBigMuffNycFace},
            {"ehx-muff-wicker", &makeEhxMuffWickerFace},
            {"ehx-green-russian", &makeEhxGreenRussianFace},
            {"ehx-deluxe-muff", &makeEhxDeluxeMuffFace},
            {"boss-ce2", &makeBossCe2Face},
            {"boss-ce2w", &makeBossCe2wFace},
            {"mxr-phase90-script", &makeMxrPhase90ScriptFace},
            {"boss-dd3t", &makeBossDd3tFace},
            {"tc-ditto", &makeTcDittoFace},
            {"ehx-nano-360", &makeEhxNano360Face},
            {"boss-rc1", &makeBossRc1Face},
            {"boss-rc5", &makeBossRc5Face},
        };
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
