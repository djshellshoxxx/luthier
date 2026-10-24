// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossRc1Face()
{
    FaceProfile profile {"boss-rc1","REF LOOP",{73.0f,129.0f},juce::Colour(0xff343638),juce::Colour(0xfff5f3e9),30.0f,
        {6.5f,73.0f,60.0f,44.0f},{36.5f,52.0f},{
            {"level|loop_level",36.5f,22.0f,17.0f,"LEVEL"},
        }};
    profile.momentaryFootswitch="pedal";
    profile.detail=FaceProfile::Detail::LooperRing;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
