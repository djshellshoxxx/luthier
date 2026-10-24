// OWNER: WP19
#include "GenericFace.h"
#include "../Theme.h"
namespace luthier::reference
{
GenericFace::GenericFace (const PedalDesc* d) : desc(d) {}
juce::Point<float> GenericFace::designSizeMm() const
{
    if (!desc) return {75,125};
    const juce::String id (desc->modelId);
    if (id.startsWith("boss-")) return {73,129};
    if (id == "ibanez-ts808") return {74,125};
    if (id == "mxr-phase90-script" || id.startsWith("mxr-")) return {60,111};
    if (id == "tc-ditto") return {48,93};
    if (id == "ehx-deluxe-muff") return {145,120};
    if (id == "generic.wah") return {54,150};
    return {90,140};
}
void GenericFace::paintFace (juce::Graphics& g, const FaceState& s)
{
    auto sz = designSizeMm()*1.6f;
    const auto hash = desc ? juce::String(desc->modelId).hashCode() : 0;
    const auto colour = s.highContrast ? Palette::panelRaised : juce::Colour::fromHSV((float)((unsigned)hash%360)/360.f, .35f, .38f,1.f);
    g.setColour(colour); g.fillRoundedRectangle(0,0,sz.x,sz.y,6);
    g.setColour(Palette::textPrimary); g.drawRoundedRectangle(1,1,sz.x-2,sz.y-2,5,s.highContrast?2.f:1.f);
    // Original workshop enclosure: no manufacturer logo or copied graphics.
    g.setFont(11.f);
    g.drawFittedText(desc ? desc->displayName : "Missing model", 6,5,(int)sz.x-12,30,juce::Justification::centred,2);
}
void GenericFace::layout (FaceLayout& out)
{
    out.clear(); const auto sz = designSizeMm()*1.6f;
    out.jacks.push_back({FaceJack::Kind::In,{0,sz.y*.42f}});
    out.jacks.push_back({FaceJack::Kind::Out,{sz.x,sz.y*.42f}});
    out.footswitch = {sz.x*.5f-22,sz.y-50,44,44}; out.led = {sz.x*.5f-4,sz.y-64,8,8};
    if (desc) for (int i=0,n=0;i<desc->numParams;++i)
    {
        const auto& p=desc->params[i]; if(p.has(PF_Engage)) continue;
        out.controls.push_back({i, (p.kind==ParamKind::Momentary||p.kind==ParamKind::Trigger)?FaceControlKind::Button:p.isDiscreteKind()?FaceControlKind::Selector:FaceControlKind::Knob,
            {6.f+(n%3)*(sz.x-12)/3,40.f+(n/3)*48,(sz.x-12)/3,46},p.label}); ++n;
    }
}
}
