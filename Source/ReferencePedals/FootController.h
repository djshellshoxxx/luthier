// OWNER: WP6
#pragma once
#include <cstdint>
namespace luthier::reference
{
enum class FootMode : uint8_t { Latch, Momentary, Auto };
// Slot-owned engagement model; gestures for looper actions are decoded separately.
class FootController
{
public:
    void prepare (double sr) noexcept { rate = sr > 0 ? sr : 48000; reset(); }
    void reset (bool value = false) noexcept { engaged = original = value; held = false; lastDown = -1; }
    void setMode (FootMode v) noexcept { mode = v; }
    bool press (int64_t sample) noexcept
    {
        if (held || (lastDown >= 0 && sample - lastDown < rate * .005)) return engaged;
        original = engaged; held = true; downAt = lastDown = sample;
        engaged = mode == FootMode::Momentary ? true : ! engaged;
        return engaged;
    }
    bool release (int64_t sample) noexcept
    {
        if (! held) return engaged;
        held = false;
        if (mode == FootMode::Momentary) engaged = false;
        else if (mode == FootMode::Auto && sample - downAt >= rate * .4) engaged = original;
        return engaged;
    }
    bool value() const noexcept { return engaged; }
    bool isHeld() const noexcept { return held; }
private:
    double rate = 48000;
    FootMode mode = FootMode::Latch;
    bool engaged = false, original = false, held = false;
    int64_t downAt = 0, lastDown = -1;
};
}
