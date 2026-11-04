// OWNER: WP1 (frozen interface I-7; function bodies: WP2 nodePedal/nodeLimiter, WP3 the rest)
#pragma once

// The routing function table. BoardEngine executes Topology::nodes by calling
// nodeTable()[(int) node.kind]. Every function is audio-thread safe: noexcept,
// no allocation, no lock. Per-node memory comes from ExecCtx::state[nodeIndex]
// (allocated by the engine at prepare time from the node's kind).

#include "../Topology.h"

namespace luthier::reference::routing
{

// WP2: NodeTable.cpp
void nodePedal (const Node&, ExecCtx&) noexcept;
void nodeLimiter (const Node&, ExecCtx&) noexcept;

// WP3: Routing/Lanes.cpp
void nodeSplit (const Node&, ExecCtx&) noexcept;
void nodeSum (const Node&, ExecCtx&) noexcept;
void nodeLaneGain (const Node&, ExecCtx&) noexcept;
// WP3: Routing/LatencyEqualiser.cpp
void nodeDelay (const Node&, ExecCtx&) noexcept;
// WP3: Routing/SendReturn.cpp
void nodeSendTap (const Node&, ExecCtx&) noexcept;
void nodeReturnInject (const Node&, ExecCtx&) noexcept;
// WP3: Routing/Feedback.cpp
void nodeFbSend (const Node&, ExecCtx&) noexcept;
void nodeFbReturn (const Node&, ExecCtx&) noexcept;
// WP3: Routing/StereoWidth.cpp
void nodeStereoWidth (const Node&, ExecCtx&) noexcept;

// WP2: NodeTable.cpp. kNodeKindCount entries indexed by NodeKind.
const NodeFn* nodeTable() noexcept;

} // namespace luthier::reference::routing
