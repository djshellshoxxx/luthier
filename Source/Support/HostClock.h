#pragma once

#include <cmath>

namespace luthier::HostClock
{

inline bool isValidTempo (double bpm) noexcept
{
    return std::isfinite (bpm) && bpm > 0.0;
}

inline bool isValidPosition (double position) noexcept
{
    return std::isfinite (position);
}

} // namespace luthier::HostClock
