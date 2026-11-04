// OWNER: WP2
#include "TestFramework.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/CpuBudget.h"
#include <limits>
using namespace luthier::reference;
namespace
{
const PedalDesc scaledCostDesc { "test.scaled_cost", "test_scaled_cost_", 1, "Milli-percent cost", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 300, 1000 };
const PedalDesc percentCostDesc { "test.percent_cost", "test_percent_cost_", 1, "Percent cost", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 1 };
// These tests exercise model admission without compiling or preparing a runtime.
std::unique_ptr<IPedal> unusedCostFactory() { return {}; }
}
LUTHIER_TEST (RefPedalCpuBudget, explicitScalesPreservePercentageLimits)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (scaledCostDesc, unusedCostFactory));
    CHECK (PedalRegistry::add (percentCostDesc, unusedCostFactory));
    PedalInstance a, b; a.modelId = scaledCostDesc.modelId; a.slot = 0; b.modelId = percentCostDesc.modelId; b.slot = 1;
    CHECK (estimatedCpuUnits ({ a }) == 300); CHECK (estimatedCpuUnits ({ b }) == 1000);
    BoardModel model; juce::String error;
    const bool added = model.apply (cmd::Add { scaledCostDesc.modelId, -1, Placement::beforeExistingPre, 0, a.instanceId }, &error);
    CHECK_MSG (added, error); if (! added) return;
    CHECK (model.apply (cmd::SetBoardParam { "cpuCapPct", 1 }));
    CHECK (! model.apply (cmd::Add { percentCostDesc.modelId, -1, Placement::beforeExistingPre, 0, b.instanceId }, &error));
    CHECK (model.getInstanceCount() == 1);
    CHECK (model.apply (cmd::SetBoardParam { "cpuCapPct", 2 }));
    CHECK (model.apply (cmd::Add { percentCostDesc.modelId, -1, Placement::beforeExistingPre, 0, b.instanceId }));
    CHECK (model.getInstanceCount() == 2);
    CHECK (estimatedCpuUnits ({ a, b }) == 1300);
    CHECK (! model.apply (cmd::SetBoardParam { "cpuCapPct", 1 }));
    CHECK (model.getBoardSettings().cpuCapPct == 2);
    auto invalid = scaledCostDesc; invalid.cpuUnitsPerPercent = 0;
    CHECK (! PedalRegistry::validate (invalid));
    CHECK (normalisedCpuUnits (invalid) == std::numeric_limits<uint64_t>::max());
    auto extreme = scaledCostDesc; extreme.cpuUnits = std::numeric_limits<uint32_t>::max(); extreme.cpuUnitsPerPercent = 1;
    CHECK (normalisedCpuUnits (extreme) == static_cast<uint64_t> (std::numeric_limits<uint32_t>::max()) * 1000);
    extreme.cpuUnits = 1; extreme.cpuUnitsPerPercent = std::numeric_limits<uint32_t>::max();
    CHECK (normalisedCpuUnits (extreme) == 1); // round upward, never silently undercharge
    extreme.cpuUnits = 0; CHECK (normalisedCpuUnits (extreme) == 0);
}
LUTHIER_TEST (RefPedalCpuBudget, everyRegisteredModelFitsItsDeclaredDefaultAdmissionBudget)
{
    PedalRegistry::registerAll();
    const auto descriptors = PedalRegistry::all();
    CHECK (! descriptors.empty());
    for (const auto* desc : descriptors)
    {
        BoardModel model; juce::String error;
        const bool added = model.apply (cmd::Add { desc->modelId, -1, Placement::beforeExistingPre, 0, InstanceId() }, &error);
        CHECK_MSG (added, juce::String (desc->modelId) + ": " + error);
        if (added) CHECK (model.getInstanceCount() == 1);
    }
}
