// OWNER: WP3
#pragma once
#include "../Topology.h"
namespace luthier::reference::routing
{
struct AbSwitchState
{
    double angle = 0.0, target = -1.0, step = 0.0;
    int remaining = 0;
    bool initialised = false;
    void reset() noexcept { *this = {}; }
};
void mixAb (const Node&, ExecCtx&) noexcept;
}
