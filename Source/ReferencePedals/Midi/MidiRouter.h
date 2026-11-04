// OWNER: WP1 (frozen interface I-5; implementation MidiRouter.cpp: WP5)
#pragma once

// Audio-thread entry of the board MIDI layer (MIDI_EXPRESSION sec 2): merges
// the host MidiBuffer with events from the UI/device ring, runs the clock
// (ClockSync), the binding map and the gesture stage, and returns one RouteOut
// per block that BoardEngine applies at sample offsets. No allocation, no lock,
// no std::function in process().

#include <cstdint>
#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../BoardTypes.h"
#include "MidiTypes.h"

namespace luthier::reference::midi
{

class MidiRouter
{
public:
    MidiRouter();
    ~MidiRouter();
    MidiRouter (const MidiRouter&) = delete;
    MidiRouter& operator= (const MidiRouter&) = delete;

    // Message thread. Allocates rings and tables; idempotent.
    void prepare (double sampleRate, int maxBlock);

    // E61: non-audio control thread; serialized against other writers/reclaim.
    // RCU publish of a new immutable map; the old one is
    // retired and freed on the message thread (reclaim in Board::reclaim()).
    void publishMap (std::unique_ptr<MapSnap> snapshot);

    int reclaim() noexcept; // message thread; single-reader hazard protection (E33)

    // E46: audio thread, before process; serial zero is unwritten.
    void setHostEnabled (bool enabled, uint32_t serial) noexcept;
    // E48: explicit model intent, including writes equal to stored state.
    // A fresh model revision is applied before a fresh host serial.
    void setModelEnabled (bool enabled, uint32_t revision) noexcept;

    // E63: audio-only resolved model/host intent. Zero revision is unwritten.
    void setTargetValue (uint16_t globalTarget, float normalised, uint32_t resolvedIntentRevision) noexcept;
    void setHostTempoSource (TempoSource, uint32_t serial) noexcept;
    void setModelTempoSource (TempoSource, uint32_t revision) noexcept;
    void setHostTempoBpm (double bpm, uint32_t serial) noexcept;
    void setModelTempoBpm (double bpm, uint32_t revision) noexcept;
    // E57: audio thread, virtual jack1/2; serial0 is unwritten.
    void setVirtualExpression (int source, float normalised, uint32_t serial) noexcept;

    // Device / UI thread -> uiInRing (wait-free; drops and counts when full).
    void pushUiEvent (const MidiEv&) noexcept;

    // Audio thread. Fills `out` (cleared first); `blockStart` is the running
    // int64 sample counter of the block start (never reset by transport).
    void process (const juce::MidiBuffer&, const HostTransport&, int64_t blockStart, int n, RouteOut& out) noexcept;

    // Message thread: learn control (armed state is read by the audio thread).
    void armLearn (const LearnTarget&);
    void cancelLearn();
    bool pollLearn (LearnCapture&);

    // Message thread: drains the audio -> UI ring (LED echo, clock state).
    int drainUi (UiEv* out, int cap) noexcept;

    // Any thread (atomics).
    uint32_t droppedCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace luthier::reference::midi
