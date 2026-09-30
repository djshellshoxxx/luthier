#include "StringMotionPolicy.h"
#include "../AnimationPolicy.h"

namespace luthier::StringMotionPolicy
{

namespace
{
    // The key of the test source in AnimationPolicy's source table.
    const char testSourceKey = 0;
}

Motion getMotion()
{
    switch (AnimationPolicy::get().getStringsStyle())
    {
        case AnimationPolicy::StringsStyle::Off:      return Motion::off;
        case AnimationPolicy::StringsStyle::LowStyle: return Motion::limited;
        case AnimationPolicy::StringsStyle::Full:
        default:                                      return Motion::full;
    }
}

void setOverrideForTesting (std::optional<Motion> motion)
{
    auto& policy = AnimationPolicy::get();

    if (! motion.has_value() || *motion == Motion::full)
        policy.removeSource (&testSourceKey);
    else
        policy.setSource (&testSourceKey, *motion == Motion::off ? QualityLevel::Low : QualityLevel::Medium, 0);
}

} // namespace luthier::StringMotionPolicy
