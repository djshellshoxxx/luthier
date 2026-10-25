// OWNER: WP15/WP16
#pragma once
#include "PagedLooperCore.h"
namespace luthier::reference::common
{
// Stopped, off-audio conversion of immutable external PCM graphs. No whole
// recording buffers, extra workers, or audio-thread work. See the finite
// contract in spec/reference-pedalboard/LOOPER_ASSET_RATE_CONVERSION.md.
class LooperAssetConverter
{
public:
    static bool reservation(const PagedLooperCore::Config& source,
                            const PagedLooperCore::Config& target,
                            const PagedLooperCore::Graph&,size_t& ram,uint64_t& storage) noexcept;
    // Atomic output on failure. Content-addressed orphan blobs may remain.
    static bool convert(IAssetStore&,const PagedLooperCore::Config& source,
                        const PagedLooperCore::Config& target,
                        const PagedLooperCore::Graph&,PagedLooperCore::Graph& out,
                        juce::String* error=nullptr);
};
}
