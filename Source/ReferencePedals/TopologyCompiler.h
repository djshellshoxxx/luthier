// OWNER: WP2
#pragma once
#include "Topology.h"
#include "PedalPool.h"
namespace luthier::reference
{
struct SlotControl
{
    bool enabled = true;
    float inputGain = 1.f, outputGain = 1.f;
    std::array<float, kMaxParams> params {};
};
struct RenderPlan
{
    Topology topology;
    BoardSettings settings;
    std::array<SlotControl, kMaxSlots> controls;
    std::array<std::shared_ptr<PedalRuntime>, kMaxSlots> slots;
    std::array<int, kNumPlacements> latency {};
    // Only processing structure affects this number; controls adopt without a dip.
    uint32_t structure = 0;
};
class TopologyCompiler
{
public:
    static std::unique_ptr<RenderPlan> compile (const std::vector<PedalInstance>&,
        const BoardSettings&, PedalPool&, uint32_t version, const RenderPlan* previous, juce::String& error);
};
}
