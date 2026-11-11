// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeEhxDeluxeMuffFace()
{
    FaceProfile profile {"ehx-deluxe-muff","DELUXE MUFF",{145.0f,120.0f},juce::Colour(0xff8f969b),juce::Colour(0xff151515),80.0f,
        {98.0f,93.0f,14.0f,14.0f},{105.0f,88.0f},{
            {"sustain",25.0f,20.0f,16.0f,"SUSTAIN"},
            {"tone",72.0f,20.0f,16.0f,"TONE"},
            {"volume|level",119.0f,20.0f,16.0f,"VOLUME"},
            {"attack",25.0f,45.0f,13.0f,"ATTACK"},
            {"gate",72.0f,45.0f,13.0f,"GATE"},
            {"bass_boost",119.0f,45.0f,10.0f,"BASS"},
            {"mids_level|mid_level",25.0f,70.0f,13.0f,"MIDS LEVEL"},
            {"mids_freq|mid_freq",62.0f,70.0f,13.0f,"MIDS FREQ"},
            {"q|mids_q",99.0f,70.0f,10.0f,"Q"},
            {"mids_on|mids",40.0f,100.0f,14.0f,"MIDS"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
