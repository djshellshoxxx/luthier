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
    std::vector<Listener*> listeners;
};

BoardModel::BoardModel() : impl (std::make_unique<Impl>()) {}
BoardModel::~BoardModel() = default;

bool BoardModel::apply (const BoardCommand& command, juce::String* rejectReason)
{
    juce::String reason;
    bool settingsChanged = false;
    bool styleChanged = false;
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        auto next = impl->settings;
        const bool supported = std::visit ([&] (const auto& cmd) -> bool
        {
            using T = std::decay_t<decltype (cmd)>;
            if constexpr (std::is_same_v<T, cmd::SetBoardEnabled>)
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
            impl->settings = std::move (next);
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
