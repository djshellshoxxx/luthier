// OWNER: WP19 / WP20 / WP21
#include "ModelFace.h"
#include "../Theme.h"
namespace luthier::reference
{
ModelFace::ModelFace(FaceProfile p,const PedalDesc* d):profile(std::move(p)),descriptor(d?d:PedalRegistry::find(profile.modelId)){}
int ModelFace::parameterIndex(const char* alternatives) const
{
    if(!descriptor||!alternatives)return -1;
    const auto names=juce::StringArray::fromTokens(alternatives,"|","");
    for(const auto& name:names)
        for(int i=0;i<descriptor->numParams;++i)
            if(juce::String(descriptor->params[i].id)==name || juce::String(descriptor->params[i].id).endsWith("_"+name))return i;
    return -1;
}
void ModelFace::layout(FaceLayout& out)
{
    out.clear();const auto size=profile.millimetres*1.6f;
    out.footswitch=profile.footswitchMm*1.6f;
    out.led={profile.ledMm.x*1.6f-3,profile.ledMm.y*1.6f-3,6,6};
    if(profile.ledMm.x<0)out.led={};
    out.jacks={{FaceJack::Kind::In,{0,size.y*.42f}},{FaceJack::Kind::Out,{size.x,size.y*.42f}},{FaceJack::Kind::Dc,{size.x*.75f,0}}};
    for(const auto& placement:profile.controls)
    {
        const int index=parameterIndex(placement.parameter);if(index<0)continue;
        const auto& desc=descriptor->params[index];
        const bool selector=desc.isDiscreteKind()||desc.kind==ParamKind::Toggle;
        const auto kind=placement.kind==FaceControlKind::Knob&&selector?FaceControlKind::Selector:placement.kind;
        const float diameter=juce::jmax(kind==FaceControlKind::Selector?18.f:28.f,placement.diameter*1.6f);
        out.controls.push_back({index,kind,{placement.x*1.6f-diameter*.5f,placement.y*1.6f-diameter*.5f,diameter,kind==FaceControlKind::Knob?diameter+12:diameter},placement.legend?placement.legend:desc.label});
    }
    const int foot=parameterIndex(profile.momentaryFootswitch);
    if(foot>=0)out.controls.push_back({foot,FaceControlKind::Button,out.footswitch,descriptor->params[foot].label});
}
void ModelFace::paintFace(juce::Graphics& g,const FaceState& state)
{
    const auto size=profile.millimetres*1.6f;const juce::Rectangle<float> body(0,0,size.x,size.y);
    const auto fill=state.highContrast?Palette::panelRaised:profile.enamel;
    if(state.highContrast)g.setColour(fill);
    else g.setGradientFill(juce::ColourGradient(fill.brighter(.12f),0,0,fill.darker(.2f),size.x,size.y,false));
    g.fillRoundedRectangle(body,5);g.setColour(state.highContrast?Palette::textPrimary:fill.brighter(.4f));g.drawRoundedRectangle(body.reduced(1),4,1.2f);
    g.setColour(state.highContrast?Palette::textPrimary:profile.lettering);
    g.setFont(juce::FontOptions(juce::jlimit(9.f,16.f,size.x*.095f)).withStyle("Bold"));
    const int titleHeight=profile.titleY<15||profile.detail==FaceProfile::Detail::LooperRing?12:juce::jlimit(12,22,(int)(profile.footswitchMm.getY()*1.6f-profile.titleY*1.6f-3));
    g.drawFittedText(profile.title,4,(int)(profile.titleY*1.6f),(int)size.x-8,titleHeight,juce::Justification::centred,2);
    FaceLayout plan;layout(plan);g.setFont(juce::FontOptions(10.f));
    for(const auto& control:plan.controls)
    {
        if(control.kind==FaceControlKind::Button)continue;
        float width=48;const float x=control.bounds.getCentreX();
        for(const auto& other:plan.controls)if(&other!=&control&&std::abs(other.bounds.getY()-control.bounds.getY())<8)width=juce::jmin(width,std::abs(other.bounds.getCentreX()-x)-2);
        width=juce::jmin(width,2*juce::jmin(x-3,size.x-3-x));
        const auto label=juce::Rectangle<float>(x-width*.5f,control.bounds.getY()-13,width,12);
        g.drawFittedText(control.label,label.toNearestInt(),juce::Justification::centred,1);
        if(control.kind==FaceControlKind::Knob&&!state.highContrast)
        {
            const auto centre=control.bounds.getCentre().withY(control.bounds.getY()+control.bounds.getWidth()*.5f);
            const float radius=control.bounds.getWidth()*.52f;
            for(int i=0;i<=10;++i){const auto angle=(-.75f+1.5f*i/10.f)*juce::MathConstants<float>::pi;const juce::Point<float> a(centre.x+radius*std::sin(angle),centre.y-radius*std::cos(angle));const juce::Point<float> b(centre.x+(radius+2)*std::sin(angle),centre.y-(radius+2)*std::cos(angle));g.drawLine({a,b},.6f);}
        }
    }
    if(profile.detail==FaceProfile::Detail::Treadle)
    {
        const auto pad=body.reduced(10,34);g.setColour(state.highContrast?Palette::edgeBright:juce::Colour(0xff171819));g.fillRoundedRectangle(pad,4);
        g.setColour(Palette::textMuted.withAlpha(.45f));for(float y=pad.getY()+6;y<pad.getBottom()-3;y+=7)g.drawHorizontalLine((int)y,pad.getX()+4,pad.getRight()-4);
    }
    // Connector shapes are independent original vectors, never a traced brand asset.
    g.setColour(state.highContrast?Palette::textPrimary:juce::Colours::darkgrey);
    for(const auto& jack:plan.jacks){g.fillEllipse(jack.position.x-3,jack.position.y-3,6,6);}
}
FaceProfile genericFaceProfile(const PedalDesc* desc)
{
    const juce::String id(desc?desc->modelId:"");
    FaceProfile p {desc?desc->modelId:"missing","MISSING MODEL",{75,125},juce::Colour(0xff445264),juce::Colours::white,62,{26,86,14,14},{33,72},{}};
    if(!desc)return p;
    p.title=desc->displayName;p.millimetres={66,112};p.titleY=69;p.footswitchMm={26,88,14,14};p.ledMm={33,81};
    if(id=="generic.boost") {p.millimetres={54,90};p.enamel=juce::Colour(0xff546c82);p.titleY=58;p.footswitchMm={20,69,14,14};p.ledMm={44,62};p.controls={{"gain",27,22,16},{"tone",13,46,14},{"level",41,46,14},{"mode",43,18,11}};}
    else if(id=="generic.overdrive") {p.enamel=juce::Colour(0xff397449);p.controls={{"drive",12,24,15},{"tone",33,18,15},{"level",54,24,15},{"clip",20,52,12},{"bass",46,52,12}};}
    else if(id=="generic.fuzz") {p.enamel=juce::Colour(0xff8f3438);p.controls={{"fuzz",12,22,15},{"tone",33,22,15},{"volume",54,22,15},{"bias",18,49,12},{"type",46,49,12}};}
    else if(id=="generic.compressor") {p.enamel=juce::Colour(0xff84a5ba);p.lettering=juce::Colour(0xff151d25);p.controls={{"sustain",17,20,16},{"attack",49,20,16},{"level",17,49,16},{"blend",49,49,16},{"release",33,70,10}};p.titleY=79;p.ledMm={58,83};}
    else if(id=="generic.chorus") {p.enamel=juce::Colour(0xff247b7b);p.controls={{"rate",12,22,15},{"depth",33,22,15},{"mix",54,22,15},{"tone",18,49,12},{"mode",46,49,12}};}
    else if(id=="generic.delay") {p.millimetres={90,112};p.enamel=juce::Colour(0xff274c7c);p.titleY=73;p.footswitchMm={57,88,14,14};p.ledMm={64,81};p.controls={{"time",12,22,15},{"feedback",34,22,15,"FDBK"},{"mix",56,22,15},{"tone",78,22,15},{"mode",16,51,12},{"mod",44,51,12},{"sub",72,51,12},{"tap",24,95,14,nullptr,FaceControlKind::Button}};}
    else if(id=="generic.reverb") {p.millimetres={90,112};p.enamel=juce::Colour(0xffc4b28a);p.lettering=juce::Colour(0xff292016);p.titleY=73;p.footswitchMm={57,88,14,14};p.ledMm={64,81};p.controls={{"decay",18,22,17},{"mix",45,22,17},{"tone",72,22,17},{"predelay|pre_delay",18,52,12},{"size",45,52,12},{"type",72,52,12},{"freeze",24,95,14,nullptr,FaceControlKind::Button}};}
    else if(id=="generic.tremolo") {p.enamel=juce::Colour(0xff272629);p.lettering=juce::Colour(0xffe3c274);p.controls={{"rate",12,22,15},{"depth",33,22,15},{"shape",54,22,15},{"wave",18,49,12},{"stereo",46,49,12},{"tap",13,94,11,nullptr,FaceControlKind::Button}};}
    else if(id=="generic.wah") {p.millimetres={54,150};p.enamel=juce::Colour(0xff5a5f63);p.detail=FaceProfile::Detail::Treadle;p.titleY=126;p.footswitchMm={20,7,14,14};p.ledMm={43,17};p.controls={{"pedal",27,73,28,"PEDAL",FaceControlKind::Treadle},{"q",12,131,12},{"range_hi",42,131,12}};}
    else if(id=="generic.phaser") {p.enamel=juce::Colour(0xffcf762f);p.lettering=juce::Colour(0xff171513);p.controls={{"rate|speed",16,23,17},{"depth",49,23,17},{"feedback|resonance",16,52,13},{"mix",49,52,13}};}
    else
    {
        // Unknown/new descriptors retain a real editable fallback, not hard-coded array indices.
        int visible=0;for(int i=0;i<desc->numParams&&visible<6;++i){const auto& d=desc->params[i];if(d.has(PF_Engage)||d.has(PF_Config)||d.has(PF_Hidden)||d.has(PF_Ext))continue;p.controls.push_back({d.id,12.f+(visible%3)*21,22.f+(visible/3)*29,14});++visible;}
    }
    return p;
}
}
