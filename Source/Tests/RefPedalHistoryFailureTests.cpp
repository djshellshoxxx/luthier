// OWNER: WP2
#include "TestFramework.h"
#include "AllocationFailureTestHook.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardState.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <array>
#include <new>
using namespace luthier::reference;
using namespace luthier::tests;
namespace
{
const PedalDesc historyFailureDesc { "test.history_failure", "history_failure_", 1, "History failure probe", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 0, false, false, false, 0 };
class HistoryFailurePedal final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return historyFailureDesc; }
    void prepare (double, int, int) override {}
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void reset() noexcept override {}
    void setParam (int, float) noexcept override {} float getParam (int) const noexcept override { return 0; }
    void process (double* l, double* r, int n) noexcept override
    { for (int i = 0; i < n; ++i) { l[i] *= .25; r[i] *= .25; } }
};
std::unique_ptr<IPedal> makeHistoryFailurePedal() { return std::make_unique<HistoryFailurePedal>(); }
enum class Operation { Apply, Undo, Redo, Snapshot, Replace };
struct Fixture
{
    Board board;
    bool ready = false;
    juce::var preset;
    std::vector<PedalInstance> replacement;
    BoardSettings replacementSettings;
    explicit Fixture (Operation operation)
    {
        board.setAssetStore (nullptr); board.prepare (48000, 64);
        PedalInstance p; p.modelId = historyFailureDesc.modelId; p.slot = 0;
        BoardSettings settings; settings.enabled = true; settings.limiterOn = false; settings.mixLaw = MixLaw::Linear;
        ready = board.model().replaceAll ({ p }, settings)
            && board.model().apply (cmd::SetMix { .25f, 0 }) && board.model().apply (cmd::SetMix { .5f, 0 });
        if (operation != Operation::Undo) ready = ready && board.model().undo();
        render(); board.reclaim();
        if (operation == Operation::Snapshot || operation == Operation::Replace)
        {
            board.model().snapshot (replacement, replacementSettings); replacementSettings.mix = .75f;
            replacementSettings.ui = new juce::DynamicObject();
            replacementSettings.ui.getDynamicObject()->setProperty ("vendor", "preserved history allocation fixture");
            ready = ready && BoardState::encode (replacement, replacementSettings, preset);
        }
    }
    bool operate (Operation operation)
    {
        if (operation == Operation::Apply) return board.model().apply (cmd::SetMix { .75f, 0 });
        if (operation == Operation::Snapshot) return board.model().apply (cmd::ApplySnapshot { 0, preset });
        if (operation == Operation::Replace) return board.model().replaceAll (replacement, replacementSettings);
        return operation == Operation::Undo ? board.model().undo() : board.model().redo();
    }
    double render()
    {
        double l[64], r[64];
        // Settle the existing20ms mix slew, preserving the exact steady-state oracle.
        for (int block = 0; block < 512; ++block)
        { std::fill_n (l, 64, 1.); std::fill_n (r, 64, 1.); board.processStereo (l, r, 64, Placement::beforeExistingPre); }
        return l[63];
    }
};
void checkOperation (TestContext& ctx, Operation operation)
{
    long count = 0;
    { Fixture warm (operation); CHECK (warm.ready); if (! warm.ready) return;
      const auto before = allocationsOnThisThread(); const bool accepted = warm.operate (operation); count = allocationsOnThisThread() - before;
      CHECK (accepted); }
    CHECK (count > 0 && count <= 2048); if (count <= 0 || count > 2048) return;
    int triggered = 0, thrown = 0;
    for (long ordinal = 0; ordinal <= count; ++ordinal)
    {
        Fixture fixture (operation); CHECK (fixture.ready); if (! fixture.ready) return;
        auto& model = fixture.board.model(); const auto version = model.getTopologyInfo().version;
        const float initial = operation == Operation::Undo ? .5f : .25f;
        const float desired = operation == Operation::Undo ? .25f : operation == Operation::Redo ? .5f : .75f;
        bool accepted = false, caught = false;
        // JUCE var::clone is noexcept despite allocating; arbitrary failure in
        // that dependency terminates. Snapshot tests target recoverable history
        // vector copies, not a universal JSON allocation-failure guarantee.
        const bool vectorOnly = operation == Operation::Snapshot || operation == Operation::Replace;
        if (vectorOnly) failMatchingAllocationAfterOnThisThread (sizeof (PedalInstance), static_cast<size_t> (ordinal));
        else failAllocationAfterOnThisThread (static_cast<size_t> (ordinal));
        try { accepted = fixture.operate (operation); } catch (const std::bad_alloc&) { caught = true; }
        const bool failed = allocationFailureTriggeredOnThisThread(); clearAllocationFailureOnThisThread();
        triggered += failed ? 1 : 0; thrown += caught ? 1 : 0;
        const float actual = model.getBoardSettings().mix;
        const bool committed = actual == desired;
        CHECK (committed || actual == initial);
        CHECK (! accepted || committed);
        CHECK (accepted || caught || ! committed);
        CHECK (model.getTopologyInfo().version == version + (committed ? 1u : 0u));
        CHECK_NEAR (fixture.render(), 1. - .75 * actual, 1.e-9);
        fixture.board.reclaim();
        // Verify the complete observable undo and redo chains, not just flags.
        std::vector<float> undo, redo;
        while (model.canUndo() && undo.size() < 8) { CHECK (model.undo()); undo.push_back (model.getBoardSettings().mix); }
        while (model.canRedo() && redo.size() < 8) { CHECK (model.redo()); redo.push_back (model.getBoardSettings().mix); }
        std::vector<float> expectedUndo, expectedRedo;
        if (operation == Operation::Replace && committed) { /* whole restore clears both chains */ }
        else if ((operation == Operation::Apply || operation == Operation::Snapshot) && committed) { expectedUndo = { .25f, 1.f }; expectedRedo = { .25f, .75f }; }
        else if ((operation == Operation::Undo && ! committed) || (operation == Operation::Redo && committed))
        { expectedUndo = { .25f, 1.f }; expectedRedo = { .25f, .5f }; }
        else { expectedUndo = { 1.f }; expectedRedo = { .25f, .5f }; }
        CHECK (undo == expectedUndo); CHECK (redo == expectedRedo);
        if (vectorOnly && ! failed) break;
    }
    CHECK (triggered > 0);
    std::cout << "history fault op=" << static_cast<int> (operation) << " sites=" << count << " triggered=" << triggered << " exceptions=" << thrown << "\n";
}
}
LUTHIER_TEST (RefPedalHistoryFailure, applyPublicationAndHistoryStayAtomicAtEveryAllocation)
{ PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>)); checkOperation (ctx, Operation::Apply); }
LUTHIER_TEST (RefPedalHistoryFailure, undoPublicationAndHistoryStayAtomicAtEveryAllocation)
{ PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>)); checkOperation (ctx, Operation::Undo); }
LUTHIER_TEST (RefPedalHistoryFailure, redoPublicationAndHistoryStayAtomicAtEveryAllocation)
{ PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>)); checkOperation (ctx, Operation::Redo); }

LUTHIER_TEST (RefPedalHistoryFailure, snapshotRuntimeHistoryVectorCopiesPrecedePublication)
{ PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>)); checkOperation (ctx, Operation::Snapshot); }
LUTHIER_TEST (RefPedalHistoryFailure, wholeRestoreVectorCopiesPrecedePublication)
{ PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>)); checkOperation (ctx, Operation::Replace); }
LUTHIER_TEST (RefPedalHistoryFailure, transactionSetupFailurePreservesDepthAndPriorRedo)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>));
    long sites = 0;
    { Fixture fixture (Operation::Apply); CHECK (fixture.ready); const auto before = allocationsOnThisThread();
      fixture.board.model().beginTransaction ("test"); sites = allocationsOnThisThread() - before; CHECK (fixture.board.model().cancelTransaction()); }
    std::cout << "history auxiliary fault sites=" << sites << "\n";
    CHECK (sites > 0 && sites < 128); if (sites <= 0 || sites >= 128) return;
    for (long ordinal = 0; ordinal <= sites; ++ordinal)
    {
        Fixture fixture (Operation::Apply); CHECK (fixture.ready); auto& model = fixture.board.model(); bool caught = false;
        failAllocationAfterOnThisThread (static_cast<size_t> (ordinal));
        try { model.beginTransaction ("test"); } catch (const std::bad_alloc&) { caught = true; }
        clearAllocationFailureOnThisThread();
        if (caught) { CHECK (model.canUndo()); CHECK (model.canRedo()); CHECK (! model.cancelTransaction()); }
        else { CHECK (! model.canUndo()); CHECK (! model.canRedo()); model.beginTransaction ("nested"); CHECK (model.cancelTransaction()); }
        CHECK_NEAR (model.getBoardSettings().mix, .25, 1.e-9);
        CHECK (model.undo()); CHECK_NEAR (model.getBoardSettings().mix, 1., 1.e-9);
        CHECK (model.redo()); CHECK_NEAR (model.getBoardSettings().mix, .25, 1.e-9);
        CHECK (model.redo()); CHECK_NEAR (model.getBoardSettings().mix, .5, 1.e-9);
    }
}
LUTHIER_TEST (RefPedalHistoryFailure, failedEditAtHundredEntriesPreservesOldestUndoAndGesture)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (historyFailureDesc, makeHistoryFailurePedal, &fixedPreparedMemory<HistoryFailurePedal>));
    // A model-only fixture targets history capacity independently of compiler allocations.
    long sites = 0;
    {
        BoardModel model; PedalInstance p; p.modelId = historyFailureDesc.modelId; p.slot = 0; BoardSettings settings;
        CHECK (model.replaceAll ({ p }, settings));
        for (int i = 1; i <= 100; ++i) CHECK (model.apply (cmd::SetMix { i / 128.f, 0 }));
        const auto before = allocationsOnThisThread(); const bool accepted = model.apply (cmd::SetMix { .9f, 777 });
        sites = allocationsOnThisThread() - before; CHECK (accepted);
    }
    std::cout << "history auxiliary fault sites=" << sites << "\n";
    CHECK (sites > 0 && sites < 128); if (sites <= 0 || sites >= 128) return;
    bool sawFailure = false, sawSuccess = false;
    for (size_t ordinal = 0; ordinal <= static_cast<size_t> (sites); ++ordinal)
    {
        BoardModel model; PedalInstance p; p.modelId = historyFailureDesc.modelId; p.slot = 0; BoardSettings settings;
        CHECK (model.replaceAll ({ p }, settings));
        for (int i = 1; i <= 100; ++i) CHECK (model.apply (cmd::SetMix { i / 128.f, 0 }));
        bool accepted = false;
        failAllocationAfterOnThisThread (ordinal);
        try { accepted = model.apply (cmd::SetMix { .9f, 777 }); } catch (const std::bad_alloc&) {}
        clearAllocationFailureOnThisThread();
        sawFailure = sawFailure || ! accepted; sawSuccess = sawSuccess || accepted;
        if (accepted) CHECK (model.apply (cmd::SetMix { .95f, 777 })); // same gesture still coalesces
        int count = 0; while (model.undo()) { ++count; if (count > 100) break; }
        CHECK (count == 100);
        CHECK_NEAR (model.getBoardSettings().mix, accepted ? 1.f / 128.f : 1.f, 1.e-9);
    }
    CHECK (sawFailure); CHECK (sawSuccess);
}
