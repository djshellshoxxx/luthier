// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeMxrPhase90ScriptFace()
{
    FaceProfile profile {"mxr-phase90-script","PHASE 90",{111.0f,60.0f},juce::Colour(0xffe8731a),juce::Colour(0xff151515),31.0f,
        {45.5f,34.0f,20.0f,20.0f},{-1.0f,-1.0f},{
            {"speed|rate",55.5f,18.0f,18.0f,"SPEED"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
