// OWNER: WP1 (frozen interface I-1; changes only via errata request to the WP1 owner)
#pragma once
#include "PreparedMemory.h"

// The one pedal API (errata E6). Every generic and branded pedal implements
// IPedal; the board never knows a concrete type. juce_core only: no GUI, so
// this header is safe in the headless renderer. Spec: BOARD_CORE.md sec 11.
//
// Thread contract:
//   message thread   prepare, setServices, saveExtra, loadExtra (may allocate)
//   audio thread     everything else; noexcept, no alloc, no lock, no log, no IO
//
// Dispatch order per block (BOARD_CORE 11): (1) param snapshot via setParam,
// (2) setTime, (3) transport / foot / param events at their sample offsets
// (the board splits the block), (4) process.

#include <cstdint>
#include <memory>
#include <juce_core/juce_core.h>
#include "PedalDesc.h"

namespace luthier::reference
{

class IAssetStore;

struct TimeInfo
{
    double bpm = 120;
    double ppq = 0;
    bool playing = false;
    bool clockLocked = false;
    bool valid = false;
    int64_t sample = 0;
};

// Canonical raw event. Only pedals with PedalDesc::wantsRawMidi receive it.
struct MidiEvent
{
    uint8_t status = 0, d1 = 0, d2 = 0;
    int offset = 0;
};

// Footswitch press as seen by a pedal: speed and force are 0..1 (MIDI velocity
// and aftertouch emulation, BOARD_CORE 9.6).
struct FootEvent
{
    bool press = false;
    float speed = 0.5f;
    float force = 0.f;
    int offset = 0;
};

enum class TransportEvent : uint8_t { Start, Stop, Continue, Locate };

struct PedalServices
{
    IAssetStore* assets = nullptr;
    uint32_t instanceSeed = 0;
    int maxBlock = 0;
    double sampleRate = 48000;
};

enum PedalUiAvailability : uint32_t
{
    UiTransport = 1, UiLoop = 2, UiTempo = 4, UiProgress = 8, UiFaults = 16
};
struct PedalUiMeters
{
    uint32_t available = 0;
    int transport = 0;
    uint64_t loopLength = 0, playhead = 0;
    double bpm = 0;
    float progress = 0;
    uint32_t faults = 0;
};

class IPedal
{
public:
    virtual ~IPedal() = default;

    virtual const PedalDesc& desc() const noexcept = 0;

    // Message thread, may allocate. Called on prepareToPlay and on sample-rate
    // or block-size change. oversamplePolicy: 0 Eco, 1 Normal, 2 High.
    virtual void prepare (double sampleRate, int maxBlock, int oversamplePolicy) = 0;
    virtual void setServices (const PedalServices&) {}   // message thread, before prepare()
    // Off audio, after prepare/loadExtra: actual sizeof(model)+owned allocation
    // capacities. Unknown cannot be admitted to the prepared-memory pool.
    virtual uint64_t getPreparedMemoryBytes() const noexcept { return kUnknownPreparedMemoryBytes; }
    virtual void reset() noexcept = 0;                    // clears state, RT-safe

    // Audio thread. In place, n <= maxBlock.
    virtual void process (double* L, double* R, int n) noexcept = 0;
    // Opt-in board-owned bypass: called instead of process() only for a
    // non-tail EqualPower descriptor. Render the active signal exactly once,
    // without an internal engagement envelope. setEnabled still controls the
    // model's LFO/transport state. Default preserves existing model behavior.
    virtual void processForBoard (double* L, double* R, int n) noexcept
    { process (L, R, n); }
    // E68: render exactly once and optionally expose the actual wet contribution
    // before dry summation/final output guard. Board supplies separate, nonnull
    // writable residual buffers of n samples, disjoint from L/R. Return true only
    // when both are fully written, finite and bounded. No added latency/state pass.
    // False means residual unavailable: the board uses ordinary bypass fading.
    virtual bool processWithWetResidual (double* L, double* R, double*, double*, int n) noexcept
    { process (L, R, n); return false; }
    // Additive sidechain input (implementation erratum E32). The board supplies
    // read-only buffers for the immediately following process() call only.
    // Null pointers mean the external bus is unavailable; consumers must never
    // silently substitute the internal detector when external mode is selected.
    virtual void setSidechain (const double* /*L*/, const double* /*R*/, int /*n*/) noexcept {}
    virtual void setParam (int idx, float norm) noexcept = 0;   // norm 0..1; applies taper, smooths; Trigger: rising edge fires
    virtual float getParam (int idx) const noexcept = 0;

    // E60: board-aligned dry in/out; apply only buffer coloration, with no added
    // algorithmic latency. Separate prepared state; no allocation or locking.
    virtual void processBypass (double*, double*, int) noexcept {}
    virtual void setEnabled (bool) noexcept {}            // mirror of BoardModel.enabled (trails state machine); the model is truth
    virtual int getLatencySamples() const noexcept { return 0; }
    virtual float getTailEnergyDb() const noexcept { return 0.f; }   // <= -90 lets the board skip process()
    virtual void setTime (const TimeInfo&) noexcept {}    // once per block, before events
    virtual void onTransport (TransportEvent, int /*offset*/) noexcept {}
    virtual void onMidi (const MidiEvent&) noexcept {}    // only if desc().wantsRawMidi
    virtual int drainMidiOut (MidiEvent*, int /*cap*/) noexcept { return 0; }   // RC-5 clock master; audio thread
    virtual void onFoot (const FootEvent&) noexcept {}
    // E41: loopers use onFoot for transport while the board remains engaged.
    virtual bool footControlsEngagement() const noexcept { return true; }
    // E41: audio-thread one-shot request (e.g. a wah toe switch). The board
    // consumes this after processing; only the board owns the bypass envelope.
    virtual bool consumeEngageToggle() noexcept { return false; }
    virtual PedalUiMeters readUiMeters() const noexcept { return {}; } // E47: AUDIO THREAD ONLY
    virtual void setExpression (int /*jack*/, float /*v*/) noexcept {}
    virtual bool isStateless() const noexcept { return false; }
    virtual bool outputsDualMono() const noexcept { return false; }

    virtual void saveExtra (juce::var&) const {}          // message thread; non-param state (tap time, loop asset refs)
    virtual void loadExtra (const juce::var&) {}
};

using PedalFactory = std::unique_ptr<IPedal> (*) ();

} // namespace luthier::reference
