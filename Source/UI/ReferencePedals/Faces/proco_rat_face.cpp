// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeProcoRatFace()
{
    FaceProfile profile {"proco-rat","RODENT DISTORTION",{120.0f,95.0f},juce::Colour(0xff17171a),juce::Colour(0xffffffff),52.0f,
        {53.0f,65.0f,14.0f,14.0f},{60.0f,50.0f},{
            {"distortion|dist|gain",24.0f,28.0f,18.0f,"DISTORTION"},
            {"filter",60.0f,28.0f,18.0f,"FILTER"},
            {"volume|level",96.0f,28.0f,18.0f,"VOLUME"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
