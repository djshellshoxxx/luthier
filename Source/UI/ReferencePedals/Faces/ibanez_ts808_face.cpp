// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeIbanezTs808Face()
{
    FaceProfile profile {"ibanez-ts808","OVERDRIVE",{70.0f,125.0f},juce::Colour(0xff2f8f3c),juce::Colour(0xffffffff),37.0f,
        {7.0f,72.0f,56.0f,38.0f},{35.0f,52.0f},{
            {"drive",12.0f,18.0f,14.0f,"DRIVE"},
            {"tone",35.0f,18.0f,14.0f,"TONE"},
            {"level",58.0f,18.0f,14.0f,"LEVEL"},
        }};
    profile.detail=FaceProfile::Detail::Hinged;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
