// OWNER: WP2
// STUB (WP1): the interface is frozen in BoardModel.h; WP2 implements it. This
// body exists so the facade, the UI headers and the tests link today. apply()
// rejects everything and the getters report an empty board.

#include "BoardModel.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>
#include <mutex>

namespace luthier::reference
{

struct BoardModel::Impl
{
    mutable std::mutex mutex;        // message-thread mutex, never taken on the audio thread
    BoardSettings settings;
    std::vector<PedalInstance> instances;
    struct History { BoardSettings settings; std::vector<PedalInstance> instances; };
    std::vector<History> undoStack, redoStack;
    std::vector<Listener*> listeners;
};

BoardModel::BoardModel() : impl (std::make_unique<Impl>()) {}
BoardModel::~BoardModel() = default;

bool BoardModel::apply (const BoardCommand& command, juce::String* rejectReason)
{
    juce::String reason;
    bool settingsChanged = false;
    bool styleChanged = false;
    bool orderChanged = false;
    InstanceId changedId = InstanceId::null();
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        auto next = impl->settings;
        auto nextInstances = impl->instances;
        auto findPedal = [&] (const InstanceId& id) { return std::find_if (nextInstances.begin(), nextInstances.end(), [&] (const PedalInstance& p) { return p.instanceId == id; }); };
        const bool supported = std::visit ([&] (const auto& cmd) -> bool
        {
            using T = std::decay_t<decltype (cmd)>;
            if constexpr (std::is_same_v<T, cmd::Add>)
            {
                if (cmd.modelId.empty() || nextInstances.size() >= kMaxSlots
                    || ! isValidPlacement (static_cast<int> (cmd.placement))
                    || cmd.lane < 0 || cmd.lane >= kMaxBranches
                    || cmd.index < -1 || cmd.index > static_cast<int> (nextInstances.size()))
                    return false;
                PedalInstance p;
                p.instanceId = cmd.id.isNull() ? InstanceId() : cmd.id;
                if (findPedal (p.instanceId) != nextInstances.end()) return false;
                p.modelId = cmd.modelId;
                p.placement = cmd.placement;
                p.lane = cmd.lane;
                changedId = p.instanceId;
                const auto index = cmd.index == -1 ? nextInstances.size() : static_cast<size_t> (cmd.index);
                nextInstances.insert (nextInstances.begin() + static_cast<std::ptrdiff_t> (index), std::move (p));
                orderChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::Remove>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end()) return false;
                changedId = cmd.id;
                nextInstances.erase (it);
                orderChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::Move>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || cmd.newIndex < 0
                    || cmd.newIndex >= static_cast<int> (nextInstances.size())) return false;
                PedalInstance p = std::move (*it);
                nextInstances.erase (it);
                nextInstances.insert (nextInstances.begin() + cmd.newIndex, std::move (p));
                changedId = cmd.id;
                orderChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::SetEnabled>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end()) return false;
                it->enabled = cmd.enabled;
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetPlacement>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || ! isValidPlacement (static_cast<int> (cmd.placement))) return false;
                it->placement = cmd.placement;
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetLane>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || cmd.lane < 0 || cmd.lane >= kMaxBranches) return false;
                it->lane = cmd.lane;
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetParam>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || cmd.paramIdx < 0 || cmd.paramIdx >= kMaxParams
                    || ! std::isfinite (cmd.norm) || cmd.norm < 0.f || cmd.norm > 1.f) return false;
                it->params[static_cast<size_t> (cmd.paramIdx)] = cmd.norm;
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetBoardEnabled>)
                next.enabled = cmd.enabled;
            else if constexpr (std::is_same_v<T, cmd::SetMix>)
            {
                if (! std::isfinite (cmd.mix) || cmd.mix < 0.f || cmd.mix > 1.f)
                    return false;
                next.mix = cmd.mix;
            }
            else if constexpr (std::is_same_v<T, cmd::SetStyle>)
            {
                if (static_cast<int> (cmd.style) < 0 || cmd.style > BoardStyle::loose)
                    return false;
                next.style = cmd.style;
                styleChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::SetMode>)
            {
                if (cmd.mode > Mode::Loop) return false;
                next.mode = cmd.mode;
            }
            else if constexpr (std::is_same_v<T, cmd::SetMixLaw>)
            {
                if (cmd.law > MixLaw::ThirdLaw) return false;
                next.mixLaw = cmd.law;
            }
            else if constexpr (std::is_same_v<T, cmd::SetBoardParam>)
            {
                if (! std::isfinite (cmd.value)) return false;
                if (cmd.key == "tempoBpm" && cmd.value >= 20.0 && cmd.value <= 300.0)
                    next.tempoBpm = cmd.value;
                else if (cmd.key == "cpuCapPct" && cmd.value >= 1.0 && cmd.value <= 100.0
                         && std::floor (cmd.value) == cmd.value)
                    next.cpuCapPct = static_cast<int> (cmd.value);
                else if (cmd.key == "midiChan" && cmd.value >= 0.0 && cmd.value <= 16.0
                         && std::floor (cmd.value) == cmd.value)
                    next.midiChan = static_cast<int> (cmd.value);
                else return false;
            }
            else return false; // Unsupported commands must never report success.
            return true;
        }, command);
        if (supported)
        {
            if (impl->undoStack.size() == 100) impl->undoStack.erase (impl->undoStack.begin());
            impl->undoStack.push_back ({ impl->settings, impl->instances });
            impl->redoStack.clear();
            impl->settings = std::move (next);
            impl->instances = std::move (nextInstances);
            settingsChanged = true;
        }
        else reason = juce::String ("Unsupported or invalid board command: ") + commandName (command);
    }
    if (rejectReason != nullptr) *rejectReason = reason;
    // Listener callbacks are deliberately outside the message-thread mutex.
    if (settingsChanged)
    {
        for (auto* listener : impl->listeners)
        {
            if (orderChanged) listener->orderChanged();
            if (! changedId.isNull()) listener->instanceChanged (changedId);
            listener->settingsChanged();
            if (styleChanged) listener->styleChanged();
        }
    }
    else for (auto* listener : impl->listeners) listener->rejected (reason);
    return settingsChanged;
}

std::vector<InstanceId> BoardModel::getOrder() const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    std::vector<InstanceId> order;
    order.reserve (impl->instances.size());
    for (const auto& pedal : impl->instances) order.push_back (pedal.instanceId);
    return order;
}
int BoardModel::getInstanceCount() const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    return static_cast<int> (impl->instances.size());
}
std::optional<PedalInstance> BoardModel::getInstance (const InstanceId& id) const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    for (const auto& pedal : impl->instances)
        if (pedal.instanceId == id) return pedal;
    return std::nullopt;
}
TopologyInfo BoardModel::getTopologyInfo() const { return {}; }

BoardSettings BoardModel::getBoardSettings() const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    return impl->settings;
}

bool BoardModel::undo()
{
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        if (impl->undoStack.empty()) return false;
        impl->redoStack.push_back ({ impl->settings, impl->instances });
        auto previous = std::move (impl->undoStack.back());
        impl->undoStack.pop_back();
        impl->settings = std::move (previous.settings);
        impl->instances = std::move (previous.instances);
    }
    for (auto* l : impl->listeners) { l->orderChanged(); l->settingsChanged(); l->styleChanged(); }
    return true;
}
bool BoardModel::redo()
{
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        if (impl->redoStack.empty()) return false;
        impl->undoStack.push_back ({ impl->settings, impl->instances });
        auto next = std::move (impl->redoStack.back());
        impl->redoStack.pop_back();
        impl->settings = std::move (next.settings);
        impl->instances = std::move (next.instances);
    }
    for (auto* l : impl->listeners) { l->orderChanged(); l->settingsChanged(); l->styleChanged(); }
    return true;
}
bool BoardModel::canUndo() const { const std::lock_guard<std::mutex> lock (impl->mutex); return ! impl->undoStack.empty(); }
bool BoardModel::canRedo() const { const std::lock_guard<std::mutex> lock (impl->mutex); return ! impl->redoStack.empty(); }
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

bool BoardModel::replaceAll (const std::vector<PedalInstance>& orderedInstances,
                             const BoardSettings& settings, juce::String* error)
{
    juce::String reason;
    if (orderedInstances.size() > static_cast<size_t> (kMaxSlots))
        reason = "Board exceeds 16 pedal slots";
    else if (! std::isfinite (settings.mix) || settings.mix < 0.f || settings.mix > 1.f)
        reason = "Invalid board mix";
    else
    {
        for (size_t i = 0; i < orderedInstances.size(); ++i)
        {
            const auto& pedal = orderedInstances[i];
            if (pedal.instanceId.isNull() || ! isValidPlacement (static_cast<int> (pedal.placement))
                || pedal.lane < 0 || pedal.lane >= kMaxBranches)
                { reason = "Invalid pedal identifier, placement or lane"; break; }
            for (size_t j = 0; j < i; ++j)
                if (orderedInstances[j].instanceId == pedal.instanceId)
                    { reason = "Duplicate pedal identifier"; break; }
            if (reason.isNotEmpty()) break;
        }
    }
    if (error != nullptr) *error = reason;
    if (reason.isNotEmpty()) return false;
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        impl->instances = orderedInstances;
        impl->settings = settings;
        impl->undoStack.clear();
        impl->redoStack.clear();
    }
    for (auto* listener : impl->listeners)
    {
        listener->orderChanged();
        listener->settingsChanged();
        listener->styleChanged();
    }
    return true;
}

void BoardModel::snapshot (std::vector<PedalInstance>& instances, BoardSettings& settings) const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    instances = impl->instances;
    settings = impl->settings;
}

} // namespace luthier::reference
