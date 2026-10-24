// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeMxrDynacompFace()
{
    FaceProfile profile {"mxr-dynacomp","DYNAMIC COMP",{111.0f,60.0f},juce::Colour(0xffc0262d),juce::Colour(0xffffffff),30.0f,
        {48.5f,36.0f,14.0f,14.0f},{55.5f,10.0f},{
            {"sensitivity|sustain",29.0f,18.0f,14.0f,"SENSITIVITY"},
            {"output|level",82.0f,18.0f,14.0f,"OUTPUT"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
