#pragma once

/*  Fingerstyle bass (bass-techniques.md 6) - MODEL-GAPS workstream.

    The default bass technique. Two things a real right hand does that a
    guitar's finger pluck does not:

    - Alternation. Index and middle take turns, and they are not the same
      finger: the middle one is longer, lands a little later and a little
      further from the bridge, and is a touch softer. `alternationVariation`
      (finger_alternation_variation, 0-1, default 0.25) scales that
      difference; at 0 the two fingers are identical. Nothing here is random,
      so a performance repeats exactly and the difference is the pulse, not
      noise.

    - The rest stroke. The plucking finger comes to rest on the next-lower
      string and stops it (rest_stroke, default on). That is why a real
      fingerstyle line is cleaner than the MIDI version: each note mutes the
      one below it.

    Only on a bass (0.1), only for a plain finger pluck: a pick, a slap, a pop
    or a ghost is not fingerstyle. The engine asks; this decides and shapes.
    Audio thread; nothing allocates.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

struct BassFingerstyleSettings
{
    double alternationVariation = 0.25;   ///< finger_alternation_variation
    bool restStroke = true;               ///< rest_stroke
};

/** How one finger's stroke differs from the reference (index) finger. */
struct FingerStroke
{
    int finger = 0;                  ///< 0 index, 1 middle
    double delaySeconds = 0.0;       ///< how much later than the grid it lands
    double brightnessScale = 1.0;    ///< on the excitation's brightness
    double pluckPositionOffset = 0.0;///< added to the pluck position (fraction of the string)
    double velocityScale = 1.0;
};

class BassFingerstyle
{
public:
    /** At variation 1: the middle finger is this much later ... */
    static constexpr double kMaxDelaySeconds = 0.006;
    /** ... this much darker ... */
    static constexpr double kMaxDarkening = 0.35;
    /** ... lands this much further from the bridge (fraction of the string) ... */
    static constexpr double kMaxPositionShift = 0.05;
    /** ... and this much softer. */
    static constexpr double kMaxSoftening = 0.10;

    /** How hard the resting finger stops the string below (Damping::Chuck's amount).
        0.9 takes a bass string down more than 12 dB inside 10 ms (the test). */
    static constexpr double kRestStrokeDamping = 0.9;

    /** A string plucked this recently is part of the same chord, not a string to rest on. */
    static constexpr double kSameStrokeSeconds = 0.030;

    void setSettings (const BassFingerstyleSettings& s) noexcept
    {
        settings.alternationVariation = juce::jlimit (0.0, 1.0, std::isfinite (s.alternationVariation) ? s.alternationVariation : 0.25);
        settings.restStroke = s.restStroke;
    }

    const BassFingerstyleSettings& getSettings() const noexcept { return settings; }

    void reset() noexcept { nextFinger = 0; }

    /** The next stroke, alternating index and middle. */
    FingerStroke next() noexcept
    {
        const auto stroke = strokeFor (nextFinger);
        nextFinger = 1 - nextFinger;
        return stroke;
    }

    /** Which finger plays next, without taking the stroke. */
    int peekFinger() const noexcept { return nextFinger; }

    FingerStroke strokeFor (int finger) const noexcept
    {
        FingerStroke s;
        s.finger = finger;

        if (finger == 0)
            return s;

        const double v = settings.alternationVariation;
        s.delaySeconds = kMaxDelaySeconds * v;
        s.brightnessScale = 1.0 - kMaxDarkening * v;
        s.pluckPositionOffset = kMaxPositionShift * v;
        s.velocityScale = 1.0 - kMaxSoftening * v;
        return s;
    }

    /** The string the finger comes to rest on: the next-lower one (string 0 is
        the highest). -1 when there is none or the rest stroke is off. */
    int restStringFor (int stringIndex, int numStrings) const noexcept
    {
        if (! settings.restStroke || stringIndex + 1 >= numStrings)
            return -1;

        return stringIndex + 1;
    }

private:
    BassFingerstyleSettings settings;
    int nextFinger = 0;
};

} // namespace luthier
