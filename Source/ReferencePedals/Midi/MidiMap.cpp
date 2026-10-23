#include "MidiMap.h"
namespace luthier::reference::midi
{
bool reservedCc (int cc) noexcept
{
    return cc == 0 || cc == 32 || cc == 7 || cc == 120 || cc == 121 || cc == 123;
}
bool MapSnap::add (const Binding& b) noexcept
{
    if (numBindings >= kMaxBindings || b.chan > 16 || b.srcNum > 127 || b.srcType >= kSourceTypes
        || (b.srcType == (int) MidiSrc::CC && reservedCc (b.srcNum))
        || ! std::isfinite (b.inMin) || ! std::isfinite (b.inMax) || b.inMax <= b.inMin
        || ! std::isfinite (b.outMin) || ! std::isfinite (b.outMax)
        || (b.target >= kTargetBoardBase && b.target >= kTargetBoardBase + 16)) return false;
    bindings[(size_t) numBindings++] = b; return true;
}
void MapSnap::compile() noexcept
{
    std::sort (bindings.begin(), bindings.begin() + numBindings, [] (auto& a, auto& b) { return a.id < b.id; });
    for (auto& bucket : lookup) bucket.count = 0;
    for (int i = 0; i < numBindings; ++i)
    {
        auto& b = bindings[(size_t) i];
        for (int j = 0; j < 256; ++j) curves[(size_t) i][(size_t) j] = curveValue (b.curve(), (float) j / 255.f, b.curveParam);
    }
    // Exact channel bindings claim a source before omni, regardless of id.
    for (int exact = 1; exact >= 0; --exact)
        for (int i = 0; i < numBindings; ++i)
        {
            const auto& b = bindings[(size_t) i];
            if (! b.has (BF_Enabled) || (b.chan != 0) != (exact != 0)) continue;
            for (int channel = 1; channel <= 16; ++channel)
            {
                if (b.chan != 0 && b.chan != channel) continue;
                auto& bucket = lookup[((size_t) b.srcType * 16 + (size_t) channel - 1) * 128 + b.srcNum];
                if (exact == 0 && bucket.count > 0 && bindings[bucket.index[0]].chan != 0) continue;
                if (bucket.count < 8) bucket.index[bucket.count++] = (uint16_t) i;
            }
        }
}
const MapSnap::Bucket& MapSnap::find (MidiSrc t, int ch, int number) const noexcept
{
    static const Bucket empty {};
    if ((int) t >= kSourceTypes || ch < 1 || ch > 16 || number < 0 || number > 127) return empty;
    return lookup[((size_t) t * 16 + (size_t) ch - 1) * 128 + (size_t) number];
}
}
