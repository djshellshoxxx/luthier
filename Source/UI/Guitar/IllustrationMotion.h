#pragma once

/*  The illustration's only motion (guitar-illustration.md 12.1, 16 and 19;
    visual-polish.md 0.4), and what replaces it under reduced motion.

    - A change of guitar crossfades over 250 ms from the old picture to the new
      one (12.1's family switch, and a part swap). Under reduced motion there is
      no crossfade: the new picture is there at once and the parts that changed
      carry a static outline (16) until the next click.
    - A played note's dot appears on the first frame after the audio note-on
      and fades over 60 ms when the note ends (19). Under reduced motion it is
      on or off, never in between.

    Pure state with the clock passed in, so the timing is testable without a
    window or a timer.
*/

#include <juce_graphics/juce_graphics.h>
#include "GuitarRenderer.h"

#include <array>

namespace luthier
{

//==============================================================================
struct SceneCrossfade
{
    static constexpr double kMs = 250.0;

    juce::Image previous;
    double startMs = -1.0;

    /** The picture is changing from `old`: fade from it, unless motion is reduced. */
    void begin (const juce::Image& old, double nowMs, bool reducedMotion)
    {
        if (reducedMotion || ! old.isValid())
        {
            previous = {};
            startMs = -1.0;
            return;
        }

        previous = old;
        startMs = nowMs;
    }

    /** How much of the old picture still shows, 1 -> 0 over 250 ms. */
    float alpha (double nowMs) const noexcept
    {
        if (startMs < 0.0 || ! previous.isValid())
            return 0.0f;

        const double t = (nowMs - startMs) / kMs;
        return t >= 1.0 ? 0.0f : (float) (1.0 - juce::jmax (0.0, t));
    }

    bool isActive (double nowMs) const noexcept { return alpha (nowMs) > 0.0f; }

    void finishIfDone (double nowMs)
    {
        if (! isActive (nowMs))
        {
            previous = {};
            startMs = -1.0;
        }
    }
};

//==============================================================================
struct NoteDots
{
    static constexpr double kDecayMs = 60.0;
    static constexpr float kOnLevel = 0.05f;   ///< the overlay's 0..1 string level

    std::array<float, 12> alpha {}, fret {};
    std::array<double, 12> offAtMs {};
    std::array<bool, 12> held {};

    /** One string, one frame. Returns true if what is drawn changed. */
    bool update (int s, float level, float fretNow, double nowMs, bool reducedMotion)
    {
        if (! juce::isPositiveAndBelow (s, 12))
            return false;

        const auto i = (size_t) s;
        const float before = alpha[i], fretBefore = fret[i];
        const bool sounding = level > kOnLevel && fretNow > 0.0f;

        if (sounding)
        {
            // It appears on the first frame that sees the note.
            alpha[i] = 1.0f;
            fret[i] = fretNow;
            held[i] = true;
        }
        else
        {
            if (held[i])
            {
                held[i] = false;
                offAtMs[i] = nowMs;
            }

            alpha[i] = reducedMotion ? 0.0f
                                     : (float) juce::jlimit (0.0, 1.0, 1.0 - (nowMs - offAtMs[i]) / kDecayMs);

            if (alpha[i] <= 0.0f)
                alpha[i] = 0.0f;
        }

        return alpha[i] != before || fret[i] != fretBefore;
    }

    void copyTo (GuitarOverlay& overlay) const
    {
        overlay.useDots = true;
        overlay.dotAlpha = alpha;
        overlay.dotFret = fret;
    }
};

//==============================================================================
/** 16: the regions whose description changed between two scenes - what a
    reduced-motion change outlines instead of fading. */
inline std::array<bool, (size_t) GuitarRegion::numRegions> changedRegions (const GuitarScene& before, const GuitarScene& after)
{
    std::array<bool, (size_t) GuitarRegion::numRegions> changed {};

    for (auto& h : after.hits)
    {
        bool same = false;

        for (auto& o : before.hits)
            if (o.region == h.region && o.description == h.description)
                same = true;

        if (! same && h.region != GuitarRegion::none)
            changed[(size_t) h.region] = true;
    }

    return changed;
}

} // namespace luthier
