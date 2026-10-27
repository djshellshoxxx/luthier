// OWNER: WP15/WP16
#pragma once
#include "PagedLooperCore.h"
namespace luthier::reference::common
{
// Message-thread codec for the model-owned extras.looper subtree. Wrappers
// overlay this one key onto current extras, preserving MIDI and other owners.
class LooperStateCodec
{
public:
    struct Stored
    {
        PagedLooperCore::Config config;
        PagedLooperCore::Graph graph;
        juce::var source;
        bool forward=false;
    };
    // Writes schema2 with ordered redo; reads legacy schema1 without redo.
    // Void means no saved loop. A newer integral schema remains opaque and
    // must not be loaded into DSP; encode preserves its complete original tree.
    // All failures leave the previous Stored/output untouched.
    static bool decode(const juce::var& looper,Stored&,juce::String* error=nullptr);
    static bool encode(const Stored&,const PagedLooperCore::Graph&,const PagedLooperCore::Config&,juce::var& looper,juce::String* error=nullptr);
};
}
