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
