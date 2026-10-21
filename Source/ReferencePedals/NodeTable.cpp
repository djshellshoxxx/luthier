// OWNER: WP2
// STUB (WP1): the node table is frozen in Routing/RoutingNodes.h. WP2 fills
// nodePedal (per-slot trims, enable ramp, tail skip, dual-mono shortcut) and
// nodeLimiter (BoardLimiter); the table itself stays as is.

#include "Routing/RoutingNodes.h"

namespace luthier::reference::routing
{

void nodePedal (const Node&, ExecCtx&) noexcept {}
void nodeLimiter (const Node&, ExecCtx&) noexcept {}

const NodeFn* nodeTable() noexcept
{
    // Indexed by NodeKind; keep in the enum's order.
    static const NodeFn table[kNodeKindCount] {
        &nodePedal, &nodeSplit, &nodeSum, &nodeLaneGain, &nodeDelay, &nodeSendTap,
        &nodeReturnInject, &nodeFbSend, &nodeFbReturn, &nodeLimiter, &nodeStereoWidth
    };
    return table;
}

} // namespace luthier::reference::routing
