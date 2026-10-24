// OWNER: WP2
#include "Board.h"
#include "BoardModel.h"
#include "BoardEngine.h"
#include "Midi/MidiRouter.h"
#include "Midi/MidiState.h"
#include <algorithm>
#include <numeric>
#include <mutex>
namespace luthier::reference
{
struct Board::Impl : BoardModel::Listener
{
    BoardModel model;
    BoardEngine engine { model };
    midi::MidiRouter router;
    midi::RouteOut routed;
    int64_t sample = 0;
    int blockSamples = 0;
    bool explicitBlock = false, explicitlyPrepared = false;
    juce::MidiBuffer emptyMidi;
    std::array<int, kMaxSlots> legacyOrder;
    std::array<Placement, kMaxSlots> emptyPlacement {};
    juce::String mapKey;
    std::mutex mapPublishMutex; // E61: message/host-state publishers only; never acquired by audio
    Impl() { std::iota (legacyOrder.begin(), legacyOrder.end(), 0); model.addListener (this); settingsChanged(); }
    ~Impl() override { model.removeListener (this); }
    void settingsChanged() override
    {
        const std::lock_guard<std::mutex> guard (mapPublishMutex);
        std::vector<PedalInstance> instances; BoardSettings settings; model.snapshotStored (instances, settings);
        auto key = juce::JSON::toString (settings.midi) + juce::String (settings.midiChan)
            + ":" + juce::String (settings.enabled ? 1 : 0) + ":" + juce::String (settings.tempoBpm) + ":" + juce::String (static_cast<int> (settings.tempoSource));
        for (const auto& instance : instances)
            key += instance.instanceId.toString() + juce::String (instance.modelId) + juce::String (instance.slot) + juce::JSON::toString (instance.extras["midi"]);
        if (key != mapKey) { router.publishMap (midi::MidiState::buildMap (instances, settings)); mapKey = key; }
    }
    std::optional<PedalInstance> slot (int index) const
    {
        if (index < 0 || index >= kMaxSlots) return {};
        for (const auto& id : model.getOrder())
            if (auto instance = model.getInstance (id); instance && instance->slot == index) return instance;
        return {};
    }
};
Board::Board() : impl (std::make_unique<Impl>()) {}
Board::~Board() = default;
void Board::prepare (double rate, int maxBlock)
{
    rate = std::isfinite (rate) && rate >= 8000.0 ? rate : 48000.0;
    impl->engine.prepare (rate, maxBlock); impl->router.prepare (rate, std::max (1, maxBlock));
    impl->sample = 0; impl->explicitlyPrepared = true;
}
juce::String Board::getPreparationError() const { return impl->engine.preparationError(); }
void Board::releaseResources() { impl->engine.releaseResources(); impl->explicitlyPrepared = false; }
bool Board::setAssetStore (std::shared_ptr<IAssetStore> store)
{ return ! impl->explicitlyPrepared && impl->engine.setAssetStore (std::move (store)); }
void Board::reset() noexcept { impl->engine.requestReset(); }
BoardModel& Board::model() noexcept { return impl->model; }
midi::MidiRouter& Board::midi() noexcept { return impl->router; }
int Board::reclaim() noexcept { impl->engine.drainControlEcho(); return impl->engine.reclaim() + impl->router.reclaim(); }
PedalMeters Board::readPedalMeters (int slot) const noexcept { return impl->engine.readPedalMeters (slot); }
BoardMeters Board::readMeters() const noexcept { auto result = impl->engine.meters(); result.midiDropped += impl->router.droppedCount(); return result; }
void Board::setProcessorLoad (double fraction, bool measuredWindow, double seconds) noexcept
{ impl->engine.setProcessorLoad (fraction, measuredWindow, seconds); }
void Board::setAuxOutputs (const BoardAuxOutputs& outputs) noexcept { impl->engine.setAuxOutputs (outputs); }
void Board::setSidechain (const float* left, const float* right, int samples) noexcept { impl->engine.setSidechain (left, right, samples); }
void Board::setExtendedHostAutomation (const ExtendedHostAutomationSnapshot& snapshot) noexcept
{
    impl->engine.setExtendedHostAutomation (snapshot);
    const auto tempoSource = static_cast<size_t> (ExtendedHostField::TempoSource);
    if (std::isfinite (snapshot.values[tempoSource]))
        impl->router.setHostTempoSource (static_cast<TempoSource> (static_cast<int> (std::clamp (snapshot.values[tempoSource], 0.f, 3.f))), snapshot.serials[tempoSource]);
    for (int source = 1; source <= 2; ++source)
    {
        const auto index = static_cast<size_t> (source == 1 ? ExtendedHostField::Exp1 : ExtendedHostField::Exp2);
        impl->router.setVirtualExpression (source, snapshot.values[index], snapshot.serials[index]);
    }
}
void Board::setHostAutomation (const HostAutomationSnapshot& snapshot) noexcept
{
    impl->engine.setHostAutomation (snapshot);
    impl->router.setHostEnabled (snapshot.values[0] >= 0.5f, snapshot.serials[0]);
    impl->router.setHostTempoBpm (snapshot.values[2], snapshot.serials[2]);
}
void Board::beginBlock (const juce::MidiBuffer& input, const HostTransport& host, int samples) noexcept
{
    impl->explicitBlock = true;
    impl->blockSamples = std::max (0, samples);
    if (impl->engine.preparationFailed())
    {
        impl->routed = {}; impl->routed.time.sample = impl->sample;
        impl->engine.beginRawMidi (impl->emptyMidi, impl->blockSamples);
        impl->engine.setTime (impl->routed.time); impl->engine.beginEvents (impl->routed); return;
    }
    impl->engine.beginRawMidi (input, impl->blockSamples);
    impl->engine.syncRouting (impl->router);
    impl->router.process (input, host, impl->sample, impl->blockSamples, impl->routed);
    impl->engine.setTime (impl->routed.time);
    impl->engine.beginEvents (impl->routed);
}
int Board::drainMidiOut (MidiEvent* destination, int capacity) noexcept { return impl->engine.drainMidiOut (destination, capacity); }
void Board::endBlock (juce::MidiBuffer&) noexcept { impl->sample += impl->blockSamples; impl->explicitBlock = false; impl->engine.endEvents(); }
void Board::processStereo (double* left, double* right, int n, Placement placement) noexcept
{
    const bool standaloneCall = ! impl->explicitBlock;
    if (standaloneCall) beginBlock (impl->emptyMidi, {}, n);
    impl->engine.process (left, right, n, placement);
    if (standaloneCall) endBlock (impl->emptyMidi);
}
int Board::getLatencySamples() const noexcept { return impl->engine.latency(); }
bool Board::isActive() const noexcept { return impl->engine.isActive(); }
void Board::setBoardEnabled (bool value) noexcept { impl->model.apply (cmd::SetBoardEnabled { value }); }
bool Board::isBoardEnabled() const noexcept { return impl->model.getBoardSettings().enabled; }
void Board::setModel (int slot, Model model) noexcept
{
    if (slot < 0 || slot >= kMaxSlots || static_cast<int> (model) < 0 || static_cast<int> (model) >= modelCount) return;
    if (auto instance = impl->slot (slot))
    {
        if (model == Model::empty) impl->model.apply (cmd::Remove { instance->instanceId });
        else impl->model.apply (cmd::SetModel { instance->instanceId, legacyModelId (model) });
    }
    else if (model != Model::empty)
    {
        // The compatibility API explicitly addresses an empty storage slot.
        // Restore assigns that exact slot; all actual instances still use model order.
        std::vector<PedalInstance> instances; BoardSettings settings; impl->model.snapshotStored (instances, settings);
        PedalInstance instance; instance.slot = slot; instance.modelId = legacyModelId (model);
        instance.placement = impl->emptyPlacement[static_cast<size_t> (slot)]; instance.params.fill (0.5f);
        instances.push_back (instance);
        std::stable_sort (instances.begin(), instances.end(), [&] (const auto& a, const auto& b)
        { return std::find (impl->legacyOrder.begin(), impl->legacyOrder.end(), a.slot) < std::find (impl->legacyOrder.begin(), impl->legacyOrder.end(), b.slot); });
        impl->model.replaceAll (instances, settings);
    }
}
Model Board::getModel (int slot) const noexcept
{
    if (auto instance = impl->slot (slot))
        for (int model = 1; model < modelCount; ++model)
            if (instance->modelId == legacyModelId (static_cast<Model> (model))) return static_cast<Model> (model);
    return Model::empty;
}
void Board::setSlotEnabled (int slot, bool enabled) noexcept { if (auto p = impl->slot (slot)) impl->model.apply (cmd::SetEnabled { p->instanceId, enabled }); }
bool Board::isSlotEnabled (int slot) const noexcept { if (auto p = impl->slot (slot)) return p->enabled; return slot >= 0 && slot < kMaxSlots; }
void Board::setPlacement (int slot, Placement placement) noexcept
{
    if (slot < 0 || slot >= kMaxSlots || ! isValidPlacement (static_cast<int> (placement))) return;
    impl->emptyPlacement[static_cast<size_t> (slot)] = placement;
    if (auto p = impl->slot (slot)) impl->model.apply (cmd::SetPlacement { p->instanceId, placement });
}
Placement Board::getPlacement (int slot) const noexcept
{
    if (auto p = impl->slot (slot)) return p->placement;
    return slot >= 0 && slot < kMaxSlots ? impl->emptyPlacement[static_cast<size_t> (slot)] : Placement::beforeExistingPre;
}
void Board::setKnob (int slot, int knob, double value) noexcept
{
    if (knob >= 0 && knob < 3 && std::isfinite (value))
        if (auto p = impl->slot (slot)) impl->model.apply (cmd::SetParam { p->instanceId, knob, static_cast<float> (std::clamp (value, 0.0, 1.0)) });
}
double Board::getKnob (int slot, int knob) const noexcept
{
    if (knob < 0 || knob >= 3) return 0.0;
    if (auto p = impl->slot (slot)) return p->params[static_cast<size_t> (knob)];
    return 0.5;
}
void Board::move (int from, int to) noexcept
{
    if (from < 0 || from >= kMaxSlots || to < 0 || to >= kMaxSlots || from == to) return;
    std::array<int, kMaxSlots> display;
    for (int position = 0; position < kMaxSlots; ++position) display[static_cast<size_t> (position)] = getIdAtPosition (position);
    impl->legacyOrder = display;
    const int slot = impl->legacyOrder[static_cast<size_t> (from)];
    if (from < to) std::move (impl->legacyOrder.begin() + from + 1, impl->legacyOrder.begin() + to + 1, impl->legacyOrder.begin() + from);
    else std::move_backward (impl->legacyOrder.begin() + to, impl->legacyOrder.begin() + from, impl->legacyOrder.begin() + from + 1);
    impl->legacyOrder[static_cast<size_t> (to)] = slot;
    if (const auto moved = impl->slot (slot))
    {
        int audioPosition = 0;
        for (int position = 0; position < to; ++position)
            if (impl->slot (impl->legacyOrder[static_cast<size_t> (position)])) ++audioPosition;
        impl->model.apply (cmd::Move { moved->instanceId, audioPosition });
    }
}
int Board::getIdAtPosition (int position) const noexcept
{
    if (position < 0 || position >= kMaxSlots) return -1;
    const auto order = impl->model.getOrder();
    const int storedSlot = impl->legacyOrder[static_cast<size_t> (position)];
    if (! impl->slot (storedSlot)) return storedSlot;
    int audioPosition = 0;
    for (int index = 0; index < position; ++index)
        if (impl->slot (impl->legacyOrder[static_cast<size_t> (index)])) ++audioPosition;
    return impl->model.getInstance (order[static_cast<size_t> (audioPosition)])->slot;
}
}
