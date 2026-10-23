// OWNER: WP17
#include "BoardUiState.h"
#include <cmath>
namespace luthier::reference
{
juce::Rectangle<int> BoardUiState::clampWindow (juce::Rectangle<int> r, juce::Rectangle<int> work)
{
    if (work.isEmpty()) work = {0,0,1280,800};
    const auto inset = work.reduced (16);
    const int w = juce::jlimit (juce::jmin (480, inset.getWidth()), inset.getWidth(), r.getWidth());
    const int h = juce::jlimit (juce::jmin (260, inset.getHeight()), inset.getHeight(), r.getHeight());
    if (! work.intersects (r)) return inset.withSizeKeepingCentre (w,h);
    return {juce::jlimit (inset.getX(), inset.getRight()-w, r.getX()),
            juce::jlimit (inset.getY(), inset.getBottom()-h, r.getY()), w,h};
}
BoardUiState BoardUiState::read (const juce::var& v)
{
    BoardUiState s;
    auto p = v.getProperty ("popout", juce::var());
    s.window = {(int)p.getProperty("x",120), (int)p.getProperty("y",80),
                juce::jlimit(480,4096,(int)p.getProperty("w",1100)),
                juce::jlimit(260,2160,(int)p.getProperty("h",520))};
    s.popoutOpen = p.getProperty ("open", false);
    const auto d = v.getProperty ("dock",juce::var());
    s.dockOpen = d.getProperty("open",false);
    s.dockHeight = juce::jlimit(160,420,(int)d.getProperty("h",232));
    const auto z = (double)v.getProperty("zoom",1.0);
    s.zoom = std::isfinite(z) ? (float)juce::jlimit(0.5,2.0,z) : 1.f;
    s.scrollX = juce::jmax(0,(int)v.getProperty("scrollX",0));
    s.scrollY = juce::jmax(0,(int)v.getProperty("scrollY",0));
    s.cables = v.getProperty("showCables",true); s.browser = v.getProperty("showBrowser",true);
    return s;
}
juce::var BoardUiState::write (const juce::var& previous) const
{
    auto v = previous.clone();
    if (!v.isObject()) v = new juce::DynamicObject();
    auto* o = v.getDynamicObject();
    auto p = v.getProperty("popout",juce::var()).clone();
    if (!p.isObject()) p = new juce::DynamicObject();
    auto* po = p.getDynamicObject();
    po->setProperty("open",popoutOpen); po->setProperty("x",window.getX()); po->setProperty("y",window.getY());
    po->setProperty("w",window.getWidth()); po->setProperty("h",window.getHeight());
    o->setProperty("popout",p);
    auto d = v.getProperty("dock",juce::var()).clone();
    if (!d.isObject()) d = new juce::DynamicObject();
    d.getDynamicObject()->setProperty("open",dockOpen); d.getDynamicObject()->setProperty("h",dockHeight);
    o->setProperty("dock",d); o->setProperty("zoom",zoom); o->setProperty("scrollX",scrollX); o->setProperty("scrollY",scrollY);
    o->setProperty("showCables",cables); o->setProperty("showBrowser",browser);
    return v;
}
}
