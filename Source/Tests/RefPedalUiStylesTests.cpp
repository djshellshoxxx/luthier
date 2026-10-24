// OWNER: WP18
#include "TestFramework.h"
#include "../UI/ReferencePedals/BoardSurfacePainter.h"
#include "../UI/Theme.h"
using namespace luthier;
using namespace luthier::reference;
namespace {
juce::uint64 pixelHash(const juce::Image& im) {
    juce::uint64 h=1469598103934665603ULL;
    for(int y=0;y<im.getHeight();++y)for(int x=0;x<im.getWidth();++x)h=(h^im.getPixelAt(x,y).getARGB())*1099511628211ULL;
    return h;
}
juce::Image surface(BoardStyle style) {
    juce::Image im(juce::Image::ARGB,300,180,true);juce::Graphics g(im);
    BoardSurfacePainter::paint(g,{0,0,300,180},style);return im;
}
}
LUTHIER_TEST(RefPedalUiStyles, sevenSurfacesAreDeterministicAndDistinct) {
    const juce::ScopedValueSetter<bool> textured(Palette::textured,true);
    std::vector<juce::uint64> hashes;
    for(int i=0;i<7;++i){auto style=(BoardStyle)i;const auto a=pixelHash(surface(style));CHECK(a==pixelHash(surface(style)));CHECK(std::find(hashes.begin(),hashes.end(),a)==hashes.end());hashes.push_back(a);}
}
LUTHIER_TEST(RefPedalUiStyles, highContrastAndMixedMountingsPaint) {
    const juce::ScopedValueSetter<bool> textured(Palette::textured,false);
    for(int i=0;i<7;++i){auto im=surface((BoardStyle)i);juce::Graphics g(im);for(int j=1;j<=7;++j)BoardSurfacePainter::paintMount(g,{30.f+j*26.f,50,20,70},(Mounting)j,431);CHECK(im.getPixelAt(15,15).getAlpha()>0);}
}
#include "../UI/ReferencePedals/BoardCanvas.h"
#include "../ReferencePedals/PedalRegistry.h"
LUTHIER_TEST(RefPedalUiStyles, highContrastBypassRetainsBodyContrast)
{
    PedalRegistry::registerAll();const juce::ScopedValueSetter<bool> contrast(Palette::textured,false);BoardModel model;CHECK(model.apply(cmd::Add{"generic.boost"}));BoardCanvas canvas(model);const auto id=model.getOrder()[0];auto* view=canvas.viewFor(id);const auto enabled=view->createComponentSnapshot(view->getLocalBounds());CHECK(model.apply(cmd::SetEnabled{id,false}));const auto bypassed=view->createComponentSnapshot(view->getLocalBounds());CHECK(enabled.getPixelAt(8,52)==bypassed.getPixelAt(8,52));
}
