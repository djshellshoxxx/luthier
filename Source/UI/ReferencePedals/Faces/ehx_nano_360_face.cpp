// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeEhxNano360Face()
{
    FaceProfile profile {"ehx-nano-360","LOOPER 360",{70.0f,115.0f},juce::Colour(0xff3b5a73),juce::Colour(0xffecece4),60.0f,
        {24.0f,73.0f,22.0f,22.0f},{35.0f,47.0f},{
            {"level|loop_level",21.0f,26.0f,16.0f,"LEVEL"},
            {"loop|memory|slot",49.0f,26.0f,16.0f,"LOOP"},
        }};
    profile.momentaryFootswitch="footswitch";
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
