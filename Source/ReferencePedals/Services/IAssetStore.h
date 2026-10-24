// OWNER: WP1 (frozen interface I-8; implementation AssetStore.{h,cpp} is WP4)
#pragma once

// Content-addressed external storage for audio that must never be inlined in
// the host state blob (looper loops, RC-5 memories; BOARD_CORE 10.1, 256 KB
// state cap). Implementation: WP4 Services/AssetStore. All calls are for the
// message thread or a worker thread; the audio thread never touches a store.
// Pedals receive a pointer through PedalServices::assets and may get nullptr
// (then they must degrade to RAM only).

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <juce_core/juce_core.h>

namespace luthier::reference
{

// SHA-256 of the blob content.
struct AssetHash
{
    std::array<uint8_t, 32> bytes {};

    bool isNull() const noexcept
    {
        for (auto b : bytes) if (b != 0) return false;
        return true;
    }
    bool operator== (const AssetHash& o) const noexcept { return bytes == o.bytes; }
    bool operator!= (const AssetHash& o) const noexcept { return bytes != o.bytes; }

    juce::String toHex() const { return juce::String::toHexString (bytes.data(), (int) bytes.size(), 0); }

    static AssetHash fromHex (const juce::String& hex) noexcept
    {
        AssetHash h;
        const auto clean = hex.removeCharacters (" ");
        if (clean.length() != 64)
            return h;
        for (int i = 0; i < 32; ++i)
            h.bytes[(size_t) i] = (uint8_t) clean.substring (i * 2, i * 2 + 2).getHexValue32();
        return h;
    }
};

using AssetBlob = std::shared_ptr<const std::vector<uint8_t>>;

class IAssetStore
{
public:
    virtual ~IAssetStore() = default;

    // Synchronous (message or worker thread). putBlob writes a temp file and
    // renames atomically; a failed write leaves any previous file untouched.
    // The store verifies that `hash` matches the content and refuses otherwise.
    virtual bool putBlob (const AssetHash& hash, const uint8_t* data, size_t numBytes) = 0;
    virtual bool hasBlob (const AssetHash& hash) const = 0;

    // File holding the blob, or an empty juce::File when it is missing.
    virtual juce::File getBlobPath (const AssetHash& hash) const = 0;

    // Async API: the callback runs on the store's worker thread (post to the
    // message thread yourself). The blob is shared and immutable.
    virtual void putBlobAsync (const AssetHash& hash, AssetBlob blob, std::function<void (bool ok)> done) = 0;
    virtual void getBlobAsync (const AssetHash& hash, std::function<void (AssetBlob blob)> done) = 0;

    // Hashes referenced by saved state but absent from disk (missing-asset report).
    virtual std::vector<AssetHash> findMissing (const std::vector<AssetHash>& referenced) const = 0;
};

} // namespace luthier::reference
