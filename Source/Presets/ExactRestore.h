#pragma once

/*  Storing and restoring a normalised parameter value so that it reads back
    exactly: save -> load -> save is byte-identical, and a reload reproduces
    what the host read before the save.

    A preset, a host chunk and a snapshot store each parameter as the
    normalised value getValue() reports. JUCE's float parameters keep the
    *plain* value inside, so applying a normalised value N with
    setValueNotifyingHost stores convertFrom0to1 (N), and the next getValue()
    returns convertTo0to1 (convertFrom0to1 (N)). For a linear range that is N
    again; for a skewed range (pow / exp / log in float) it can land a float
    ULP or two away, so the next save writes a different number - which
    clap-validator's state-reproducibility tests and HostStateTests catch.

    Two things have to hold at once, and each alone was tried and failed:

      - The stored value must be exactly what getValue() reported when the host
        saved: clap-validator reloads a state and compares every parameter with
        what it read before. An earlier fixed-point nudge at capture time moved
        skewed values a ULP away from that (a Whammy Up failure).

      - A restore must leave getValue() exactly on the stored value, or the
        next save differs. Applying the stored value directly does not.

    So the restore side searches: `inputFor` finds the normalised input, within
    a few ULPs of the stored value, whose plain-value round trip reads back as
    exactly that value. For any value that arrived through a set (a host's
    plain or normalised write, a slider, typed text, a reset to default) such an
    input exists - the value is the image of a round trip already - and the
    stored number is getValue() to the bit. The one plain value that never went
    through the round trip is a parameter's constructor default (3.0 exactly,
    say), and its normalised form can be unreachable from every input: the
    skewed map is one-to-one there and misses it. For that case alone the
    capture side (`storable`) writes the value the parameter would read back
    after any set, which is reachable, so the first restore lands exactly and
    every save after it is identical. The plain value moves by a ULP or two of
    the plain range, far below anything audible or displayed.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace luthier
{

namespace ExactRestore
{
    /** How far, in float ULPs of the normalised value, the search may look on
        either side of the stored value before settling for the closest miss. */
    inline constexpr int kMaxUlps = 32;

    /** The parameter as the one kind this applies to: a float parameter keeps
        the plain value and reads it back through its (possibly skewed) range.
        A bool keeps the normalised float it was given and reads it back as is,
        and an int or choice snaps on a linear range, so for those the stored
        value is applied directly, as before. */
    inline const juce::AudioParameterFloat* asFloat (const juce::AudioProcessorParameter& parameter) noexcept
    {
        return dynamic_cast<const juce::AudioParameterFloat*> (&parameter);
    }

    /** The normalised value getValue() would report after setValue (x). */
    inline float readBack (const juce::AudioParameterFloat& parameter, float x) noexcept
    {
        return parameter.convertTo0to1 (parameter.convertFrom0to1 (x));
    }

    /** The normalised input to hand setValueNotifyingHost so that getValue()
        afterwards returns `target` (clamped to 0..1) exactly - or, when no
        input within kMaxUlps does, the one whose read-back is closest to it.
        A parameter that is not a float one, or whose round trip is already
        exact, is returned the target itself. */
    inline float inputFor (const juce::AudioProcessorParameter& parameter, double storedNormalised) noexcept
    {
        const float target = (float) juce::jlimit (0.0, 1.0, storedNormalised);
        auto* ranged = asFloat (parameter);

        if (ranged == nullptr)
            return target;

        const float direct = readBack (*ranged, target);

        if (direct == target)
            return target;

        float best = target;
        float bestError = std::abs (direct - target);

        // Walk outwards a ULP at a time, alternating sides, so the first exact
        // hit is also the closest one to the stored value.
        float up = target, down = target;

        for (int step = 0; step < kMaxUlps; ++step)
        {
            up = std::nextafter (up, 2.0f);
            down = std::nextafter (down, -1.0f);

            for (const float x : { up, down })
            {
                if (x < 0.0f || x > 1.0f)
                    continue;

                const float error = std::abs (readBack (*ranged, x) - target);

                if (error == 0.0f)
                    return x;

                if (error < bestError)
                {
                    best = x;
                    bestError = error;
                }
            }
        }

        return best;
    }

    /** True when applying `normalised` through inputFor reads back exactly. */
    inline bool isRestorable (const juce::AudioProcessorParameter& parameter, float normalised) noexcept
    {
        auto* ranged = asFloat (parameter);
        return ranged == nullptr || readBack (*ranged, inputFor (parameter, normalised)) == normalised;
    }

    /** The value to write into a file for a parameter whose getValue() is
        `live`: `live` itself whenever a restore can reproduce it exactly (every
        value that arrived through a set), otherwise the nearest value the
        parameter reads back that can be - see the file comment. */
    inline double storable (const juce::AudioProcessorParameter& parameter, float live) noexcept
    {
        auto* ranged = asFloat (parameter);

        if (ranged == nullptr)
            return (double) live;

        float value = live;

        for (int pass = 0; pass < 8; ++pass)
        {
            if (isRestorable (parameter, value))
                break;

            value = readBack (*ranged, value);
        }

        return (double) value;
    }

    /** Applies a stored normalised value so that getValue() reads it back
        exactly where the parameter's range allows (see the file comment). */
    inline void applyNormalised (juce::AudioProcessorParameter& parameter, double storedNormalised)
    {
        parameter.setValueNotifyingHost (inputFor (parameter, storedNormalised));
    }
}

} // namespace luthier
