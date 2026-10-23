// OWNER: WP2 (interface frozen by WP1, I-4: the public section below changes only via errata)
#pragma once

// Message-thread truth of the board (BOARD_CORE 3). The UI, the state reader
// and the automation mirror read and edit it ONLY through apply(); the audio
// thread never sees it (it sees the compiled Topology). Rule O1: getOrder() is
// the only ordering authority, a view must never keep its own list.
//
// Editing and snapshot history are implemented in BoardModel.cpp. Audio
// topology compilation/publication remains pending integration.

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <juce_core/juce_core.h>
#include "BoardCommand.h"
#include "BoardTypes.h"

namespace luthier::reference
{

class BoardEngine;
class TopologyCompiler;

class BoardModel
{
public:
    // Notifications run on the message thread, after the change is published.
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void orderChanged() {}
        virtual void instanceChanged (const InstanceId&) {}
        virtual void settingsChanged() {}
        virtual void styleChanged() {}
        virtual void rejected (const juce::String& /*reason*/) {}
    };

    BoardModel();
    ~BoardModel();
    BoardModel (const BoardModel&) = delete;
    BoardModel& operator= (const BoardModel&) = delete;

    //==========================================================================
    // Message thread only.

    // Validates, mutates, compiles and publishes a new topology. Returns false
    // and (when given) a reason if the command is rejected (capacity 16, latency
    // over kMaxLatencySamp, CPU cap, bad feedback ordering, unknown id...).
    // Rejected commands change nothing and notify Listener::rejected.
    bool apply (const BoardCommand&, juce::String* rejectReason = nullptr);

    // Instance ids in audio order (== visual order).
    std::vector<InstanceId> getOrder() const;
    int getInstanceCount() const;
    std::optional<PedalInstance> getInstance (const InstanceId&) const;   // copy
    TopologyInfo getTopologyInfo() const;
    BoardSettings getBoardSettings() const;

    // Undo / redo (100 steps, drags coalesce). One transaction == one step.
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    void beginTransaction (const juce::String& name);
    void endTransaction();

    void addListener (Listener*);
    void removeListener (Listener*);

    // State hooks (WP4): atomically replace everything, clear undo, publish one
    // topology. Partial failure keeps the previous model (returns false).
    bool replaceAll (const std::vector<PedalInstance>& orderedInstances, const BoardSettings& settings,
                     juce::String* error = nullptr);
    void setUiState (juce::var); // presentation-only; no undo or audio publication
    void snapshot (std::vector<PedalInstance>& orderedInstances, BoardSettings& settings) const;

private:
    friend class BoardEngine;
    friend class TopologyCompiler;
    using Compiler = std::function<bool (const std::vector<PedalInstance>&, const BoardSettings&, TopologyInfo&, juce::String&)>;
    void setCompiler (Compiler);
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace luthier::reference
