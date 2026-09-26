#include "StringMotionPolicy.h"
#include "../../Accessibility/Accessibility.h"

namespace luthier::StringMotionPolicy
{

namespace
{
    std::optional<Motion> testOverride;
}

Motion getMotion()
{
    if (testOverride.has_value())
        return *testOverride;

    // cpu-quality-modes 6: Reduced motion is Off. Swap for AnimationPolicy::get().getMotion().
    return AccessibilitySettings::get().isReducedMotion() ? Motion::off : Motion::full;
}

void setOverrideForTesting (std::optional<Motion> motion)
{
    testOverride = motion;
}

} // namespace luthier::StringMotionPolicy
