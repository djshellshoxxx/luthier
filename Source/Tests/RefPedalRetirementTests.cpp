// OWNER: WP2
#include "TestFramework.h"
#include "AllocationFailureTestHook.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardEngine.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/Rt/RetireList.h"
#if __has_include("../ReferencePedals/PreparedMemory.h")
#include "../ReferencePedals/PreparedMemory.h"
#define LUTHIER_RETIRE_MEMORY_CONTRACT 1
#endif
#include <array>
#include <new>
#include <thread>
#include <atomic>
#include <chrono>
using namespace luthier::reference;
using namespace luthier::tests;
namespace
{
std::atomic<bool> holdRetirementAudio { false }, retirementAudioEntered { false };
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
        if (holdRetirementAudio.load())
        { retirementAudioEntered.store (true); while (holdRetirementAudio.load()) std::this_thread::yield(); }
        for (int i = 0; i < n; ++i)
        { const auto a = left[head], b = right[head]; left[head] = l[i]; right[head] = r[i]; l[i] = a; r[i] = b; if (++head == 7) head = 0; }
    }
private:
    std::array<double, 7> left {}, right {}; size_t head = 0;
};
std::unique_ptr<IPedal> makeRetirementPedal() { return std::make_unique<RetirementPedal>(); }
struct HeldAudio
{
    std::thread thread;
    template <typename Fn> explicit HeldAudio (Fn fn)
    {
        retirementAudioEntered.store (false); holdRetirementAudio.store (true);
        thread = std::thread (fn);
        while (! retirementAudioEntered.load()) std::this_thread::yield();
    }
    void release() { holdRetirementAudio.store (false); if (thread.joinable()) thread.join(); }
    ~HeldAudio() { release(); }
};
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
    double l[4096] {}, r[4096] {};
    // Hold an actual borrowed plan while four replacements fill capacity4.
    // The fifth retirement must reserve8 before changing any visible state.
    HeldAudio audio ([&] { board.processStereo (l, r, 4096, Placement::beforeExistingPre); });
    for (int i = 0; i < 3; ++i) CHECK (board.model().apply (cmd::SetMix { (i + 1) / 40.f, 0 }));
    CHECK (board.model().apply (cmd::SetStyle { BoardStyle::velcro }));
    CHECK (board.getLatencySamples() == 7);
    const auto version = board.model().getTopologyInfo().version;
    juce::String error; bool accepted = false, threw = false;
    failNextAllocationOfSizeOnThisThread (8 * sizeof (RetirementEntryLayout));
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
    audio.release();
    if (threw || board.getLatencySamples() != 7) return;
    std::fill_n (l, 4096, .25); std::fill_n (r, 4096, -.25);
    const auto before = allocationsOnThisThread(); board.processStereo (l, r, 4096, Placement::beforeExistingPre);
    CHECK (allocationsOnThisThread() == before); CHECK_NEAR (l[4095], .25, 1.e-12); CHECK_NEAR (r[4095], -.25, 1.e-12);
    CHECK (board.model().undo()); CHECK (board.model().getBoardSettings().style == settings.style);
    CHECK (board.model().getBoardSettings().reportLatency);
}

LUTHIER_TEST (RefPedalRetirement, heldReaderSaturatesAtEightWithoutChangingHistoryAndRecovers)
{
    PedalRegistry::TestScope scope;
#if defined(LUTHIER_RETIRE_MEMORY_CONTRACT)
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal, &fixedPreparedMemory<RetirementPedal>));
#else
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal));
#endif
    BoardModel model; BoardEngine engine (model); CHECK (engine.setAssetStore (nullptr));
    PedalInstance instance; instance.modelId = retirementDesc.modelId; instance.slot = 0;
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false;
    const bool ready = model.replaceAll ({ instance }, settings); CHECK (ready); if (! ready) return;
    double l[4096] {}, r[4096] {};
    HeldAudio audio ([&] { engine.process (l, r, 4096, Placement::beforeExistingPre); });
    for (int i = 0; i < 8; ++i) CHECK (model.apply (cmd::SetMix { (i + 1) / 10.f, 0 }));
    CHECK (engine.retiredPlanCount() == 8); CHECK (engine.retiredPlanHighWater() == 8);
    const auto version = model.getTopologyInfo().version;
    juce::String error; CHECK (! model.apply (cmd::SetStyle { BoardStyle::velcro }, &error));
    CHECK_MSG (error.containsIgnoreCase ("saturated"), error);
    CHECK (model.getTopologyInfo().version == version); CHECK (model.getBoardSettings().style == settings.style);
    CHECK_NEAR (model.getBoardSettings().mix, .8, 1.e-6);
    audio.release(); engine.reclaim();
    CHECK (engine.retiredPlanCount() <= 1);
    CHECK (model.undo()); CHECK_NEAR (model.getBoardSettings().mix, .7, 1.e-6);
    CHECK (model.apply (cmd::SetStyle { BoardStyle::velcro }));
    CHECK (engine.retiredPlanHighWater() <= 8);
}
LUTHIER_TEST (RefPedalRetirement, stoppedPublicationStormCoalescesNeverAdoptedPlans)
{
    BoardModel model; BoardEngine engine (model); CHECK (engine.setAssetStore (nullptr));
    for (int i = 0; i < 1000; ++i)
    {
        CHECK (model.apply (cmd::SetMix { (i % 100) / 100.f, 0 }));
        CHECK (engine.retiredPlanCount() <= 1); CHECK (engine.retiredPlanHighWater() <= 8);
    }
    CHECK (model.undo()); CHECK_NEAR (model.getBoardSettings().mix, .98, 1.e-6);
}
LUTHIER_TEST (RefPedalRetirement, genericListRefusesNinthOwnerWithoutTransfer)
{
    RetireList<int> list;
    for (int i = 0; i < 8; ++i) { auto owner = std::make_unique<int> (i); list.retire (std::move (owner), 1); }
    auto ninth = std::make_unique<int> (9); auto* pointer = ninth.get(); bool rejected = false;
    try { list.retire (std::move (ninth), 1); } catch (const std::length_error&) { rejected = true; }
    CHECK (rejected); CHECK (ninth.get() == pointer); CHECK (list.size() == 8); CHECK (list.highWater() == 8);
}
LUTHIER_TEST (RefPedalRetirement, concurrentThreePlacementStormKeepsAudioAndBound)
{
    PedalRegistry::TestScope scope;
#if defined(LUTHIER_RETIRE_MEMORY_CONTRACT)
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal, &fixedPreparedMemory<RetirementPedal>));
#else
    CHECK (PedalRegistry::add (retirementDesc, makeRetirementPedal));
#endif
    BoardModel model; BoardEngine engine (model); CHECK (engine.setAssetStore (nullptr));
    std::vector<PedalInstance> instances (3);
    for (int i = 0; i < 3; ++i)
    { instances[i].modelId = retirementDesc.modelId; instances[i].slot = i; instances[i].placement = static_cast<Placement> (i); }
    BoardSettings settings; settings.enabled = true; settings.limiterOn = false; settings.mixLaw = MixLaw::Linear;
    const bool ready = model.replaceAll (instances, settings); CHECK (ready); if (! ready) return;
    std::atomic<bool> started { false }, stop { false }; std::atomic<int> failures { 0 };
    std::thread reader ([&]
    {
        double l[64] {}, r[64] {}; midi::MidiRouter router; router.prepare (48000, 64); midi::RouteOut events;
        started.store (true);
        while (! stop.load())
        {
            std::fill_n (l, 64, .25); std::fill_n (r, 64, -.25);
            const auto before = allocationsOnThisThread(); engine.syncRouting (router); engine.beginEvents (events);
            for (int p = 0; p < 3; ++p) engine.process (l, r, 64, static_cast<Placement> (p));
            engine.endEvents();
            if (before != allocationsOnThisThread()) ++failures;
            for (int i = 0; i < 64; ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) ++failures;
        }
    });
    while (! started.load()) std::this_thread::yield();
    // First a burst, then a full second at the original 1,000 attempts/s gate.
    // Saturation is an explicit permitted rejection, never silent publication.
    int acceptedCount = 0, rejectedCount = 0;
    auto pacedStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 2000; ++i)
    {
        if (i == 1000) pacedStart = std::chrono::steady_clock::now();
        if (i >= 1000) std::this_thread::sleep_until (pacedStart + std::chrono::microseconds (1000 * (i - 1000)));
        juce::String error; const auto version = model.getTopologyInfo().version;
        const bool accepted = i % 2 == 0
            ? model.apply (cmd::SetPlacement { instances[0].instanceId, static_cast<Placement> ((i / 2) % 3) }, &error)
            : model.apply (cmd::SetMix { (i % 100) / 100.f, 0 }, &error);
        if (accepted) ++acceptedCount; else ++rejectedCount;
        CHECK_MSG (accepted || error.containsIgnoreCase ("saturated"), error);
        if (! accepted) CHECK (model.getTopologyInfo().version == version);
        CHECK (engine.retiredPlanCount() <= 8); CHECK (engine.retiredPlanHighWater() <= 8);
    }
    stop.store (true); reader.join(); CHECK (failures.load() == 0);
    CHECK (acceptedCount > 0); CHECK (acceptedCount + rejectedCount == 2000);
    engine.reclaim(); CHECK (engine.retiredPlanCount() <= 6);
}
