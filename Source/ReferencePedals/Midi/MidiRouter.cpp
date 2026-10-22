// OWNER: WP5
// STUB (WP1): the interface is frozen in MidiRouter.h; WP5 implements it. Until
// then the router produces an empty RouteOut (no events, default TimeInfo) and
// drops UI events, which keeps the board's MIDI path inert and allocation-free.

#include "MidiRouter.h"
#include "MidiMap.h"
#include <atomic>

namespace luthier::reference::midi
{

struct MidiRouter::Impl
{
    std::atomic<uint32_t> dropped { 0 };
};

MidiRouter::MidiRouter() : impl (std::make_unique<Impl>()) {}
MidiRouter::~MidiRouter() = default;

void MidiRouter::prepare (double, int) {}
void MidiRouter::publishMap (std::unique_ptr<MapSnap>) {}

void MidiRouter::pushUiEvent (const MidiEv&) noexcept
{
    impl->dropped.fetch_add (1, std::memory_order_relaxed);
}

void MidiRouter::process (const juce::MidiBuffer&, const HostTransport& host, int64_t blockStart, int, RouteOut& out) noexcept
{
    out.clear();
    out.time.bpm = host.bpm;
    out.time.ppq = host.ppq;
    out.time.playing = host.playing;
    out.time.valid = host.valid;
    out.time.sample = blockStart;
}

void MidiRouter::armLearn (const LearnTarget&) {}
void MidiRouter::cancelLearn() {}
bool MidiRouter::pollLearn (LearnCapture&) { return false; }
int MidiRouter::drainUi (UiEv*, int) noexcept { return 0; }
uint32_t MidiRouter::droppedCount() const noexcept { return impl->dropped.load (std::memory_order_relaxed); }

} // namespace luthier::reference::midi
