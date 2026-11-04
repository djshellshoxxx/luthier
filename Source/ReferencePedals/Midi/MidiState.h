// OWNER: WP6
#pragma once
#include "MidiMap.h"
#include "../BoardTypes.h"
#include <memory>
#include <vector>
namespace luthier::reference::midi
{
// Resolves stable instance UUID + descriptor id into an immutable RT table.
// Unresolved bindings remain in the original juce::var, never removed on load.
class MidiState
{
public:
    static std::unique_ptr<MapSnap> buildMap (const std::vector<PedalInstance>&, const BoardSettings&);
    static bool validate (const juce::var&, juce::String* error = nullptr);
};
}
