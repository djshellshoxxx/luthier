// OWNER: WP16
#pragma once
#include "../Common/LooperStateCodec.h"
#include <array>
#include <vector>

namespace luthier::reference::models
{
// Message-thread-only metadata index. No asset I/O, audio selector, or runtime
// handoff. Source trees and returned memories are deep-cloned, never aliases.
class Rc5MemoryBank
{
public:
    static constexpr int memoryCount=99;
    struct Settings
    {
        juce::String name;
        double loopLevel=100,rhythmLevel=80,tempo=120;
        int pattern=1,variation=0,kit=1;
        bool reverse=false;
    };
    struct Memory
    {
        Settings settings;
        common::LooperStateCodec::Stored loop;
        juce::var source; // original entry, including unowned properties
    };
    struct References
    {
        std::vector<AssetHash> hashes;
        bool complete=true; // known manifest roots only, never recursive pages
    };
    // Public numbers are 1..99. Invalid operations leave destinations intact.
    bool get(int number,Memory&,juce::String* error=nullptr) const;
    bool setSettings(int number,const Settings&,juce::String* error=nullptr);
    // Caller supplies an actual Ready core capture and its source config.
    // Source metadata/hashes are validated; no assets are read or verified here.
    bool save(int number,const common::PagedLooperCore::Graph&,
              const common::PagedLooperCore::Config&,juce::String* error=nullptr);
    // Explicitly clears known audio/history, retaining settings/unknown fields.
    bool clear(int number,juce::String* error=nullptr);
    bool opaque() const noexcept {return forward;}
    bool capacityKnown() const noexcept;
    bool references(References&,bool deduplicate=false,juce::String* error=nullptr) const;
    // Missing subtree means the default bank. Schema1 requires exactly99
    // nullable entries; objects require settings(object) and looper subtrees.
    // Missing known settings use defaults. Newer schemas round-trip read-only.
    static bool decode(const juce::var&,Rc5MemoryBank&,juce::String* error=nullptr);
    bool encode(juce::var&,juce::String* error=nullptr) const;
private:
    static bool numberValid(int number) noexcept {return number>=1 && number<=memoryCount;}
    static bool withinCapacity(const std::array<Memory,memoryCount>&,juce::String*);
    std::array<Memory,memoryCount> entries;
    juce::var source;
    bool forward=false;
};
}
