#include "KitRoom.h"

namespace luthier
{

void KitRoom::prepare (double sampleRate)
{
    sr = sampleRate;

    // Mutually prime lengths near a small room's first reflections.
    const double ms[4] = { 29.7, 37.1, 41.1, 43.7 };

    for (int i = 0; i < 4; ++i)
    {
        lengths[(size_t) i] = juce::jmax (8, (int) std::round (ms[i] * 0.001 * sr));
        lines[(size_t) i].assign ((size_t) lengths[(size_t) i], 0.0);
        damp[(size_t) i].prepare (sr);
        damp[(size_t) i].setCutoff (6000.0);
    }

    dcLeft.prepare (sr, 7.0);
    dcRight.prepare (sr, 7.0);
    setDecay (0.25);
    reset();
}

void KitRoom::reset() noexcept
{
    for (int i = 0; i < 4; ++i)
    {
        std::fill (lines[(size_t) i].begin(), lines[(size_t) i].end(), 0.0);
        writePos[(size_t) i] = 0;
        damp[(size_t) i].reset();
    }

    dcLeft.reset();
    dcRight.reset();
}

void KitRoom::setDecay (double rt60Seconds) noexcept
{
    const double rt = juce::jlimit (0.1, 0.6, rt60Seconds);

    for (int i = 0; i < 4; ++i)
        gain[(size_t) i] = std::pow (10.0, -3.0 * (double) lengths[(size_t) i] / (rt * sr));
}

} // namespace luthier
