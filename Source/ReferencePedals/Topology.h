// OWNER: WP1 (frozen interface I-7; changes only via errata request to the WP1 owner)
#pragma once

// The compiled, immutable audio-thread view of a board (BOARD_CORE 3, 4, 6).
// TopologyCompiler (WP2) builds one on the message thread from the BoardModel
// and publishes it through an atomic pointer; BoardEngine (WP2) executes the
// node program on the audio thread with a fixed set of node functions
// (RoutingNodes.h). No dynamic graph, no allocation, no virtual dispatch
// beyond IPedal in a node.

#include <cstdint>
#include "BoardTypes.h"

namespace luthier::reference
{

class IPedal;

//==============================================================================
// Node kinds (BOARD_CORE 6). FbSend is added (errata E25): feedback needs a
// tap node after pedal s as well as the FbReturn injection at pedal r.
enum class NodeKind : uint8_t
{
    Pedal,          // run pedal `slot` in place on bus `lane`
    Split,          // copy bus 0 to buses 1..aux-1 (aux = lane count)
    Sum,            // sum buses 0..aux-1 into bus 0 with norm gain f0
    LaneGain,       // bus `lane` *= f0 (linear gain; negative f0 = polarity invert)
    Delay,          // latency equaliser: delay bus `lane` by i0 samples (aux = delay line index)
    SendTap,        // loop `aux`: copy bus 0 (main) to the loop bus, level f0
    ReturnInject,   // loop `aux`: mix the loop bus back into main per LoopKind (f0 = blend, f1 = return gain)
    FbSend,         // tap bus `lane` into the feedback ring (after the source pedal)
    FbReturn,       // add the delayed, filtered, limited feedback to bus `lane` (before the destination pedal)
    Limiter,        // BoardLimiter on bus 0 (segment end)
    StereoWidth     // mid/side width on bus `lane`, f0 = width
};
constexpr int kNodeKindCount = (int) NodeKind::StereoWidth + 1;

struct Node
{
    NodeKind kind = NodeKind::Pedal;
    uint8_t slot = 0xFF;    // Pedal: storage index
    uint8_t lane = 0;       // bus this node reads and writes
    uint8_t aux = 0;        // kind specific (see NodeKind)
    int32_t i0 = 0;         // kind specific (Delay: samples)
    float f0 = 1.f;
    float f1 = 0.f;
};
static_assert (sizeof (Node) == 16, "Node is a 16-byte POD");

//==============================================================================
// Buses: every bus is a planar double L/R scratch pair preallocated by the
// engine. Lane k uses bus k (bus 0 is lane 0 and the main path), loop j uses
// bus kMaxBranches + j, the feedback tap uses bus kBusFeedback.
constexpr int kBusMain = 0;
constexpr int kBusLoopBase = kMaxBranches;
constexpr int kBusFeedback = kMaxBranches + kMaxLoops;
constexpr int kMaxBuses = kBusFeedback + 1;
constexpr int kMaxNodes = 128;

struct Lane
{
    uint8_t count = 0;
    uint8_t slots[kMaxSlots] {};    // storage indices, signal order inside the lane
};

struct LoopCfg
{
    bool used = false;
    bool enabled = true;
    LoopKind kind = LoopKind::Serial;
    uint8_t firstPos = 0, lastPos = 0;   // inclusive indices into Topology::order
    float blend = 1.f;
    float sendGain = 1.f, returnGain = 1.f;   // linear
};

struct FeedbackCfg
{
    bool on = false;
    uint8_t srcSlot = 0, dstSlot = 0;
    float gain = 0.f;                    // already clamped by the compiler (0.98 unless runawayOk)
    float lpHz = 12000.f;
    bool runawayOk = false;
    int delaySamples = kSubBlock;        // unit delay: previous sub-block, >= 64 (BOARD_CORE 6.6)
};

//==============================================================================
struct Topology
{
    uint32_t version = 0;                // monotonically increasing
    Mode mode = Mode::Series;
    uint8_t n = 0;                       // active (non-empty) slots
    uint8_t numLanes = 1;
    uint8_t order[kMaxSlots] {};         // storage indices in signal order == left-to-right visual order (rule O1)
    uint8_t placement[kMaxSlots] {};     // Placement of order[i]
    Lane lanes[kMaxBranches];
    LoopCfg loops[kMaxLoops];
    FeedbackCfg fb;

    int latencyTotal = 0;                // L_board
    int laneDelay[kMaxBranches] {};      // maxLaneLat - lat_k
    int dryDelay = 0;                    // wet/dry latency compensation (<= kMaxLatencySamp)
    int slotLatency[kMaxSlots] {};
    uint32_t cpuUnits = 0; // normalized declared cost:1000 units =1%

    IPedal* pedals[kMaxSlots] {};        // non-owning; PedalPool keeps them alive while any non-retired Topology references them

    // Compiled node program; one contiguous range per Placement segment.
    Node nodes[kMaxNodes];
    int numNodes = 0;
    int segBegin[kNumPlacements] {};
    int segEnd[kNumPlacements] {};       // exclusive; empty segment == begin == end
};

//==============================================================================
// Everything a node function may touch. Owned and filled by BoardEngine; plain
// pointers into preallocated memory.
struct ExecCtx
{
    int n = 0;                           // samples in this (sub-)block
    int nodeIndex = 0;                   // index of the executing node in Topology::nodes
    double* bus[kMaxBuses][2] {};        // planar L/R per bus
    IPedal* const* pedals = nullptr;     // storage index -> pedal (== Topology::pedals)
    const Topology* topo = nullptr;
    void* state[kMaxNodes] {};           // per-node private state (delay lines, filters), allocated at prepare
    double sampleRate = 48000.0;
    bool dualMono = false;               // true while L == R is guaranteed (CPU shortcut, BOARD_CORE 6.8)
};

using NodeFn = void (*) (const Node&, ExecCtx&) noexcept;

} // namespace luthier::reference
