// BoardModel regression tests: message-thread state must not silently reject edits.
#include "TestFramework.h"
#include "../ReferencePedals/BoardModel.h"
#include <cmath>
#include <limits>
#include <utility>
using namespace luthier::reference;

LUTHIER_TEST (RefPedalBoardCore, boardSettingsCommandsRoundTrip)
{
    BoardModel model;
    CHECK (model.apply (cmd::SetBoardEnabled { true }));
    CHECK (model.getBoardSettings().enabled);
    CHECK (model.apply (cmd::SetMix { 0.25f, 0 }));
    CHECK (std::abs (model.getBoardSettings().mix - 0.25f) < 1.0e-6f);
    CHECK (model.apply (cmd::SetStyle { BoardStyle::rail }));
    CHECK (model.getBoardSettings().style == BoardStyle::rail);
    CHECK (model.apply (cmd::SetMode { Mode::Parallel }));
    CHECK (model.getBoardSettings().mode == Mode::Parallel);
}

LUTHIER_TEST (RefPedalBoardCore, invalidSettingsDoNotMutate)
{
    BoardModel model;
    const auto before = model.getBoardSettings();
    CHECK (! model.apply (cmd::SetMix { -0.1f, 0 }));
    CHECK (! model.apply (cmd::SetMix { std::numeric_limits<float>::quiet_NaN(), 0 }));
    CHECK (! model.apply (cmd::SetBoardParam { "unknown", 1.0 }));
    CHECK (model.getBoardSettings().mix == before.mix);
    CHECK (model.getBoardSettings().enabled == before.enabled);
}

LUTHIER_TEST (RefPedalBoardCore, stateSnapshotRoundTrip)
{
    BoardModel model;
    BoardSettings state;
    state.enabled = true;
    state.mix = 0.6f;
    state.style = BoardStyle::velcro;
    CHECK (model.replaceAll ({}, state));
    std::vector<PedalInstance> pedals;
    BoardSettings saved;
    model.snapshot (pedals, saved);
    CHECK (pedals.empty());
    CHECK (saved.enabled);
    CHECK (std::abs (saved.mix - 0.6f) < 1.0e-6f);
    CHECK (saved.style == BoardStyle::velcro);
}

LUTHIER_TEST (RefPedalBoardCore, instanceMoveRemoveAndUndo)
{
    BoardModel model;
    const InstanceId a { juce::Uuid() }, b { juce::Uuid() };
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, a }));
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, b }));
    CHECK (model.getInstanceCount() == 2);
    CHECK (model.apply (cmd::Move { a, 1 }));
    CHECK (model.getOrder()[1] == a);
    CHECK (model.undo());
    CHECK (model.getOrder()[0] == a);
    CHECK (model.redo());
    CHECK (model.getOrder()[1] == a);
    CHECK (model.apply (cmd::Remove { a }));
    CHECK (model.getInstanceCount() == 1);
}

LUTHIER_TEST (RefPedalBoardCore, duplicatePreservesControlsAndHasIndependentIdentity)
{
    BoardModel model;
    const InstanceId source, duplicate;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::postCab, 1, source }));
    CHECK (model.apply (cmd::SetParam { source, 2, 0.75f }));
    CHECK (model.apply (cmd::SetEnabled { source, false }));
    CHECK (model.apply (cmd::Duplicate { source, duplicate }));
    CHECK (model.getOrder().size() == 2);
    CHECK (model.getOrder().back() == duplicate);
    const auto copy = model.getInstance (duplicate);
    CHECK (copy.has_value());
    if (copy)
    {
        CHECK (copy->modelId == "generic.overdrive");
        CHECK (copy->placement == Placement::postCab);
        CHECK (copy->lane == 1);
        CHECK (! copy->enabled);
        CHECK_NEAR (copy->params[2], 0.75f, 1.e-6f);
    }
    CHECK (! model.apply (cmd::Duplicate { source, source }));
    CHECK (model.undo());
    CHECK (model.getInstanceCount() == 1);
    CHECK (model.redo());
    CHECK (model.getInstance (duplicate).has_value());
}

LUTHIER_TEST (RefPedalBoardCore, trimAndMountingValidateBeforeMutation)
{
    BoardModel model;
    const InstanceId id;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (model.apply (cmd::SetTrim { id, -12.f, 12.f }));
    CHECK_NEAR (model.getInstance (id)->inDb, -12.f, 1.e-6f);
    CHECK_NEAR (model.getInstance (id)->outDb, 12.f, 1.e-6f);
    CHECK (! model.apply (cmd::SetTrim { id, 13.f, 0.f }));
    CHECK (! model.apply (cmd::SetTrim { id, 0.f, std::numeric_limits<float>::quiet_NaN() }));
    CHECK_NEAR (model.getInstance (id)->inDb, -12.f, 1.e-6f);
    CHECK (model.apply (cmd::SetMounting { id, Mounting::clamps }));
    CHECK (model.getInstance (id)->mounting == Mounting::clamps);
    CHECK (! model.apply (cmd::SetMounting { id, static_cast<Mounting> (255) }));
    CHECK (model.getInstance (id)->mounting == Mounting::clamps);
}

LUTHIER_TEST (RefPedalBoardCore, transactionIsOneUndoStepAndRetainsEarlierHistory)
{
    BoardModel model;
    CHECK (model.apply (cmd::SetBoardEnabled { true }));
    model.beginTransaction ("Change appearance and mix");
    CHECK (model.apply (cmd::SetMix { 0.25f }));
    CHECK (model.apply (cmd::SetStyle { BoardStyle::ceramic }));
    model.endTransaction();
    CHECK (model.undo());
    CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-6f);
    CHECK (model.getBoardSettings().style == BoardStyle::plywood);
    CHECK (model.getBoardSettings().enabled);
    CHECK (model.undo());
    CHECK (! model.getBoardSettings().enabled);
    CHECK (model.redo());
    CHECK (model.redo());
    CHECK_NEAR (model.getBoardSettings().mix, 0.25f, 1.e-6f);
    CHECK (model.getBoardSettings().style == BoardStyle::ceramic);
}

LUTHIER_TEST (RefPedalBoardCore, emptyAndRejectedTransactionsDoNotConsumeUndo)
{
    BoardModel model;
    model.beginTransaction ("No valid edit");
    CHECK (! model.apply (cmd::SetMix { -1.f }));
    model.endTransaction();
    CHECK (! model.canUndo());
    CHECK (model.apply (cmd::SetMix { 0.5f }));
    CHECK (model.undo());
    model.beginTransaction ("Empty");
    model.endTransaction();
    CHECK (model.canRedo());
}

LUTHIER_TEST (RefPedalBoardCore, nestedTransactionsRemainOneStep)
{
    BoardModel model;
    model.beginTransaction ("outer");
    CHECK (model.apply (cmd::SetMix { 0.2f }));
    model.beginTransaction ("inner");
    CHECK (model.apply (cmd::SetStyle { BoardStyle::rail }));
    model.endTransaction();
    CHECK (model.apply (cmd::SetBoardEnabled { true }));
    model.endTransaction();
    CHECK (model.undo());
    CHECK (! model.canUndo());
    CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-6f);
    CHECK (! model.getBoardSettings().enabled);
    CHECK (model.getBoardSettings().style == BoardStyle::plywood);
}

LUTHIER_TEST (RefPedalBoardCore, undoDuringOpenTransactionWaitsUntilCommit)
{
    BoardModel model;
    CHECK (model.apply (cmd::SetBoardEnabled { true }));
    model.beginTransaction ("mix drag");
    CHECK (model.apply (cmd::SetMix { 0.4f }));
    CHECK (! model.undo());
    CHECK (! model.redo());
    model.endTransaction();
    CHECK (model.undo());
    CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-6f);
    CHECK (model.getBoardSettings().enabled);
}

LUTHIER_TEST (RefPedalBoardCore, consecutiveGestureWritesCoalesceByTarget)
{
    BoardModel model;
    const InstanceId id;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (model.apply (cmd::SetParam { id, 0, 0.2f, 7 }));
    CHECK (model.apply (cmd::SetParam { id, 0, 0.7f, 7 }));
    CHECK (model.apply (cmd::SetParam { id, 1, 0.8f, 7 }));
    CHECK (model.undo());
    CHECK_NEAR (model.getInstance (id)->params[1], 0.f, 1.e-6f);
    CHECK_NEAR (model.getInstance (id)->params[0], 0.7f, 1.e-6f);
    CHECK (model.undo());
    CHECK_NEAR (model.getInstance (id)->params[0], 0.f, 1.e-6f);
    CHECK (model.redo());
    CHECK_NEAR (model.getInstance (id)->params[0], 0.7f, 1.e-6f);
    // Reusing a gesture after navigating history starts a new undo group.
    CHECK (model.apply (cmd::SetParam { id, 0, 0.3f, 7 }));
    CHECK (model.undo());
    CHECK_NEAR (model.getInstance (id)->params[0], 0.7f, 1.e-6f);
}

LUTHIER_TEST (RefPedalBoardCore, mixAndTrimGesturesHaveIndependentHistory)
{
    BoardModel model;
    const InstanceId id;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (model.apply (cmd::SetMix { 0.2f, 9 }));
    CHECK (model.apply (cmd::SetMix { 0.4f, 9 }));
    CHECK (model.apply (cmd::SetTrim { id, -1.f, 1.f, 9 }));
    CHECK (model.apply (cmd::SetTrim { id, -2.f, 2.f, 9 }));
    CHECK (model.undo());
    CHECK_NEAR (model.getInstance (id)->inDb, 0.f, 1.e-6f);
    CHECK_NEAR (model.getBoardSettings().mix, 0.4f, 1.e-6f);
    CHECK (model.undo());
    CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-6f);
}

LUTHIER_TEST (RefPedalBoardCore, historyIsBoundedToOneHundredSteps)
{
    BoardModel model;
    for (int i = 0; i < 105; ++i)
        CHECK (model.apply (cmd::SetMix { static_cast<float> (i) / 105.f }));
    int steps = 0;
    while (model.undo()) ++steps;
    CHECK (steps == 100);
    CHECK_NEAR (model.getBoardSettings().mix, 4.f / 105.f, 1.e-6f);
    steps = 0;
    while (model.redo()) ++steps;
    CHECK (steps == 100);
    CHECK_NEAR (model.getBoardSettings().mix, 104.f / 105.f, 1.e-6f);
}

LUTHIER_TEST (RefPedalBoardCore, automationSlotsStayWithInstancesAcrossEdits)
{
    BoardModel model;
    const InstanceId first, second, duplicate, replacement;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, first }));
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, second }));
    CHECK (model.getInstance (first)->slot == 0);
    CHECK (model.getInstance (second)->slot == 1);
    CHECK (model.apply (cmd::Move { second, 0 }));
    CHECK (model.getInstance (first)->slot == 0);
    CHECK (model.getInstance (second)->slot == 1);
    CHECK (model.apply (cmd::Duplicate { first, duplicate }));
    CHECK (model.getInstance (duplicate)->slot == 2);
    CHECK (model.apply (cmd::Remove { second }));
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, replacement }));
    CHECK (model.getInstance (replacement)->slot == 1);
    CHECK (model.undo());
    CHECK (model.undo());
    CHECK (model.getInstance (second)->slot == 1);
    CHECK (model.getInstance (duplicate)->slot == 2);
}

LUTHIER_TEST (RefPedalBoardCore, restorePreservesSlotsAndAssignsUnspecifiedOnes)
{
    BoardModel model;
    PedalInstance first, second;
    first.modelId = second.modelId = "generic.overdrive";
    first.slot = -1;
    second.slot = 0;
    CHECK (model.replaceAll ({ first, second }, {}));
    CHECK (model.getInstance (first.instanceId)->slot == 1);
    CHECK (model.getInstance (second.instanceId)->slot == 0);
    first.slot = 0;
    CHECK (! model.replaceAll ({ first, second }, {}));
    CHECK (model.getInstance (first.instanceId)->slot == 1);
    first.slot = kMaxSlots;
    CHECK (! model.replaceAll ({ first }, {}));
    CHECK (model.getInstanceCount() == 2);
    first.slot = -2;
    CHECK (! model.replaceAll ({ first }, {}));
}

LUTHIER_TEST (RefPedalBoardCore, scalarBoardControlsRoundTripAndUndo)
{
    BoardModel model;
    const std::pair<const char*, double> edits[] {
        { "reportLatency", 0 }, { "limiterOn", 0 }, { "trails", 0 }, { "mixTail", 0 },
        { "abSelect", 1 }, { "abOut", 1 }, { "parallelNorm", 2 },
        { "reampEnable", 1 }, { "reampReplace", 1 }, { "reampGainDb", 12 },
        { "limiterCeilDb", -6 }, { "tempoSource", 2 }, { "numLanes", 4 },
        { "laneGainDb0", -24 }, { "laneGainDb3", 6 }, { "lanePhase0", 1 }, { "lanePhase3", 1 }
    };
    model.beginTransaction ("Board configuration");
    for (const auto& edit : edits)
        CHECK (model.apply (cmd::SetBoardParam { edit.first, edit.second }));
    model.endTransaction();
    const auto s = model.getBoardSettings();
    CHECK (! s.reportLatency && ! s.limiterOn && ! s.trails && ! s.mixTail);
    CHECK (s.abSelect == AbSelect::B && s.abOut == AbOutMode::Dual);
    CHECK (s.lanes.norm == ParallelNorm::None && s.lanes.numLanes == 4);
    CHECK (s.reampEnable && s.reampReplace);
    CHECK_NEAR (s.reampGainDb, 12.f, 1.e-6f);
    CHECK_NEAR (s.limiterCeilDb, -6.f, 1.e-6f);
    CHECK (s.tempoSource == TempoSource::MidiClock);
    CHECK_NEAR (s.lanes.gainDb[0], -24.f, 1.e-6f);
    CHECK_NEAR (s.lanes.gainDb[3], 6.f, 1.e-6f);
    CHECK (s.lanes.phaseInvert[0] && s.lanes.phaseInvert[3]);
    CHECK (model.undo());
    const auto original = model.getBoardSettings();
    CHECK (original.reportLatency && original.limiterOn && original.trails && original.mixTail);
    CHECK (original.lanes.numLanes == 2 && ! original.reampEnable);
    CHECK (model.redo());
    CHECK (model.getBoardSettings().lanes.numLanes == 4);
}

LUTHIER_TEST (RefPedalBoardCore, malformedScalarControlsDoNotMutateOrConsumeHistory)
{
    BoardModel model;
    const std::pair<const char*, double> invalid[] {
        { "reportLatency", 0.5 }, { "limiterOn", -1 }, { "trails", 2 }, { "mixTail", 1.1 },
        { "abSelect", 4 }, { "abSelect", 0.5 }, { "abOut", -1 }, { "parallelNorm", 3 },
        { "reampEnable", 2 }, { "reampReplace", -1 }, { "reampGainDb", 12.1 },
        { "limiterCeilDb", -6.1 }, { "limiterCeilDb", 6.1 }, { "tempoSource", 4 },
        { "numLanes", 1 }, { "numLanes", 5 }, { "numLanes", 2.5 },
        { "laneGainDb4", 0 }, { "laneGainDb-1", 0 }, { "laneGainDb00", 0 },
        { "laneGainDb0", 6.1 }, { "lanePhase0", 0.5 }, { "lanePhase4", 1 },
        { "tempoBpm", std::numeric_limits<double>::infinity() },
        { "cpuCapPct", 0 }, { "midiChan", 17 }
    };
    for (const auto& edit : invalid)
        CHECK (! model.apply (cmd::SetBoardParam { edit.first, edit.second }));
    CHECK (! model.canUndo());
    const auto s = model.getBoardSettings();
    CHECK (s.reportLatency && s.limiterOn && s.trails && s.mixTail);
    CHECK (s.lanes.numLanes == 2 && s.lanes.gainDb[0] == 0.f && ! s.lanes.phaseInvert[0]);
}

LUTHIER_TEST (RefPedalBoardCore, invalidRestoredSettingsLeaveBoardAndHistoryIntact)
{
    BoardModel model;
    CHECK (model.apply (cmd::SetMix { 0.4f }));
    const auto reject = [&] (const BoardSettings& settings)
    {
        juce::String reason;
        CHECK (! model.replaceAll ({}, settings, &reason));
        CHECK (reason.isNotEmpty());
        CHECK_NEAR (model.getBoardSettings().mix, 0.4f, 1.e-6f);
        CHECK (model.canUndo());
    };
    BoardSettings s;
    s.mode = static_cast<Mode> (255); reject (s); s = {};
    s.mixLaw = static_cast<MixLaw> (255); reject (s); s = {};
    s.style = static_cast<BoardStyle> (255); reject (s); s = {};
    s.abSelect = static_cast<AbSelect> (255); reject (s); s = {};
    s.abOut = static_cast<AbOutMode> (255); reject (s); s = {};
    s.tempoSource = static_cast<TempoSource> (255); reject (s); s = {};
    s.lanes.norm = static_cast<ParallelNorm> (255); reject (s); s = {};
    s.lanes.numLanes = 5; reject (s); s = {};
    s.lanes.gainDb[2] = std::numeric_limits<float>::quiet_NaN(); reject (s); s = {};
    s.tempoBpm = std::numeric_limits<double>::infinity(); reject (s); s = {};
    s.cpuCapPct = 0; reject (s); s = {};
    s.midiChan = 17; reject (s); s = {};
    s.limiterCeilDb = 7.f; reject (s); s = {};
    s.reampGainDb = 13.f; reject (s); s = {};
    s.loops[0].kind = static_cast<LoopKind> (255); reject (s); s = {};
    s.loops[1].blend = -0.1f; reject (s); s = {};
    s.loops[0].sendDb = -25.f; reject (s); s = {};
    s.loops[1].returnDb = std::numeric_limits<float>::quiet_NaN(); reject (s); s = {};
    s.fb.gain = 1.1f; reject (s); s = {};
    s.fb.lpHz = 0.f; reject (s);
    CHECK (model.undo());
    CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-6f);
}

LUTHIER_TEST (RefPedalBoardCore, invalidRestoredPedalControlsDoNotReplaceExistingBoard)
{
    BoardModel model;
    const InstanceId id;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, id }));
    const auto reject = [&] (const PedalInstance& pedal)
    {
        CHECK (! model.replaceAll ({ pedal }, {}));
        CHECK (model.getOrder().front() == id);
        CHECK (model.canUndo());
    };
    PedalInstance p; p.modelId = "generic.overdrive";
    const auto valid = p;
    p.modelId.clear(); reject (p); p = valid;
    p.modelRevision = 0; reject (p); p = valid;
    p.inDb = -13.f; reject (p); p = valid;
    p.outDb = 13.f; reject (p); p = valid;
    p.params[0] = std::numeric_limits<float>::quiet_NaN(); reject (p); p = valid;
    p.params[kMaxParams - 1] = 1.01f; reject (p); p = valid;
    p.x = std::numeric_limits<float>::infinity(); reject (p); p = valid;
    p.mounting = static_cast<Mounting> (255); reject (p);
}

#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/DelayLine.h"
#include "../ReferencePedals/WetDryMixer.h"
#include "../ReferencePedals/BoardLimiter.h"

LUTHIER_TEST (RefPedalBoardCore, modelEditsPublishToFacadeAudio)
{
    Board board;
    board.prepare (48000, 128);
    const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "prototype.ts808", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    CHECK (board.isActive());
    CHECK (board.model().getTopologyInfo().pedalCount == 1);
    double l[128], r[128];
    for (int block = 0; block < 20; ++block)
    {
        std::fill_n (l, 128, 0.2); std::fill_n (r, 128, 0.2);
        board.processStereo (l, r, 128, Placement::beforeExistingPre);
    }
    CHECK (std::abs (l[127] - 0.2) > 1.e-4);
    CHECK (l[127] == r[127]);
    CHECK (board.model().apply (cmd::SetMix { 0.f }));
    for (int block = 0; block < 200; ++block)
    {
        std::fill_n (l, 128, 0.2); std::fill_n (r, 128, 0.2);
        board.processStereo (l, r, 128, Placement::beforeExistingPre);
    }
    CHECK_NEAR (l[127], 0.2, 1.e-9);
}

LUTHIER_TEST (RefPedalBoardCore, rejectedUnknownFactoryLeavesModelAndHistoryUntouched)
{
    Board board;
    juce::String reason;
    CHECK (! board.model().apply (cmd::Add { "does.not.exist" }, &reason));
    CHECK (reason.isNotEmpty());
    CHECK (board.model().getInstanceCount() == 0);
    CHECK (! board.model().canUndo());
}

LUTHIER_TEST (RefPedalBoardCore, delayAndMixEndpointsAreExact)
{
    DelayLine delay;
    delay.prepare (8);
    for (int i = 0; i < 20; ++i)
        CHECK (delay.process (i == 0 ? 1.0 : 0.0, 3) == (i == 3 ? 1.0 : 0.0));
    CHECK (WetDryMixer::mix (0.25, -0.75, 0.0, MixLaw::EqualPower) == 0.25);
    CHECK (WetDryMixer::mix (0.25, -0.75, 1.0, MixLaw::EqualPower) == -0.75);
    CHECK_NEAR (WetDryMixer::mix (1.0, 1.0, 0.5, MixLaw::EqualPower), std::sqrt (2.0), 1.e-12);
    CHECK (BoardLimiter::process (std::numeric_limits<double>::infinity(), false, 1.0) == 0.0);
    CHECK (BoardLimiter::process (100.0, false, 1.0) == 16.0);
    CHECK (BoardLimiter::process (100.0, true, 1.0) <= 1.0);
}

#include "../ReferencePedals/TopologyCompiler.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <thread>

LUTHIER_TEST (RefPedalBoardCore, poolKeepsPedalIdentityAcrossReorderAndControls)
{
    PedalRegistry::registerAll(); PedalPool pool; pool.prepare (48000, 64);
    PedalInstance a, b; a.modelId = "prototype.ds1"; b.modelId = "prototype.ts808"; a.slot = 0; b.slot = 1;
    juce::String error;
    auto first = TopologyCompiler::compile ({ a, b }, {}, pool, 1, nullptr, error);
    CHECK (first != nullptr);
    auto reordered = TopologyCompiler::compile ({ b, a }, {}, pool, 2, first.get(), error);
    CHECK (reordered != nullptr);
    CHECK (first->slots[0] == reordered->slots[0]);
    CHECK (first->slots[1] == reordered->slots[1]);
    CHECK (first->structure != reordered->structure);
    a.params[0] = 0.9f;
    auto controls = TopologyCompiler::compile ({ b, a }, {}, pool, 3, reordered.get(), error);
    CHECK (controls->structure == reordered->structure);
    CHECK (controls->slots[0] == first->slots[0]);
    a.extras = new juce::DynamicObject(); a.extras.getDynamicObject()->setProperty ("state", 1);
    auto restored = TopologyCompiler::compile ({ b, a }, {}, pool, 4, controls.get(), error);
    CHECK (restored->slots[0] != first->slots[0]);
    a.extras.getDynamicObject()->setProperty ("midi", new juce::DynamicObject());
    auto mapping = TopologyCompiler::compile ({ b, a }, {}, pool, 5, restored.get(), error);
    CHECK (mapping->slots[0] == restored->slots[0]);
}

LUTHIER_TEST (RefPedalBoardCore, outputIsIndependentOfHostBlockPartitionIncludingReorder)
{
    const auto render = [] (int size)
    {
        Board board; board.prepare (48000, 2048);
        board.setModel (0, Model::ts808Prototype); board.setModel (1, Model::ds1Prototype);
        board.setBoardEnabled (true);
        std::vector<double> result (8192), right (8192);
        for (int i = 0; i < 8192; ++i) result[static_cast<size_t> (i)] = right[static_cast<size_t> (i)] = 0.2 * std::sin (i * 0.037);
        for (int offset = 0; offset < 8192;)
        {
            if (offset == 4096) board.move (0, 1);
            const int n = std::min ({ size, 8192 - offset, offset < 4096 ? 4096 - offset : 8192 - offset });
            board.processStereo (result.data() + offset, right.data() + offset, n, Placement::beforeExistingPre);
            offset += n;
        }
        return result;
    };
    const auto reference = render (1);
    for (int size : { 16, 37, 64, 481, 2048 })
    {
        const auto actual = render (size);
        for (size_t i = 0; i < actual.size(); ++i) CHECK_NEAR (actual[i], reference[i], 1.e-9);
    }
}

LUTHIER_TEST (RefPedalBoardCore, publicationReclamationAndControlEditsCanOverlapAudio)
{
    Board board; board.prepare (48000, 64);
    board.setModel (0, Model::ts808Prototype); board.setModel (1, Model::ds1Prototype);
    board.setBoardEnabled (true);
    std::atomic<bool> stop { false }, finite { true };
    std::thread audio ([&]
    {
        double l[64] {}, r[64] {};
        while (! stop.load())
        {
            std::fill_n (l, 64, 0.1); std::fill_n (r, 64, -0.1);
            board.processStereo (l, r, 64, Placement::beforeExistingPre);
            for (int i = 0; i < 64; ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite.store (false);
        }
    });
    for (int edit = 0; edit < 1000; ++edit)
    {
        board.move (0, 1); board.setKnob (0, 0, static_cast<double> (edit % 100) / 100.0);
        if (edit % 17 == 0) board.setModel (1, edit % 2 ? Model::phase90Prototype : Model::ds1Prototype);
        board.reclaim();
    }
    stop.store (true); audio.join();
    CHECK (finite.load());
    board.setKnob (0, 0, 0.333); // deterministic retired plan after concurrent reclamation finishes
    double l[1024] {}, r[1024] {}; board.processStereo (l, r, 1024, Placement::beforeExistingPre);
    CHECK (board.reclaim() > 0);
    board.releaseResources();
}

LUTHIER_TEST (RefPedalBoardCore, uiMetadataAndSnapshotsDoNotShareMutableVarObjects)
{
    BoardModel model;
    juce::var ui (new juce::DynamicObject()); ui.getDynamicObject()->setProperty ("zoom", 1.0);
    model.setUiState (ui);
    ui.getDynamicObject()->setProperty ("zoom", 2.0);
    CHECK_NEAR (static_cast<double> (model.getBoardSettings().ui["zoom"]), 1.0, 1.e-12);
    CHECK (! model.canUndo());
    auto settings = model.getBoardSettings(); settings.ui.getDynamicObject()->setProperty ("zoom", 3.0);
    CHECK_NEAR (static_cast<double> (model.getBoardSettings().ui["zoom"]), 1.0, 1.e-12);
}

namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RefPedalBoardCore, callbackAllocatesNothingDuringStructuralTransitions)
{
    Board board; board.prepare (48000, 64);
    board.setModel (0, Model::ts808Prototype); board.setModel (1, Model::ds1Prototype);
    board.setBoardEnabled (true);
    juce::MidiBuffer input, output; double left[64] {}, right[64] {};
    long allocated = 0;
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        board.move (0, 1);
        const auto before = luthier::tests::allocationsOnThisThread();
        board.beginBlock (input, {}, 64);
        for (int p = 0; p < kNumPlacements; ++p) board.processStereo (left, right, 64, static_cast<Placement> (p));
        board.endBlock (output);
        allocated += luthier::tests::allocationsOnThisThread() - before;
        board.reclaim();
    }
    CHECK (allocated == 0);
}

LUTHIER_TEST (RefPedalBoardCore, boardDisableFinishesInTwentyMilliseconds)
{
    Board board; board.prepare (48000, 1024); board.setModel (0, Model::ts808Prototype); board.setBoardEnabled (true);
    double left[1024] {}, right[1024] {};
    board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    board.setBoardEnabled (false);
    board.processStereo (left, right, 959, Placement::beforeExistingPre);
    CHECK (board.isActive());
    board.processStereo (left, right, 1, Placement::beforeExistingPre);
    CHECK (! board.isActive());
    left[0] = 0.12345; right[0] = -0.98765;
    board.processStereo (left, right, 1, Placement::beforeExistingPre);
    CHECK (left[0] == 0.12345 && right[0] == -0.98765);
}

#include "../ReferencePedals/BoardEngine.h"
namespace
{
const ParamDesc eventGainParam[] {
    { "test_event_gain", "Gain", "", ParamKind::Continuous, Taper::Linear, 0, 1, 0.5f, 1, 0, nullptr, PF_Automatable, 0, -1 }
};
const PedalDesc eventDesc { "test.event", "test_event_", 1, "Event probe", "Test", nullptr, ChannelMode::Stereo,
    BypassMode::True, 1, eventGainParam, 0, false, false, false, 0 };
const PedalDesc transportDesc { "test.transport", "test_transport_", 1, "Transport foot probe", "Test", nullptr, ChannelMode::Stereo,
    BypassMode::True, 0, nullptr, 0, false, false, false, 0 };
class EventProbe final : public IPedal
{
public:
    explicit EventProbe (bool transport = false) : transportFoot (transport) {}
    const PedalDesc& desc() const noexcept override { return transportFoot ? transportDesc : eventDesc; }
    void prepare (double, int, int) override {}
    void reset() noexcept override {}
    void process (double* l, double* r, int n) noexcept override { samples += n; for (int i = 0; i < n; ++i) { l[i] *= value; r[i] *= value; } }
    PedalUiMeters readUiMeters() const noexcept override
    {
        if (! transportFoot) return {};
        PedalUiMeters result;
        result.available = UiTransport | UiLoop | UiTempo | UiProgress;
        result.transport = 2; result.loopLength = 4096; result.playhead = samples % 4096;
        result.progress = static_cast<float> (result.playhead) / 4096.f; result.bpm = 123.0;
        return result;
    }
    void setParam (int, float v) noexcept override { value = v; }
    float getParam (int) const noexcept override { return value; }
    bool footControlsEngagement() const noexcept override { return ! transportFoot; }
private:
    uint64_t samples = 0;
    bool transportFoot = false;
    float value = 0.5f;
};
std::unique_ptr<IPedal> makeEventProbe() { return std::make_unique<EventProbe>(); }
std::unique_ptr<IPedal> makeTransportProbe() { return std::make_unique<EventProbe> (true); }
}

LUTHIER_TEST (RefPedalBoardCore, midiWritesStartAtOffsetAndPersistUntilModelChanges)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (eventDesc, makeEventProbe);
    BoardModel model; BoardEngine engine (model);
    const InstanceId id;
    CHECK (model.apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (model.apply (cmd::SetBoardEnabled { true }));
    double l[1024] {}, r[1024] {}; engine.process (l, r, 1024, Placement::beforeExistingPre);
    midi::RouteOut events; events.nWrites = 1; events.writes[0] = { 0, 0, 0.8f, 0, 17 };
    std::fill_n (l, 128, 0.1); std::fill_n (r, 128, 0.1);
    engine.beginEvents (events); engine.process (l, r, 128, Placement::beforeExistingPre); engine.endEvents();
    for (int i = 0; i < 128; ++i) CHECK_NEAR (l[i], i < 17 ? 0.05 : 0.08, 1.e-8);
    events.clear(); engine.beginEvents (events);
    std::fill_n (l, 128, 0.1); std::fill_n (r, 128, 0.1);
    engine.process (l, r, 128, Placement::beforeExistingPre); engine.endEvents();
    CHECK_NEAR (l[127], 0.08, 1.e-8);
    CHECK (model.apply (cmd::SetParam { id, 0, 0.3f }));
    engine.beginEvents (events); std::fill_n (l, 128, 0.1); std::fill_n (r, 128, 0.1);
    engine.process (l, r, 128, Placement::beforeExistingPre); engine.endEvents();
    CHECK_NEAR (l[127], 0.03, 1.e-8);
}

LUTHIER_TEST (RefPedalBoardCore, footPressEngagesPedalsWithoutBypassingTransportLoopers)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (eventDesc, makeEventProbe); PedalRegistry::add (transportDesc, makeTransportProbe);
    for (const bool transport : { false, true })
    {
        BoardModel model; BoardEngine engine (model); const InstanceId id;
        CHECK (model.apply (cmd::Add { transport ? "test.transport" : "test.event", -1, Placement::beforeExistingPre, 0, id }));
        CHECK (model.apply (cmd::SetBoardEnabled { true }));
        double l[1024] {}, r[1024] {}; engine.process (l, r, 1024, Placement::beforeExistingPre);
        midi::RouteOut events; events.nFoot = 1; events.footSlot[0] = 0; events.foot[0] = { true, 1.0f, 0, 16 };
        std::fill_n (l, 1024, 0.1); std::fill_n (r, 1024, 0.1);
        engine.beginEvents (events); engine.process (l, r, 1024, Placement::beforeExistingPre); engine.endEvents();
        CHECK_NEAR (l[1023], transport ? 0.05 : 0.1, 1.e-8);
        engine.drainControlEcho();
        CHECK (model.getInstance (id)->enabled == transport);
    }
}

LUTHIER_TEST (RefPedalBoardCore, boardMidiEnableCanWakeAnInitiallyDisabledBoard)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (eventDesc, makeEventProbe);
    BoardModel model; BoardEngine engine (model); const InstanceId id;
    CHECK (model.apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, id }));
    double l[1024] {}, r[1024] {}; engine.process (l, r, 1024, Placement::beforeExistingPre);
    midi::RouteOut events; events.nWrites = 1; events.writes[0] = { midi::kBoardTargetSlot, static_cast<uint16_t> (midi::BoardTarget::Enabled), 1.f, 0, 32 };
    std::fill_n (l, 1024, 0.1); std::fill_n (r, 1024, 0.1);
    engine.beginEvents (events); engine.process (l, r, 1024, Placement::beforeExistingPre); engine.endEvents();
    for (int i = 0; i < 32; ++i) CHECK_NEAR (l[i], 0.1, 1.e-12);
    CHECK_NEAR (l[1023], 0.05, 1.e-8);
    CHECK (engine.isActive());
}

LUTHIER_TEST (RefPedalBoardCore, hostAutomationIsImmediatePersistentAndDoesNotOverwriteModelEdits)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (eventDesc, makeEventProbe);
    Board board; board.prepare (48000, 128); const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, id }));
    HostAutomationSnapshot host;
    host.values[0] = 1.f; host.serials[0] = 1;
    host.values[4] = 0.8f; host.serials[4] = 1; // slot 0, host lane 0
    juce::MidiBuffer input, output; double l[1024], r[1024];
    const auto run = [&]
    {
        std::fill_n (l, 1024, 0.1); std::fill_n (r, 1024, 0.1);
        board.setHostAutomation (host); board.beginBlock (input, {}, 1024);
        board.processStereo (l, r, 1024, Placement::beforeExistingPre); board.endBlock (output);
    };
    run(); run(); CHECK_NEAR (l[1023], 0.08, 1.e-8);
    host.values[4] = 0.2f; run(); CHECK_NEAR (l[1023], 0.08, 1.e-8); // serial unchanged
    CHECK (board.model().apply (cmd::SetParam { id, 0, 0.3f }));
    run(); CHECK_NEAR (l[1023], 0.03, 1.e-8);
    host.values[4] = 0.9f; host.serials[4] = 2;
    const auto before = luthier::tests::allocationsOnThisThread(); run();
    CHECK (luthier::tests::allocationsOnThisThread() == before);
    CHECK_NEAR (l[1023], 0.09, 1.e-8);
    CHECK (board.model().applyHostEcho (cmd::SetParam { id, 0, 0.2f })); // stale timer captured an earlier host value
    run(); CHECK_NEAR (l[1023], 0.09, 1.e-8);
    CHECK_NEAR (board.model().getInstance (id)->params[0], 0.2, 1.e-7);
    CHECK (board.model().apply (cmd::SetParam { id, 0, 0.2f })); // same persisted echo value, explicit new intent
    run(); CHECK_NEAR (l[1023], 0.02, 1.e-8);
    host.values[4] = 0.7f; host.serials[4] = std::numeric_limits<uint32_t>::max(); run();
    host.values[4] = 0.6f; host.serials[4] = 1; run(); // producer skips reserved zero at wrap
    CHECK_NEAR (l[1023], 0.06, 1.e-8);
    CHECK (board.model().apply (cmd::Remove { id }));
    const InstanceId replacement;
    CHECK (board.model().apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, replacement }));
    run(); run(); CHECK_NEAR (l[1023], 0.05, 1.e-8); // no old occupant's value
    host.values[4] = 0.4f; host.serials[4] = 2; run(); CHECK_NEAR (l[1023], 0.04, 1.e-8);
}

LUTHIER_TEST (RefPedalBoardCore, midiFocusIsMetadataWithoutUndoOrTopologyPublication)
{
    BoardModel model; const InstanceId existing, absent;
    CHECK (model.apply (cmd::Add { "generic.overdrive", -1, Placement::beforeExistingPre, 0, existing }));
    const auto version = model.getTopologyInfo().version;
    CHECK (! model.setMidiFocus (absent));
    CHECK (model.setMidiFocus (existing));
    CHECK (model.getBoardSettings().midi["selectedInstance"].toString() == existing.toString());
    CHECK (model.getTopologyInfo().version == version);
    CHECK (model.undo()); CHECK (model.getInstanceCount() == 0);
}

LUTHIER_TEST (RefPedalBoardCore, hostEchoDoesNotCreateHistoryOrOverrideFreshHostSerial)
{
    PedalRegistry::TestScope registryScope;
    BoardModel model;
    CHECK (model.applyHostEcho (cmd::SetMix { 0.2f }));
    CHECK (! model.canUndo());
    CHECK (! model.applyHostEcho (cmd::SetStyle { BoardStyle::rail }));
    PedalRegistry::add (eventDesc, makeEventProbe);
    Board board; const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    HostAutomationSnapshot host; host.values[4] = 0.9f; host.serials[4] = 1;
    CHECK (board.model().apply (cmd::SetParam { id, 0, 0.2f }));
    board.setHostAutomation (host);
    double left[1024], right[1024]; std::fill_n (left, 1024, 0.1); std::fill_n (right, 1024, 0.1);
    board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    CHECK_NEAR (left[1023], 0.09, 1.e-8); // new host serial wins when both arrive together
}

LUTHIER_TEST (RefPedalBoardCore, pedalMetersPublishActualDataAndFullInstanceIdentity)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (eventDesc, makeEventProbe); PedalRegistry::add (transportDesc, makeTransportProbe);
    Board board; const InstanceId id;
    CHECK (! board.readPedalMeters (-1).present);
    CHECK (board.model().apply (cmd::Add { "test.transport", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    double left[1024] {}, right[1024] {}; board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    auto meter = board.readPedalMeters (0);
    CHECK (meter.present && meter.identity == id && meter.enabled);
    CHECK ((meter.model.available & UiLoop) != 0);
    CHECK (meter.model.loopLength == 4096 && meter.model.playhead == 1024);
    CHECK_NEAR (meter.model.progress, 0.25, 1.e-8);
    CHECK_NEAR (meter.model.bpm, 123.0, 1.e-8);
    std::atomic<bool> done { false }; std::atomic<int> incoherent { 0 };
    std::thread audio ([&] { for (int i = 0; i < 2000; ++i) board.processStereo (left, right, 64, Placement::beforeExistingPre); done.store (true); });
    while (! done.load())
    {
        meter = board.readPedalMeters (0);
        if (meter.present && (meter.identity != id || meter.model.loopLength != 4096
            || meter.model.progress != static_cast<float> (meter.model.playhead) / 4096.f)) ++incoherent;
    }
    audio.join(); CHECK (incoherent.load() == 0);
    CHECK (board.model().apply (cmd::Remove { id }));
    const InstanceId replacement;
    CHECK (board.model().apply (cmd::Add { "test.event", -1, Placement::beforeExistingPre, 0, replacement }));
    board.processStereo (left, right, 1024, Placement::beforeExistingPre); board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    meter = board.readPedalMeters (0);
    CHECK (meter.present && meter.identity == replacement);
    CHECK (meter.model.available == 0); // unsupported telemetry remains explicitly unavailable
    CHECK (board.model().apply (cmd::Remove { replacement }));
    board.processStereo (left, right, 1024, Placement::beforeExistingPre); board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    CHECK (! board.readPedalMeters (0).present);
}

namespace
{
const ParamDesc pulseParam[] {
    { "test_pulse_fire", "Fire", "", ParamKind::Trigger, Taper::Linear, 0, 1, 0, 1, 0, nullptr, PF_Automatable, 0, -1 }
};
const PedalDesc pulseDesc { "test.pulse", "test_pulse_", 1, "Pulse probe", "Test", nullptr, ChannelMode::Stereo,
    BypassMode::True, 1, pulseParam, 0, false, false, false, 0 };
class PulseProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return pulseDesc; }
    void prepare (double, int, int) override {}
    void reset() noexcept override { count = 0; value = 0; }
    void process (double* l, double* r, int n) noexcept override
    { for (int i = 0; i < n; ++i) l[i] = r[i] = count * 0.01; }
    void setParam (int, float v) noexcept override { if (v >= 0.5f && value < 0.5f) ++count; value = v; }
    float getParam (int) const noexcept override { return value; }
private:
    int count = 0; float value = 0;
};
std::unique_ptr<IPedal> makePulseProbe() { return std::make_unique<PulseProbe>(); }
}
LUTHIER_TEST (RefPedalBoardCore, rapidUiTriggerEdgesSurviveSnapshotCoalescing)
{
    PedalRegistry::TestScope registryScope;
    PedalRegistry::add (pulseDesc, makePulseProbe);
    Board board; const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "test.pulse", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    double left[1024] {}, right[1024] {}; board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    for (int i = 0; i < 3; ++i)
    {
        CHECK (board.model().apply (cmd::SetParam { id, 0, 1.f }));
        CHECK (board.model().apply (cmd::SetParam { id, 0, 0.f }));
    }
    const auto before = luthier::tests::allocationsOnThisThread();
    board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    CHECK (luthier::tests::allocationsOnThisThread() == before);
    CHECK_NEAR (left[1023], 0.03, 1.e-10);
    board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    CHECK_NEAR (left[1023], 0.03, 1.e-10); // consumed once
}

LUTHIER_TEST (RefPedalBoardCore, cancelTransactionRestoresOuterBaselineAndExistingHistory)
{
    BoardModel model;
    CHECK (! model.cancelTransaction());
    CHECK (model.apply (cmd::SetMix { 0.2f }));
    CHECK (model.apply (cmd::SetMix { 0.4f }));
    CHECK (model.undo()); // preexisting redo must survive cancelled drag
    model.beginTransaction ("outer");
    CHECK (model.apply (cmd::SetMix { 0.6f }));
    model.beginTransaction ("inner");
    CHECK (model.apply (cmd::SetMix { 0.8f }));
    CHECK (model.cancelTransaction());
    CHECK_NEAR (model.getBoardSettings().mix, 0.2f, 1.e-8);
    CHECK (model.canUndo() && model.canRedo());
    CHECK (model.redo()); CHECK_NEAR (model.getBoardSettings().mix, 0.4f, 1.e-8);
    CHECK (model.undo()); CHECK_NEAR (model.getBoardSettings().mix, 0.2f, 1.e-8);
    CHECK (model.undo()); CHECK_NEAR (model.getBoardSettings().mix, 1.f, 1.e-8);
    model.beginTransaction ("rejected");
    CHECK (! model.apply (cmd::SetMix { -1.f }));
    CHECK (model.cancelTransaction()); CHECK (model.canRedo());
    CHECK (! model.cancelTransaction());
}

LUTHIER_TEST (RefPedalBoardCore, legacyMoveRetainsAllSixteenSlotsWithSparsePedals)
{
    Board board; board.setModel (0, Model::ds1Prototype);
    board.move (0, 15);
    CHECK (board.getIdAtPosition (15) == 0);
    std::array<bool, kMaxSlots> seen {};
    for (int i = 0; i < kMaxSlots; ++i)
    {
        const int slot = board.getIdAtPosition (i);
        CHECK (slot >= 0 && slot < kMaxSlots && ! seen[static_cast<size_t> (slot)]);
        seen[static_cast<size_t> (slot)] = true;
    }
    board.setModel (1, Model::ts808Prototype);
    board.move (15, 0);
    CHECK (board.getIdAtPosition (0) == 0);
    const auto order = board.model().getOrder();
    CHECK (board.model().getInstance (order[0])->slot == 0);
    CHECK (board.model().getInstance (order[1])->slot == 1);
}

namespace
{
const PedalDesc rawDesc { "test.raw", "test_raw_", 1, "Raw MIDI probe", "Test", nullptr, ChannelMode::Stereo,
    BypassMode::True, 0, nullptr, 0, false, false, true, 0 };
class RawProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return rawDesc; }
    void prepare (double, int, int) override {}
    void reset() noexcept override { pending = 0; value = 0.5; }
    void process (double* l, double* r, int n) noexcept override { for (int i = 0; i < n; ++i) { l[i] *= value; r[i] *= value; } }
    void setParam (int, float) noexcept override {}
    float getParam (int) const noexcept override { return 0; }
    void onMidi (const MidiEvent& event) noexcept override
    {
        if ((event.status & 0xf0) != 0xc0) return;
        value = event.d1 / 127.0;
        if (pending < 64) { output[static_cast<size_t> (pending)] = event; output[static_cast<size_t> (pending++)].offset = 0; }
    }
    int drainMidiOut (MidiEvent* destination, int capacity) noexcept override
    {
        const int count = std::min (pending, capacity); std::copy_n (output.data(), count, destination);
        std::move (output.begin() + count, output.begin() + pending, output.begin()); pending -= count; return count;
    }
private:
    double value = 0.5;
    std::array<MidiEvent, 64> output {};
    int pending = 0;
};
std::unique_ptr<IPedal> makeRawProbe() { return std::make_unique<RawProbe>(); }
}
LUTHIER_TEST (RefPedalBoardCore, rawMidiAndBoundedOutputPreserveOffsetsWithoutAllocation)
{
    PedalRegistry::TestScope registryScope; CHECK (PedalRegistry::add (rawDesc, makeRawProbe));
    Board board; const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "test.raw", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    double l[1024] {}, r[1024] {}; board.processStereo (l, r, 1024, Placement::beforeExistingPre);
    juce::MidiBuffer input, output;
    input.addEvent (juce::MidiMessage::programChange (1, 127), 17);
    input.addEvent (juce::MidiMessage::programChange (1, 0), 72);
    std::fill_n (l, 128, 0.1); std::fill_n (r, 128, 0.1);
    MidiEvent outgoing[256];
    const auto before = luthier::tests::allocationsOnThisThread();
    board.beginBlock (input, {}, 128); board.processStereo (l, r, 128, Placement::beforeExistingPre); board.endBlock (output);
    const int count = board.drainMidiOut (outgoing, 256);
    CHECK (luthier::tests::allocationsOnThisThread() == before);
    CHECK (count == 2 && outgoing[0].offset == 17 && outgoing[1].offset == 72);
    for (int i = 0; i < 128; ++i) CHECK_NEAR (l[i], i < 17 ? 0.05 : i < 72 ? 0.1 : 0.0, 1.e-10);
    CHECK (board.drainMidiOut (outgoing, 256) == 0);
    CHECK (board.model().apply (cmd::SetBoardEnabled { false }));
    board.beginBlock (input, {}, 128); board.processStereo (l, r, 128, Placement::beforeExistingPre); board.endBlock (output);
    CHECK (board.drainMidiOut (outgoing, 256) == 0);
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    const InstanceId second;
    CHECK (board.model().apply (cmd::Add { "test.raw", -1, Placement::beforeExistingPre, 0, second }));
    input.clear(); board.beginBlock (input, {}, 1024); board.processStereo (l, r, 1024, Placement::beforeExistingPre); board.endBlock (output);
    for (int i = 0; i < 300; ++i) input.addEvent (juce::MidiMessage::programChange (1, i % 128), i);
    board.beginBlock (input, {}, 512); board.processStereo (l, r, 512, Placement::beforeExistingPre); board.endBlock (output);
    CHECK (board.drainMidiOut (outgoing, 256) == 256);
    for (int i = 1; i < 256; ++i) CHECK (outgoing[i - 1].offset <= outgoing[i].offset);
    CHECK (outgoing[255].offset == 127); // newest output is dropped deterministically
    CHECK (board.readMeters().midiDropped >= 300); // 44 raw overflow + 256 output overflow, plus router counters
}

namespace
{
const ParamDesc captureParam[] {
    { "test_capture_mode", "Mode", "", ParamKind::Toggle, Taper::Linear, 0, 1, 0, 1, 0, nullptr, PF_Config, -1, -1 }
};
const PedalDesc captureDesc { "test.capture", "test_capture_", 1, "Capture probe", "Test", nullptr, ChannelMode::Stereo,
    BypassMode::True, 1, captureParam, 0, false, false, false, 0 };
class CaptureProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return captureDesc; }
    void prepare (double, int, int) override { frames = 0; published.store (0); }
    void reset() noexcept override {}
    void process (double*, double*, int n) noexcept override { frames += n; published.store (frames, std::memory_order_release); }
    void setParam (int, float v) noexcept override { value = v; }
    float getParam (int) const noexcept override { return value; }
    void saveExtra (juce::var& extra) const override
    {
        if (! extra.getDynamicObject()) extra = new juce::DynamicObject();
        extra.getDynamicObject()->setProperty ("frames", static_cast<juce::int64> (published.load (std::memory_order_acquire)));
    }
    void loadExtra (const juce::var& extra) override
    { frames = static_cast<uint64_t> (static_cast<juce::int64> (extra.getProperty ("frames", 0))); published.store (frames); }
private:
    uint64_t frames = 0;
    std::atomic<uint64_t> published { 0 };
    float value = 0;
};
std::unique_ptr<IPedal> makeCaptureProbe() { return std::make_unique<CaptureProbe>(); }
}
LUTHIER_TEST (RefPedalBoardCore, runtimeExtraCaptureIsIsolatedAndSurvivesPrepareReorderAndUndo)
{
    PedalRegistry::TestScope registryScope; CHECK (PedalRegistry::add (captureDesc, makeCaptureProbe));
    Board board; const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "test.capture", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    double left[1024] {}, right[1024] {}; board.processStereo (left, right, 1024, Placement::beforeExistingPre);
    const auto read = [&]
    {
        std::vector<PedalInstance> instances; BoardSettings settings; board.model().snapshot (instances, settings);
        for (const auto& instance : instances) if (instance.instanceId == id) return static_cast<juce::int64> (instance.extras["frames"]);
        return juce::int64 (-1);
    };
    CHECK (read() == 1024);
    CHECK (board.model().getInstance (id)->extras.isVoid()); // save did not mutate model or create a topology
    const auto version = board.model().getTopologyInfo().version;
    for (int i = 0; i < 10; ++i) CHECK (read() == 1024);
    CHECK (board.model().getTopologyInfo().version == version);
    board.prepare (48000, 128); CHECK (read() == 1024); // prepare resets DSP, then captured extra is loaded
    board.releaseResources(); CHECK (read() == 1024);
    board.prepare (48000, 128); CHECK (read() == 1024); // host release/prepare cycle retains captured handles
    CHECK (board.model().apply (cmd::SetParam { id, 0, 1.f })); CHECK (read() == 1024);
    CHECK (board.model().undo()); CHECK (read() == 1024); // config replacement snapshots the active runtime
    std::atomic<bool> done { false };
    std::thread audio ([&] { for (int i = 0; i < 1000; ++i) board.processStereo (left, right, 64, Placement::beforeExistingPre); done.store (true); });
    juce::int64 previous = 1024;
    while (! done.load()) { const auto current = read(); CHECK (current >= previous); previous = current; }
    audio.join(); CHECK (read() >= 1024 + 64000);
    const auto beforeReorder = read();
    const InstanceId second;
    CHECK (board.model().apply (cmd::Add { "test.capture", -1, Placement::beforeExistingPre, 0, second }));
    CHECK (board.model().apply (cmd::Move { id, 1 })); CHECK (read() == beforeReorder);
    CHECK (board.model().undo()); CHECK (read() == beforeReorder);
}
