// OWNER: WP20
#include "../ModelFace.h"
namespace luthier::reference
{
std::unique_ptr<PedalFace> makeEhxGreenRussianFace()
{
    FaceProfile profile {"ehx-green-russian","GREEN FUZZ",{69.9f,114.3f},juce::Colour(0xff4f6b3a),juce::Colour(0xffe8e6da),43.0f,
        {28.0f,80.0f,14.0f,14.0f},{35.0f,36.0f},{
            {"volume|level",18.0f,16.0f,14.0f,"VOLUME"},
            {"tone",35.0f,16.0f,14.0f,"TONE"},
            {"sustain",52.0f,16.0f,14.0f,"SUSTAIN"},
        }};
    profile.detail=FaceProfile::Detail::Dome;
    return std::make_unique<ModelFace>(std::move(profile));
}
}
