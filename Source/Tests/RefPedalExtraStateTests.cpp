// OWNER: WP2
#include "TestFramework.h"
#include <utility>
#include <stdexcept>
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalPool.h"
#include "../ReferencePedals/PedalRegistry.h"
using namespace luthier::reference;
namespace
{
const PedalDesc exactExtraDesc { "test.exact-extra", "exact_extra_", 1, "Exact extra fixture", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 0 };
bool failNextPrepare = false;
class ExactExtraPedal final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return exactExtraDesc; }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void prepare (double, int, int) override
    { if (std::exchange (failNextPrepare, false)) throw std::runtime_error ("injected prepare failure"); }
    void reset() noexcept override {}
    void setParam (int, float) noexcept override {} float getParam (int) const noexcept override { return 0; }
    void process (double*, double*, int) noexcept override {}
    void loadExtra (const juce::var& extra) override { live.store (static_cast<double> (extra["vendor"])); }
    void saveExtra (juce::var& extra) const override
    { extra = new juce::DynamicObject(); extra.getDynamicObject()->setProperty ("vendor", live.load()); }
    void publishValue (double value) noexcept { live.store (value); }
private:
    std::atomic<double> live { 0 };
};
ExactExtraPedal* latestExtraPedal = nullptr;
std::unique_ptr<IPedal> makeExactExtra()
{ auto result = std::make_unique<ExactExtraPedal>(); latestExtraPedal = result.get(); return result; }
PedalInstance exactInstance (double value)
{
    PedalInstance p; p.modelId = exactExtraDesc.modelId; p.slot = 0; p.extras = new juce::DynamicObject();
    p.extras.getDynamicObject()->setProperty ("vendor", value); return p;
}
}
LUTHIER_TEST (RefPedalExtraState, tinyVendorChangesNeverReuseOldRuntimeOrOverlayOldState)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (exactExtraDesc, makeExactExtra, &fixedPreparedMemory<ExactExtraPedal>));
    const double changedValue = std::nextafter (1.e-200, 1.0);
    auto first = exactInstance (1.e-200), second = exactInstance (changedValue); second.instanceId = first.instanceId;
    CHECK (static_cast<double> (first.extras["vendor"]) == 1.e-200);
    CHECK (static_cast<double> (second.extras["vendor"]) == changedValue);
    // This is the original lossy-key collision, not a decimal tolerance test.
    CHECK (juce::JSON::toString (first.extras) == juce::JSON::toString (second.extras));
    PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (48000, 64); juce::String error;
    auto a = pool.acquire (first, error); CHECK (a != nullptr); if (! a) return;
    auto b = pool.acquire (second, error, a.get()); CHECK (b != nullptr); if (! b) return;
    CHECK (a != b); juce::var extra; b->pedal->saveExtra (extra); CHECK (static_cast<double> (extra["vendor"]) == changedValue);
    Board board; CHECK (board.setAssetStore (nullptr)); BoardSettings settings;
    CHECK (board.model().replaceAll ({ first }, settings));
    CHECK (board.model().replaceAll ({ second }, settings));
    std::vector<PedalInstance> captured; board.model().snapshot (captured, settings);
    CHECK (captured.size() == 1); if (captured.size() != 1) return;
    CHECK (static_cast<double> (captured[0].extras["vendor"]) == changedValue);
}

LUTHIER_TEST (RefPedalExtraState, changedSourceAfterFailedPrepareWinsOverCapturedRetryState)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (exactExtraDesc, makeExactExtra, &fixedPreparedMemory<ExactExtraPedal>));
    Board board; CHECK (board.setAssetStore (nullptr)); board.prepare (48000, 64);
    auto first = exactInstance (1.0), second = exactInstance (2.0); second.instanceId = first.instanceId;
    BoardSettings settings; CHECK (board.model().replaceAll ({ first }, settings));
    failNextPrepare = true; board.prepare (96000, 64); CHECK (board.readMeters().preparationFailed);
    CHECK (board.model().replaceAll ({ second }, settings)); CHECK (! board.readMeters().preparationFailed);
    std::vector<PedalInstance> captured; board.model().snapshot (captured, settings);
    CHECK (captured.size() == 1); if (captured.size() != 1) return;
    CHECK (static_cast<double> (captured[0].extras["vendor"]) == 2.0);
}

LUTHIER_TEST (RefPedalExtraState, liveAdjacentDoubleCaptureAndExplicitRestoreUseExactAssignment)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (exactExtraDesc, makeExactExtra, &fixedPreparedMemory<ExactExtraPedal>));
    const double changed = std::nextafter (1.e-200, 1.0);
    auto p = exactInstance (1.e-200); PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (48000, 64);
    juce::String error; auto first = pool.acquire (p, error); CHECK (first != nullptr); if (! first) return;
    static_cast<ExactExtraPedal*> (first->pedal.get())->publishValue (changed);
    auto restored = pool.acquire (p, error, first.get(), true); CHECK (restored != nullptr); if (! restored) return;
    CHECK (restored != first); juce::var extra; restored->pedal->saveExtra (extra);
    CHECK (static_cast<double> (extra["vendor"]) == 1.e-200);
    Board board; CHECK (board.setAssetStore (nullptr)); BoardSettings settings;
    CHECK (board.model().replaceAll ({ p }, settings)); latestExtraPedal->publishValue (changed);
    std::vector<PedalInstance> captured; board.model().snapshot (captured, settings);
    CHECK (captured.size() == 1); if (captured.size() != 1) return;
    CHECK (static_cast<double> (captured[0].extras["vendor"]) == changed);
}
