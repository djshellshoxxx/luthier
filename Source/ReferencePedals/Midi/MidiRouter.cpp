// OWNER: WP5
#include "MidiRouter.h"
#include "MidiMap.h"
#include "ClockSync.h"
#include "../Rt/SpscRing.h"
#include <atomic>
#include <array>
#include <vector>
#include <mutex>
namespace luthier::reference::midi
{
namespace {
struct CaptureRecord { LearnCapture capture; uint32_t generation = 0; };
bool clockMessage (uint8_t status) noexcept
{ return status == 0xf8 || status == 0xfa || status == 0xfb || status == 0xfc || status == 0xf2; }
bool releaseMessage (const MidiEv& event) noexcept
{
    const auto kind = event.status & 0xf0;
    return kind == 0x80 || (kind == 0x90 && event.d2 == 0)
        || (kind == 0xb0 && (event.d1 >= 120 || ((event.d1 == 64 || event.d1 == 66 || event.d1 == 67) && event.d2 < 64)));
}
}
struct MidiRouter::Impl
{
    struct Retired { std::unique_ptr<MapSnap> map; };
    std::atomic<const MapSnap*> published { nullptr };
    std::unique_ptr<MapSnap> current;
    std::vector<Retired> retired;
    std::mutex nonRealtimeWriterMutex;
    int collectRetired() noexcept
    {
        int freed = 0;
        const auto* reader = borrowed.load (std::memory_order_seq_cst);
        for (size_t i = 0; i < retired.size();)
            if (reader != retired[i].map.get()) { retired.erase (retired.begin() + (std::ptrdiff_t) i); ++freed; } else ++i;
        return freed;
    }
    std::atomic<const MapSnap*> borrowed { nullptr };
    std::atomic<uint32_t> dropped { 0 };
    SpscRing<MidiEv, 1024> input;
    SpscRing<MidiEv, 256> priority;
    SpscRing<UiEv, 512> feedback;
    SpscRing<CaptureRecord, 16> captures;
    std::atomic<uint32_t> learn { 0 }; // generation + armed + continuous; message producer
    uint32_t learnSeen = 0;
    LearnCapture previous;
    double sr = 48000;
    int64_t candidateAt = 0;
    ClockSync clock;
    std::array<MidiEv, 768> events {};
    std::array<float, kMaxBindings> values {}, lastSource {};
    std::array<bool, kMaxBindings> held {}, picked {};
    std::array<float, kTargetBoardBase + 16> targetValues {};
    std::array<uint32_t, kTargetBoardBase + 16> targetIntentRevisions {}, targetGenerations {};
    std::array<uint32_t, kMaxBindings> pickedGenerations {};
    std::array<bool, kTargetBoardBase + 16> targetKnown {};
    void updateTarget (uint16_t target, float value, int binding = -1) noexcept
    {
        targetValues[target] = value; targetKnown[target] = true;
        if (++targetGenerations[target] == 0) ++targetGenerations[target];
        if (binding >= 0) pickedGenerations[(size_t) binding] = targetGenerations[target];
    }
    std::array<std::array<uint8_t, 32>, 16> msb {}, lsb {};
    std::array<std::array<bool, 32>, 16> hasMsb {};
    std::array<float, 2> hostExpression {}, expressionValue {};
    std::array<uint32_t, 2> hostExpressionSerial {}, appliedExpressionSerial {};
    std::array<bool, 2> expressionKnown {};
    bool replayExpression = false;
    template <typename T> struct Control
    {
        T effective {}, model {}, host {};
        uint32_t modelRevision = 0, appliedModel = 0, hostSerial = 0, appliedHost = 0;
        bool known = false;
        void consume (T fallback) noexcept
        {
            if (modelRevision == 0 && ! known) { effective = model = fallback; known = true; }
            if (modelRevision != 0 && modelRevision != appliedModel) { effective = model; appliedModel = modelRevision; known = true; }
            if (hostSerial != 0 && hostSerial != appliedHost) { effective = host; appliedHost = hostSerial; }
        }
        void reset() noexcept { appliedModel = appliedHost = 0; known = false; }
    };
    Control<TempoSource> tempoSource;
    Control<double> tempoBpm;
    std::array<bool, kMaxSlots> footHeld {};
    std::array<std::array<int8_t, 128>, 16> heldNoteSlots = []
    { std::array<std::array<int8_t, 128>, 16> a {}; for (auto& channel : a) channel.fill (-1); return a; }();
    uint32_t mapVersion = 0, activeVersion = 0;
    bool uiClockLocked = false, uiClockLost = false;
    double uiTempo = 0;
    uint32_t uiDropped = 0;
    int64_t routeSample = 0;
    bool effectiveEnabled = true, modelEnabled = true, modelEnabledKnown = false, hostEnabled = false;
    uint32_t hostEnabledSerial = 0, appliedHostEnabledSerial = 0;
    bool modelIntentEnabled = false;
    uint32_t modelEnabledRevision = 0, appliedModelEnabledRevision = 0;
    std::array<int64_t, kTargetBoardBase + 16> lastWrite {};
    std::array<uint8_t, kTargetBoardBase + 16> ownerPriority {};
    void drop() noexcept { dropped.fetch_add (1, std::memory_order_relaxed); }
    void foot (int slot, bool down, float speed, int offset, RouteOut& out) noexcept
    {
        if (slot < 0 || slot >= kMaxSlots || footHeld[(size_t) slot] == down) return;
        if (out.nFoot == kMaxRouteFoot) { drop(); return; }
        footHeld[(size_t) slot] = down;
        out.footSlot[out.nFoot] = (uint8_t) slot;
        out.foot[out.nFoot++] = { down, speed, 0, offset };
    }
    void route (const MapSnap& map, MidiSrc type, int ch, int num, float value, int offset, RouteOut& out) noexcept
    {
        const auto& bucket = map.find (type, ch, num);
        for (int k = 0; k < bucket.count; ++k)
        {
            const auto index = bucket.index[(size_t) k]; const auto& b = map.bindings[index];
            if (! effectiveEnabled && b.target != kTargetBoardBase + (uint16_t) BoardTarget::Enabled) continue;
            const bool down = type == MidiSrc::Note ? value > 0.f : value >= .5f, rising = down && ! held[index];
            float v = std::clamp ((value - b.inMin) / (b.inMax - b.inMin), 0.f, 1.f);
            if (b.has (BF_Invert)) v = 1.f - v;
            v = b.outMin + evalCurve (map.curves[index], v) * (b.outMax - b.outMin);
            bool emit = true;
            switch (b.mode())
            {
                case BindMode::Toggle: emit = rising; v = targetValues[b.target] < .5f ? 1.f : 0.f; break;
                case BindMode::Momentary: v = down ? 1.f : 0.f; break;
                case BindMode::Trigger: emit = rising; v = 1; break;
                case BindMode::Relative:
                { const int raw = (int) std::round (value * 127); v = targetValues[b.target] + (raw >= 64 ? raw - 128 : raw) / 128.f; break; }
                case BindMode::Step:
                    if (map.steps[index] >= 2) v = std::round (v * (map.steps[index] - 1)) / (map.steps[index] - 1);
                    else emit = false;
                    break;
                default: break;
            }
            if (pickedGenerations[index] != targetGenerations[b.target])
            { picked[index] = false; pickedGenerations[index] = targetGenerations[b.target]; }
            if (b.has (BF_SoftTakeover) && b.mode() == BindMode::Absolute && ! picked[index])
            {
                const float current = targetValues[b.target];
                const float old = lastSource[index];
                if (std::abs (v - current) <= .03f || (old < current && v >= current) || (old > current && v <= current)) picked[index] = true;
                else emit = false;
            }
            held[index] = down; lastSource[index] = v;
            if (! emit) continue;
            if (b.priority < ownerPriority[b.target] && routeSample - lastWrite[b.target] < (int64_t) (sr * .3)) continue;
            if (out.nWrites == kMaxRouteWrites) { drop(); continue; }
            ownerPriority[b.target] = b.priority; lastWrite[b.target] = routeSample;
            values[index] = std::clamp (v, 0.f, 1.f);
            updateTarget (b.target, values[index], index);
            if (b.target == kTargetBoardBase + (uint16_t) BoardTarget::Tempo)
            { tempoBpm.effective = 20.0 + 280.0 * values[index]; clock.setInternalTempo (tempoBpm.effective, routeSample); }
            if (b.target == kTargetBoardBase + (uint16_t) BoardTarget::Enabled)
            {
                effectiveEnabled = values[index] >= .5f;
                if (! effectiveEnabled) { footHeld.fill (false); for (auto& channel : heldNoteSlots) channel.fill (-1); }
            }
            out.writes[out.nWrites++] = { b.target >= kTargetBoardBase ? kBoardTargetSlot : (uint8_t) (b.target / kMaxParams),
                b.target >= kTargetBoardBase ? (uint16_t) (b.target - kTargetBoardBase) : (uint16_t) (b.target % kMaxParams), values[index], b.smoothMs, offset };
            const auto& write = out.writes[out.nWrites - 1];
            feedback.push ({ UiEvKind::Activity, write.slot, write.paramIdx, write.norm, (uint32_t) offset, 0 });
        }
    }
    bool learnEvent (MidiSrc type, int ch, int num, int value, int64_t t) noexcept
    {
        const auto state = learn.load (std::memory_order_acquire);
        if ((state & 1u) == 0) return false;
        if (state != learnSeen) { previous = {}; learnSeen = state; }
        if (type == MidiSrc::CC && reservedCc (num)) return true;
        const bool same = previous.valid && previous.type == type && previous.chan == ch && previous.num == num;
        bool accept = (state & 2u) == 0 || type == MidiSrc::Note || type == MidiSrc::ProgramChange;
        if (! accept && same && t - candidateAt <= (int64_t) (sr * .4))
            accept = type == MidiSrc::CC ? previous.value != value : std::abs ((int) previous.value - value) > 12;
        if (accept)
        {
            captures.push ({ { true, type, (uint8_t) ch, (uint8_t) num, (uint8_t) value }, state & ~3u });
            auto expected = state; learn.compare_exchange_strong (expected, state & ~1u, std::memory_order_acq_rel);
        }
        else if (! same || t - candidateAt > (int64_t) (sr * .4))
        { previous = { true, type, (uint8_t) ch, (uint8_t) num, (uint8_t) value }; candidateAt = t; }
        return true;
    }
};
MidiRouter::MidiRouter() : impl (std::make_unique<Impl>())
{
    // One audio reader can protect only one old map. Collection before each
    // publication therefore leaves at most one entry; retiring current adds one.
    // Allocate both slots before any publication, never after exchanging owners.
    impl->retired.reserve (2);
}
MidiRouter::~MidiRouter() = default;
void MidiRouter::prepare (double sr, int)
{
    const std::lock_guard<std::mutex> writer (impl->nonRealtimeWriterMutex);
    impl->sr = std::isfinite (sr) && sr > 0 ? sr : 48000;
    impl->clock.prepare (impl->sr); impl->input.clear(); impl->priority.clear(); impl->feedback.clear();
    impl->uiClockLocked = impl->uiClockLost = false; impl->uiTempo = 0; impl->uiDropped = droppedCount();
    for (auto& channel : impl->heldNoteSlots) channel.fill (-1);
    impl->footHeld.fill (false); impl->held.fill (false); impl->values.fill (0);
    impl->targetValues = {}; impl->targetIntentRevisions = {}; impl->targetGenerations = {}; impl->pickedGenerations = {}; impl->targetKnown = {};
    impl->msb = {}; impl->lsb = {}; impl->hasMsb = {};
    impl->tempoSource.reset(); impl->tempoBpm.reset();
    impl->appliedExpressionSerial = {}; impl->expressionKnown = {}; impl->replayExpression = false;
    impl->borrowed.store (nullptr, std::memory_order_seq_cst);
    impl->retired.clear(); impl->activeVersion = 0; impl->modelEnabledKnown = false; impl->appliedHostEnabledSerial = 0; impl->appliedModelEnabledRevision = 0;
}
void MidiRouter::publishMap (std::unique_ptr<MapSnap> map)
{
    const std::lock_guard<std::mutex> writer (impl->nonRealtimeWriterMutex);
    // Reclaim before publication: at most the reader's snapshot plus one newly
    // retired map remain, even while audio is stopped and no epochs advance.
    impl->collectRetired();
    if (map) { if (++impl->mapVersion == 0) ++impl->mapVersion; map->version = impl->mapVersion; map->compile(); }
    auto old = std::move (impl->current); impl->current = std::move (map);
    impl->published.store (impl->current.get(), std::memory_order_seq_cst);
    if (old) impl->retired.push_back ({ std::move (old) });
}
int MidiRouter::reclaim() noexcept
{
    const std::lock_guard<std::mutex> writer (impl->nonRealtimeWriterMutex);
    return impl->collectRetired();
}
void MidiRouter::setModelEnabled (bool enabled, uint32_t revision) noexcept
{ if (revision != 0) { impl->modelIntentEnabled = enabled; impl->modelEnabledRevision = revision; } }
void MidiRouter::setHostEnabled (bool enabled, uint32_t serial) noexcept
{ if (serial != 0) { impl->hostEnabled = enabled; impl->hostEnabledSerial = serial; } }
void MidiRouter::setTargetValue (uint16_t target, float normalised, uint32_t revision) noexcept
{
    if (target >= impl->targetValues.size() || revision == 0 || ! std::isfinite (normalised) || revision == impl->targetIntentRevisions[target]) return;
    impl->targetIntentRevisions[target] = revision;
    impl->updateTarget (target,std::clamp (normalised,0.f,1.f)); impl->ownerPriority[target] = 0;
}
void MidiRouter::setHostTempoSource (TempoSource source, uint32_t serial) noexcept
{ if (serial != 0 && source <= TempoSource::Auto) { impl->tempoSource.host = source; impl->tempoSource.hostSerial = serial; } }
void MidiRouter::setModelTempoSource (TempoSource source, uint32_t revision) noexcept
{ if (revision != 0 && source <= TempoSource::Auto) { impl->tempoSource.model = source; impl->tempoSource.modelRevision = revision; } }
void MidiRouter::setHostTempoBpm (double bpm, uint32_t serial) noexcept
{ if (serial != 0 && std::isfinite (bpm)) { impl->tempoBpm.host = std::clamp (bpm,20.,300.); impl->tempoBpm.hostSerial = serial; } }
void MidiRouter::setModelTempoBpm (double bpm, uint32_t revision) noexcept
{ if (revision != 0 && std::isfinite (bpm)) { impl->tempoBpm.model = std::clamp (bpm,20.,300.); impl->tempoBpm.modelRevision = revision; } }
void MidiRouter::setVirtualExpression (int source, float normalised, uint32_t serial) noexcept
{
    if (source < 1 || source > 2 || serial == 0 || ! std::isfinite (normalised)) return;
    impl->hostExpression[(size_t) source - 1] = std::clamp (normalised, 0.f, 1.f);
    impl->hostExpressionSerial[(size_t) source - 1] = serial;
}
void MidiRouter::pushUiEvent (const MidiEv& e) noexcept
{ if (clockMessage (e.status)) impl->priority.push (e); else impl->input.push (e); }
void MidiRouter::process (const juce::MidiBuffer& in, const HostTransport& host, int64_t start, int n, RouteOut& out) noexcept
{
    out.clear(); if (n <= 0) return;
    const MapSnap* map = nullptr; bool protectedMap = false;
    // Protect then validate before dereferencing. Publisher can never delete a
    // protected map; a raced candidate is never dereferenced. Two attempts bound
    // callback work under a continuously publishing message thread.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const auto* candidate = impl->published.load (std::memory_order_seq_cst);
        impl->borrowed.store (candidate, std::memory_order_seq_cst);
        if (candidate == impl->published.load (std::memory_order_seq_cst))
        { map = candidate; protectedMap = true; break; }
        impl->borrowed.store (nullptr, std::memory_order_seq_cst);
    }
    if (! protectedMap)
    {
        impl->drop();
        // Losing a raced map must not strand a held momentary control.
        for (int slot = 0; slot < kMaxSlots; ++slot) impl->foot (slot,false,0.f,0,out);
        for (auto& channel : impl->heldNoteSlots) channel.fill (-1);
    }
    struct BorrowScope
    {
        std::atomic<const MapSnap*>& pointer;
        ~BorrowScope() { pointer.store (nullptr, std::memory_order_seq_cst); }
    } borrowScope { impl->borrowed };
    if (map && map->version != impl->activeVersion)
    {
        if (impl->modelEnabledRevision == 0 && (! impl->modelEnabledKnown || impl->modelEnabled != map->boardEnabled))
        { impl->effectiveEnabled = impl->modelEnabled = map->boardEnabled; impl->modelEnabledKnown = true; }
        if (! impl->effectiveEnabled) { impl->footHeld.fill (false); for (auto& channel : impl->heldNoteSlots) channel.fill (-1); }
        if (impl->tempoSource.modelRevision == 0 && impl->tempoSource.hostSerial == 0) impl->tempoSource.known = false;
        if (impl->tempoBpm.modelRevision == 0 && impl->tempoBpm.hostSerial == 0) impl->tempoBpm.known = false;
        impl->replayExpression = true;
        impl->activeVersion = map->version; impl->held.fill (false); impl->picked.fill (false);
        impl->values = map->initialValues; impl->lastSource = map->initialValues;
        for (int i = 0; i < map->numBindings; ++i)
            if (! impl->targetKnown[map->bindings[(size_t) i].target])
                impl->updateTarget (map->bindings[(size_t) i].target,map->initialValues[(size_t) i]);
        impl->lastWrite.fill (0); impl->ownerPriority.fill (0);
    }
    if (impl->modelEnabledRevision != 0 && impl->modelEnabledRevision != impl->appliedModelEnabledRevision)
    {
        impl->effectiveEnabled = impl->modelEnabled = impl->modelIntentEnabled;
        impl->modelEnabledKnown = true; impl->appliedModelEnabledRevision = impl->modelEnabledRevision;
    }
    if (impl->hostEnabledSerial != 0 && impl->hostEnabledSerial != impl->appliedHostEnabledSerial)
    { impl->effectiveEnabled = impl->hostEnabled; impl->appliedHostEnabledSerial = impl->hostEnabledSerial; }
    if (! impl->effectiveEnabled)
    { impl->footHeld.fill (false); for (auto& channel : impl->heldNoteSlots) channel.fill (-1); }
    if (map && impl->effectiveEnabled)
    {
        impl->routeSample = start;
        for (size_t source = 0; source < 2; ++source)
        {
            const bool fresh = impl->hostExpressionSerial[source] != 0 && impl->hostExpressionSerial[source] != impl->appliedExpressionSerial[source];
            if (fresh)
            { impl->expressionValue[source] = impl->hostExpression[source]; impl->appliedExpressionSerial[source] = impl->hostExpressionSerial[source]; impl->expressionKnown[source] = true; }
            if (impl->expressionKnown[source] && (fresh || impl->replayExpression))
                impl->route (*map, MidiSrc::VirtualExpression, 1, (int) source + 1, impl->expressionValue[source], 0, out);
        }
        impl->replayExpression = false;
    }
    impl->tempoSource.consume (map ? map->tempoSource : TempoSource::Auto);
    impl->tempoBpm.consume (map ? map->internalBpm : 120.);
    impl->clock.begin (host, impl->tempoSource.effective, impl->tempoBpm.effective, start, out);
    int count = 0, ordinary = 0;
    auto append = [&] (MidiEv e, bool realtime)
    {
        if (e.status < 0x80 || e.d1 > 127 || e.d2 > 127) { impl->drop(); return; }
        if ((! realtime && ordinary >= 512) || count >= (int) impl->events.size())
        {
            // A late release replaces the newest ordinary non-release. Clock
            // priority and earlier releases retain their reserved positions.
            int replace = -1;
            if (! realtime && releaseMessage (e))
                for (int i = count - 1; i >= 0; --i)
                    if (! clockMessage (impl->events[(size_t) i].status) && ! releaseMessage (impl->events[(size_t) i])) { replace = i; break; }
            impl->drop();
            if (replace < 0) return;
            for (int i = replace + 1; i < count; ++i) impl->events[(size_t) i - 1] = impl->events[(size_t) i];
            --count; --ordinary;
        }
        if (! realtime) ++ordinary;
        e.sampleOffset = (uint32_t) std::clamp ((int) e.sampleOffset, 0, n - 1);
        int j = count++; // stable insertion by sample offset, storage is fixed
        while (j > 0 && impl->events[(size_t) j - 1].sampleOffset > e.sampleOffset)
        { impl->events[(size_t) j] = impl->events[(size_t) j - 1]; --j; }
        impl->events[(size_t) j] = e;
    };
    // First pass reserves capacity for host transport/clock even after a controller flood.
    for (const auto md : in)
        if (md.numBytes > 0 && clockMessage (md.data[0]))
            append ({ (uint32_t) std::max (0, md.samplePosition), md.data[0], md.numBytes > 1 ? md.data[1] : uint8_t (0), md.numBytes > 2 ? md.data[2] : uint8_t (0), 0, 0, 0 }, true);
    MidiEv e;
    for (int i = 0; i < 256 && impl->priority.pop (e); ++i) { e.sampleOffset = 0; append (e, true); }
    for (const auto md : in)
        if (md.numBytes > 0 && md.data[0] < 0xf0)
            append ({ (uint32_t) std::max (0, md.samplePosition), md.data[0], md.numBytes > 1 ? md.data[1] : uint8_t (0), md.numBytes > 2 ? md.data[2] : uint8_t (0), 0, 0, 0 }, false);
    for (int i = 0; i < 1024 && impl->input.pop (e); ++i) { e.sampleOffset = 0; append (e, false); }
    for (int i = 0; i < count; ++i)
    {
        e = impl->events[(size_t) i]; const int off = (int) e.sampleOffset, kind = e.status & 0xf0, ch = (e.status & 15) + 1;
        if (e.status < 0x80 || e.d1 > 127 || e.d2 > 127) { impl->drop(); continue; }
        if (e.status >= 0xf0) { impl->clock.event (e.status, e.d1, e.d2, start + off, out); continue; }
        if (map && map->midiChan != 0 && map->midiChan != ch) continue;
        if (kind == 0xb0 && (e.d1 == 120 || e.d1 == 123))
        { for (int slot = 0; slot < kMaxSlots; ++slot) impl->foot (slot, false, .5f, off, out); for (auto& channel : impl->heldNoteSlots) channel.fill (-1); continue; }
        MidiSrc type = MidiSrc::CC; int num = e.d1; float value = e.d2 / 127.f;
        switch (kind)
        {
            case 0x80: type = MidiSrc::Note; value = 0; break;
            case 0x90: type = MidiSrc::Note; break;
            case 0xa0: type = MidiSrc::PolyAT; break;
            case 0xb0: if (reservedCc (num)) continue; break;
            case 0xc0: type = MidiSrc::ProgramChange; value = e.d1 / 127.f; break;
            case 0xd0: type = MidiSrc::ChanAT; num = 0; value = e.d1 / 127.f; break;
            case 0xe0: type = MidiSrc::PitchBend; num = 0; value = (e.d1 | (e.d2 << 7)) / 16383.f; break;
            default: continue;
        }
        if (e.src == (uint8_t) MidiOrigin::UiSlider) type = MidiSrc::UiSlider;
        if (impl->learnEvent (type, ch, num, (int) (value * 127), start + off)) continue;
        if (! map) continue;
        impl->routeSample = start + off;
        if (type == MidiSrc::Note && impl->effectiveEnabled)
        {
            auto& heldSlot = impl->heldNoteSlots[(size_t) ch - 1][(size_t) num];
            if (value > 0 && heldSlot < 0)
            {
                int slot = map->noteToSlot[(size_t) num];
                if (slot < 0 && num >= map->keyboardBaseNote && num < map->keyboardBaseNote + map->keyboardSlots) slot = num - map->keyboardBaseNote;
                if (slot >= 0 && slot < kMaxSlots) { heldSlot = (int8_t) slot; impl->foot (slot, true, value, off, out); }
            }
            else if (value == 0 && heldSlot >= 0)
            {
                const int slot = heldSlot; heldSlot = -1; bool stillHeld = false;
                for (const auto& channel : impl->heldNoteSlots)
                    for (auto s : channel) if (s == slot) stillHeld = true;
                if (! stillHeld) impl->foot (slot, false, kind == 0x80 ? e.d2 / 127.f : 0.f, off, out);
            }
        }
        if (type == MidiSrc::CC && impl->effectiveEnabled)
        {
            const int slot = map->footCc[(size_t) ch - 1][(size_t) num];
            const auto& bucket = map->find (type, ch, num);
            if (slot >= 0 && (bucket.count == 0 || map->layers[bucket.index[0]] >= map->footLayer[(size_t) ch - 1][(size_t) num]))
            { impl->foot (slot, value >= .5f, value, off, out); continue; }
        }
        impl->route (*map, type, ch, num, value, off, out);
        if (type == MidiSrc::CC && (num == 11 || num == 4) && impl->effectiveEnabled && map->find (type, ch, num).count == 0)
        {
            const size_t source = num == 11 ? 0 : 1;
            impl->expressionValue[source] = value; impl->expressionKnown[source] = true;
            impl->route (*map, MidiSrc::VirtualExpression, ch, (int) source + 1, value, off, out);
        }
        if (type == MidiSrc::Note && kind == 0x90 && e.d2 > 0)
        {
            impl->route (*map, MidiSrc::NoteVel, ch, num, value, off, out);
            impl->route (*map, MidiSrc::KeyboardVelocity, ch, 0, value, off, out);
        }
        if (type == MidiSrc::CC && num < 64)
        {
            const int base = num & 31; const size_t channel = (size_t) ch - 1;
            if (num < 32) { impl->msb[channel][(size_t) base] = e.d2; impl->hasMsb[channel][(size_t) base] = true; }
            else if (impl->hasMsb[channel][(size_t) base]) impl->lsb[channel][(size_t) base] = e.d2;
            if (impl->hasMsb[channel][(size_t) base]) impl->route (*map, MidiSrc::CC14, ch, base,
                (impl->msb[channel][(size_t) base] * 128 + impl->lsb[channel][(size_t) base]) / 16383.f, off, out);
        }
    }
    impl->clock.finish (start + n, out);
    const bool lost = impl->clock.isLost();
    if (out.time.clockLocked != impl->uiClockLocked)
    { impl->uiClockLocked = out.time.clockLocked; impl->feedback.push ({ UiEvKind::ClockLock, 0xff, 0, impl->uiClockLocked ? 1.f : 0.f }); }
    if (lost != impl->uiClockLost)
    { impl->uiClockLost = lost; impl->feedback.push ({ UiEvKind::ClockLost, 0xff, 0, lost ? 1.f : 0.f }); }
    if (std::abs (out.time.bpm - impl->uiTempo) >= .01)
    { impl->uiTempo = out.time.bpm; impl->feedback.push ({ UiEvKind::TempoChanged, 0xff, 0, (float) impl->uiTempo }); }
    const auto dropped = droppedCount();
    if (dropped != impl->uiDropped)
    { impl->uiDropped = dropped; impl->feedback.push ({ UiEvKind::Overload, 0xff, 0, 0, dropped, 0 }); }
    if (out.nTr > 0)
        for (int i = 0; i < out.nTr; ++i) if (out.tr[i] == TransportEvent::Stop)
            for (int slot = 0; slot < kMaxSlots; ++slot) impl->foot (slot, false, .5f, out.trOff[i], out);
    // RouteOut owns copied POD only; BorrowScope releases the last map read.
}
void MidiRouter::armLearn (const LearnTarget& t)
{ CaptureRecord c; while (impl->captures.pop (c)) {} const auto old = impl->learn.load(); impl->learn.store (((old + 4) & ~3u) | 1u | (t.continuous ? 2u : 0u), std::memory_order_release); }
void MidiRouter::cancelLearn() { impl->learn.fetch_and (~1u, std::memory_order_release); }
bool MidiRouter::pollLearn (LearnCapture& c)
{
    CaptureRecord record;
    while (impl->captures.pop (record))
        if (record.generation == (impl->learn.load (std::memory_order_acquire) & ~3u)) { c = record.capture; return true; }
    return false;
}
int MidiRouter::drainUi (UiEv* out, int cap) noexcept { return out && cap > 0 ? (int) impl->feedback.popMany (out, (size_t) cap) : 0; }
uint32_t MidiRouter::droppedCount() const noexcept
{ return impl->dropped.load (std::memory_order_relaxed) + impl->input.dropped() + impl->priority.dropped() + impl->captures.dropped(); }
}
