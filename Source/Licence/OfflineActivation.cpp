#include "OfflineActivation.h"

namespace luthier
{

namespace
{
constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int base32Value (juce::juce_wchar c) noexcept
{
    const auto u = juce::CharacterFunctions::toUpperCase (c);
    if (u >= 'A' && u <= 'Z') return (int) (u - 'A');
    if (u >= '2' && u <= '7') return 26 + (int) (u - '2');
    return -1;
}
}

juce::String OfflineActivation::encodeBase32 (const void* data, size_t size)
{
    const auto* bytes = static_cast<const unsigned char*> (data);
    juce::String out;
    unsigned int buffer = 0;
    int bits = 0;

    for (size_t i = 0; i < size; ++i)
    {
        buffer = (buffer << 8) | bytes[i];
        bits += 8;

        while (bits >= 5)
        {
            bits -= 5;
            out += juce::String::charToString ((juce::juce_wchar) alphabet[(buffer >> bits) & 31]);
        }
    }

    if (bits > 0)
        out += juce::String::charToString ((juce::juce_wchar) alphabet[(buffer << (5 - bits)) & 31]);

    return out;
}

bool OfflineActivation::decodeBase32 (const juce::String& text, juce::MemoryBlock& out)
{
    juce::MemoryOutputStream stream;
    unsigned int buffer = 0;
    int bits = 0;

    for (auto c : text.removeCharacters (" -\r\n\t"))
    {
        const int value = base32Value (c);
        if (value < 0)
            return false;

        buffer = (buffer << 5) | (unsigned int) value;
        bits += 5;

        if (bits >= 8)
        {
            bits -= 8;
            const auto byte = (unsigned char) ((buffer >> bits) & 0xff);
            stream.writeByte ((char) byte);
        }
    }

    out = stream.getMemoryBlock();
    return out.getSize() > 0;
}

OfflineActivation::Challenge OfflineActivation::createChallenge (
    const juce::String& key, const juce::String& version)
{
    auto nonceBytes = juce::Random::getSystemRandom().nextInt64();
    const auto nonce = juce::String::toHexString (nonceBytes).paddedLeft ('0', 16);

    auto* root = new juce::DynamicObject();
    root->setProperty ("key", key.trim());
    root->setProperty ("product", "com.luthieraudio.luthier");
    root->setProperty ("version", version);
    root->setProperty ("nonce", nonce);

    juce::Array<juce::var> fp;
    for (const auto& value : MachineFingerprint::collect())
        fp.add (value);
    root->setProperty ("fp", fp);

    const auto json = juce::JSON::toString (juce::var (root), false);

    return { encodeBase32 (json.toRawUTF8(), (size_t) json.getNumBytesAsUTF8()), nonce };
}

LicenceClient::Result OfflineActivation::applyResponse (
    const juce::String& response, LicenceClient& client)
{
    juce::MemoryBlock bytes;

    if (! decodeBase32 (response, bytes))
        return { LicenceClient::ResultCode::invalidInput, "Offline response is not valid base32.", {} };

    const juce::String envelope (static_cast<const char*> (bytes.getData()), bytes.getSize());
    return client.installSignedEnvelope (envelope);
}

} // namespace luthier
