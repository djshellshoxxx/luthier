#include "GestureDecoder.h"
#include <algorithm>
#include <cmath>
namespace luthier::reference::common
{
void GestureDecoder::prepare (double sampleRate) noexcept { sr = sampleRate > 0 ? sampleRate : 48000; reset(); }
void GestureDecoder::setThresholds (double tap, double dbl, double hold, double erase) noexcept
{
    tapMs = std::clamp (tap, 1., 5000.); doubleMs = std::clamp (dbl, 1., 5000.);
    holdMs = std::clamp (hold, 1., 10000.); eraseMs = std::clamp (erase, holdMs, 20000.);
}
void GestureDecoder::reset() noexcept { held = pending = holdSent = eraseSent = doublePress = false; head = count = 0; }
void GestureDecoder::emit (GestureKind kind, int64_t sample) noexcept
{ if (count < (int) queue.size()) { queue[(size_t) ((head + count) % 8)] = { kind, sample }; ++count; } }
void GestureDecoder::down (int64_t sample, float h) noexcept
{
    advance (sample);
    if (held) return;
    const bool second = pending && sample - upAt <= doubleWindow;
    if (second) { emit (GestureKind::DoubleTap, sample); pending = false; }
    held = true; doublePress = second; downAt = sample; holdSent = eraseSent = false;
    hardness = std::isfinite (h) ? std::clamp (h, 0.f, 1.f) : .5f;
    doubleWindow = (int64_t) std::llround (sr * (doubleMs * .001 * (1. - .2 * hardness)));
}
void GestureDecoder::up (int64_t sample) noexcept
{
    if (! held || sample < downAt) return;
    advance (sample); held = false;
    if (! holdSent && ! eraseSent && ! doublePress && sample - downAt <= (int64_t) std::llround (sr * tapMs * .001))
    {
        if (doubleEnabled) { pending = true; upAt = sample; }
        else emit (GestureKind::Tap, sample);
    }
}
void GestureDecoder::advance (int64_t sample) noexcept
{
    if (pending && sample - upAt > doubleWindow) { emit (GestureKind::Tap, upAt + doubleWindow); pending = false; }
    if (held && sample >= downAt)
    {
        const auto elapsed = sample - downAt;
        if (! stopped && ! holdSent && elapsed >= (int64_t) std::llround (sr * holdMs * .001))
        { emit (GestureKind::Hold, downAt + (int64_t) std::llround (sr * holdMs * .001)); holdSent = true; }
        if (stopped && ! eraseSent && elapsed >= (int64_t) std::llround (sr * eraseMs * .001))
        { emit (GestureKind::Erase, downAt + (int64_t) std::llround (sr * eraseMs * .001)); eraseSent = true; }
    }
}
bool GestureDecoder::pop (Gesture& out) noexcept
{ if (count == 0) return false; out = queue[(size_t) head]; head = (head + 1) % 8; --count; return true; }
}
