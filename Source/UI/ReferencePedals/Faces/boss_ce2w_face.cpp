// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossCe2wFace()
{
    FaceProfile profile {"boss-ce2w","CHORUS W",{73.0f,129.0f},juce::Colour(0xff4aabb8),juce::Colour(0xff151515),59.0f,
        {10.0f,63.0f,53.0f,62.0f},{36.5f,52.0f},{
            {"rate",17.5f,18.0f,12.0f,"RATE"},
            {"depth",55.5f,18.0f,12.0f,"DEPTH"},
            {"mode",36.5f,40.0f,12.0f,"MODE"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
