// OWNER: WP3
#pragma once
#include <array>
#include "../DelayLine.h"
namespace luthier::reference::routing
{
struct LatencyEqualiser
{
    std::array<DelayLine, 2> channels;
    void prepare (int capacity) { for (auto& line : channels) line.prepare (capacity); }
    void reset() noexcept { for (auto& line : channels) line.reset(); }
};
}
