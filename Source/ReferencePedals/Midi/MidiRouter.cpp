// OWNER: WP5
#include "MidiRouter.h"
#include "MidiMap.h"
#include "ClockSync.h"
#include "../Rt/SpscRing.h"
#include <atomic>
#include <array>
#include <vector>
namespace luthier::reference::midi
{
namespace { struct CaptureRecord { LearnCapture capture; uint32_t generation = 0; }; }
struct MidiRouter::Impl
{
    struct Retired { std::unique_ptr<MapSnap> map; uint64_t epoch; };
    std::atomic<const MapSnap*> published { nullptr };
    std::unique_ptr<MapSnap> current;
    std::vector<Retired> retired;
    std::atomic<uint64_t> completed { 0 };
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
    std::array<std::array<uint8_t, 32>, 16> msb {}, lsb {};
    std::array<std::array<bool, 32>, 16> hasMsb {};
    std::array<bool, kMaxSlots> footHeld {};
    std::array<std::array<int8_t, 128>, 16> heldNoteSlots = []
    { std::array<std::array<int8_t, 128>, 16> a {}; for (auto& channel : a) channel.fill (-1); return a; }();
    uint32_t mapVersion = 0, activeVersion = 0;
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
                case BindMode::Toggle: emit = rising; v = values[index] < .5f ? 1.f : 0.f; break;
                case BindMode::Momentary: v = down ? 1.f : 0.f; break;
                case BindMode::Trigger: emit = rising; v = 1; break;
                case BindMode::Relative:
                { const int raw = (int) std::round (value * 127); v = values[index] + (raw >= 64 ? raw - 128 : raw) / 128.f; break; }
                case BindMode::Step:
                    if (map.steps[index] >= 2) v = std::round (v * (map.steps[index] - 1)) / (map.steps[index] - 1);
                    else emit = false;
                    break;
                default: break;
            }
            if (b.has (BF_SoftTakeover) && b.mode() == BindMode::Absolute && ! picked[index])
            {
                const float current = values[index];
                const float old = lastSource[index];
                if (std::abs (v - current) <= .03f || (old < current && v >= current) || (old > current && v <= current)) picked[index] = true;
                else emit = false;
            }
            held[index] = down; lastSource[index] = v;
            if (! emit) continue;
            if (b.priority < ownerPriority[b.target] && routeSample - lastWrite[b.target] < (int64_t) (sr * .3)) continue;
            ownerPriority[b.target] = b.priority; lastWrite[b.target] = routeSample;
            values[index] = std::clamp (v, 0.f, 1.f);
            if (b.target == kTargetBoardBase + (uint16_t) BoardTarget::Enabled)
            {
                effectiveEnabled = values[index] >= .5f;
                if (! effectiveEnabled) { footHeld.fill (false); for (auto& channel : heldNoteSlots) channel.fill (-1); }
            }
            if (out.nWrites == kMaxRouteWrites) { drop(); continue; }
            out.writes[out.nWrites++] = { b.target >= kTargetBoardBase ? kBoardTargetSlot : (uint8_t) (b.target / kMaxParams),
                b.target >= kTargetBoardBase ? (uint16_t) (b.target - kTargetBoardBase) : (uint16_t) (b.target % kMaxParams), values[index], b.smoothMs, offset };
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
MidiRouter::MidiRouter() : impl (std::make_unique<Impl>()) {}
MidiRouter::~MidiRouter() = default;
void MidiRouter::prepare (double sr, int)
{
    impl->sr = std::isfinite (sr) && sr > 0 ? sr : 48000;
    impl->clock.prepare (impl->sr); impl->input.clear(); impl->priority.clear();
    for (auto& channel : impl->heldNoteSlots) channel.fill (-1);
    impl->footHeld.fill (false); impl->held.fill (false); impl->values.fill (0);
    impl->retired.clear(); impl->activeVersion = 0; impl->modelEnabledKnown = false; impl->appliedHostEnabledSerial = 0; impl->appliedModelEnabledRevision = 0;
}
void MidiRouter::publishMap (std::unique_ptr<MapSnap> map)
{
    if (map) { map->version = ++impl->mapVersion; map->compile(); }
    auto old = std::move (impl->current); impl->current = std::move (map);
    impl->published.store (impl->current.get(), std::memory_order_seq_cst);
    if (old) impl->retired.push_back ({ std::move (old), impl->completed.load (std::memory_order_seq_cst) + 2 });
    reclaim();
}
int MidiRouter::reclaim() noexcept
{
    int freed = 0; auto& list = impl->retired;
    const auto epoch = impl->completed.load (std::memory_order_seq_cst);
    for (size_t i = 0; i < list.size();)
        if (epoch >= list[i].epoch) { list.erase (list.begin() + (std::ptrdiff_t) i); ++freed; } else ++i;
    return freed;
}
void MidiRouter::setModelEnabled (bool enabled, uint32_t revision) noexcept
{ if (revision != 0) { impl->modelIntentEnabled = enabled; impl->modelEnabledRevision = revision; } }
void MidiRouter::setHostEnabled (bool enabled, uint32_t serial) noexcept
{ if (serial != 0) { impl->hostEnabled = enabled; impl->hostEnabledSerial = serial; } }
void MidiRouter::pushUiEvent (const MidiEv& e) noexcept
{ if (e.status >= 0xf8 && e.status <= 0xfc) impl->priority.push (e); else impl->input.push (e); }
void MidiRouter::process (const juce::MidiBuffer& in, const HostTransport& host, int64_t start, int n, RouteOut& out) noexcept
{
    out.clear(); if (n <= 0) return;
    const auto* map = impl->published.load (std::memory_order_seq_cst);
    if (map && map->version != impl->activeVersion)
    {
        if (impl->modelEnabledRevision == 0 && (! impl->modelEnabledKnown || impl->modelEnabled != map->boardEnabled))
        { impl->effectiveEnabled = impl->modelEnabled = map->boardEnabled; impl->modelEnabledKnown = true; }
        if (! impl->effectiveEnabled) { impl->footHeld.fill (false); for (auto& channel : impl->heldNoteSlots) channel.fill (-1); }
        impl->activeVersion = map->version; impl->held.fill (false); impl->picked.fill (false);
        impl->values = map->initialValues; impl->lastSource = map->initialValues;
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
    impl->clock.begin (host, map ? map->tempoSource : TempoSource::Auto, map ? map->internalBpm : 120, start, out);
    int count = 0, ordinary = 0;
    auto append = [&] (MidiEv e, bool realtime)
    {
        if ((! realtime && ordinary >= 512) || count >= (int) impl->events.size()) { impl->drop(); return; }
        if (! realtime) ++ordinary;
        e.sampleOffset = (uint32_t) std::clamp ((int) e.sampleOffset, 0, n - 1);
        int j = count++; // stable insertion by sample offset, storage is fixed
        while (j > 0 && impl->events[(size_t) j - 1].sampleOffset > e.sampleOffset)
        { impl->events[(size_t) j] = impl->events[(size_t) j - 1]; --j; }
        impl->events[(size_t) j] = e;
    };
    // First pass reserves capacity for host transport/clock even after a controller flood.
    for (const auto md : in)
        if (md.numBytes > 0 && (md.data[0] >= 0xf8 || md.data[0] == 0xf2))
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
                if (! stillHeld) impl->foot (slot, false, value, off, out);
            }
        }
        impl->route (*map, type, ch, num, value, off, out);
        if (type == MidiSrc::Note && kind == 0x90 && e.d2 > 0) impl->route (*map, MidiSrc::NoteVel, ch, num, value, off, out);
        if (type == MidiSrc::CC && num < 64)
        {
            const int base = num & 31; const size_t channel = (size_t) ch - 1;
            if (num < 32) { impl->msb[channel][(size_t) base] = e.d2; impl->hasMsb[channel][(size_t) base] = true; }
            else impl->lsb[channel][(size_t) base] = e.d2;
            if (impl->hasMsb[channel][(size_t) base]) impl->route (*map, MidiSrc::CC14, ch, base,
                (impl->msb[channel][(size_t) base] * 128 + impl->lsb[channel][(size_t) base]) / 16383.f, off, out);
        }
    }
    impl->clock.finish (start + n, out);
    if (out.nTr > 0)
        for (int i = 0; i < out.nTr; ++i) if (out.tr[i] == TransportEvent::Stop)
            for (int slot = 0; slot < kMaxSlots; ++slot) impl->foot (slot, false, .5f, out.trOff[i], out);
    impl->completed.fetch_add (1, std::memory_order_seq_cst);
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
