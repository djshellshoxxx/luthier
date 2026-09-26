#pragma once

/*  How much the strings may move: cpu-quality-modes.md 6's AnimationPolicy as
    the animated strings see it (animated-strings.md 2.6).

    A thin adapter, so the swap is one function body: when FEAT-CPU's
    Source/UI/AnimationPolicy lands, getMotion() returns
    AnimationPolicy::get().getMotion() (the strings are its Decorative class).
    Until then it derives the same answer from what exists: Reduced motion is
    Off; nothing yet produces Limited (CPU quality Medium / relief 1) or Off
    from CPU quality Low, and tests set those through the override.

        Full     the preference's quality, up to 60 Hz
        Limited  forced to the Low style at 30 Hz
        Off      no motion: the static overlay with its fixed glow
*/

#include <optional>

namespace luthier::StringMotionPolicy
{
    enum class Motion { full = 0, limited, off };

    /** Message thread. */
    Motion getMotion();

    /** Stands in for AnimationPolicy's inputs in tests; nullopt restores them. */
    void setOverrideForTesting (std::optional<Motion> motion);
}
