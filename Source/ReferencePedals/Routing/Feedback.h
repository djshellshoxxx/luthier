// OWNER: WP3
#pragma once
#include "../Topology.h"
#include <array>
namespace luthier::reference::routing
{
struct FeedbackState
{
    std::array<std::array<double, kSubBlock>, 2> ring {};
    std::array<double, 2> low {}, previous {}, dc {};
    int position = 0, ramp = 0, hotSamples = 0, killSamples = 0;
    uint8_t source = 0, destination = 0;
    bool active = false, fault = false, tripped = false;
    double gain = 0.0, target = 0.0, step = 0.0, lp = 1.0, hp = 0.0, energy = 0.0, rate = 48000;
    void reset() noexcept { *this = {}; }
    void configure (const FeedbackCfg&, bool kill, double sampleRate) noexcept;
    void setTarget (double value, double seconds) noexcept;
    void fail() noexcept;
};
}
