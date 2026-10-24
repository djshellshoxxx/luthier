// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeEhxMuffWickerFace()
{
    FaceProfile profile {"ehx-muff-wicker","TONE WICKER",{120.7f,101.6f},juce::Colour(0xffbdc2c8),juce::Colour(0xff151515),43.0f,
        {54.0f,77.0f,13.0f,13.0f},{60.35f,11.75f},{
            {"volume|level",29.9f,28.5f,20.0f,"VOLUME"},
            {"tone",60.35f,28.5f,20.0f,"TONE"},
            {"sustain",90.8f,28.5f,20.0f,"SUSTAIN"},
            {"tone_switch|tone_on|tone_bypass",45.1f,60.5f,10.0f,"TONE"},
            {"wicker|wicker_on",75.6f,60.5f,10.0f,"WICKER"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
