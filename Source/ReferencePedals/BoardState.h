// OWNER: WP4
#pragma once
#include "BoardModel.h"

namespace luthier::reference
{
// Message/host-thread codec. Outputs are assigned only after complete validation.
// unknown retains a private source envelope; consumers should use midi/ui fields.
class BoardState
{
public:
    static constexpr size_t maxBytes = 256 * 1024;
    static bool encode (const std::vector<PedalInstance>&, const BoardSettings&,
                        juce::var& json, juce::String* error = nullptr);
    static bool decode (const juce::var& json, std::vector<PedalInstance>&,
                        BoardSettings&, juce::String* error = nullptr);
    static bool save (const BoardModel&, juce::String& json, juce::String* error = nullptr);
    static bool restore (BoardModel&, const juce::String& json, juce::String* error = nullptr);
    static bool save (const BoardModel&, juce::var& json, juce::String* error = nullptr);
    static bool restore (BoardModel&, const juce::var& json, juce::String* error = nullptr);
};
} // namespace luthier::reference
