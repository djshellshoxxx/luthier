// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossSd1Face()
{
    FaceProfile profile {"boss-sd1","SUPER OVERDRIVE",{73.0f,129.0f},juce::Colour(0xfff4d21f),juce::Colour(0xff151515),113.0f,
        {5.0f,47.0f,63.0f,62.0f},{36.5f,38.0f},{
            {"level",15.5f,19.0f,16.0f,"LEVEL"},
            {"tone",36.5f,19.0f,16.0f,"TONE"},
            {"drive",57.5f,19.0f,16.0f,"DRIVE"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
