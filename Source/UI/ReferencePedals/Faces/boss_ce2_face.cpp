// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossCe2Face()
{
    FaceProfile profile {"boss-ce2","CHORUS",{70.0f,125.0f},juce::Colour(0xff4fa5d8),juce::Colour(0xff151515),62.0f,
        {7.5f,75.0f,55.0f,42.0f},{35.0f,50.0f},{
            {"rate",17.0f,21.0f,17.0f,"RATE"},
            {"depth",53.0f,21.0f,17.0f,"DEPTH"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
