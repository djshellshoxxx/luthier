// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeMxrDistortionPlusFace()
{
    FaceProfile profile {"mxr-distortion-plus","DISTORTION +",{59.0f,111.0f},juce::Colour(0xfff2c200),juce::Colour(0xff151515),52.0f,
        {22.5f,75.0f,14.0f,14.0f},{29.5f,40.0f},{
            {"distortion|dist|gain",15.0f,22.0f,15.0f,"DISTORTION"},
            {"output|level",44.0f,22.0f,15.0f,"OUTPUT"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
