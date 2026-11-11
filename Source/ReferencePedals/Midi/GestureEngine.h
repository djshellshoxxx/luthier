// OWNER: WP6
#pragma once
#include "MidiTypes.h"
#include <array>
#include <algorithm>
namespace luthier::reference::midi
{
// Pressure model used by keyboard footswitch clients. Allocates no storage.
class GestureEngine
{
public:
    static float travelMs (float h) noexcept { return 60.f - 52.f * std::clamp (h, 0.f, 1.f); }
    static float engageMs (float h) noexcept { return 25.f - 23.f * std::clamp (h, 0.f, 1.f); }
    static double tapCompensationMs (float h) noexcept { h = std::clamp (h, 0.f, 1.f); return travelMs (h) * .5 * (1 - h); }
    void reset() noexcept { held.fill (false); pressure.fill (0); }
    void note (int key, bool down) noexcept { if (key >= 0 && key < 128) { held[(size_t) key] = down; if (! down) pressure[(size_t) key] = 0; } }
    void polyPressure (int key, float v) noexcept { if (key >= 0 && key < 128 && held[(size_t) key]) pressure[(size_t) key] = std::clamp (v, 0.f, 1.f); }
    float maxPressure() const noexcept { float result = 0; for (float v : pressure) result = std::max (result, v); return result; }
private:
    std::array<bool, 128> held {};
    std::array<float, 128> pressure {};
};
}
