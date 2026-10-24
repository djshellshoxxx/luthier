// OWNER: WP3
#pragma once
#include "LatencyEqualiser.h"
namespace luthier::reference::routing
{
struct SendReturnState
{
    LatencyEqualiser dry;
    double mix = 1.0;
    bool initialised = false;
    void prepare (int capacity) { dry.prepare (capacity); reset(); }
    void reset() noexcept { dry.reset(); initialised = false; }
};
}
