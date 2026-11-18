/*  writeState: the host state is written straight into the block (no String
    copies), and undo / A-B / export snapshots use compact JSON. Both forms
    must describe the same state, and the host form must stay byte-stable.
*/
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../ReferencePedals/BoardModel.h"

using namespace luthier;
using namespace luthier::reference;

namespace
{
    juce::String text (const juce::MemoryBlock& b) { return juce::String::fromUTF8 (static_cast<const char*> (b.getData()), (int) b.getSize()); }

    // One canonical rendering of parsed JSON, so whitespace never matters.
    juce::String canonical (const juce::MemoryBlock& b)
    {
        juce::var parsed;
        const auto result = juce::JSON::parse (text (b), parsed);
        return result.wasOk() ? juce::JSON::toString (parsed, true) : juce::String ("<invalid: " + result.getErrorMessage() + ">");
    }
}

LUTHIER_TEST (StateWrite, compactSnapshotMatchesHostStateWithBoard)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (48000.0, 256);
    auto& board = p->getEngine().getReferencePedals();
    CHECK (board.model().apply (cmd::SetMix { .41f }));
    CHECK (board.model().apply (cmd::Add { "mxr-phase90-script" }));

    juce::MemoryBlock host;
    p->getStateInformation (host);
    const auto compact = p->captureStateBlock();

    CHECK (text (host).contains ("\"referencePedals\":"));
    CHECK (text (compact).contains ("\"referencePedals\":"));
    CHECK (compact.getSize() < host.getSize());                    // no indentation
    CHECK (! text (compact).containsChar ('\n'));
    CHECK (canonical (host) == canonical (compact));
    CHECK (! canonical (host).startsWith ("<invalid"));

    // Two host saves of an unchanged instance are byte-identical.
    juce::MemoryBlock again;
    p->getStateInformation (again);
    CHECK (again == host);

    // The compact snapshot restores like the host state (undo / A-B use it).
    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (compact.getData(), (int) compact.getSize());
    restored->prepareToPlay (48000.0, 256);
    const auto& model = restored->getEngine().getReferencePedals().model();
    CHECK_NEAR (model.getBoardSettings().mix, .41f, 1e-6);
    CHECK (model.getInstanceCount() == 1);
    restored->releaseResources();
    p->releaseResources();
}
