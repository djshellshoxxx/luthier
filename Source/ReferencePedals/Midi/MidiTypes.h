// OWNER: WP1 (frozen interface I-5; changes only via errata request to the WP1 owner)
#pragma once

// POD vocabulary of the board MIDI layer (MIDI_EXPRESSION.md sec 2, 4, 6, 12).
// Everything here is fixed-size and trivially copyable: it crosses lock-free
// rings and lives in preallocated audio-thread buffers. Sizes are static_assert'd
// and re-checked in RefPedalApiTests (T-REG).

#include <cstdint>
#include <type_traits>
#include "../BoardTypes.h"
#include "../IPedal.h"

namespace luthier::reference::midi
{

// Router-internal event (errata E7). Converted to the pedal-facing MidiEvent
// only for wantsRawMidi pedals.
struct MidiEv
{
    uint32_t sampleOffset = 0;   // within the block; UI-origin events use 0 of the next block
    uint8_t status = 0, d1 = 0, d2 = 0;
    uint8_t src = 0;             // MidiOrigin
    uint16_t pad = 0;
    double ppq = 0.0;
};
static_assert (sizeof (MidiEv) == 24, "MidiEv is 24 bytes (MIDI_EXPRESSION 1)");
static_assert (std::is_trivially_copyable_v<MidiEv>, "MidiEv crosses SPSC rings");

enum class MidiOrigin : uint8_t { Host = 0, Device = 1, OnScreen = 2, UiSlider = 3 };

// E36: slot 0xff in ParamWrite addresses one board target. Tempo is normalised
// over 20..300 BPM. The remaining slots address descriptor parameters.
enum class BoardTarget : uint16_t { Mix = 0, Tempo = 1, Enabled = 2 };
constexpr uint8_t kBoardTargetSlot = 0xff;

// Router -> pedal parameter write at a sample offset (errata E7).
struct ParamWrite
{
    uint8_t slot = 0;
    uint16_t paramIdx = 0;       // descriptor index (not lane index)
    float norm = 0.f;
    uint16_t smoothMs = 0;
    int offset = 0;
};

// Source taxonomy (MIDI_EXPRESSION 2.1).
enum class MidiSrc : uint8_t
{
    CC = 0, CC14, NRPN, Note, NoteVel, PolyAT, ChanAT, PitchBend, ProgramChange,
    ClockStart, ClockStop, ClockContinue, UiSlider, KeyboardVelocity, VirtualExpression
};

enum class BindMode : uint8_t { Absolute = 0, Relative, Toggle, Momentary, Trigger, Step };

enum BindFlag : uint8_t
{
    BF_HiRes = 1, BF_Relative = 2, BF_Invert = 4, BF_Latch = 8, BF_SoftTakeover = 16, BF_Enabled = 32
};

enum class CurveId : uint8_t { Linear = 0, Log, Exp, SCurve, Inverse, Step, Custom5 };

// Binding record, fixed 32 bytes (MIDI_EXPRESSION 4.1). `target` is a global
// target index: slot * kMaxParams + paramIdx for pedal params, then the board
// targets (kTargetBoardBase + n). mode (3 bits) and curve id (3 bits) share
// one byte; use the accessors.
constexpr uint16_t kTargetBoardBase = (uint16_t) (kMaxSlots * kMaxParams);

struct Binding
{
    uint16_t id = 0;
    uint16_t target = 0;
    uint8_t chan = 0;            // 0 = omni
    uint8_t srcType = 0;         // MidiSrc
    uint8_t srcNum = 0;
    uint8_t flags = BF_Enabled;  // BindFlag bitset
    uint8_t modeCurve = 0;       // mode in bits 0..2, curve id in bits 3..5
    uint8_t priority = 0;
    uint16_t smoothMs = 10;
    float inMin = 0.f, inMax = 1.f;      // normalised source window
    float outMin = 0.f, outMax = 1.f;    // normalised target window (outMin > outMax inverts)
    float curveParam = 0.f;

    BindMode mode() const noexcept { return (BindMode) (modeCurve & 7u); }
    CurveId curve() const noexcept { return (CurveId) ((modeCurve >> 3) & 7u); }
    void setModeCurve (BindMode m, CurveId c) noexcept { modeCurve = (uint8_t) (((unsigned) m & 7u) | (((unsigned) c & 7u) << 3)); }
    bool has (BindFlag f) const noexcept { return (flags & (uint8_t) f) != 0; }
};
static_assert (sizeof (Binding) == 32, "Binding is 32 bytes (MIDI_EXPRESSION 4.1)");
static_assert (std::is_trivially_copyable_v<Binding>, "Binding lives in a flat MapSnap");

// Immutable binding table published to the audio thread (WP5, Midi/MidiMap.h).
struct MapSnap;

//==============================================================================
// Router output for one block (I-5). Large: hold it in preallocated engine
// memory, not on a small stack.
constexpr int kMaxRouteWrites = 512;
constexpr int kMaxRouteFoot = 64;
constexpr int kMaxRouteTransport = 8;

struct RouteOut
{
    ParamWrite writes[kMaxRouteWrites];
    int nWrites = 0;
    FootEvent foot[kMaxRouteFoot];
    uint8_t footSlot[kMaxRouteFoot] {};
    int nFoot = 0;
    TransportEvent tr[kMaxRouteTransport] {};
    int trOff[kMaxRouteTransport] {};
    int nTr = 0;
    TimeInfo time;

    void clear() noexcept { nWrites = 0; nFoot = 0; nTr = 0; time = TimeInfo {}; }
};

//==============================================================================
// Learn (MIDI_EXPRESSION 6.1). The FSM lives on the message thread (WP6); the
// router only forwards candidate events while armed.
struct LearnTarget
{
    uint8_t slot = 0xFF;         // 0xFF = board-level target
    uint16_t paramIdx = 0;
    bool continuous = true;      // knob/slider (needs >= 2 distinct CC values) vs switch (first message)
};

struct LearnCapture
{
    bool valid = false;
    MidiSrc type = MidiSrc::CC;
    uint8_t chan = 0;            // 1..16
    uint8_t num = 0;
    uint8_t value = 0;
};

// Audio -> message thread feedback ring entry (LED echo, learn candidates, clock state).
enum class UiEvKind : uint8_t { Activity = 0, LearnCandidate, ClockLock, ClockLost, TempoChanged, Overload };

struct UiEv
{
    UiEvKind kind = UiEvKind::Activity;
    uint8_t slot = 0xFF;
    uint16_t paramIdx = 0;
    float value = 0.f;
    uint32_t a = 0, b = 0;
};
static_assert (std::is_trivially_copyable_v<UiEv>, "UiEv crosses an SPSC ring");

} // namespace luthier::reference::midi
