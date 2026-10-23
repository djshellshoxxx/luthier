// OWNER: WP19
#include "PedalFaceCache.h"
#include "../Theme.h"
#include <map>
namespace luthier::reference
{
namespace {
struct Entry {juce::Image image;size_t bytes=0;uint64_t used=0;};
std::map<juce::String,Entry> entries;
size_t bytes=0;uint64_t serial=0;
}
juce::Image PedalFaceCache::get(PedalFace& face,const char* id,int rev,juce::Point<int> size,float scale,const FaceState& state)
{
    scale=juce::jlimit(.5f,3.f,scale);
    const int w=juce::jlimit(1,2048,(int)std::ceil(size.x*scale));
    const int h=juce::jlimit(1,4096,(int)std::ceil(size.y*scale));
    const auto key=juce::String(id)+"|"+juce::String(rev)+"|"+juce::String(w)+"x"+juce::String(h)+"|"+juce::String(Palette::revision)+"|"+juce::String(state.highContrast?1:0);
    if(auto it=entries.find(key);it!=entries.end()){it->second.used=++serial;return it->second.image;}
    const size_t needed=(size_t)w*(size_t)h*4;
    while(bytes+needed>maxBytes&&!entries.empty())
    {
        auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        bytes-=oldest->second.bytes;entries.erase(oldest);
    }
    juce::Image image(juce::Image::ARGB,w,h,true);juce::Graphics g(image);const auto mm=face.designSizeMm();
    g.addTransform(juce::AffineTransform::scale((float)w/(mm.x*1.6f),(float)h/(mm.y*1.6f)));face.paintFace(g,state);
    if(needed<=maxBytes){entries.emplace(key,Entry{image,needed,++serial});bytes+=needed;}
    return image;
}
void PedalFaceCache::clear(){entries.clear();bytes=0;}
size_t PedalFaceCache::bytesUsed(){return bytes;}
}
