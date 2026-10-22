// Message-thread board edits and bounded snapshot history. Audio topology
// compilation/publication remains a separate integration work package.

#include "BoardModel.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>
#include <mutex>

namespace luthier::reference
{

namespace
{
    int firstFreeSlot (const std::vector<PedalInstance>& instances)
    {
        std::array<bool, kMaxSlots> used {};
        for (const auto& pedal : instances)
            if (pedal.slot >= 0 && pedal.slot < kMaxSlots)
                used[static_cast<size_t> (pedal.slot)] = true;
        for (int slot = 0; slot < kMaxSlots; ++slot)
            if (! used[static_cast<size_t> (slot)]) return slot;
        return -1;
    }
}

struct BoardModel::Impl
{
    mutable std::mutex mutex;        // message-thread mutex, never taken on the audio thread
    BoardSettings settings;
    std::vector<PedalInstance> instances;
    struct History { BoardSettings settings; std::vector<PedalInstance> instances; };
    std::vector<History> undoStack, redoStack;
    unsigned transactionDepth = 0;
    bool transactionHasEdits = false;

    struct Gesture
    {
        int kind = 0, param = -1;
        InstanceId id = InstanceId::null();
        uint32_t number = 0;
        bool continues (const Gesture& other) const
        {
            return number != 0 && number == other.number && kind == other.kind
                && param == other.param && id == other.id;
        }
    } lastGesture;

    static Gesture gestureFor (const BoardCommand& command)
    {
        if (const auto* c = std::get_if<cmd::SetParam> (&command))
            return { 1, c->paramIdx, c->id, c->gesture };
        if (const auto* c = std::get_if<cmd::SetTrim> (&command))
            return { 2, -1, c->id, c->gesture };
        if (const auto* c = std::get_if<cmd::SetMix> (&command))
            return { 3, -1, InstanceId::null(), c->gesture };
        return {};
    }

    // Capture the state before the first accepted edit, never for an empty or
    // rejected transaction. This also preserves redo until an edit succeeds.
    void recordUndo (const BoardCommand& command)
    {
        const auto gesture = gestureFor (command);
        const bool capture = transactionDepth != 0 ? ! transactionHasEdits
                                                   : ! gesture.continues (lastGesture);
        if (capture)
        {
            if (undoStack.size() == 100) undoStack.erase (undoStack.begin());
            undoStack.push_back ({ settings, instances });
        }
        redoStack.clear();
        if (transactionDepth != 0) transactionHasEdits = true;
        lastGesture = gesture;
    }
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
                p.slot = firstFreeSlot (nextInstances);
                if (p.slot < 0) return false;
                p.modelId = cmd.modelId;
                p.placement = cmd.placement;
                p.lane = cmd.lane;
                changedId = p.instanceId;
                const auto index = cmd.index == -1 ? nextInstances.size() : static_cast<size_t> (cmd.index);
                nextInstances.insert (nextInstances.begin() + static_cast<std::ptrdiff_t> (index), std::move (p));
                orderChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::Duplicate>)
            {
                const auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || nextInstances.size() >= kMaxSlots) return false;
                auto copy = *it;
                const auto index = std::distance (nextInstances.begin(), it) + 1;
                copy.instanceId = cmd.newId.isNull() ? InstanceId() : cmd.newId;
                if (findPedal (copy.instanceId) != nextInstances.end()) return false;
                // Never alias the source's host-automation storage index.
                copy.slot = firstFreeSlot (nextInstances);
                if (copy.slot < 0) return false;
                // juce::var objects are reference-counted: detach mutable extras.
                copy.extras = copy.extras.clone();
                changedId = copy.instanceId;
                nextInstances.insert (nextInstances.begin() + index, std::move (copy));
                orderChanged = true;
            }
            else if constexpr (std::is_same_v<T, cmd::SetTrim>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || ! std::isfinite (cmd.inDb) || ! std::isfinite (cmd.outDb)
                    || cmd.inDb < -12.f || cmd.inDb > 12.f || cmd.outDb < -12.f || cmd.outDb > 12.f)
                    return false;
                it->inDb = cmd.inDb;
                it->outDb = cmd.outDb;
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetMounting>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || cmd.mounting > Mounting::none) return false;
                it->mounting = cmd.mounting;
                changedId = cmd.id;
                styleChanged = true;
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
            impl->recordUndo (command);
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
        if (impl->transactionDepth != 0 || impl->undoStack.empty()) return false;
        impl->lastGesture = {};
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
        if (impl->transactionDepth != 0 || impl->redoStack.empty()) return false;
        impl->lastGesture = {};
        impl->undoStack.push_back ({ impl->settings, impl->instances });
        auto next = std::move (impl->redoStack.back());
        impl->redoStack.pop_back();
        impl->settings = std::move (next.settings);
        impl->instances = std::move (next.instances);
    }
    for (auto* l : impl->listeners) { l->orderChanged(); l->settingsChanged(); l->styleChanged(); }
    return true;
}
bool BoardModel::canUndo() const { const std::lock_guard<std::mutex> lock (impl->mutex); return impl->transactionDepth == 0 && ! impl->undoStack.empty(); }
bool BoardModel::canRedo() const { const std::lock_guard<std::mutex> lock (impl->mutex); return impl->transactionDepth == 0 && ! impl->redoStack.empty(); }
void BoardModel::beginTransaction (const juce::String&)
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    if (impl->transactionDepth++ == 0)
    {
        impl->transactionHasEdits = false;
        impl->lastGesture = {};
    }
}
void BoardModel::endTransaction()
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    if (impl->transactionDepth != 0 && --impl->transactionDepth == 0)
    {
        impl->transactionHasEdits = false;
        impl->lastGesture = {};
    }
}

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
    auto restored = orderedInstances;
    std::array<bool, kMaxSlots> usedSlots {};
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
            if (pedal.slot < -1 || pedal.slot >= kMaxSlots)
                { reason = "Invalid pedal automation slot"; break; }
            if (pedal.slot >= 0)
            {
                auto& used = usedSlots[static_cast<size_t> (pedal.slot)];
                if (used) { reason = "Duplicate pedal automation slot"; break; }
                used = true;
            }
            for (size_t j = 0; j < i; ++j)
                if (orderedInstances[j].instanceId == pedal.instanceId)
                    { reason = "Duplicate pedal identifier"; break; }
            if (reason.isNotEmpty()) break;
        }
    }
    if (error != nullptr) *error = reason;
    if (reason.isNotEmpty()) return false;
    // Reserve all explicit slots before filling omissions: an earlier missing
    // slot must not steal a later instance's saved automation assignment.
    for (auto& pedal : restored)
        if (pedal.slot == -1) pedal.slot = firstFreeSlot (restored);
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        impl->instances = std::move (restored);
        impl->settings = settings;
        impl->undoStack.clear();
        impl->redoStack.clear();
        impl->transactionDepth = 0;
        impl->transactionHasEdits = false;
        impl->lastGesture = {};
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
