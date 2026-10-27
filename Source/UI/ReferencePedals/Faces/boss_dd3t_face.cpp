// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossDd3tFace()
{
    FaceProfile profile {"boss-dd3t","DIGITAL DELAY",{73.0f,129.0f},juce::Colour(0xffe9e7dd),juce::Colour(0xff151515),68.0f,
        {9.0f,80.0f,55.0f,42.0f},{36.5f,62.0f},{
            {"e_level|effect_level|level|mix",18.0f,20.0f,17.0f,"E.LEVEL"},
            {"feedback",55.0f,20.0f,17.0f,"FEEDBACK"},
            {"time",18.0f,44.0f,17.0f,"TIME"},
            {"mode",55.0f,44.0f,17.0f,"MODE"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
