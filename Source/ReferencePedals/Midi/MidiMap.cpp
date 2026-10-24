// OWNER: WP5
#include "MidiMap.h"
namespace luthier::reference::midi
{
bool reservedCc (int cc) noexcept
{
    return cc == 0 || cc == 32 || cc == 7 || cc == 120 || cc == 121 || cc == 123;
}
bool MapSnap::add (const Binding& b, uint8_t layer) noexcept
{
    if ((int) b.mode() > (int) BindMode::Step || (int) b.curve() > (int) CurveId::Custom5) return false;
    if (numBindings >= kMaxBindings || b.chan > 16 || b.srcNum > 127 || b.srcType >= kSourceTypes
        || ((b.srcType == (int) MidiSrc::CC || b.srcType == (int) MidiSrc::CC14) && reservedCc (b.srcNum))
        || ! std::isfinite (b.inMin) || ! std::isfinite (b.inMax) || b.inMax <= b.inMin
        || ! std::isfinite (b.outMin) || ! std::isfinite (b.outMax)
        || (b.target >= kTargetBoardBase && b.target >= kTargetBoardBase + 16)) return false;
    if (b.srcType == (int) MidiSrc::CC14 && b.srcNum >= 32) return false;
    if (b.has (BF_HiRes) && b.srcType == (int) MidiSrc::CC && b.srcNum >= 32) return false;
    bindings[(size_t) numBindings] = b;
    if (b.has (BF_HiRes) && b.srcType == (int) MidiSrc::CC) bindings[(size_t) numBindings].srcType = (uint8_t) MidiSrc::CC14;
    layers[(size_t) numBindings++] = std::min (layer, uint8_t (2)); return true;
}
void MapSnap::compile() noexcept
{
    numBindings = std::clamp (numBindings, 0, kMaxBindings);
    // Stable insertion keeps descriptor-derived initial values/steps paired.
    for (int i = 1; i < numBindings; ++i)
        for (int j = i; j > 0 && bindings[(size_t) j].id < bindings[(size_t) j - 1].id; --j)
        {
            std::swap (bindings[(size_t) j], bindings[(size_t) j - 1]);
            std::swap (initialValues[(size_t) j], initialValues[(size_t) j - 1]);
            std::swap (steps[(size_t) j], steps[(size_t) j - 1]);
            std::swap (layers[(size_t) j], layers[(size_t) j - 1]);
            std::swap (customPoints[(size_t) j], customPoints[(size_t) j - 1]);
        }
    for (auto& bucket : lookup) bucket.count = 0;
    for (int i = 0; i < numBindings; ++i)
    {
        auto& b = bindings[(size_t) i];
        buildCurve (curves[(size_t) i], b.curve(), b.curveParam, &customPoints[(size_t) i]);
    }
    // Exact channel bindings claim a source before omni, regardless of id.
    for (int layer = 0; layer <= 2; ++layer)
    for (int exact = 1; exact >= 0; --exact)
        for (int i = 0; i < numBindings; ++i)
        {
            const auto& b = bindings[(size_t) i];
            if (layers[(size_t) i] != layer || ! b.has (BF_Enabled) || (b.chan != 0) != (exact != 0)) continue;
            for (int channel = 1; channel <= 16; ++channel)
            {
                if (b.chan != 0 && b.chan != channel) continue;
                auto& bucket = lookup[((size_t) b.srcType * 16 + (size_t) channel - 1) * 128 + b.srcNum];
                if (bucket.count > 0 && (layers[bucket.index[0]] < layer || layer > 0)) continue;
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
