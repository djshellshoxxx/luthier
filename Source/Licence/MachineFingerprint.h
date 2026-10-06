#pragma once

#include <juce_core/juce_core.h>

namespace luthier
{

class MachineFingerprint
{
public:
    static juce::StringArray collect();

    /** Hash one independent machine component with a product-specific salt,
        truncating SHA-256 to 16 bytes (32 hex characters). */
    static juce::String hashComponent (int index, const juce::String& raw);

private:
    static constexpr const char* kSalt = "luthier-audio/licence-fingerprint/v1";
};

} // namespace luthier
