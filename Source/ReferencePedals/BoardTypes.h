// OWNER: WP1 (frozen interface I-3 / I-4; changes only via errata request to the WP1 owner)
#pragma once

// Shared vocabulary of the reference pedalboard: constants, enums, the
// message-thread data model (PedalInstance, BoardSettings), host transport and
// meters. juce_core only, so the Board facade header stays cheap for
// LuthierEngine.h. Spec: BOARD_CORE.md sec 2, 3, 6, 7, 10.

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <juce_core/juce_core.h>
#include "PedalDesc.h"
#include "IPedal.h"

namespace luthier::reference
{

//==============================================================================
// Constants (BOARD_CORE 2)
constexpr int kMaxSlots = 16;          // storage capacity; UI shows 8 by default
constexpr int kMaxBranches = 4;        // parallel / split lanes
constexpr int kMaxLoops = 2;           // send / return loops
constexpr int kMaxFb = 1;              // feedback loops
constexpr int kMaxXfadeMs = 12;
constexpr int kReorderDipMs = 6;       // per half (out + in)
constexpr int kBypassXfadeMs = 5;
constexpr int kSubBlock = 64;          // control-smoothing sub-block
constexpr int kMinSplitSamples = 16;   // event coalescing (MIDI_EXPRESSION 2)
constexpr int kMaxSplitPoints = 32;    // per block (BOARD_CORE 9.5)
constexpr int kMaxLatencySamp = 8192;  // board total; adds exceeding this are rejected
constexpr int kSchemaVersion = 1;      // referencePedals JSON schema (E13)

//==============================================================================
// Placement relative to the legacy engine (E12). Values 0 and 1 are the
// prototype's; the old draft value 3 is dropped.
enum class Placement : int
{
    beforeExistingPre = 0,   // UI tag PRE
    afterExistingPost = 1,   // UI tag LOOP: after legacy post rack, before the cabinet
    postCab = 2              // UI tag POST: after the cabinet, before the room
};
constexpr int kNumPlacements = 3;

inline bool isValidPlacement (int v) noexcept { return v >= 0 && v < kNumPlacements; }

enum class Mode : uint8_t { Series, Parallel, AB, Loop };
enum class MixLaw : uint8_t { EqualPower, Linear, ThirdLaw };
enum class AbSelect : uint8_t { A, B, AplusB, Toggle };
enum class AbOutMode : uint8_t { Sum, Dual };
enum class ParallelNorm : uint8_t { Sqrt, Unity, None };
enum class LoopKind : uint8_t { Serial, Parallel, KillDry };
enum class TempoSource : uint8_t { Internal = 0, Host = 1, MidiClock = 2, Auto = 3 };

// Presentation only (zero audio effect, E16).
enum class BoardStyle : uint8_t { plywood, velcro, clamps, routed, rail, ceramic, loose };
enum class Mounting : uint8_t { automatic, screws, velcro, clamps, routed, rail, ceramic, none };

inline const char* toString (BoardStyle s) noexcept
{
    static constexpr const char* names[] { "plywood", "velcro", "clamps", "routed", "rail", "ceramic", "loose" };
    return names[(int) s];
}
inline const char* toString (Mounting m) noexcept
{
    static constexpr const char* names[] { "auto", "screws", "velcro", "clamps", "routed", "rail", "ceramic", "none" };
    return names[(int) m];
}
inline const char* toString (Mode m) noexcept
{
    static constexpr const char* names[] { "series", "parallel", "ab", "loop" };
    return names[(int) m];
}

// Unknown strings return false and leave `out` untouched (the reader keeps the
// original text, UI_BOARD_STYLES 2).
inline bool boardStyleFromString (const juce::String& s, BoardStyle& out) noexcept
{
    for (int i = 0; i <= (int) BoardStyle::loose; ++i)
        if (s == toString ((BoardStyle) i)) { out = (BoardStyle) i; return true; }
    return false;
}
inline bool mountingFromString (const juce::String& s, Mounting& out) noexcept
{
    for (int i = 0; i <= (int) Mounting::none; ++i)
        if (s == toString ((Mounting) i)) { out = (Mounting) i; return true; }
    return false;
}
inline bool modeFromString (const juce::String& s, Mode& out) noexcept
{
    for (int i = 0; i <= (int) Mode::Loop; ++i)
        if (s == toString ((Mode) i)) { out = (Mode) i; return true; }
    return false;
}

//==============================================================================
// Audio-thread facing PODs (Board facade, I-3)
struct HostTransport
{
    bool valid = false, playing = false;
    double bpm = 120, ppq = 0;
    int tsNum = 4, tsDen = 4;
};

// E44: audio-owned copy from cached host parameter atomics. Serial zero means
// never written. Layout: enabled/mix/BPM, then 16 slots of enabled,16 lanes,
// input dB,output dB,position. Position is structural and handled off audio.
struct HostAutomationSnapshot
{
    static constexpr int count = 3 + kMaxSlots * 20;
    std::array<float, count> values {};
    std::array<uint32_t, count> serials {};
};

struct BoardAuxOutputs
{
    struct Stereo { float* left = nullptr; float* right = nullptr; int samples = 0; };
    Stereo pedalB, wet, dry;
};

struct BoardMeters
{
    float inPeakDb = -120.f, outPeakDb = -120.f, cpuPct = 0.f;
    int latencySamples = 0;
    bool fbTripped = false, fbFault = false, clockLocked = false, clockLost = false;
    uint32_t midiDropped = 0;
    bool sidechainAvailable = false;
    bool auxBAvailable = false, auxWetAvailable = false, auxDryAvailable = false;
};

//==============================================================================
// Message-thread data model (BOARD_CORE 3 and 10.1). Plain copyable structs so
// the UI, the state reader and the undo stack share one vocabulary.
using InstanceId = juce::Uuid;   // persisted, stable across reorder and sessions

struct PedalMeters
{
    InstanceId identity = InstanceId::null();
    bool present = false, enabled = false;
    float tailEnergyDb = -120.f;
    PedalUiMeters model;
};

struct PedalInstance
{
    InstanceId instanceId;
    std::string modelId;                 // "generic.overdrive", "boss-ds1", ...
    uint16_t modelRevision = 1;
    bool enabled = true;                 // pedal footswitch; truth for engage (E3)
    Placement placement = Placement::beforeExistingPre;
    int lane = 0;                        // 0 for series
    int slot = -1;                       // storage index 0..kMaxSlots-1 (APVTS mirror), assigned by the model
    float inDb = 0.f, outDb = 0.f;       // slot trims (E4), -12..+12
    std::array<float, kMaxParams> params {};   // normalised 0..1, descriptor order
    float x = 0.f;                       // lateral offset, 'loose' style seed
    Mounting mounting = Mounting::automatic;
    bool missing = false;                // unknown model id: passes audio bit-exact, keeps state
    juce::var extras;                    // IPedal::saveExtra payload + unknown per-instance fields
};

struct LoopSettings
{
    bool used = false;
    bool enabled = true;                 // footswitch
    LoopKind kind = LoopKind::Serial;
    float blend = 1.f;                   // Parallel: loop blend 0..1
    float sendDb = 0.f, returnDb = 0.f;  // -24..+12
    InstanceId first = InstanceId::null(), last = InstanceId::null();   // the loop covers [first .. last] in the order; send before first, return after last
};

struct FeedbackSettings
{
    bool on = false;
    InstanceId src = InstanceId::null(), dst = InstanceId::null();   // tap after src, return at the input of dst; dst must not come after src (r <= s)
    float gain = 0.5f;                   // 0..1.05; > 0.98 needs runawayOk
    float lpHz = 12000.f;
    bool runawayOk = false;
    bool kill = false; // E56 momentary; persistence writes false
};

struct LaneSettings
{
    std::array<float, kMaxBranches> gainDb { 0.f, 0.f, 0.f, 0.f };
    std::array<bool, kMaxBranches> phaseInvert { false, false, false, false };
    ParallelNorm norm = ParallelNorm::Sqrt;
    int numLanes = 2;                    // Parallel: 2..4
};

struct BoardSettings
{
    bool enabled = false;                // default OFF: board absent == legacy path (T-LEGACY)
    Mode mode = Mode::Series;
    float mix = 1.f;                     // ref_board_mix, 1 = 100% wet
    MixLaw mixLaw = MixLaw::EqualPower;
    bool reportLatency = true;
    bool limiterOn = true;
    float limiterCeilDb = 0.f;
    bool trails = true;
    bool mixTail = true;
    AbSelect abSelect = AbSelect::AplusB;
    AbOutMode abOut = AbOutMode::Sum;
    LaneSettings lanes;
    std::array<LoopSettings, kMaxLoops> loops;
    FeedbackSettings fb;
    bool reampEnable = false, reampReplace = false;
    float reampGainDb = 0.f;
    int cpuCapPct = 30;
    TempoSource tempoSource = TempoSource::Auto;
    double tempoBpm = 120.0;
    int midiChan = 0;                    // 0 = omni
    BoardStyle style = BoardStyle::plywood;
    juce::var midi;                      // MIDI_EXPRESSION sec 11 block, owned by WP6 (opaque here)
    juce::var ui;                        // UI_BOARD_STYLES sec 2 block, owned by WP17 (opaque here)
    juce::var unknown;                   // unrecognised root keys, preserved on rewrite
};

// Read-only summary of the last compiled topology (BoardModel::getTopologyInfo).
struct TopologyInfo
{
    uint32_t version = 0;
    Mode mode = Mode::Series;
    int pedalCount = 0;                  // non-empty instances
    int latencyTotal = 0;                // L_board in samples
    int dryDelay = 0;
    uint32_t cpuUnits = 0;               // sum of PedalDesc::cpuUnits
    bool feedbackValid = true;
    juce::String note;                   // last compile warning, empty if none
};

} // namespace luthier::reference
