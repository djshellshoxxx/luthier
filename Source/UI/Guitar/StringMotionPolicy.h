#pragma once

/*  How much the strings may move: cpu-quality-modes.md 6's AnimationPolicy as
    the animated strings see it (animated-strings.md 2.6).

    Retired as a policy: it used to derive the answer itself (Reduced motion
    only), so CPU quality never reached the strings and the illustration (which
    reads AnimationPolicy) disagreed with them. It is now a read-only view of
    AnimationPolicy::getStringsStyle(), kept only as a bridge for older callers
    and tests. New code reads AnimationPolicy directly.

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

    /** Tests: feeds AnimationPolicy a source at the matching CPU quality level
        (Limited = Medium, Off = Low), so every consumer of the policy sees it;
        nullopt removes that source. */
    void setOverrideForTesting (std::optional<Motion> motion);
}
