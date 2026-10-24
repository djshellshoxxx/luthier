// OWNER: WP2
#include "TestFramework.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalPool.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <vector>
using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
namespace
{
const ParamDesc memoryParams[] {
    { "test_ram_size", "Large", "", ParamKind::Toggle, Taper::Linear, 0, 1, 0, 1, 2, nullptr, PF_Config, 0, -1 },
    { "test_ram_fail", "Fail", "", ParamKind::Toggle, Taper::Linear, 0, 1, 0, 1, 2, nullptr, PF_Config, 0, -1 },
    { "test_ram_underreport", "Underreport", "", ParamKind::Toggle, Taper::Linear, 0, 1, 0, 1, 2, nullptr, PF_Config, 0, -1 },
    { "test_ram_unknown", "Unknown report", "", ParamKind::Toggle, Taper::Linear, 0, 1, 0, 1, 2, nullptr, PF_Config, 0, -1 }
};
const PedalDesc memoryDesc { "test.ram", "test_ram_", 1, "Prepared memory fixture", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 4, memoryParams, 0, false, false, false, 0 };
int factories = 0, prepared = 0, liveMemoryPedals = 0;
class MemoryPedal final : public IPedal
{
public:
    MemoryPedal() { ++factories; ++liveMemoryPedals; }
    ~MemoryPedal() override { --liveMemoryPedals; }
    const PedalDesc& desc() const noexcept override { return memoryDesc; }
    static uint64_t payload (double sr, const float* params) noexcept
    {
        if (! std::isfinite (sr) || sr < 8000 || sr > 192000) return kUnknownPreparedMemoryBytes;
        return static_cast<uint64_t> ((params[0] >= 0.5f ? 32000000.0 : 4000000.0) * sr / 48000.0);
    }
    static PreparedMemoryRequirement estimate (double sr, int, int, const float* params) noexcept
    {
        auto bytes = payload (sr, params); uint64_t total;
        if (! addPreparedBytes (sizeof (MemoryPedal), bytes, total)) return {};
        return { total - (params[2] >= 0.5f ? 1u : 0u), 256 };
    }
    uint64_t getPreparedMemoryBytes() const noexcept override
    { return values[3] >= 0.5f ? kUnknownPreparedMemoryBytes : sizeof (*this) + storage.capacity(); }
    void prepare (double sr, int, int) override
    {
        ++prepared;
        storage.assign (static_cast<size_t> (payload (sr, values)), 0);
        std::vector<uint8_t> scratch (256);
        if (values[1] >= 0.5f) throw std::runtime_error ("injected memory prepare failure");
    }
    void loadExtra (const juce::var& extra) override { marker = static_cast<int> (extra["marker"]); }
    void saveExtra (juce::var& extra) const override
    { if (! extra.isObject()) extra = new juce::DynamicObject(); extra.getDynamicObject()->setProperty ("marker", marker); }
    void reset() noexcept override {}
    void setParam (int p, float v) noexcept override { values[p] = v; }
    float getParam (int p) const noexcept override { return values[p]; }
    void process (double* l, double* r, int n) noexcept override
    { for (int i = 0; i < n; ++i) { l[i] *= 0.5; r[i] *= 0.5; } }
private:
    int marker = 0;
    float values[4] {};
    std::vector<uint8_t> storage;
};
std::unique_ptr<IPedal> makeMemoryPedal() { return std::make_unique<MemoryPedal>(); }
PreparedMemoryRequirement overflowEstimate (double, int, int, const float*) noexcept { return { UINT64_MAX - 1, 8 }; }
PreparedMemoryRequirement excessiveScratch (double, int, int, const float*) noexcept { return { sizeof (MemoryPedal), kPreparedPedalMemoryLimit }; }
PedalInstance instance() { PedalInstance p; p.modelId = memoryDesc.modelId; p.slot = 0; return p; }
}
LUTHIER_TEST (RefPedalMemory, unsupportedOverflowAndPeakRefuseBeforeFactory)
{
    for (auto estimate : { PreparedMemoryEstimator (nullptr), &overflowEstimate, &excessiveScratch })
    {
        PedalRegistry::TestScope registry;
        CHECK (PedalRegistry::add (memoryDesc, makeMemoryPedal, estimate));
        PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (48000, 64);
        factories = prepared = 0; juce::String error;
        CHECK (! pool.acquire (instance(), error)); CHECK (error.isNotEmpty());
        CHECK (factories == 0 && prepared == 0); CHECK (pool.preparedBytes() == 0);
    }
}
LUTHIER_TEST (RefPedalMemory, actualCapacityFailureAndSharedRuntimeReservations)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (memoryDesc, makeMemoryPedal, &MemoryPedal::estimate));
    PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (48000, 64);
    auto p = instance(); juce::String error;
    auto first = pool.acquire (p, error); CHECK (first != nullptr); if (! first) return;
    const uint64_t retained = sizeof (PedalRuntime) + sizeof (MemoryPedal) + 4000000 + 2 * sizeof (double);
    CHECK (pool.preparedBytes() == retained); // temporary256B is refunded after prepare
    auto same = pool.acquire (p, error, first.get()); CHECK (same == first); CHECK (pool.preparedBytes() == retained);
    for (const int rejectParam : { 1, 2, 3 })
    {
        auto rejected = p; rejected.params[static_cast<size_t> (rejectParam)] = 1;
        CHECK (! pool.acquire (rejected, error, first.get())); CHECK (error.isNotEmpty());
        CHECK (pool.preparedBytes() == retained); CHECK (liveMemoryPedals == 1);
    }
    first.reset(); CHECK (pool.preparedBytes() == retained); same.reset(); CHECK (pool.preparedBytes() == 0);
    p.params[0] = 1; pool.prepare (192000, 2048); factories = prepared = 0;
    CHECK (! pool.acquire (p, error)); CHECK (factories == 0 && prepared == 0); CHECK (pool.preparedBytes() == 0);
    p.params[0] = 0;
    auto highRate = pool.acquire (p, error); CHECK (highRate != nullptr);
    CHECK (pool.preparedBytes() == retained + 12000000);
    highRate.reset(); CHECK (pool.preparedBytes() == 0);
}
LUTHIER_TEST (RefPedalMemory, rejectedConfigWaitsForRealAudioRetirementWithoutEviction)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (memoryDesc, makeMemoryPedal, &MemoryPedal::estimate));
    Board board; CHECK (board.setAssetStore (nullptr)); board.prepare (48000, 64);
    auto a = instance(), b = instance(); b.slot = 1; b.params[0] = 1;
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false;
    CHECK (board.model().replaceAll ({ a, b }, settings));
    double l[2048] {}, r[2048] {}; board.processStereo (l, r, 2048, Placement::beforeExistingPre); board.reclaim();
    CHECK (liveMemoryPedals == 2);
    CHECK (board.model().apply (cmd::Remove { b.instanceId }));
    const auto version = board.model().getTopologyInfo().version; juce::String error;
    factories = 0;
    CHECK (! board.model().apply (cmd::SetParam { a.instanceId, 0, 1 }, &error));
    CHECK (error.contains ("memory")); CHECK (factories == 0); CHECK (liveMemoryPedals == 2);
    CHECK (board.model().getTopologyInfo().version == version); CHECK (board.model().getInstance (a.instanceId)->params[0] == 0);
    std::fill_n (l, 64, 0.1); std::fill_n (r, 64, 0.1);
    const auto allocations = luthier::tests::allocationsOnThisThread();
    board.processStereo (l, r, 64, Placement::beforeExistingPre);
    CHECK (luthier::tests::allocationsOnThisThread() == allocations); CHECK (std::isfinite (l[63]));
    std::fill_n (l, 2048, 0.0); std::fill_n (r, 2048, 0.0); board.processStereo (l, r, 2048, Placement::beforeExistingPre); board.reclaim();
    CHECK (liveMemoryPedals == 1);
    CHECK (board.model().apply (cmd::SetParam { a.instanceId, 0, 1 }, &error));
    CHECK (board.model().undo()); CHECK (board.model().getInstance (a.instanceId)->params[0] == 0); // rejection added no history
}
LUTHIER_TEST (RefPedalMemory, failedRateChangeIsDryAndRecoversWithoutChangingStoredModel)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (memoryDesc, makeMemoryPedal, &MemoryPedal::estimate));
    Board board; CHECK (board.setAssetStore (nullptr)); board.prepare (48000, 64);
    auto p = instance(); p.params[0] = 1;
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false;
    CHECK (board.model().replaceAll ({ p }, settings));
    CHECK (board.model().apply (cmd::SetMix { 0.75f, 0 }));
    factories = prepared = 0;
    board.prepare (192000, 64);
    CHECK (factories == 0 && prepared == 0); // whole candidate rejected before any factory
    CHECK (board.readMeters().preparationFailed); CHECK (board.getPreparationError().contains ("memory"));
    CHECK (! board.isActive()); CHECK (board.getLatencySamples() == 0); CHECK (liveMemoryPedals == 0);
    CHECK (board.model().getInstance (p.instanceId)->params[0] == 1);
    CHECK (board.model().getBoardSettings().mix == 0.75f);
    double l[64], r[64]; std::fill_n (l, 64, 0.25); std::fill_n (r, 64, -0.5);
    const auto allocations = luthier::tests::allocationsOnThisThread();
    board.processStereo (l, r, 64, Placement::beforeExistingPre);
    CHECK (luthier::tests::allocationsOnThisThread() == allocations);
    for (int i = 0; i < 64; ++i) { CHECK (l[i] == 0.25); CHECK (r[i] == -0.5); }
    // Correcting the offending config recovers at the requested new rate.
    CHECK (board.model().apply (cmd::SetParam { p.instanceId, 0, 0 }));
    CHECK (! board.readMeters().preparationFailed); CHECK (board.getPreparationError().isEmpty());
    CHECK (board.readMeters().preparedMemoryBytes >= 16000000);
    CHECK (board.readMeters().routingScratchBytes > 0);
    // The failed prepare added no history: after undoing the correction at a
    // viable rate, the prior mix edit is still the next undo step.
    board.prepare (48000, 64);
    CHECK (board.model().undo()); CHECK (board.model().getInstance (p.instanceId)->params[0] == 1);
    CHECK (board.model().undo()); CHECK (board.model().getBoardSettings().mix == 1.f);
}

LUTHIER_TEST (RefPedalMemory, capturedExtrasSurviveRejectedCandidateAndExplicitRestoreWins)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (memoryDesc, makeMemoryPedal, &MemoryPedal::estimate));
    PedalPool pool; CHECK (pool.setAssetStore (nullptr));
    auto p = instance(), saved = p;
    saved.extras = new juce::DynamicObject(); saved.extras.getDynamicObject()->setProperty ("marker", 42);
    pool.prepare (48000, 64, { saved }); juce::String error;
    auto broken = p; broken.params[1] = 1;
    CHECK (! pool.acquire (broken, error)); CHECK (pool.preparedBytes() == 0);
    auto restored = pool.acquire (p, error); CHECK (restored != nullptr); if (! restored) return;
    juce::var extra; restored->pedal->saveExtra (extra); CHECK (static_cast<int> (extra["marker"]) == 42);
    restored.reset(); pool.prepare (48000, 64, { saved });
    p.extras = new juce::DynamicObject(); p.extras.getDynamicObject()->setProperty ("marker", 17);
    restored = pool.acquire (p, error, nullptr, true); CHECK (restored != nullptr); if (! restored) return;
    restored->pedal->saveExtra (extra); CHECK (static_cast<int> (extra["marker"]) == 17);
}
LUTHIER_TEST (RefPedalMemory, realTimeEffectBanksRefuseHighRatePeakAndRecoverAtLowerRate)
{
    PedalRegistry::registerAll();
    for (const auto& entry : { std::pair<const char*, int> { "generic.delay", 16 }, { "generic.reverb", 5 } })
    {
    Board board; CHECK (board.setAssetStore (nullptr)); board.prepare (48000, 64);
    std::vector<PedalInstance> instances;
    const auto* descriptor = PedalRegistry::find (entry.first); CHECK (descriptor != nullptr); if (! descriptor) return;
    for (int slot = 0; slot < entry.second; ++slot)
    {
        PedalInstance p; p.modelId = descriptor->modelId; p.slot = static_cast<uint8_t> (slot);
        for (int i = 0; i < descriptor->numParams; ++i) p.params[static_cast<size_t> (i)] = static_cast<float> (descriptor->params[i].defaultNormalised());
        instances.push_back (p);
    }
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false;
    juce::String error;
    const bool accepted = board.model().replaceAll (instances, settings, &error);
    CHECK_MSG (accepted, error); if (! accepted) return;
    CHECK (board.readMeters().preparedMemoryBytes > (entry.second == 16 ? 29000000 : 17000000));
    CHECK (board.readMeters().preparedMemoryBytes < kPreparedPedalMemoryLimit);
    board.prepare (192000, 64);
    CHECK (board.readMeters().preparationFailed); CHECK (board.readMeters().preparedMemoryBytes == 0);
    CHECK (board.model().getInstanceCount() == entry.second); CHECK (! board.isActive());
    board.prepare (48000, 64);
    CHECK (! board.readMeters().preparationFailed); CHECK (board.isActive());
    CHECK (board.readMeters().preparedMemoryBytes < kPreparedPedalMemoryLimit);
    }
}
