// BoardModel regression tests: message-thread state must not silently reject edits.
#include "TestFramework.h"
#include "../ReferencePedals/BoardModel.h"
#include <cmath>
#include <limits>
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
    const InstanceId a (juce::Uuid()), b (juce::Uuid());
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
