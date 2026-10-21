// OWNER: WP2
// STUB (WP1): the interface is frozen in BoardModel.h; WP2 implements it. This
// body exists so the facade, the UI headers and the tests link today. apply()
// rejects everything and the getters report an empty board.

#include "BoardModel.h"
#include <algorithm>
#include <mutex>

namespace luthier::reference
{

struct BoardModel::Impl
{
    mutable std::mutex mutex;        // message-thread mutex, never taken on the audio thread
    BoardSettings settings;
    std::vector<Listener*> listeners;
};

BoardModel::BoardModel() : impl (std::make_unique<Impl>()) {}
BoardModel::~BoardModel() = default;

bool BoardModel::apply (const BoardCommand& command, juce::String* rejectReason)
{
    const juce::String reason = juce::String ("not implemented yet (WP2): ") + commandName (command);
    if (rejectReason != nullptr)
        *rejectReason = reason;
    for (auto* l : impl->listeners)
        l->rejected (reason);
    return false;
}

std::vector<InstanceId> BoardModel::getOrder() const { return {}; }
int BoardModel::getInstanceCount() const { return 0; }
std::optional<PedalInstance> BoardModel::getInstance (const InstanceId&) const { return std::nullopt; }
TopologyInfo BoardModel::getTopologyInfo() const { return {}; }

BoardSettings BoardModel::getBoardSettings() const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    return impl->settings;
}

bool BoardModel::undo() { return false; }
bool BoardModel::redo() { return false; }
bool BoardModel::canUndo() const { return false; }
bool BoardModel::canRedo() const { return false; }
void BoardModel::beginTransaction (const juce::String&) {}
void BoardModel::endTransaction() {}

void BoardModel::addListener (Listener* l)
{
    if (l != nullptr)
        impl->listeners.push_back (l);
}

void BoardModel::removeListener (Listener* l)
{
    impl->listeners.erase (std::remove (impl->listeners.begin(), impl->listeners.end(), l), impl->listeners.end());
}

bool BoardModel::replaceAll (const std::vector<PedalInstance>&, const BoardSettings&, juce::String* error)
{
    if (error != nullptr)
        *error = "not implemented yet (WP2)";
    return false;
}

void BoardModel::snapshot (std::vector<PedalInstance>& instances, BoardSettings& settings) const
{
    instances.clear();
    settings = getBoardSettings();
}

} // namespace luthier::reference
