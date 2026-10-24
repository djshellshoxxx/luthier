// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossDs1Face()
{
    FaceProfile profile {"boss-ds1","DISTORTION",{73.0f,129.0f},juce::Colour(0xffe8751a),juce::Colour(0xff151515),52.0f,
        {6.5f,75.0f,60.0f,40.0f},{36.5f,42.0f},{
            {"level",13.0f,20.0f,15.0f,"LEVEL"},
            {"tone",36.5f,20.0f,15.0f,"TONE"},
            {"dist|distortion",60.0f,20.0f,15.0f,"DIST"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
