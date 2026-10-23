#pragma once
#include <array>
#include <cstdint>
namespace luthier::reference::common
{
enum class GestureKind : uint8_t { None, Tap, DoubleTap, Hold, Erase };
struct Gesture { GestureKind kind = GestureKind::None; int64_t sample = 0; };
class GestureDecoder
{
public:
    void prepare (double sampleRate) noexcept;
    void setThresholds (double tapMs, double doubleMs, double holdMs, double eraseMs) noexcept;
    void reset() noexcept;
    void setDoubleTapEnabled (bool v) noexcept { doubleEnabled = v; }
    void setStopped (bool v) noexcept { stopped = v; }
    void down (int64_t sample, float hardness = .5f) noexcept;
    void up (int64_t sample) noexcept;
    void advance (int64_t sample) noexcept;
    bool pop (Gesture&) noexcept;
private:
    void emit (GestureKind, int64_t) noexcept;
    double sr = 48000, tapMs = 350, doubleMs = 350, holdMs = 1000, eraseMs = 2000;
    int64_t downAt = 0, upAt = 0, doubleWindow = 16800;
    float hardness = .5f;
    bool held = false, pending = false, holdSent = false, eraseSent = false, doublePress = false;
    bool doubleEnabled = true, stopped = false;
    std::array<Gesture, 8> queue {};
    int head = 0, count = 0;
};
}
