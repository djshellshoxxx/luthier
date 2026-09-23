#include "EBowDriver.h"

namespace luthier
{

namespace
{
    /** Drive at the bottom of the swell: enough that a plucked note at half
        intensity reaches its target inside 500 ms (EBowTests). */
    constexpr double kDriveGain = 0.35;
    constexpr double kNarrowQ = 30.0;
    constexpr double kSoundingLevel = 1.0e-5;
}

void EBowDriver::reset() noexcept
{
    for (auto& p : peaks)
        p.reset();

    designedHz.fill (0.0);
    gain.fill (0.0);
    driving.fill (false);
}

void EBowDriver::setSettings (const EBowSettings& s) noexcept
{
    EBowSettings clamped = s;
    clamped.stringMask = s.stringMask & ((1 << kMaxStrings) - 1);
    clamped.intensity = juce::jlimit (0.0, 1.0, s.intensity);
    clamped.harmonic = juce::jlimit (1, 5, s.harmonic);

    if (clamped.harmonic != settings.harmonic)
        designedHz.fill (0.0);

    settings = clamped;
}

bool EBowDriver::isDrivingAny() const noexcept
{
    for (const bool d : driving)
        if (d)
            return true;

    return false;
}

void EBowDriver::beginBlock (const double* stringHz, const double* stringLevels, const bool* held,
                             int numStrings, std::array<bool, kMaxStrings>& letGo) noexcept
{
    letGo.fill (false);

    const double target = targetLevelFor (settings.intensity);

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const bool present = s < numStrings;
        const bool selected = settings.stringMask == 0 ? (present && held[s])
                                                       : (present && (settings.stringMask & (1 << s)) != 0);
        const bool drive = settings.enabled && selected && present && stringLevels[s] > kSoundingLevel;

        if (! drive)
        {
            if (driving[(size_t) s])
            {
                letGo[(size_t) s] = true;
                peaks[(size_t) s].reset();
            }

            driving[(size_t) s] = false;
            gain[(size_t) s] = 0.0;
            continue;
        }

        driving[(size_t) s] = true;

        // 2.2: the partial the player picked, of the note the string is on.
        const double f = juce::jlimit (40.0, sr * 0.45, stringHz[s] * (double) settings.harmonic);

        if (std::abs (f - designedHz[(size_t) s]) > f * 0.0005)
        {
            peaks[(size_t) s].setBandpass (sr, f, kNarrowQ);
            designedHz[(size_t) s] = f;
        }

        // Proportional to how far the string is below its target, so it swells
        // quickly and then holds rather than overshooting.
        const double shortfall = juce::jlimit (0.0, 1.0, 1.0 - stringLevels[s] / target);
        gain[(size_t) s] = kDriveGain * (0.25 + 0.75 * settings.intensity) * shortfall;
    }
}

} // namespace luthier
