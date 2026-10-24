// OWNER: WP2
// Message-thread board edits and bounded snapshot history. Audio topology
// compilation/publication remains a separate integration work package.

#include "BoardModel.h"
#include "PedalRegistry.h"
#include "Midi/MidiState.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <limits>
#include <utility>
#include <mutex>

namespace luthier::reference
{

namespace
{
    // All scalar commands validate before assigning, so a malformed value
    // cannot contaminate a snapshot or consume an undo step.
    bool setBoardScalar (BoardSettings& settings, const std::string& key, double value)
    {
        if (! std::isfinite (value)) return false;
        const auto integer = [&] (auto& target, int minimum, int maximum)
        {
            if (value < minimum || value > maximum || std::floor (value) != value) return false;
            target = static_cast<std::decay_t<decltype (target)>> (value);
            return true;
        };
        const auto real = [&] (auto& target, double minimum, double maximum)
        {
            if (value < minimum || value > maximum) return false;
            target = static_cast<std::decay_t<decltype (target)>> (value);
            return true;
        };
        const std::pair<const char*, bool BoardSettings::*> flags[] {
            { "reportLatency", &BoardSettings::reportLatency }, { "limiterOn", &BoardSettings::limiterOn },
            { "trails", &BoardSettings::trails }, { "mixTail", &BoardSettings::mixTail },
            { "reampEnable", &BoardSettings::reampEnable }, { "reampReplace", &BoardSettings::reampReplace }
        };
        for (const auto& flag : flags)
            if (key == flag.first) return integer (settings.*flag.second, 0, 1);

        if (key == "tempoBpm") return real (settings.tempoBpm, 20.0, 300.0);
        if (key == "cpuCapPct") return integer (settings.cpuCapPct, 1, 100);
        if (key == "midiChan") return integer (settings.midiChan, 0, 16);
        if (key == "abSelect") return integer (settings.abSelect, 0, static_cast<int> (AbSelect::Toggle));
        if (key == "abOut") return integer (settings.abOut, 0, static_cast<int> (AbOutMode::Dual));
        if (key == "parallelNorm") return integer (settings.lanes.norm, 0, static_cast<int> (ParallelNorm::None));
        if (key == "tempoSource") return integer (settings.tempoSource, 0, static_cast<int> (TempoSource::Auto));
        if (key == "numLanes") return integer (settings.lanes.numLanes, 2, kMaxBranches);
        if (key == "limiterCeilDb") return real (settings.limiterCeilDb, -6.0, 6.0);
        // A finite negative dB value may approach silence. Guard the float
        // conversion as well as the specified upper gain limits.
        constexpr double lowestGain = -std::numeric_limits<float>::max();
        if (key == "reampGainDb") return real (settings.reampGainDb, lowestGain, 12.0);
        for (int lane = 0; lane < kMaxBranches; ++lane)
        {
            const auto suffix = std::to_string (lane);
            if (key == "laneGainDb" + suffix)
                return real (settings.lanes.gainDb[static_cast<size_t> (lane)], lowestGain, 6.0);
            if (key == "lanePhase" + suffix)
                return integer (settings.lanes.phaseInvert[static_cast<size_t> (lane)], 0, 1);
        }
        return false;
    }

    bool inRange (double value, double minimum, double maximum)
    {
        return std::isfinite (value) && value >= minimum && value <= maximum;
    }

    bool validBoardSettings (const BoardSettings& settings)
    {
        if (settings.mode > Mode::Loop || settings.mixLaw > MixLaw::ThirdLaw
            || settings.style > BoardStyle::loose || ! inRange (settings.mix, 0.0, 1.0))
            return false;
        // Reuse the command bounds when restoring state; deserialisation must
        // not provide a second, less restrictive path into the model.
        BoardSettings checked;
        const std::pair<const char*, double> scalars[] {
            { "abSelect", static_cast<int> (settings.abSelect) },
            { "abOut", static_cast<int> (settings.abOut) },
            { "parallelNorm", static_cast<int> (settings.lanes.norm) },
            { "tempoSource", static_cast<int> (settings.tempoSource) },
            { "numLanes", settings.lanes.numLanes }, { "tempoBpm", settings.tempoBpm },
            { "cpuCapPct", settings.cpuCapPct }, { "midiChan", settings.midiChan },
            { "reampGainDb", settings.reampGainDb }, { "limiterCeilDb", settings.limiterCeilDb }
        };
        for (const auto& scalar : scalars)
            if (! setBoardScalar (checked, scalar.first, scalar.second)) return false;
        for (int lane = 0; lane < kMaxBranches; ++lane)
            if (! setBoardScalar (checked, "laneGainDb" + std::to_string (lane),
                                  settings.lanes.gainDb[static_cast<size_t> (lane)]))
                return false;
        for (const auto& loop : settings.loops)
            if (loop.kind > LoopKind::KillDry || ! inRange (loop.blend, 0.0, 1.0)
                || ! inRange (loop.sendDb, -24.0, 12.0) || ! inRange (loop.returnDb, -24.0, 12.0))
                return false;
        return inRange (settings.fb.gain, 0.0, 1.05f)
            && std::isfinite (settings.fb.lpHz) && settings.fb.lpHz > 0.f;
    }

    BoardSettings cloneSettings (const BoardSettings& input)
    {
        auto result = input; result.midi = input.midi.clone(); result.ui = input.ui.clone(); result.unknown = input.unknown.clone(); return result;
    }
    std::vector<PedalInstance> cloneInstances (const std::vector<PedalInstance>& input)
    {
        auto result = input; for (auto& instance : result) instance.extras = instance.extras.clone(); return result;
    }
    bool validPedalControls (const PedalInstance& pedal)
    {
        return ! pedal.modelId.empty() && pedal.modelRevision >= 1
            && inRange (pedal.inDb, -12.0, 12.0) && inRange (pedal.outDb, -12.0, 12.0)
            && std::isfinite (pedal.x) && pedal.mounting <= Mounting::none
            && std::all_of (pedal.params.begin(), pedal.params.end(),
                            [] (float value) { return inRange (value, 0.0, 1.0); });
    }

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
    TopologyInfo info;
    Compiler compiler;
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
{ return applyImpl (command, rejectReason, false); }
bool BoardModel::applyHostEcho (const BoardCommand& command, juce::String* rejectReason)
{
    const auto* scalar = std::get_if<cmd::SetBoardParam> (&command);
    if (! std::holds_alternative<cmd::SetParam> (command) && ! std::holds_alternative<cmd::SetEnabled> (command)
        && ! std::holds_alternative<cmd::SetTrim> (command) && ! std::holds_alternative<cmd::SetMix> (command)
        && ! std::holds_alternative<cmd::SetBoardEnabled> (command) && ! (scalar && scalar->key == "tempoBpm"))
    { if (rejectReason) *rejectReason = "Host echo requires a scalar control"; return false; }
    return applyImpl (command, rejectReason, true);
}
bool BoardModel::applyImpl (const BoardCommand& command, juce::String* rejectReason, bool hostEcho)
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
                if (impl->compiler)
                    if (const auto* desc = PedalRegistry::find (p.modelId.c_str()))
                        for (int i = 0; i < desc->numParams; ++i) p.params[static_cast<size_t> (i)] = desc->params[i].defaultNormalised();
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
            else if constexpr (std::is_same_v<T, cmd::SetModel>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end()) return false;
                const auto* desc = PedalRegistry::find (cmd.modelId.c_str());
                if (! desc) return false;
                it->modelId = cmd.modelId; it->modelRevision = desc->revision; it->missing = false;
                it->params.fill (0.f); it->extras = juce::var();
                for (int i = 0; i < desc->numParams; ++i) it->params[static_cast<size_t> (i)] = desc->params[i].defaultNormalised();
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetLoop>)
            {
                if (cmd.loop < 0 || cmd.loop >= kMaxLoops) return false;
                next.loops[static_cast<size_t> (cmd.loop)] = cmd.settings;
                if (! validBoardSettings (next)) return false;
            }
            else if constexpr (std::is_same_v<T, cmd::SetFeedback>)
            {
                next.fb = cmd.settings;
                if (! validBoardSettings (next)) return false;
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
            else if constexpr (std::is_same_v<T, cmd::SetSlotChannel>)
            {
                auto it = findPedal (cmd.id);
                if (it == nextInstances.end() || cmd.chan < 0 || cmd.chan > 16) return false;
                it->extras = it->extras.clone();
                if (! it->extras.getDynamicObject()) it->extras = new juce::DynamicObject();
                juce::var midi (new juce::DynamicObject());
                midi.getDynamicObject()->setProperty ("chan", cmd.chan);
                midi.getDynamicObject()->setProperty ("useProfile", cmd.useProfile);
                it->extras.getDynamicObject()->setProperty ("midi", midi);
                changedId = cmd.id;
            }
            else if constexpr (std::is_same_v<T, cmd::SetMidiBinding>)
            {
                if (! cmd.binding.getDynamicObject() || ! cmd.binding.getDynamicObject()->hasProperty ("id")) return false;
                next.midi = next.midi.clone();
                if (! next.midi.getDynamicObject()) next.midi = new juce::DynamicObject();
                auto* object = next.midi.getDynamicObject();
                if (! object->getProperty ("bindings").isArray()) object->setProperty ("bindings", juce::Array<juce::var>());
                auto* bindings = object->getProperty ("bindings").getArray();
                int found = -1;
                for (int i = 0; i < bindings->size(); ++i)
                    if ((*bindings)[i]["id"] == cmd.binding["id"]) { found = i; break; }
                if (cmd.op == cmd::SetMidiBinding::Op::Add)
                { if (found >= 0 || bindings->size() >= midi::kMaxBindings) return false; bindings->add (cmd.binding.clone()); }
                else
                {
                    if (found < 0) return false;
                    if (cmd.op == cmd::SetMidiBinding::Op::Remove) bindings->remove (found);
                    else bindings->set (found, cmd.binding.clone());
                }
                if (! midi::MidiState::validate (next.midi)) return false;
            }
            else if constexpr (std::is_same_v<T, cmd::SetBoardParam>)
            {
                if (! setBoardScalar (next, cmd.key, cmd.value)) return false;
            }
            else return false; // Unsupported commands must never report success.
            return true;
        }, command);
        auto info = impl->info;
        const bool compiled = supported && (! impl->compiler || impl->compiler (nextInstances, next, info, reason, &command, hostEcho));
        if (compiled)
        {
            impl->info = info;
            if (! hostEcho) impl->recordUndo (command);
            impl->settings = std::move (next);
            impl->instances = std::move (nextInstances);
            settingsChanged = true;
        }
        else if (reason.isEmpty()) reason = juce::String ("Unsupported or invalid board command: ") + commandName (command);
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
        if (pedal.instanceId == id) { auto copy = pedal; copy.extras = pedal.extras.clone(); return copy; }
    return std::nullopt;
}
TopologyInfo BoardModel::getTopologyInfo() const { const std::lock_guard<std::mutex> lock (impl->mutex); return impl->info; }

BoardSettings BoardModel::getBoardSettings() const
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    return cloneSettings (impl->settings);
}

bool BoardModel::undo()
{
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        if (impl->transactionDepth != 0 || impl->undoStack.empty()) return false;
        juce::String error; auto info = impl->info;
        const auto& candidate = impl->undoStack.back();
        if (impl->compiler && ! impl->compiler (candidate.instances, candidate.settings, info, error, nullptr, false)) return false;
        impl->info = info;
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
        juce::String error; auto info = impl->info;
        const auto& candidate = impl->redoStack.back();
        if (impl->compiler && ! impl->compiler (candidate.instances, candidate.settings, info, error, nullptr, false)) return false;
        impl->info = info;
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
    std::array<bool, kMaxSlots> usedSlots {};
    if (orderedInstances.size() > static_cast<size_t> (kMaxSlots))
        reason = "Board exceeds 16 pedal slots";
    else if (! validBoardSettings (settings))
        reason = "Invalid board settings";
    else
    {
        for (size_t i = 0; i < orderedInstances.size(); ++i)
        {
            const auto& pedal = orderedInstances[i];
            if (pedal.instanceId.isNull() || ! isValidPlacement (static_cast<int> (pedal.placement))
                || pedal.lane < 0 || pedal.lane >= kMaxBranches)
                { reason = "Invalid pedal identifier, placement or lane"; break; }
            if (! validPedalControls (pedal))
                { reason = "Invalid pedal controls"; break; }
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
    auto restored = cloneInstances (orderedInstances);
    // Reserve all explicit slots before filling omissions: an earlier missing
    // slot must not steal a later instance's saved automation assignment.
    for (auto& pedal : restored)
        if (pedal.slot == -1) pedal.slot = firstFreeSlot (restored);
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        auto info = impl->info;
        if (impl->compiler && ! impl->compiler (restored, settings, info, reason, nullptr, false))
        { if (error) *error = reason; return false; }
        impl->info = info;
        impl->instances = std::move (restored);
        impl->settings = cloneSettings (settings);
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
    instances = cloneInstances (impl->instances);
    settings = cloneSettings (impl->settings);
}

void BoardModel::setCompiler (Compiler compiler)
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    impl->compiler = std::move (compiler);
}
bool BoardModel::setMidiFocus (const InstanceId& id)
{
    {
        const std::lock_guard<std::mutex> lock (impl->mutex);
        if (! id.isNull() && std::none_of (impl->instances.begin(), impl->instances.end(),
            [&] (const auto& instance) { return instance.instanceId == id; })) return false;
        impl->settings.midi = impl->settings.midi.clone();
        if (! impl->settings.midi.getDynamicObject()) impl->settings.midi = new juce::DynamicObject();
        impl->settings.midi.getDynamicObject()->setProperty ("selectedInstance", id.isNull() ? juce::String() : id.toString());
    }
    for (auto* listener : impl->listeners) listener->settingsChanged();
    return true;
}
void BoardModel::setUiState (juce::var state)
{
    const std::lock_guard<std::mutex> lock (impl->mutex);
    impl->settings.ui = state.clone();
}

} // namespace luthier::reference
