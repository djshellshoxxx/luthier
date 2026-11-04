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
    // Exact finite JSON for runtime extra-state keys, without board schema
    // interpretation. Compact, deterministic property order/number spelling;
    // the shared depth/size bounds apply and failure leaves output unchanged.
    static bool canonicalJson(const juce::var&,juce::String&,juce::String* error=nullptr);
    static bool encode (const std::vector<PedalInstance>&, const BoardSettings&,
                        juce::var& json, juce::String* error = nullptr);
    static bool decode (const juce::var& json, std::vector<PedalInstance>&,
                        BoardSettings&, juce::String* error = nullptr);
    // Extract the exact top-level host block before a generic host JSON parser
    // can round its numbers or discard future-schema formatting. Absent block
    // succeeds with an empty output; failures leave output unchanged.
    static bool extractFromHostJson (const juce::String& hostJson, juce::String& boardJson,
                                     juce::String* error = nullptr);
    static bool save (const BoardModel&, juce::String& json, juce::String* error = nullptr);
    static bool restore (BoardModel&, const juce::String& json, juce::String* error = nullptr);
    static bool save (const BoardModel&, juce::var& json, juce::String* error = nullptr);
    static bool restore (BoardModel&, const juce::var& json, juce::String* error = nullptr);
};
} // namespace luthier::reference
