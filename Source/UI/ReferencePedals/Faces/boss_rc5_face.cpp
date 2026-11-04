// OWNER: WP21
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeBossRc5Face()
{
    FaceProfile profile {"boss-rc5","REF LOOP 5",{73.0f,129.0f},juce::Colour(0xffa73135),juce::Colour(0xfff5f3e9),69.0f,
        {17.2f,79.0f,38.5f,34.0f},{36.5f,62.0f},{
            {"loop_level|level|memory",36.5f,34.0f,15.0f,"LOOP LEVEL"},
            {"setup",14.0f,52.0f,10.0f,"SETUP"},
            {"memory",28.5f,52.0f,10.0f,"MEMORY"},
            {"rhythm",44.5f,52.0f,10.0f,"RHYTHM"},
            {"tempo",59.0f,52.0f,10.0f,"TEMPO"},
        }};
    profile.momentaryFootswitch="pedal1";
    profile.detail=FaceProfile::Detail::LooperDisplay;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
