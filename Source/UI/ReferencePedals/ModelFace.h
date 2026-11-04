// OWNER: WP19 / WP20 / WP21
#pragma once
#include "PedalFace.h"
#include "../../ReferencePedals/PedalRegistry.h"
namespace luthier::reference
{
struct FacePlacement
{
    const char* parameter; // Exact descriptor suffix, or alternatives separated by '|'.
    float x,y,diameter;
    const char* legend=nullptr;
    FaceControlKind kind=FaceControlKind::Knob;
};
struct FaceProfile
{
    const char* modelId;
    const char* title;
    juce::Point<float> millimetres;
    juce::Colour enamel;
    juce::Colour lettering;
    float titleY;
    juce::Rectangle<float> footswitchMm;
    juce::Point<float> ledMm;
    std::vector<FacePlacement> controls;
    const char* momentaryFootswitch=nullptr;
    enum class Detail { Dome, Hinged, Treadle, LooperRing, LooperDisplay } detail=Detail::Dome;
};
class ModelFace : public PedalFace
{
public:
    explicit ModelFace(FaceProfile,const PedalDesc* descriptor=nullptr);
    juce::Point<float> designSizeMm() const override {return profile.millimetres;}
    void paintFace(juce::Graphics&,const FaceState&) override;
    void layout(FaceLayout&) override;
    const FaceProfile& faceProfile() const {return profile;}
private:
    int parameterIndex(const char*) const;
    FaceProfile profile;
    const PedalDesc* descriptor;
};
FaceProfile genericFaceProfile(const PedalDesc*);
}
