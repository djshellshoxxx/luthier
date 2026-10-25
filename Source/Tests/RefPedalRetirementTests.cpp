// OWNER: WP2
#include "TestFramework.h"
#include "AllocationFailureTestHook.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/Rt/RetireList.h"
#if __has_include("../ReferencePedals/PreparedMemory.h")
#include "../ReferencePedals/PreparedMemory.h"
#define LUTHIER_RETIRE_MEMORY_CONTRACT 1
#endif
#include <array>
#include <new>
using namespace luthier::reference;
using namespace luthier::tests;
namespace
{
struct RetirementEntryLayout { std::unique_ptr<int> object; uint32_t version; };
const PedalDesc retirementDesc { "test.retirement", "retirement_", 1, "Retirement fixture", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 7, false, false, false, 0 };
class RetirementPedal final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return retirementDesc; }
    void prepare (double, int, int) override { reset(); }
    void reset() noexcept override { left.fill (0); right.fill (0); head = 0; }
    void setParam (int, float) noexcept override {} float getParam (int) const noexcept override { return 0; }
    int getLatencySamples() const noexcept override { return 7; }
#if defined(LUTHIER_RETIRE_MEMORY_CONTRACT)
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
#endif
    void process (double* l, double* r, int n) noexcept override
    {
        for (int i = 0; i < n; ++i)
        { const auto a = left[head], b = right[head]; left[head] = l[i]; right[head] = r[i]; l[i] = a; r[i] = b; if (++head == 7) head = 0; }
    }
private:
    std::array<double, 7> left {}, right {}; size_t head = 0;
};
std::unique_ptr<IPedal> makeRetirementPedal() { return std::make_unique<RetirementPedal>(); }
}
LUTHIER_TEST (RefPedalRetirement, failedGrowthPreservesCallerOwnershipAndBorrowedPointer)
{
    struct Probe { int& destroyed; int value = 73; ~Probe() { ++destroyed; } };
    int destroyed = 0; RetireList<Probe> list; auto owner = std::make_unique<Probe> (Probe { destroyed });
    destroyed = 0; auto* borrowed = owner.get(); bool threw = false;
    failNextAllocationOfSizeOnThisThread (sizeof (RetirementEntryLayout));
    try { list.retire (std::move (owner), 7); } catch (const std::bad_alloc&) { threw = true; }
    clearAllocationFailureOnThisThread();
    CHECK (allocationFailureTriggeredOnThisThread()); CHECK (threw);
    CHECK (owner.get() == borrowed); CHECK (destroyed == 0); CHECK (list.size() == 0);
    if (owner) CHECK (borrowed->value == 73);
}
LUTHIER_TEST (RefPedalRetirement, failedReservationDoesNotPublishOrChangeModelHistory)
{
    PedalRegistry::TestScope scope;
#if defined(LUTHIER_RETIRE_MEMORY_CONTRACT)
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal, &fixedPreparedMemory<RetirementPedal>));
#else
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal));
#endif
    Board board; CHECK (board.setAssetStore (nullptr)); board.prepare (48000, 64);
    PedalInstance instance; instance.modelId = retirementDesc.modelId; instance.slot = 0;
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false; settings.mixLaw = MixLaw::Linear;
    const bool ready = board.model().replaceAll ({ instance }, settings); CHECK (ready); if (! ready) return;
    // Keep32 retired plans without reclamation. Adopt the newest plan on audio
    // immediately before forcing growth32->64; it is now genuinely borrowed.
    for (int i = 0; i < 30; ++i) CHECK (board.model().apply (cmd::SetMix { (i + 1) / 40.f, 0 }));
    CHECK (board.model().apply (cmd::SetStyle { BoardStyle::velcro }));
    double l[4096] {}, r[4096] {}; board.processStereo (l, r, 4096, Placement::beforeExistingPre);
    CHECK (board.getLatencySamples() == 7);
    const auto version = board.model().getTopologyInfo().version;
    juce::String error; bool accepted = false, threw = false;
    failNextAllocationOfSizeOnThisThread (64 * sizeof (RetirementEntryLayout));
    try { accepted = board.model().apply (cmd::SetBoardParam { "reportLatency", 0 }, &error); }
    catch (const std::bad_alloc&) { threw = true; }
    clearAllocationFailureOnThisThread();
    CHECK (allocationFailureTriggeredOnThisThread()); CHECK (! threw); CHECK (! accepted);
    CHECK_MSG (error.containsIgnoreCase ("retirement"), error);
    CHECK (board.model().getBoardSettings().reportLatency);
    CHECK (board.model().getTopologyInfo().version == version);
    CHECK (board.getLatencySamples() == 7);
    // Skip dereferencing the old audio plan if the unfixed implementation has
    // already published/freed it; the preceding checks still fail explicitly.
    if (threw || board.getLatencySamples() != 7) return;
    std::fill_n (l, 4096, .25); std::fill_n (r, 4096, -.25);
    const auto before = allocationsOnThisThread(); board.processStereo (l, r, 4096, Placement::beforeExistingPre);
    CHECK (allocationsOnThisThread() == before); CHECK_NEAR (l[4095], .25, 1.e-12); CHECK_NEAR (r[4095], -.25, 1.e-12);
    CHECK (board.model().undo()); CHECK (board.model().getBoardSettings().style == settings.style);
    CHECK (board.model().getBoardSettings().reportLatency);
}
