// OWNER: WP4
#pragma once
#include "IAssetStore.h"
#include <juce_cryptography/juce_cryptography.h>
namespace luthier::reference
{
// Content-addressed local store. Callbacks finish on a single owned worker;
// destruction drains queued jobs before releasing files or callback captures.
class AssetStore final : public IAssetStore
{
public:
    explicit AssetStore (juce::File directory);
    ~AssetStore() override;
    static AssetHash hashBlob (const uint8_t*, size_t);
    bool putBlob (const AssetHash&, const uint8_t*, size_t) override;
    bool hasBlob (const AssetHash&) const override;
    juce::File getBlobPath (const AssetHash&) const override;
    void putBlobAsync (const AssetHash&, AssetBlob, std::function<void(bool)>) override;
    void getBlobAsync (const AssetHash&, std::function<void(AssetBlob)>) override;
    std::vector<AssetHash> findMissing (const std::vector<AssetHash>&) const override;
    // Suitable for state/logs: no user directory or absolute path.
    static juce::String reference (const AssetHash&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
