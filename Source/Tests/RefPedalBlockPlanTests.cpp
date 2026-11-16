#include "TestFramework.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/PreparedMemory.h"

using namespace luthier::tests;
using namespace luthier::reference;

namespace
{
int samplesAdvanced = 0;
const PedalDesc description {"test.blockplan", "test_blockplan_", 1, "Block plan probe", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 0};
class BlockProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return description; }
    void prepare (double, int, int) override {}
    void reset() noexcept override {}
    void setParam (int, float) noexcept override {}
    float getParam (int) const noexcept override { return 0; }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void process (double*, double*, int n) noexcept override { samplesAdvanced += n; }
};
std::unique_ptr<IPedal> makeProbe() { return std::make_unique<BlockProbe>(); }
}

LUTHIER_TEST (RefPedalBlockPlan, placementMoveBetweenCallsCannotAdvanceOnePedalTwice)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (description, makeProbe, &fixedPreparedMemory<BlockProbe>));
    BoardModel model;
    BoardEngine engine (model);
    CHECK (engine.setAssetStore (nullptr));
    engine.prepare (48000.0, 64);
    const InstanceId id;
    CHECK (model.apply (cmd::Add {description.modelId, -1, Placement::postCab, 0, id}));
    CHECK (model.apply (cmd::SetBoardEnabled {true}));
    Placement placement = Placement::postCab;
    int maximumSamples = 0, silentBlocks = 0, acceptedMoves = 0;
    for (int block = 0; block < 1024; ++block)
    {
        double left[64] {}, right[64] {};
        midi::RouteOut events;
        engine.beginEvents (events);
        samplesAdvanced = 0;
        engine.process (left, right, 64, Placement::beforeExistingPre);
        // Simulate a publisher between placements. The shared pedal runtime
        // must still see one block of time across all three placement calls.
        if (block >= 32 && block % 32 == 0)
        {
            placement = placement == Placement::postCab ? Placement::beforeExistingPre : Placement::postCab;
            const bool accepted = model.apply (cmd::SetPlacement {id, placement});
            CHECK (accepted);
            acceptedMoves += accepted ? 1 : 0;
            engine.reclaim();
        }
        engine.process (left, right, 64, Placement::afterExistingPost);
        engine.process (left, right, 64, Placement::postCab);
        engine.endEvents();
        engine.reclaim();
        if (block >= 32)
        {
            maximumSamples = std::max (maximumSamples, samplesAdvanced);
            if (samplesAdvanced == 0) ++silentBlocks;
        }
    }
    CHECK (acceptedMoves == 31);
    CHECK_MSG (maximumSamples <= 64, "one runtime advanced " + juce::String (maximumSamples) + " samples in a 64-sample block");
    CHECK (silentBlocks == 0);
    CHECK (engine.retiredPlanHighWater() <= 8);
}
