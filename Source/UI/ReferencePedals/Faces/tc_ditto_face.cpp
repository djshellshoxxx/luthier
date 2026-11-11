// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeTcDittoFace()
{
    FaceProfile profile {"tc-ditto","LOOPER",{48.0f,93.0f},juce::Colour(0xff262729),juce::Colour(0xfff5f2e9),3.0f,
        {16.0f,60.0f,16.0f,16.0f},{24.0f,50.0f},{
            {"loop_level|level",24.0f,30.0f,14.0f,"LOOP LEVEL"},
        }};
    profile.momentaryFootswitch="fs";
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
