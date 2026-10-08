#pragma once

#include "LicenceClient.h"

namespace luthier
{

class OfflineActivation
{
public:
    struct Challenge
    {
        juce::String text;
        juce::String nonce;
    };

    static Challenge createChallenge (const juce::String& key,
                                      const juce::String& version);

    /** A response is the signed licence envelope encoded as base32. */
    static LicenceClient::Result applyResponse (const juce::String& response,
                                                LicenceClient& client);

    static juce::String encodeBase32 (const void* data, size_t size);
    static bool decodeBase32 (const juce::String&, juce::MemoryBlock&);
};

} // namespace luthier
