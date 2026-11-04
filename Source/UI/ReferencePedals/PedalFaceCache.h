// OWNER: WP19
#pragma once
#include "PedalFace.h"
namespace luthier::reference
{
// Process-shared, message-thread-only LRU of static vector faces. Live controls
// and LEDs are drawn after this image. The cap covers all cached image pixels.
class PedalFaceCache
{
public:
    static juce::Image get(PedalFace&,const char* modelId,int revision,juce::Point<int> size,float scale,const FaceState&);
    static void clear();
    static size_t bytesUsed();
    static constexpr size_t maxBytes=96u*1024u*1024u;
};
}
