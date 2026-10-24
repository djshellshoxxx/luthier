// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeEhxBigMuffNycFace()
{
    FaceProfile profile {"ehx-big-muff-nyc","MUFF SUSTAINER",{111.0f,121.0f},juce::Colour(0xffb8bdc2),juce::Colour(0xff151515),59.0f,
        {49.5f,86.0f,12.0f,12.0f},{55.5f,14.0f},{
            {"volume|level",27.5f,34.0f,20.0f,"VOLUME"},
            {"tone",55.5f,34.0f,20.0f,"TONE"},
            {"sustain",83.5f,34.0f,20.0f,"SUSTAIN"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
